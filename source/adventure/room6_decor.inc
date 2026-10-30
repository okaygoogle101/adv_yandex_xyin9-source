// Room 6 / decorative treasure. No per-frame or per-player work.
// Map geometry supplies pedestal collision; only this named model is made nonsolid.
new g_Room6Treasure;

stock Room6_Init()
{
    g_Room6Treasure = Map_FindEntity("room6_fake_treasure");
    Room6_Reset();
}

stock Room6_Reset()
{
    if (!pev_valid(g_Room6Treasure)) return;
    new name[32], classname[16], model[64];
    pev(g_Room6Treasure, pev_targetname, name, charsmax(name));
    pev(g_Room6Treasure, pev_classname, classname, charsmax(classname));
    pev(g_Room6Treasure, pev_model, model, charsmax(model));
    if (!equal(name, "room6_fake_treasure") || !equal(classname, "cycler")
        || !equal(model, "models/yx_xyin9/yx_fake_treasure.mdl")) return;

    set_pev(g_Room6Treasure, pev_solid, SOLID_NOT);
    set_pev(g_Room6Treasure, pev_takedamage, 0.0);
    new Float:origin[3];
    pev(g_Room6Treasure, pev_origin, origin);
    engfunc(EngFunc_SetOrigin, g_Room6Treasure, origin);
}
