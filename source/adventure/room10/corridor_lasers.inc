// Optional difficulty button before room10. Native teleports handle contact;
// no player roster, per-player tasks, traces or damage callbacks are required.
const Float:R10_CORRIDOR_DURATION = 60.0;
const Float:R10_CORRIDOR_COOLDOWN = 300.0;
new bool:g_R10CorridorReady, bool:g_R10CorridorActive, bool:g_R10CorridorLocked;
new g_R10CorridorButton, g_R10CorridorVisual, g_R10CorridorTrigger;
new Float:g_R10CorridorEndsAt, Float:g_R10CorridorReadyAt;

stock bool:Room10Corridor_Init()
{
    if (!Map_FindEntity("adventure_corridor_lasers_v86")) return true;
    g_R10CorridorButton=Map_FindEntity("room10_corridor_button");
    g_R10CorridorVisual=Map_FindEntity("room10_corridor_lasers");
    g_R10CorridorTrigger=Map_FindEntity("room10_corridor_return");
    if (!g_R10CorridorButton || !g_R10CorridorVisual || !g_R10CorridorTrigger
        || !Map_FindEntity("room10_corridor_start"))
    { set_fail_state("Corridor laser entities missing (map v86).");return false; }
    g_R10CorridorReady=true;
    return true;
}

stock Room10Corridor_SetActive(bool:on)
{
    g_R10CorridorActive=on;
    Entity_SetVisible(g_R10CorridorVisual,on);
    set_pev(g_R10CorridorTrigger,pev_solid,on?SOLID_TRIGGER:SOLID_NOT);
    new Float:p[3];pev(g_R10CorridorTrigger,pev_origin,p);
    engfunc(EngFunc_SetOrigin,g_R10CorridorTrigger,p); // relink native touch volume
}

stock Room10Corridor_Use(ent,id)
{
    if (!g_R10CorridorReady || ent!=g_R10CorridorButton) return HAM_IGNORED;
    if (id<1 || id>g_MaxPlayers || !Room10_CanUse(ent,id)) return HAM_SUPERCEDE;
    new Float:now=get_gametime();
    if (now<g_R10CorridorReadyAt) return HAM_SUPERCEDE;
    g_R10CorridorReadyAt=now+R10_CORRIDOR_COOLDOWN;
    g_R10CorridorEndsAt=now+R10_CORRIDOR_DURATION;
    g_R10CorridorLocked=true;
    set_pev(g_R10CorridorButton,pev_frame,1.0);
    Room10Corridor_SetActive(true);
    emit_sound(ent,CHAN_ITEM,"buttons/button3.wav",0.7,ATTN_NORM,0,100);
    return HAM_SUPERCEDE;
}

stock Room10Corridor_Update(Float:now)
{
    if (!g_R10CorridorReady) return;
    if (g_R10CorridorActive && now>=g_R10CorridorEndsAt) Room10Corridor_SetActive(false);
    if (g_R10CorridorLocked && now>=g_R10CorridorReadyAt)
    {
        g_R10CorridorLocked=false;
        set_pev(g_R10CorridorButton,pev_frame,0.0);
    }
}

stock Room10Corridor_Reset()
{
    if (!g_R10CorridorReady) return;
    Room10Corridor_SetActive(false);
    // A round restart disables the lasers, but does not bypass the shared cooldown.
    g_R10CorridorLocked=get_gametime()<g_R10CorridorReadyAt;
    set_pev(g_R10CorridorButton,pev_frame,g_R10CorridorLocked?1.0:0.0);
}
