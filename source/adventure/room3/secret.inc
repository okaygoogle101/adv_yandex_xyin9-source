// Optional v112 branch. The native teleport is enabled only by a completed Chaos run.
new g_ChaosSecretTrigger, g_ChaosSecretLight;
new bool:g_ChaosSecretEnabled;

stock bool:ChaosSecret_Init()
{
    g_ChaosSecretTrigger = Map_FindEntity("chaos_secret_trigger");
    g_ChaosSecretLight = Map_FindEntity("chaos_secret_light");
    if (!g_ChaosSecretTrigger && !g_ChaosSecretLight) return true;
    if (!pev_valid(g_ChaosSecretTrigger) || !pev_valid(g_ChaosSecretLight)
        || !Map_FindEntity("chaos_secret_arrival") || !Map_FindEntity("room4_arrival"))
    {
        set_fail_state("Chaos secret branch: incomplete map entities");
        return false;
    }
    RegisterHamFromEntity(Ham_Touch, g_ChaosSecretTrigger, "ChaosSecret_Touch");
    ChaosSecret_SetEnabled(false);
    return true;
}

stock ChaosSecret_SetEnabled(bool:enabled)
{
    g_ChaosSecretEnabled = enabled;
    if (!pev_valid(g_ChaosSecretTrigger) || !pev_valid(g_ChaosSecretLight)) return;
    Entity_SetVisible(g_ChaosSecretLight, enabled);
    set_pev(g_ChaosSecretTrigger, pev_solid, enabled ? SOLID_TRIGGER : SOLID_NOT);
    new Float:origin[3];
    pev(g_ChaosSecretTrigger, pev_origin, origin);
    engfunc(EngFunc_SetOrigin, g_ChaosSecretTrigger, origin);
}

public ChaosSecret_Touch(ent, id)
{
    if (ent != g_ChaosSecretTrigger) return HAM_IGNORED;
    if (!g_PluginReady || !g_ChaosSecretEnabled || g_Room3Phase != ROOM3_EXIT_OPEN
        || id < 1 || id > g_MaxPlayers || !is_user_alive(id)) return HAM_SUPERCEDE;
    return HAM_IGNORED; // Normal map teleport, no participant list or per-player timers.
}
