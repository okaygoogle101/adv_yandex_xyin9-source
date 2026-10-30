// Room 2: water pit and stair landing. Owns the optional waterdrop obstacle.
// Internal include; compile adventure_trust.sma, not this file.

new g_WaterdropPlatform, g_WaterdropButton, g_WaterdropHoleCount;
new Float:g_WaterdropHoles[32][3], Float:g_WaterdropBottom, Float:g_WaterdropTop;
new Float:g_WaterdropEndsAt, Float:g_WaterdropReadyAt, Float:g_WaterdropTipAt[33];
new bool:g_WaterdropActive, bool:g_WaterdropButtonOn;

stock bool:Waterdrop_Init()
{
    g_WaterdropPlatform = Map_FindEntity("waterdrop_platform");
    g_WaterdropButton = Map_FindEntity("waterdrop_button");
    if (!g_WaterdropPlatform && !g_WaterdropButton) return true;
    // v15 compatibility
    if (!pev_valid(g_WaterdropPlatform) || !pev_valid(g_WaterdropButton))
    {
        set_fail_state("Waterdrop platform/button missing in map.");
        return false;
    }
    new ent =  - 1;
    while ((ent = engfunc(EngFunc_FindEntityByString, ent, "targetname", "waterdrop_hole")) > 0)
    {
        if (g_WaterdropHoleCount >= 32)
        {
            set_fail_state("Too many waterdrop hole markers.");
            return false;
        }
        pev(ent, pev_origin, g_WaterdropHoles[g_WaterdropHoleCount]);
        g_WaterdropHoleCount++;
    }
    if (g_WaterdropHoleCount != 20)
    {
        set_fail_state("Expected 20 waterdrop holes in v16.");
        return false;
    }
    new Float:lo[3], Float:hi[3];
    pev(g_WaterdropPlatform, pev_absmin, lo);
    pev(g_WaterdropPlatform, pev_absmax, hi);
    g_WaterdropBottom = lo[2] + 1.0;
    g_WaterdropTop = hi[2] - 1.0;
    return true;
}

stock Waterdrop_Use(id)
{
    if (id < 1 || id > g_MaxPlayers || !is_user_alive(id)) return HAM_SUPERCEDE;
    if (!(pev(id, pev_button) & IN_USE) || !Player_IsAimingAt(id, g_WaterdropButton)) return HAM_SUPERCEDE;
    new Float:now = get_gametime();
    if (now < g_WaterdropReadyAt)
    {
        if (now >= g_WaterdropTipAt[id])
        {
            g_WaterdropTipAt[id] = now + 1.0;
        }
        return HAM_SUPERCEDE;
    }
    // Commit both deadlines before enabling collision; simultaneous users cannot extend them.
    g_WaterdropEndsAt = now + WATERDROP_ACTIVE_SECONDS;
    g_WaterdropReadyAt = now + WATERDROP_COOLDOWN_SECONDS;
    Waterdrop_SetActive(true);
    set_pev(g_WaterdropButton, pev_frame, 1.0);
    g_WaterdropButtonOn = true;
    return HAM_SUPERCEDE;
}

stock Waterdrop_Update(Float:now)
{
    if (!g_WaterdropPlatform) return;
    if (g_WaterdropActive && now >= g_WaterdropEndsAt) Waterdrop_SetActive(false);
    if (g_WaterdropButtonOn && now >= g_WaterdropReadyAt)
    {
        g_WaterdropButtonOn = false;
        set_pev(g_WaterdropButton, pev_frame, 0.0);
    }
}

stock bool:Waterdrop_FitsHole(const Float:lo[3], const Float:hi[3])
{
    for (new h = 0; h < g_WaterdropHoleCount; h++)
    if (lo[0] >= g_WaterdropHoles[h][0] - 24.0 && hi[0] <= g_WaterdropHoles[h][0] + 24.0 && lo[1] >= g_WaterdropHoles[h][1] - 24.0 && hi[1] <= g_WaterdropHoles[h][1] + 24.0) return true;
    return false;
}

stock Waterdrop_SetActive(bool:on)
{
    if (!g_WaterdropPlatform) return;
    g_WaterdropActive = on;
    ExecuteHamB(Ham_Use, g_WaterdropPlatform, 0, 0, on ? 1 : 0, 0.0);
    if (!on) return;
    // A falling player intersecting a newly appearing solid must not get stuck
    // inside it. Hole passages and all players swimming below it remain safe.
    new Float:lo[3], Float:hi[3];
    for (new id = 1; id <= g_MaxPlayers; id++) if (is_user_alive(id))
    {
        pev(id, pev_absmin, lo);
        pev(id, pev_absmax, hi);
        for (new a = 0; a < 3; a++)
        {
            lo[a] += 1.0;
            hi[a] -= 1.0;
        }
        if (hi[2] <= g_WaterdropBottom || lo[2] >= g_WaterdropTop || hi[0] <=  - 512.0 || lo[0] >= 512.0 || hi[1] <=  - 512.0 || lo[1] >= 512.0) continue;
        if (!Waterdrop_FitsHole(lo, hi)) ExecuteHamB(Ham_Killed, id, 0, 0);
    }
}

public Waterdrop_OnPlayerPostThink(id)
{
    if (!g_PluginReady || !g_WaterdropActive || !is_user_alive(id)) return HAM_IGNORED;
    // A miss is lethal, independent of armor or reduced fall-damage settings.
    // Ground entity distinguishes landing on the platform from touching its underside.
    if ((pev(id, pev_flags) & FL_ONGROUND) && pev(id, pev_groundentity) == g_WaterdropPlatform)
    ExecuteHamB(Ham_Killed, id, 0, 0);
    return HAM_IGNORED;
}
