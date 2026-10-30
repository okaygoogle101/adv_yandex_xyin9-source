// Room 3 / game 4: 14 independent full-width moving beams. No fairness filter.
// Each beam deals 100 HP once per player per game.
#define LASER_COUNT 14
#define LASER_TYPES 3
new const Float:LASER_BOTTOM[LASER_TYPES] = {6.0, 26.0, 63.0};
new const Float:LASER_TOP[LASER_TYPES] = {14.0, 34.0, 71.0};
new const Float:LASER_COLOR[LASER_TYPES][3] = {{40.0,110.0,255.0},{40.0,110.0,255.0},{255.0,40.0,40.0}};
new const Float:LASER_CORE_COLOR[LASER_TYPES][3] = {{150.0,205.0,255.0},{150.0,205.0,255.0},{255.0,170.0,170.0}};
enum { LASER_DORMANT, LASER_PREVIEW, LASER_MOVING, LASER_FINISHED };
new bool:g_LaserReady;
new g_LaserEntity[LASER_COUNT][2], g_LaserModel[LASER_TYPES][2][16];
new g_LaserWall[LASER_COUNT], g_LaserType[LASER_COUNT], g_LaserStage[LASER_COUNT];
new g_LaserHitUserId[LASER_COUNT][33];
new Float:g_LaserFireAt[LASER_COUNT], Float:g_LaserDistance[LASER_COUNT], Float:g_LaserNextBeepAt;

stock bool:Laser_Init()
{
    if (!Map_FindEntity("adventure_lasers_v29"))
    {
        set_fail_state("Laser game requires adventure_start_v29.map and plugin 3.7.0.");
        return false;
    }
    new name[64];
    for (new type = 0; type < LASER_TYPES; type++) for (new core = 0; core < 2; core++)
    {
        formatex(name, charsmax(name), "room3_laser_template_%d_%d", type, core);
        new sample = Map_FindEntity(name);
        if (!pev_valid(sample)) return Room3_FailMissingEntities();
        pev(sample, pev_model, g_LaserModel[type][core], 15);
        if (g_LaserModel[type][core][0] != '*') return Room3_FailMissingEntities();
        Entity_SetVisible(sample, false);
    }
    // Reuse these entities throughout the map. No per-frame creation or temp beams.
    for (new i = 0; i < LASER_COUNT; i++) for (new core = 0; core < 2; core++)
    {
        new ent = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "info_target"));
        if (!pev_valid(ent)) return Room3_FailMissingEntities();
        g_LaserEntity[i][core] = ent;
        engfunc(EngFunc_SetModel, ent, g_LaserModel[0][core]);
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_movetype, MOVETYPE_PUSH);
        set_pev(ent, pev_rendermode, kRenderTransColor);
        Entity_SetVisible(ent, false);
    }
    g_LaserReady = true;
    Laser_Reset();
    return true;
}

stock Laser_Reset()
{
    if (!g_LaserReady) return;
    for (new i = 0; i < LASER_COUNT; i++)
    {
        g_LaserStage[i] = LASER_DORMANT;
        g_LaserFireAt[i] = 0.0;
        g_LaserDistance[i] = 0.0;
        for (new core = 0; core < 2; core++) Entity_SetVisible(g_LaserEntity[i][core], false);
        for (new id = 1; id <= 32; id++) g_LaserHitUserId[i][id] = 0;
    }
    g_LaserNextBeepAt = 0.0;
}

stock Float:Laser_Coordinate(i, Float:distance)
{
    if (g_LaserWall[i] == 0) return -1216.0 - distance;
    if (g_LaserWall[i] == 1) return -2496.0 + distance;
    if (g_LaserWall[i] == 2) return -640.0 + distance;
    return 640.0 - distance;
}

stock Laser_Move(i, Float:distance)
{
    new Float:o[3];
    o[0] = 0.0;
    o[1] = -1856.0;
    o[2] = -1336.0;
    new axis = g_LaserWall[i] < 2 ? 1 : 0;
    o[axis] = Laser_Coordinate(i, distance);
    for (new core = 0; core < 2; core++) engfunc(EngFunc_SetOrigin, g_LaserEntity[i][core], o);
}

stock Laser_Show(i, bool:preview)
{
    new type = g_LaserType[i];
    for (new core = 0; core < 2; core++)
    {
        new ent = g_LaserEntity[i][core];
        set_pev(ent, pev_renderamt, preview ? 110.0 : (core ? 255.0 : 235.0));
        if (core) set_pev(ent, pev_rendercolor, LASER_CORE_COLOR[type]);
        else set_pev(ent, pev_rendercolor, LASER_COLOR[type]);
        Entity_SetVisible(ent, true);
    }
}

stock Laser_Prepare(Float:now, bool:reduced = false)
{
    Laser_Reset();
    new order[4];
    for (new wall = 0; wall < 4; wall++) order[wall] = wall;
    // Uniform random wall order. Types and intervals are drawn independently.
    for (new last = 3; last > 0; last--)
    {
        new pick = random_num(0, last), temp = order[last];
        order[last] = order[pick];
        order[pick] = temp;
    }
    new i;
    for (new rank = 0; rank < 4; rank++)
    {
        new wall = order[rank], count = wall < 2 ? 3 : 4;
        new Float:fireAt = now + LASER_WARNING_SECONDS + float(rank) * LASER_WALL_STAGGER_SECONDS;
        for (new shot = 0; shot < count; shot++)
        {
            g_LaserWall[i] = wall;
            g_LaserType[i] = random_num(0, LASER_TYPES - 1);
            g_LaserFireAt[i] = fireAt;
            new Float:angles[3];
            angles[1] = wall < 2 ? 90.0 : 0.0;
            for (new core = 0; core < 2; core++)
            {
                engfunc(EngFunc_SetModel, g_LaserEntity[i][core], g_LaserModel[g_LaserType[i]][core]);
                set_pev(g_LaserEntity[i][core], pev_angles, angles);
            }
            Laser_Move(i, 0.0);
            i++;
            fireAt += random_float(LASER_INTERVAL_MIN, LASER_INTERVAL_MAX);
        }
    }
    // Keep alternate scheduled beams: seven, with the original spacing.
    if (reduced) for (new slot = 1; slot < LASER_COUNT; slot += 2) g_LaserStage[slot] = LASER_FINISHED;
    g_Room3Phase = ROOM3_LASER_WARNING;
    g_Room3PhaseEndsAt = now + LASER_WARNING_SECONDS;
    g_LaserNextBeepAt = now + 1.0;
    Room3_PlaySound("buttons/blip1.wav");
}

stock Laser_HitPlayer(id, i)
{
    new uid = Player_ReadUserId(id);
    if (g_LaserHitUserId[i][id] == uid) return;
    g_LaserHitUserId[i][id] = uid;
    // DROWN bypasses armor; this can kill, but preserves health above 100.
    ExecuteHamB(Ham_TakeDamage, id, g_LaserEntity[i][0], 0, LASER_DAMAGE, DMG_DROWN);
}

stock Laser_CheckSegment(id, i, Float:from, Float:to)
{
    if (!is_user_alive(id) || g_LaserHitUserId[i][id] == Player_ReadUserId(id)) return;
    if (!Room3_IsInsideArena(id)) return;
    new Float:lo[3], Float:hi[3];
    Player_ReadMin(id, lo);
    Player_ReadMax(id, hi);
    for (new a = 0; a < 3; a++) { lo[a] += 1.0; hi[a] -= 1.0; }
    new type = g_LaserType[i];
    if (hi[2] < -1336.0 + LASER_BOTTOM[type] || lo[2] > -1336.0 + LASER_TOP[type]) return;
    new axis = g_LaserWall[i] < 2 ? 1 : 0;
    new Float:start = Laser_Coordinate(i, from), Float:end = Laser_Coordinate(i, to);
    // Swept 8-unit-thick band: even a long server frame cannot skip a standing player.
    if (hi[axis] >= floatmin(start, end) - 4.0 && lo[axis] <= floatmax(start, end) + 4.0) Laser_HitPlayer(id, i);
}

stock Laser_CheckHazards(id)
{
    if (!g_LaserReady || g_Room3Phase != ROOM3_LASER_ACTIVE) return;
    for (new i = 0; i < LASER_COUNT; i++) if (g_LaserStage[i] == LASER_MOVING)
        Laser_CheckSegment(id, i, g_LaserDistance[i], g_LaserDistance[i]);
}

stock bool:Laser_Update(Float:now)
{
    if (g_Room3Phase == ROOM3_LASER_WARNING)
    {
        if (now >= g_Room3PhaseEndsAt) g_Room3Phase = ROOM3_LASER_ACTIVE;
        else if (now >= g_LaserNextBeepAt)
        {
            Room3_PlaySound("buttons/blip1.wav");
            g_LaserNextBeepAt += 1.0;
        }
    }
    new finished;
    for (new i = 0; i < LASER_COUNT; i++)
    {
        if (g_LaserStage[i] == LASER_FINISHED) { finished++; continue; }
        if (now < g_LaserFireAt[i] - LASER_PREVIEW_SECONDS) continue;
        if (g_LaserStage[i] == LASER_DORMANT)
        {
            g_LaserStage[i] = LASER_PREVIEW;
            Laser_Show(i, true);
            emit_sound(g_LaserEntity[i][0], CHAN_ITEM, "buttons/button3.wav", 0.6, ATTN_NORM, 0, 110);
        }
        if (now < g_LaserFireAt[i]) continue;
        if (g_LaserStage[i] == LASER_PREVIEW)
        {
            g_LaserStage[i] = LASER_MOVING;
            Laser_Show(i, false);
        }
        new Float:distance = floatmin(1280.0, floatmax(0.0, (now - g_LaserFireAt[i]) * 1280.0 / LASER_TRAVEL_SECONDS));
        for (new id = 1; id <= g_MaxPlayers; id++) Laser_CheckSegment(id, i, g_LaserDistance[i], distance);
        g_LaserDistance[i] = distance;
        Laser_Move(i, distance);
        if (distance >= 1280.0)
        {
            g_LaserStage[i] = LASER_FINISHED;
            for (new core = 0; core < 2; core++) Entity_SetVisible(g_LaserEntity[i][core], false);
            finished++;
        }
    }
    return finished == LASER_COUNT;
}
