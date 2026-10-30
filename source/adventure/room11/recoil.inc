// Room 11 shaft: six horizontal ceilings, three native moving openings.
// Unlimited airborne recoil; no participant roster. Landing on a ceiling retries.
const R11_CLIP_OFFSET = 51;
const R11_WEAPON_LINUX_DIFF = 4;
const Float:R11_IMPULSE = 110.0;
const Float:R11_HORIZONTAL_LIMIT = 420.0;
const Float:R11_VERTICAL_LIMIT = 440.0;
const R11_CLIENT_CLIP_OFFSET = 52; // m_iClientClip: byte208 Win,224 Linux/Mac.
new bool:g_R11Map, bool:g_R11Ready;
new g_R11Static, g_R11Button, g_R11Mover[3], g_R11Direction[3];
new Float:g_R11Home[3][3], Float:g_R11WaitUntil[3];
new g_R11BlockedUid[33];
new Float:g_R11UseAt[33];
new const R11_AXIS[3] = {0,1,0};
new const Float:R11_TRAVEL[3] = {112.0,112.0,112.0};
new const Float:R11_SPEED[3] = {56.0,64.0,72.0};
new const R11_START_SIDE[3] = {-1,1,-1};
new const Float:R11_ZERO[3] = {0.0,0.0,0.0};

stock Room11_Precache()
{
    if (!g_R11Map) return;
    new ent=engfunc(EngFunc_CreateNamedEntity,engfunc(EngFunc_AllocString,"weapon_m249"));
    if (pev_valid(ent)) {ExecuteHam(Ham_Precache,ent);engfunc(EngFunc_RemoveEntity,ent);}
}

stock bool:Room11_Init()
{
    if (!g_R11Map) return true;
    g_R11Button=Map_FindEntity("room11_m249_button");
    if (!g_R11Button || !Map_FindEntity("room11_arrival") || !Map_FindEntity("room11_ceiling_static"))
    {set_fail_state("Room11 flight requires map v90 (equipment/ceilings missing)");return false;}
    g_R11Static=Map_FindEntity("room11_ceiling_static");
    new name[40];
    for (new i=0;i<3;i++)
    {
        formatex(name,charsmax(name),"room11_ceiling_mover_%d",i);
        g_R11Mover[i]=Map_FindEntity(name);
        if (!g_R11Mover[i]) {set_fail_state("Room11 flight moving gate missing");return false;}
        pev(g_R11Mover[i],pev_origin,g_R11Home[i]);
        set_pev(g_R11Mover[i],pev_movetype,MOVETYPE_PUSH);
    }
    RegisterHam(Ham_Blocked,"func_wall","Room11_Blocked");
    g_R11Ready=true;
    Room11_Reset();
    return true;
}

stock Room11_ClearPlayer(id)
{
    g_R11BlockedUid[id]=0;
    g_R11UseAt[id]=0.0;
}

stock bool:Room11_IsPlayer(id)
{
    new Float:p[3];
    return Room11_PlayerZone(id, p) == 1;
}

stock Room11_Reset()
{
    for (new id=1;id<=32;id++) Room11_ClearPlayer(id);
    if (!g_R11Ready) return;
    new Float:now=get_gametime();
    for (new i=0;i<3;i++)
    {
        new Float:p[3];for (new a=0;a<3;a++) p[a]=g_R11Home[i][a];
        p[R11_AXIS[i]]+=float(R11_START_SIDE[i])*R11_TRAVEL[i];
        set_pev(g_R11Mover[i],pev_velocity,R11_ZERO);
        engfunc(EngFunc_SetOrigin,g_R11Mover[i],p);
        g_R11Direction[i]=-R11_START_SIDE[i];
        g_R11WaitUntil[i]=now+float(i)*0.35;
    }
}

stock Room11_Stop()
{
    if (!g_R11Ready) return;
    for (new i=0;i<3;i++) if (pev_valid(g_R11Mover[i])) set_pev(g_R11Mover[i],pev_velocity,R11_ZERO);
}

stock Room11_Return(id)
{
    // Try distinct positions on the wide start dock if solid players occupy the
    // ordinary native teleport destination. Never force someone inside a solid.
    new Float:p[3],bool:placed;
    for (new slot=0;slot<32;slot++)
    {
        p[0]=-2200.0+float((slot%8)-3)*56.0;
        p[1]=680.0+float(slot/8)*48.0;p[2]=-3675.0;
        if (Player_TryPlace(id,p)) {placed=true;break;}
    }
    if (!placed) return;
    new Float:angles[3];angles[1]=90.0;
    set_pev(id,pev_angles,angles);set_pev(id,pev_v_angle,angles);set_pev(id,pev_fixangle,1);
    set_pev(id,pev_flFallVelocity,0.0);
    g_R11BlockedUid[id]=0;
}

stock Room11_PreThinkInZone(id, const Float:p[3])
{
    if (Room11_ContactsGateAt(id,false,p)) {Room11_Return(id);return FMRES_IGNORED;}
    if (pev(id,pev_flags)&FL_ONGROUND)
    {
        new ground=pev(id,pev_groundentity);
        // The floor and final landing are safe; ceiling tops are not checkpoints.
        new bool:ceiling=ground==g_R11Static;
        for (new i=0;i<3;i++) if (ground==g_R11Mover[i]) ceiling=true;
        if (ceiling) Room11_Return(id);
    }
    return FMRES_IGNORED;
}

public Room11_Blocked(ent,other)
{
    if (!g_R11Ready) return HAM_IGNORED;
    new bool:ours;for (new i=0;i<3;i++) if (ent==g_R11Mover[i]) ours=true;
    if (!ours) return HAM_IGNORED;
    if (other>=1 && other<=g_MaxPlayers && Room11_IsPlayer(other))
        g_R11BlockedUid[other]=get_user_userid(other);
    return HAM_SUPERCEDE;
}

stock Room11_OnFrame(Float:now)
{
    if (!g_R11Ready) return;
    // Resolve only actual trapped players, after native pusher rollback has ended.
    for (new id=1;id<=g_MaxPlayers;id++) if (g_R11BlockedUid[id])
    {
        new uid=g_R11BlockedUid[id];g_R11BlockedUid[id]=0;
        if (Room11_IsPlayer(id) && get_user_userid(id)==uid) Room11_Return(id);
    }
    new Float:dt;global_get(glb_frametime,dt);dt=floatmax(0.001,dt);
    for (new i=0;i<3;i++)
    {
        new ent=g_R11Mover[i];
        if (now<g_R11WaitUntil[i]) continue; // Reset / arrival already stopped the pusher.
        new axis=R11_AXIS[i];new Float:p[3],Float:v[3],Float:localTime;
        pev(ent,pev_origin,p);pev(ent,pev_ltime,localTime);
        new Float:target=g_R11Home[i][axis]+float(g_R11Direction[i])*R11_TRAVEL[i];
        new Float:diff=target-p[axis];
        if (floatabs(diff)<0.1)
        {
            set_pev(ent,pev_velocity,R11_ZERO);
            g_R11Direction[i]=-g_R11Direction[i];g_R11WaitUntil[i]=now+0.4;continue;
        }
        v[axis]=(diff>0.0?1.0:-1.0)*floatmin(R11_SPEED[i],floatabs(diff)/dt);
        // A push brush needs a future local think time to advance in GoldSrc.
        set_pev(ent,pev_nextthink,localTime+1.0);set_pev(ent,pev_velocity,v);
    }
}

stock Room11_Use(ent,id)
{
    if (!g_R11Ready || ent!=g_R11Button) return HAM_IGNORED;
    if (!Room11_IsPlayer(id) || !(pev(id,pev_button)&IN_USE) || !Player_IsAimingAt(id,ent)) return HAM_SUPERCEDE;
    new Float:now=get_gametime();if (now<g_R11UseAt[id]) return HAM_SUPERCEDE;
    g_R11UseAt[id]=now+0.5;Room11_GiveM249(id);return HAM_SUPERCEDE;
}

public client_putinserver(id)
{
    Room11_ClearPlayer(id);
    Room11Flight_ClearPlayer(id);
}

stock Room11_GiveM249(id)
{
    if (!user_has_weapon(id, CSW_M249))
    {
        new weapons[32], count, name[24]; get_user_weapons(id, weapons, count);
        for (new i = 0; i < count; i++)
        {
            switch (weapons[i])
            {
                case CSW_SCOUT, CSW_XM1014, CSW_MAC10, CSW_AUG, CSW_UMP45, CSW_SG550,
                     CSW_GALIL, CSW_FAMAS, CSW_AWP, CSW_MP5NAVY, CSW_M3, CSW_M4A1,
                     CSW_TMP, CSW_G3SG1, CSW_SG552, CSW_AK47, CSW_P90:
                { get_weaponname(weapons[i], name, charsmax(name)); engclient_cmd(id, "drop", name); }
            }
        }
        new weapon = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "weapon_m249"));
        if (pev_valid(weapon))
        {
            set_pev(weapon, pev_spawnflags, 1 << 30);
            ExecuteHamB(Ham_Spawn, weapon);
            new Float:p[3]; pev(id, pev_origin, p); engfunc(EngFunc_SetOrigin, weapon, p);
            ExecuteHamB(Ham_Touch, weapon, id);
            if (pev_valid(weapon) && pev(weapon, pev_owner) != id) engfunc(EngFunc_RemoveEntity, weapon);
        }
    }
    if (!user_has_weapon(id, CSW_M249)) return;
    ExecuteHamB(Ham_GiveAmmo, id, 200, "556NatoBox", 200);
    new weapon = -1;
    while ((weapon = engfunc(EngFunc_FindEntityByString, weapon, "classname", "weapon_m249")) > 0)
        if (pev_valid(weapon) == 2 && pev(weapon, pev_owner) == id)
        {
            set_pdata_int(weapon, R11_CLIP_OFFSET, 100, R11_WEAPON_LINUX_DIFF);
            break;
        }
    engclient_cmd(id, "weapon_m249");
}

stock Room11_PlayerZone(id, Float:p[3])
{
    if (!is_user_alive(id) || pev(id,pev_iuser1)!=0 || pev(id,pev_movetype)!=MOVETYPE_WALK) return 0;
    pev(id,pev_origin,p);
    if (p[0]>-2840.0 && p[0]<-1560.0 && p[1]>460.0 && p[1]<1740.0
        && p[2]>-3728.0 && p[2]<-1280.0) return 1;
    if (p[0]>-1024.0 && p[0]<256.0 && p[1]>-3520.0 && p[1]<2800.0
        && p[2]>-3904.0 && p[2]<-2880.0) return 2;
    return 0;
}
