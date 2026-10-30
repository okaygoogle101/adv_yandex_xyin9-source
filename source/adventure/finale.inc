// Final hall: protect only the narrow landing below the 1000-unit drop.
// No player state, think loop, global fall-damage changes or weapon handling.
stock Finale_Init()
{
    if (Map_FindEntity("adventure_finale_v98"))
        RegisterHam(Ham_TakeDamage, "player", "Finale_OnDamage");
}

public Finale_OnDamage(id, inflictor, attacker, Float:damage, damageBits)
{
    if (!g_PluginReady || damageBits != DMG_FALL || !is_user_alive(id)) return HAM_IGNORED;
    new Float:p[3]; pev(id, pev_origin, p);
    if (p[0] < -2620.0 || p[0] > -2580.0 || p[1] < -3020.0 || p[1] > -2940.0
        || p[2] < -3800.0 || p[2] > -3726.0) return HAM_IGNORED;
    return HAM_SUPERCEDE;
}
