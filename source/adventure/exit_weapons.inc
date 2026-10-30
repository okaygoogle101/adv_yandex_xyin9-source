// Remove only the room's weapon after its native exit teleport succeeds.
// Touch snapshots live only between the synchronous pre/post callbacks.
new g_ExitWeaponTrigger[2], g_ExitWeaponTouch[33];
new Float:g_ExitWeaponBefore[33][3];

stock ExitWeapons_Init()
{
    g_ExitWeaponTrigger[0]=Map_FindEntity("room8_to_train");
    g_ExitWeaponTrigger[1]=Map_FindEntity("room10_exit_trigger");
    RegisterHam(Ham_Touch,"trigger_teleport","ExitWeapons_TouchPre");
    RegisterHam(Ham_Touch,"trigger_teleport","ExitWeapons_TouchPost",1);
}
stock bool:ExitWeapons_Matches(ent)
{
    return ent>0 && (ent==g_ExitWeaponTrigger[0] || ent==g_ExitWeaponTrigger[1]);
}
public ExitWeapons_TouchPre(ent,id)
{
    if (!g_PluginReady || !ExitWeapons_Matches(ent) || id<1 || id>g_MaxPlayers) return HAM_IGNORED;
    g_ExitWeaponTouch[id]=0;
    if (!is_user_alive(id) || pev(id,pev_iuser1)!=0 || pev(ent,pev_solid)!=SOLID_TRIGGER) return HAM_IGNORED;
    pev(id,pev_origin,g_ExitWeaponBefore[id]);
    g_ExitWeaponTouch[id]=ent;
    return HAM_IGNORED;
}
public ExitWeapons_TouchPost(ent,id)
{
    if (!g_PluginReady || !ExitWeapons_Matches(ent) || id<1 || id>g_MaxPlayers) return HAM_IGNORED;
    if (g_ExitWeaponTouch[id]!=ent) return HAM_IGNORED;
    g_ExitWeaponTouch[id]=0;
    if (!is_user_alive(id)) return HAM_IGNORED;
    new Float:p[3]; pev(id,pev_origin,p);
    // Disabled/master-locked or superseded touches must not remove a weapon.
    if (get_distance_f(p,g_ExitWeaponBefore[id])<1.0) return HAM_IGNORED;
    if (ent==g_ExitWeaponTrigger[0]) ExitWeapons_Remove(id,CSW_USP,"weapon_usp");
    else ExitWeapons_Remove(id,CSW_SCOUT,"weapon_scout");
    return HAM_IGNORED;
}
stock ExitWeapons_Remove(id,weaponId,const classname[])
{
    if (!user_has_weapon(id,weaponId)) return;
    new weapon=-1;
    while ((weapon=engfunc(EngFunc_FindEntityByString,weapon,"classname",classname))>0)
    {
        if (pev_valid(weapon)!=2 || pev(weapon,pev_owner)!=id) continue;
        if (get_user_weapon(id)==weaponId) ExecuteHamB(Ham_Weapon_RetireWeapon,weapon);
        if (!ExecuteHamB(Ham_RemovePlayerItem,id,weapon)) return;
        set_pev(id,pev_weapons,pev(id,pev_weapons)&~(1<<weaponId));
        if (weaponId==CSW_SCOUT)
        {
            const primaryMask=(1<<CSW_SCOUT)|(1<<CSW_XM1014)|(1<<CSW_MAC10)|(1<<CSW_AUG)
                |(1<<CSW_UMP45)|(1<<CSW_SG550)|(1<<CSW_GALIL)|(1<<CSW_FAMAS)|(1<<CSW_AWP)
                |(1<<CSW_MP5NAVY)|(1<<CSW_M3)|(1<<CSW_M4A1)|(1<<CSW_TMP)|(1<<CSW_G3SG1)
                |(1<<CSW_SG552)|(1<<CSW_AK47)|(1<<CSW_P90)|(1<<CSW_M249);
            // Same AMXX cstrike field as the existing room11 weapon cleanup.
            set_pdata_bool(id,464,(pev(id,pev_weapons)&primaryMask)!=0,20);
        }
        // Reserve ammo is shared with other guns; preserve it and all other items.
        ExecuteHamB(Ham_Item_Kill,weapon);
        return;
    }
}
