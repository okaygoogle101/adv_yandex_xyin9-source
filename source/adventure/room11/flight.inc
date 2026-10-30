// Room 11 continuation: unlimited airborne M249 recoil and nine native pushers.
// No participant roster or timed completion. The floor uses a native teleport.
const Float:R11F_IMPULSE = 110.0;
const Float:R11F_HORIZONTAL_LIMIT = 420.0;
const Float:R11F_VERTICAL_LIMIT = 440.0;
new bool:g_R11FMap, bool:g_R11FReady;
const R11F_COUNT = 9;
new g_R11FButton, g_R11FMover[R11F_COUNT], g_R11FDirection[R11F_COUNT];
new Float:g_R11FHome[R11F_COUNT][3], Float:g_R11FWaitUntil[R11F_COUNT];
new g_R11FBlockedUid[33];
new Float:g_R11FUseAt[33];
new const R11F_AXIS[R11F_COUNT] = {0,2,0,0,2,0,0,2,0};
new const Float:R11F_TRAVEL[R11F_COUNT] = {112.0,96.0,112.0,144.0,112.0,128.0,112.0,112.0,128.0};
new const Float:R11F_SPEED[R11F_COUNT] = {76.8,67.2,76.8,76.8,67.2,86.4,76.8,67.2,86.4};
new const R11F_START_SIDE[R11F_COUNT] = {-1,1,1,-1,-1,1,-1,1,-1};
new const Float:R11F_ZERO[3] = {0.0,0.0,0.0};

stock Room11Flight_Precache()
{
    if (!g_R11FMap || g_R11Map) return; // Shaft already precaches the same gun.
    new ent=engfunc(EngFunc_CreateNamedEntity,engfunc(EngFunc_AllocString,"weapon_m249"));
    if (pev_valid(ent)) {ExecuteHam(Ham_Precache,ent);engfunc(EngFunc_RemoveEntity,ent);}
}

stock bool:Room11Flight_Init()
{
    if (!g_R11FMap) return true;
    g_R11FButton=Map_FindEntity("room11_flight_button");
    if (!g_R11FButton || !Map_FindEntity("room11_flight_arrival") || !Map_FindEntity("room11_flight_floor_return"))
    {set_fail_state("Room11 flight requires map v108 (equipment/return missing)");return false;}
    new name[40];
    for (new i=0;i<R11F_COUNT;i++)
    {
        formatex(name,charsmax(name),"room11_flight_mover_%d",i);
        g_R11FMover[i]=Map_FindEntity(name);
        if (!g_R11FMover[i]) {set_fail_state("Room11 flight requires map v108: one of nine moving walls is missing");return false;}
        pev(g_R11FMover[i],pev_origin,g_R11FHome[i]);
        set_pev(g_R11FMover[i],pev_movetype,MOVETYPE_PUSH);
    }
    RegisterHam(Ham_Blocked,"func_wall","Room11Flight_Blocked");
    g_R11FReady=true;
    Room11Flight_Reset();
    return true;
}

stock Room11Flight_ClearPlayer(id)
{
    g_R11FBlockedUid[id]=0;
    g_R11FUseAt[id]=0.0;
}

stock bool:Room11Flight_IsPlayer(id)
{
    new Float:p[3];
    return Room11_PlayerZone(id, p) == 2;
}

stock Room11Flight_Reset()
{
    for (new id=1;id<=32;id++) Room11Flight_ClearPlayer(id);
    if (!g_R11FReady) return;
    new Float:now=get_gametime();
    for (new i=0;i<R11F_COUNT;i++)
    {
        new Float:p[3];for (new a=0;a<3;a++) p[a]=g_R11FHome[i][a];
        p[R11F_AXIS[i]]+=float(R11F_START_SIDE[i])*R11F_TRAVEL[i];
        set_pev(g_R11FMover[i],pev_velocity,R11F_ZERO);
        engfunc(EngFunc_SetOrigin,g_R11FMover[i],p);
        g_R11FDirection[i]=-R11F_START_SIDE[i];
        g_R11FWaitUntil[i]=now+float(i)*0.35;
    }
}

stock Room11Flight_Stop()
{
    if (!g_R11FReady) return;
    for (new i=0;i<R11F_COUNT;i++) if (pev_valid(g_R11FMover[i])) set_pev(g_R11FMover[i],pev_velocity,R11F_ZERO);
}

stock Room11Flight_Return(id)
{
    // Try distinct positions on the wide start dock if solid players occupy the
    // ordinary native teleport destination. Never force someone inside a solid.
    new Float:p[3],bool:placed;
    for (new slot=0;slot<32;slot++)
    {
        p[0]=-384.0+float((slot%8)-3)*56.0;
        p[1]=-3296.0-float(slot/8)*48.0;p[2]=-3483.0;
        if (Player_TryPlace(id,p)) {placed=true;break;}
    }
    if (!placed) return;
    new Float:angles[3];angles[1]=90.0;
    set_pev(id,pev_angles,angles);set_pev(id,pev_v_angle,angles);set_pev(id,pev_fixangle,1);
    set_pev(id,pev_flFallVelocity,0.0);
    g_R11FBlockedUid[id]=0;
}

stock Room11Flight_PreThinkInZone(id, const Float:p[3])
{
    if (Room11_ContactsGateAt(id,true,p)) {Room11Flight_Return(id);return FMRES_IGNORED;}
    Room11Flight_CheckFinishAt(id,p);
    if (pev(id,pev_flags)&FL_ONGROUND)
    {
        // The docks are safe. Landing on an obstacle's narrow rim is a failed
        // flight, rather than an unintended intermediate checkpoint.
        if (p[1]>-2784.0 && p[1]<2192.0) Room11Flight_Return(id);
    }
    return FMRES_IGNORED;
}

public Room11Flight_Blocked(ent,other)
{
    if (!g_R11FReady) return HAM_IGNORED;
    new bool:ours;for (new i=0;i<R11F_COUNT;i++) if (ent==g_R11FMover[i]) ours=true;
    if (!ours) return HAM_IGNORED;
    if (other>=1 && other<=g_MaxPlayers && Room11Flight_IsPlayer(other))
        g_R11FBlockedUid[other]=get_user_userid(other);
    return HAM_SUPERCEDE;
}

stock Room11Flight_OnFrame(Float:now)
{
    if (!g_R11FReady) return;
    // Resolve only actual trapped players, after native pusher rollback has ended.
    for (new id=1;id<=g_MaxPlayers;id++) if (g_R11FBlockedUid[id])
    {
        new uid=g_R11FBlockedUid[id];g_R11FBlockedUid[id]=0;
        if (Room11Flight_IsPlayer(id) && get_user_userid(id)==uid) Room11Flight_Return(id);
    }
    new Float:dt;global_get(glb_frametime,dt);dt=floatmax(0.001,dt);
    for (new i=0;i<R11F_COUNT;i++)
    {
        new ent=g_R11FMover[i];
        if (now<g_R11FWaitUntil[i]) continue; // Reset / arrival already stopped the pusher.
        new axis=R11F_AXIS[i];new Float:p[3],Float:v[3],Float:localTime;
        pev(ent,pev_origin,p);pev(ent,pev_ltime,localTime);
        new Float:target=g_R11FHome[i][axis]+float(g_R11FDirection[i])*R11F_TRAVEL[i];
        new Float:diff=target-p[axis];
        if (floatabs(diff)<0.1)
        {
            set_pev(ent,pev_velocity,R11F_ZERO);
            g_R11FDirection[i]=-g_R11FDirection[i];g_R11FWaitUntil[i]=now+0.4;continue;
        }
        v[axis]=(diff>0.0?1.0:-1.0)*floatmin(R11F_SPEED[i],floatabs(diff)/dt);
        // A push brush needs a future local think time to advance in GoldSrc.
        set_pev(ent,pev_nextthink,localTime+1.0);set_pev(ent,pev_velocity,v);
    }
}

stock Room11Flight_Use(ent,id)
{
    if (!g_R11FReady || ent!=g_R11FButton) return HAM_IGNORED;
    if (!Room11Flight_IsPlayer(id) || !(pev(id,pev_button)&IN_USE) || !Player_IsAimingAt(id,ent)) return HAM_SUPERCEDE;
    new Float:now=get_gametime();if (now<g_R11FUseAt[id]) return HAM_SUPERCEDE;
    g_R11FUseAt[id]=now+0.5;Room11_GiveM249(id);return HAM_SUPERCEDE;
}

stock Room11Flight_CheckFinishAt(id, const Float:p[3])
{
    if (!user_has_weapon(id,CSW_M249)) return;
    // Vertical finish plane at the leading edge of the dock. The room bounds
    // supply its full width and floor-to-ceiling height, including airborne players.
    // Keep the far side covered too: high speed cannot skip a thin trigger slab.
    if (p[1]<2208.0) return;
    new weapon=-1;
    while ((weapon=engfunc(EngFunc_FindEntityByString,weapon,"classname","weapon_m249"))>0)
    {
        if (pev_valid(weapon)!=2 || pev(weapon,pev_owner)!=id) continue;
        if (get_user_weapon(id)==CSW_M249) ExecuteHamB(Ham_Weapon_RetireWeapon,weapon);
        if (!ExecuteHamB(Ham_RemovePlayerItem,id,weapon)) return;
        set_pev(id,pev_weapons,pev(id,pev_weapons)&~(1<<CSW_M249));
        // AMXX cstrike gamedata: m_bHasPrimary byte464 (+20 Linux),
        // m_rgAmmo int376 (+5 Linux); M249 uses ammo slot3 (556NatoBox).
        const primaryMask=(1<<CSW_SCOUT)|(1<<CSW_XM1014)|(1<<CSW_MAC10)|(1<<CSW_AUG)
            |(1<<CSW_UMP45)|(1<<CSW_SG550)|(1<<CSW_GALIL)|(1<<CSW_FAMAS)|(1<<CSW_AWP)
            |(1<<CSW_MP5NAVY)|(1<<CSW_M3)|(1<<CSW_M4A1)|(1<<CSW_TMP)|(1<<CSW_G3SG1)
            |(1<<CSW_SG552)|(1<<CSW_AK47)|(1<<CSW_P90);
        set_pdata_bool(id,464,(pev(id,pev_weapons)&primaryMask)!=0,20);
        set_pdata_int(id,379,0,5);
        ExecuteHamB(Ham_Item_Kill,weapon);
        return;
    }
}
