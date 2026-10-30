// Room 11 only: replace native M249 bullets with the existing flight impulse.
// No FireBullets / PlaybackEvent => no bullet damage, impact sounds or decals.
// AMXX CS gamedata: weapon float offsets 46/47/48, Linux adjustment +4.
const R11S_NEXT_PRIMARY = 46;
const R11S_NEXT_SECONDARY = 47;
const R11S_IDLE = 48;
const Float:R11S_CYCLE = 0.1;

stock Room11Silent_Init()
{
    if (!g_R11Ready && !g_R11FReady) return;
    RegisterHam(Ham_Weapon_PrimaryAttack,"weapon_m249","Room11Silent_AttackPre");
    RegisterHam(Ham_CS_Weapon_SendWeaponAnim,"weapon_m249","Room11Silent_AnimPre");
    register_forward(FM_UpdateClientData,"Room11Silent_ClientDataPost",1);
}

stock bool:Room11Silent_IsPlayer(id)
{
    if (!g_PluginReady || id<1 || id>g_MaxPlayers) return false;
    new Float:p[3],zone=Room11_PlayerZone(id,p);
    return (g_R11Ready && zone==1) || (g_R11FReady && zone==2);
}

public Room11Silent_ClientDataPost(id,sendWeapons,cd)
{
    if (get_cd(cd,CD_ID)!=CSW_M249 || !Room11Silent_IsPlayer(id)) return FMRES_IGNORED;
    // Only the outgoing client copy: the real server m_flNextAttack is untouched.
    // A long positive countdown prevents predicted shots even with packet delay.
    // Outside the zone / after switching weapons, native data is sent unchanged.
    set_cd(cd,CD_flNextAttack,3600.0);
    return FMRES_HANDLED;
}

public Room11Silent_AnimPre(weapon,anim,skiplocal)
{
    if (pev_valid(weapon)!=2) return HAM_IGNORED;
    new id=pev(weapon,pev_owner);
    if (!Room11Silent_IsPlayer(id) || get_user_weapon(id)!=CSW_M249) return HAM_IGNORED;
    // Prediction no longer runs ItemPostFrame: deliver idle/draw animations too.
    SetHamParamInteger(3,0);
    return HAM_HANDLED;
}

public Room11Silent_AttackPre(weapon)
{
    if (pev_valid(weapon)!=2) return HAM_IGNORED;
    new id=pev(weapon,pev_owner);
    if (!Room11Silent_IsPlayer(id) || get_user_weapon(id)!=CSW_M249) return HAM_IGNORED;
    // ItemPostFrame normally checks this itself. Keep explicit protection from
    // duplicate PrimaryAttack calls without allowing a native shot through.
    if (get_pdata_float(weapon,R11S_NEXT_PRIMARY,R11_WEAPON_LINUX_DIFF)>0.0)
        return HAM_SUPERCEDE;
    set_pdata_float(weapon,R11S_NEXT_PRIMARY,R11S_CYCLE,R11_WEAPON_LINUX_DIFF);
    set_pdata_float(weapon,R11S_NEXT_SECONDARY,R11S_CYCLE,R11_WEAPON_LINUX_DIFF);
    set_pdata_float(weapon,R11S_IDLE,1.6,R11_WEAPON_LINUX_DIFF);
    set_pdata_int(weapon,R11_CLIP_OFFSET,100,R11_WEAPON_LINUX_DIFF);
    // Only request an ammo HUD correction when it differs, avoiding 10 reliable
    // CurWeapon messages per second per player for an unchanged magazine.
    if (get_pdata_int(weapon,R11_CLIENT_CLIP_OFFSET,R11_WEAPON_LINUX_DIFF)!=100)
        set_pdata_int(weapon,R11_CLIENT_CLIP_OFFSET,-1,R11_WEAPON_LINUX_DIFF);
    // Stock v_m249.mdl sequences 1/2 are shoot1/shoot2; no sound events in them.
    ExecuteHam(Ham_CS_Weapon_SendWeaponAnim,weapon,random_num(1,2),0);
    if (pev(id,pev_flags)&FL_ONGROUND) return HAM_SUPERCEDE;
    if (g_R11Ready && Room11_IsPlayer(id))
        Room11Silent_Impulse(id,R11_IMPULSE,R11_HORIZONTAL_LIMIT,R11_VERTICAL_LIMIT);
    else
        Room11Silent_Impulse(id,R11F_IMPULSE,R11F_HORIZONTAL_LIMIT,R11F_VERTICAL_LIMIT);
    return HAM_SUPERCEDE;
}

stock Room11Silent_Impulse(id,Float:impulse,Float:horizontalLimit,Float:verticalLimit)
{
    new Float:aim[3],Float:dir[3],Float:v[3];
    pev(id,pev_v_angle,aim);
    engfunc(EngFunc_MakeVectors,aim);global_get(glb_v_forward,dir);
    pev(id,pev_velocity,v);
    for (new a=0;a<3;a++) v[a]-=dir[a]*impulse;
    new Float:speed=floatsqroot(v[0]*v[0]+v[1]*v[1]);
    if (speed>horizontalLimit)
    {new Float:scale=horizontalLimit/speed;v[0]*=scale;v[1]*=scale;}
    v[2]=floatclamp(v[2],-verticalLimit,verticalLimit);
    set_pev(id,pev_velocity,v);
}
