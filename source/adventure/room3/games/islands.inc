// Room 3 / game 6: timed stepping stones on the existing 10x10 floor.
// One reusable non-solid light slab per tile. Damage uses the same slab height.
#define ISLAND_TILE_COUNT 100
new bool:g_IslandsReady;
new g_IslandTile[ISLAND_TILE_COUNT], g_IslandCurrent, g_IslandNext;
new g_IslandTileModel[16];
new g_IslandPrevious, g_IslandTransition;
new g_IslandCurtain[2], g_IslandCurtainModel[2][16];
new bool:g_IslandOrange;
new Float:g_IslandBeepAt, Float:g_IslandPulseAt;

stock bool:Islands_Init()
{
    new sample = Map_FindEntity("room3_island_template");
    if (!pev_valid(sample))
    {
        set_fail_state("Game 6 requires adventure_start_v31.map (island tile template).");
        return false;
    }
    pev(sample, pev_model, g_IslandTileModel, charsmax(g_IslandTileModel));
    if (g_IslandTileModel[0] != '*') return Room3_FailMissingEntities();
    Entity_SetVisible(sample, false);
    for (new tile = 0; tile < ISLAND_TILE_COUNT; tile++) g_IslandTile[tile] = g_BlockWarning[tile];
    new name[48];
    for (new variant = 0; variant < 2; variant++)
    {
        formatex(name, charsmax(name), "room3_island_curtain_%d", variant);
        new source = Map_FindEntity(name);
        if (!pev_valid(source)) return Room3_FailMissingEntities();
        pev(source, pev_model, g_IslandCurtainModel[variant], 15);
        Entity_SetVisible(source, false);
        new ent = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "info_target"));
        if (!pev_valid(ent)) return Room3_FailMissingEntities();
        g_IslandCurtain[variant] = ent;
        engfunc(EngFunc_SetModel, ent, g_IslandCurtainModel[0]);
        set_pev(ent, pev_movetype, MOVETYPE_PUSH);
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_rendermode, kRenderTransAdd);
        set_pev(ent, pev_renderamt, 200.0);
        Entity_SetVisible(ent, false);
    }
    g_IslandsReady = true;
    Islands_Reset();
    return true;
}

stock Islands_Reset()
{
    if (!g_IslandsReady) return;
    for (new tile = 0; tile < ISLAND_TILE_COUNT; tile++) Entity_SetVisible(g_IslandTile[tile], false);
    for (new i = 0; i < 2; i++) Entity_SetVisible(g_IslandCurtain[i], false);
    g_IslandOrange = false;
    g_IslandCurrent = -1;
    g_IslandNext = -1;
    g_IslandPrevious = -1;
    g_IslandTransition = 0;
    g_IslandBeepAt = 0.0;
    g_IslandPulseAt = 0.0;
}

stock Islands_Paint(tile, bool:safe)
{
    new Float:color[3];
    color[0] = safe ? 40.0 : 255.0;
    color[1] = safe ? 255.0 : 35.0;
    color[2] = safe ? 90.0 : 45.0;
    set_pev(g_IslandTile[tile], pev_rendercolor, color);
    set_pev(g_IslandTile[tile], pev_renderamt, safe ? 155.0 : 105.0);
    Entity_SetVisible(g_IslandTile[tile], true);
}

stock Islands_ShowCurtain(slot, tile, bool:orange = false)
{
    new Float:o[3];
    o[0] = -576.0 + float(tile % 10) * 128.0;
    o[1] = -2432.0 + float(tile / 10) * 128.0;
    o[2] = -1336.0;
    engfunc(EngFunc_SetModel, g_IslandCurtain[slot], g_IslandCurtainModel[orange ? 1 : 0]);
    engfunc(EngFunc_SetOrigin, g_IslandCurtain[slot], o);
    set_pev(g_IslandCurtain[slot], pev_renderamt, 200.0);
    Entity_SetVisible(g_IslandCurtain[slot], true);
}

stock Islands_Prepare(Float:now)
{
    Islands_Reset();
    Islands_ConfigureTiles();
    g_IslandCurrent = 44 + random_num(0, 1) + 10 * random_num(0, 1);
    Islands_Paint(g_IslandCurrent, true);
    Islands_ShowCurtain(0, g_IslandCurrent);
    g_Room3Phase = ROOM3_ISLAND_PREPARE;
    g_Room3PhaseEndsAt = now + ISLAND_PREPARE_SECONDS;
    g_IslandBeepAt = now + 1.0;
    Room3_PlaySound("buttons/button1.wav");
}

stock Islands_ChooseNext()
{
    new col = g_IslandCurrent % 10, row = g_IslandCurrent / 10;
    new candidates[4], count, fallback = -1;
    // Ten cardinal transitions, each crossing one intervening tile.
    for (new direction = 0; direction < 4; direction++)
    {
        new dc, dr;
        if (direction < 2) dc = direction == 0 ? 2 : -2;
        else dr = direction == 2 ? 2 : -2;
        new x = col + dc, y = row + dr;
        if (x < 0 || x >= 10 || y < 0 || y >= 10) continue;
        new tile = y * 10 + x;
        fallback = tile;
        if (tile == g_IslandPrevious) continue;
        candidates[count++] = tile;
    }
    // Defensive fallback for externally changed state; never random_num(0, -1).
    if (!count) return fallback;
    return candidates[random_num(0, count - 1)];
}

stock Islands_BeginStep(Float:now)
{
    g_IslandTransition++;
    g_IslandNext = Islands_ChooseNext();
    Islands_Paint(g_IslandNext, true);
    g_Room3Phase = ROOM3_ISLAND_ACTIVE;
    g_Room3PhaseEndsAt = now + ISLAND_STEP_SECONDS + (g_IslandTransition <= 5 ? 1.0 : 0.0);
    g_IslandOrange = false;
    Islands_ShowCurtain(0, g_IslandCurrent);
    Islands_ShowCurtain(1, g_IslandNext);
    g_IslandPulseAt = now;
    emit_sound(g_IslandCurtain[1], CHAN_ITEM, "buttons/button3.wav", 1.0, ATTN_NORM, 0, 100);
}

stock bool:Islands_Update(Float:now)
{
    if (g_Room3Phase == ROOM3_ISLAND_PREPARE)
    {
        if (now >= g_Room3PhaseEndsAt)
        {
            for (new tile = 0; tile < ISLAND_TILE_COUNT; tile++) Islands_Paint(tile, tile == g_IslandCurrent);
            Islands_BeginStep(now);
        }
        else if (now >= g_IslandBeepAt)
        {
            Room3_PlaySound("buttons/blip1.wav");
            g_IslandBeepAt = now + 1.0;
        }
    }
    else if (g_Room3Phase == ROOM3_ISLAND_ACTIVE)
    {
        if (now >= g_Room3PhaseEndsAt)
        {
            Islands_Paint(g_IslandCurrent, false);
            g_IslandPrevious = g_IslandCurrent;
            g_IslandCurrent = g_IslandNext;
            g_IslandNext = -1;
            Islands_Paint(g_IslandCurrent, true);
            Islands_ShowCurtain(0, g_IslandCurrent);
            Entity_SetVisible(g_IslandCurtain[1], false);
            if (g_IslandTransition >= ISLAND_TRANSITIONS)
            {
                g_Room3Phase = ROOM3_ISLAND_FINISH;
                g_Room3PhaseEndsAt = now + ISLAND_FINISH_SECONDS;
            }
            else Islands_BeginStep(now);
        }
        else
        {
            if (!g_IslandOrange && now >= g_Room3PhaseEndsAt - 2.0)
            {
                g_IslandOrange = true;
                Islands_ShowCurtain(0, g_IslandCurrent, true);
                new Float:orange[3]; orange[0] = 255.0; orange[1] = 110.0; orange[2] = 15.0;
                set_pev(g_IslandTile[g_IslandCurrent], pev_rendercolor, orange);
            }
            if (now >= g_IslandPulseAt)
            {
                new Float:amount = 140.0 + 50.0 * floatsin(now * 360.0, degrees);
                set_pev(g_IslandCurtain[1], pev_renderamt, amount + 30.0);
                g_IslandPulseAt = now + 0.1;
            }
        }
    }
    else if (g_Room3Phase == ROOM3_ISLAND_FINISH && now >= g_Room3PhaseEndsAt) return true;
    return false;
}

stock bool:Islands_IsSafeFootprint(Float:x, Float:y)
{
    for (new sx = -1; sx <= 1; sx += 2) for (new sy = -1; sy <= 1; sy += 2)
    {
        new col = floatround((x + float(sx) * 15.99 + 640.0) / 128.0, floatround_floor);
        new row = floatround((y + float(sy) * 15.99 + 2496.0) / 128.0, floatround_floor);
        if (col < 0 || col >= 10 || row < 0 || row >= 10) return false;
        new tile = row * 10 + col;
        if (tile != g_IslandCurrent && tile != g_IslandNext) return false;
    }
    return true;
}

stock Islands_CheckHazards(id)
{
    new Float:o[3], Float:lo[3], Float:hi[3];
    Player_ReadOrigin(id, o);
    Player_ReadMin(id, lo);
    Player_ReadMax(id, hi);
    // Red light is five units above the floor; jumping above it is safe.
    if (lo[2] + 1.0 <= -1330.0 && hi[2] - 1.0 >= -1331.0
        && !Islands_IsSafeFootprint(o[0], o[1])) Player_Kill(id);
}

stock Islands_ConfigureTiles()
{
    new Float:o[3];
    for (new tile = 0; tile < ISLAND_TILE_COUNT; tile++)
    {
        new ent = g_IslandTile[tile];
        engfunc(EngFunc_SetModel, ent, g_IslandTileModel);
        set_pev(ent, pev_rendermode, kRenderTransColor);
        o[0] = -576.0 + float(tile % 10) * 128.0;
        o[1] = -2432.0 + float(tile / 10) * 128.0;
        o[2] = -1331.0;
        engfunc(EngFunc_SetOrigin, ent, o);
    }
}
