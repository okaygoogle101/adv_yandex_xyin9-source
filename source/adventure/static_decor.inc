// Four static MDLs preserve every 2x2x2 star, its color and world position.
new g_StaticStars[4];
new const Float:STATIC_STAR_MIN[4][3] = {{-379.0,-316.0,-97.0},{41.0,-316.0,-97.0},{-379.0,104.0,-97.0},{41.0,104.0,-97.0}};
new const Float:STATIC_STAR_MAX[4][3] = {{41.0,105.0,465.0},{463.0,105.0,465.0},{42.0,526.0,-47.0},{463.0,526.0,-47.0}};
stock bool:StaticDecor_Init()
{
    new name[32];
    for (new i = 0; i < 4; i++)
    {
        formatex(name, charsmax(name), "room4_stars_%d", i);
        g_StaticStars[i] = Map_FindEntity(name);
        if (!pev_valid(g_StaticStars[i])) { set_fail_state("Optimized star models/map required."); return false; }
    }
    StaticDecor_Reset();
    return true;
}
stock StaticDecor_Reset()
{
    for (new i = 0; i < 4; i++) if (pev_valid(g_StaticStars[i]))
    {
        new ent = g_StaticStars[i];
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_takedamage, DAMAGE_NO);
        set_pev(ent, pev_movetype, MOVETYPE_NONE);
        set_pev(ent, pev_framerate, 0.0);
        set_pev(ent, pev_nextthink, 0.0);
        engfunc(EngFunc_SetSize, ent, STATIC_STAR_MIN[i], STATIC_STAR_MAX[i]);
        new Float:o[3]; pev(ent, pev_origin, o); engfunc(EngFunc_SetOrigin, ent, o);
    }
}
