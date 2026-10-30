// Room 3 / game 3: four shelters and a descending ceiling, three rounds.
// The crusher is visual/non-solid: damage is explicit so bodies cannot block it.
#define CEILING_CELL_COUNT 32
#define CEILING_CELLS_PER_WALL 8
const Float:CEILING_TOP_Z = -968.0;
const Float:CEILING_FLOOR_Z = -1336.0;

new bool:g_CeilingReady, bool:g_CeilingMovingSound;
new g_CeilingEntity, g_CeilingRound;
new g_CeilingDoor[CEILING_CELL_COUNT], g_CeilingGlow[CEILING_CELL_COUNT];
new bool:g_CeilingCellOpen[CEILING_CELL_COUNT];
new g_CeilingPrevious[4];
new Float:g_CeilingOrigin[3], Float:g_CeilingBottom;
new Float:g_CeilingCellMin[CEILING_CELL_COUNT][3], Float:g_CeilingCellMax[CEILING_CELL_COUNT][3];
new Float:g_CeilingCellExit[CEILING_CELL_COUNT][3], Float:g_CeilingNextBeepAt;

stock bool:Ceiling_Init()
{
    if (!Map_FindEntity("adventure_ceiling_v26"))
    {
        set_fail_state("Game 3 requires matching ceiling and wall-cell geometry (map v26 or later).");
        return false;
    }
    g_CeilingEntity = Map_FindEntity("room3_crusher");
    if (!pev_valid(g_CeilingEntity)) return Room3_FailMissingEntities();
    pev(g_CeilingEntity, pev_origin, g_CeilingOrigin);
    set_pev(g_CeilingEntity, pev_solid, SOLID_NOT);
    new name[64], Float:center[3];
    for (new c = 0; c < CEILING_CELL_COUNT; c++)
    {
        formatex(name, charsmax(name), "room3_cell_door_%d", c);
        g_CeilingDoor[c] = Map_FindEntity(name);
        formatex(name, charsmax(name), "room3_cell_glow_%d", c);
        g_CeilingGlow[c] = Map_FindEntity(name);
        formatex(name, charsmax(name), "room3_cell_%d", c);
        new marker = Map_FindEntity(name);
        if (!pev_valid(g_CeilingDoor[c]) || !pev_valid(g_CeilingGlow[c]) || !pev_valid(marker)) return Room3_FailMissingEntities();
        pev(marker, pev_origin, center);
        new wall = c / CEILING_CELLS_PER_WALL;
        for (new a = 0; a < 3; a++) g_CeilingCellExit[c][a] = center[a];
        g_CeilingCellMin[c][0] = center[0] - (wall < 2 ? 24.0 : 32.0);
        g_CeilingCellMax[c][0] = center[0] + (wall < 2 ? 24.0 : 32.0);
        g_CeilingCellMin[c][1] = center[1] - (wall < 2 ? 32.0 : 24.0);
        g_CeilingCellMax[c][1] = center[1] + (wall < 2 ? 32.0 : 24.0);
        g_CeilingCellMin[c][2] = center[2];
        g_CeilingCellMax[c][2] = center[2] + 108.0;
        // 48 units inside the arena; other players may share this point (server semiclip).
        if (wall == 0) g_CeilingCellExit[c][1] -= 80.0;
        else if (wall == 1) g_CeilingCellExit[c][1] += 80.0;
        else if (wall == 2) g_CeilingCellExit[c][0] += 80.0;
        else g_CeilingCellExit[c][0] -= 80.0;
    }
    for (new c = 0; c < CEILING_CELL_COUNT; c++)
        ExecuteHamB(Ham_Use, g_CeilingDoor[c], 0, 0, 1, 0.0);
    g_CeilingReady = true;
    Ceiling_Reset();
    return true;
}

stock bool:Ceiling_IsInsideCell(id)
{
    if (!g_CeilingReady || !is_user_alive(id)) return false;
    new Float:o[3];
    Player_ReadOrigin(id, o);
    for (new c = 0; c < CEILING_CELL_COUNT; c++)
    {
        if (!g_CeilingCellOpen[c]) continue;
        if (o[0] >= g_CeilingCellMin[c][0] && o[0] <= g_CeilingCellMax[c][0]
            && o[1] >= g_CeilingCellMin[c][1] && o[1] <= g_CeilingCellMax[c][1]
            && o[2] >= g_CeilingCellMin[c][2] && o[2] <= g_CeilingCellMax[c][2]) return true;
    }
    return false;
}

stock bool:Ceiling_ClearCell(c)
{
    new Float:lo[3], Float:hi[3], Float:dest[3], Float:mins[3], Float:zero[3];
    for (new id = 1; id <= g_MaxPlayers; id++) if (is_user_alive(id))
    {
        Player_ReadMin(id, lo);
        Player_ReadMax(id, hi);
        if (!Bounds_Overlap(lo, hi, g_CeilingCellMin[c], g_CeilingCellMax[c])) continue;
        pev(id, pev_mins, mins);
        dest[0] = g_CeilingCellExit[c][0];
        dest[1] = g_CeilingCellExit[c][1];
        dest[2] = CEILING_FLOOR_Z - mins[2] + 0.5;
        new tr = create_tr2();
        // Ignore players, not map geometry: compatible with the server's semiclip.
        engfunc(EngFunc_TraceHull, dest, dest, 1, (pev(id, pev_flags) & FL_DUCKING) ? HULL_HEAD : HULL_HUMAN, id, tr);
        new bool:clear = !get_tr2(tr, TR_StartSolid) && !get_tr2(tr, TR_AllSolid);
        free_tr2(tr);
        if (!clear) return false; // Leave the shelter open and retry; never trap/crush its occupants.
        Player_SetOrigin(id, dest);
        set_pev(id, pev_velocity, zero);
        set_pev(id, pev_basevelocity, zero);
    }
    return true;
}

stock bool:Ceiling_CloseCells(bool:force = false)
{
    new bool:closed = true;
    for (new c = 0; c < CEILING_CELL_COUNT; c++)
    {
        if (!force && !g_CeilingCellOpen[c]) continue;
        if (g_CeilingCellOpen[c] && !Ceiling_ClearCell(c))
        {
            closed = false;
            continue;
        }
        ExecuteHamB(Ham_Use, g_CeilingDoor[c], 0, 0, 1, 0.0);
        Entity_SetVisible(g_CeilingGlow[c], false);
        g_CeilingCellOpen[c] = false;
    }
    return closed;
}

stock Ceiling_Move(Float:fraction)
{
    new Float:o[3];
    for (new a = 0; a < 3; a++) o[a] = g_CeilingOrigin[a];
    o[2] -= (CEILING_TOP_Z - CEILING_FLOOR_Z) * fraction;
    g_CeilingBottom = CEILING_TOP_Z - (CEILING_TOP_Z - CEILING_FLOOR_Z) * fraction;
    engfunc(EngFunc_SetOrigin, g_CeilingEntity, o);
}

stock Ceiling_StopSound()
{
    if (!g_CeilingMovingSound) return;
    emit_sound(g_CeilingEntity, CHAN_STATIC, "plats/heavymove1.wav", 0.0, ATTN_NORM, SND_STOP, 100);
    g_CeilingMovingSound = false;
}

stock Ceiling_Reset()
{
    if (!g_CeilingReady) return;
    Ceiling_StopSound();
    // Remove the overhead hazard BEFORE evacuating shelters.
    Ceiling_Move(0.0);
    Ceiling_CloseCells(true);
    g_CeilingRound = 0;
    g_CeilingNextBeepAt = 0.0;
    for (new wall = 0; wall < 4; wall++) g_CeilingPrevious[wall] = -1;
}

stock Ceiling_BeginRound(Float:now)
{
    g_CeilingRound++;
    g_Room3Phase = ROOM3_CEILING_WARNING;
    g_Room3PhaseEndsAt = now + CEILING_WARNING_SECONDS;
    g_CeilingNextBeepAt = now + 1.0;
    Room3_PlaySound("buttons/blip1.wav");
}

stock Ceiling_Prepare(Float:now)
{
    Ceiling_Reset();
    Ceiling_BeginRound(now);
}

stock Ceiling_OpenCells()
{
    for (new wall = 0; wall < 4; wall++)
    {
        new previous = g_CeilingPrevious[wall];
        new slot = random_num(0, previous < 0 ? 7 : 6);
        if (previous >= 0 && slot >= previous) slot++;
        g_CeilingPrevious[wall] = slot;
        new c = wall * CEILING_CELLS_PER_WALL + slot;
        ExecuteHamB(Ham_Use, g_CeilingDoor[c], 0, 0, 0, 0.0);
        g_CeilingCellOpen[c] = true;
        Entity_SetVisible(g_CeilingGlow[c], true);
    }

}

stock Ceiling_CheckHazards(id)
{
    if (!is_user_alive(id)) return;
    new Float:lo[3], Float:hi[3];
    Player_ReadMin(id, lo);
    Player_ReadMax(id, hi);
    // Actual hull overlap, not player center: a head/shoulder outside the niche is unsafe.
    for (new a = 0; a < 3; a++) { lo[a] += 1.0; hi[a] -= 1.0; }
    if (hi[0] > -640.0 && lo[0] < 640.0 && hi[1] > -2496.0 && lo[1] < -1216.0
        && hi[2] >= g_CeilingBottom && lo[2] < -952.0) Player_Kill(id);
}

stock bool:Ceiling_Update(Float:now)
{
    switch (g_Room3Phase)
    {
        case ROOM3_CEILING_WARNING:
        {
            if (now >= g_Room3PhaseEndsAt)
            {
                Ceiling_OpenCells();
                emit_sound(g_CeilingEntity, CHAN_STATIC, "plats/heavymove1.wav", 0.7, ATTN_NORM, 0, 100);
                g_CeilingMovingSound = true;
                g_Room3Phase = ROOM3_CEILING_DOWN;
                g_Room3PhaseEndsAt = now + CEILING_DOWN_SECONDS + (g_CeilingRound == 1 ? 3.0 : 0.0);
                g_CeilingNextBeepAt = now + 1.0;
            }
            else if (now >= g_CeilingNextBeepAt)
            {
                Room3_PlaySound("buttons/blip1.wav");
                g_CeilingNextBeepAt += 1.0;
            }
        }
        case ROOM3_CEILING_DOWN:
        {
            Ceiling_Move(floatmin(1.0, floatmax(0.0, 1.0 - (g_Room3PhaseEndsAt - now) / (CEILING_DOWN_SECONDS + (g_CeilingRound == 1 ? 3.0 : 0.0)))));
            if (now >= g_Room3PhaseEndsAt)
            {
                Ceiling_StopSound();
                g_Room3Phase = ROOM3_CEILING_HOLD;
                g_Room3PhaseEndsAt = now + CEILING_HOLD_SECONDS;

            }
        }
        case ROOM3_CEILING_HOLD: if (now >= g_Room3PhaseEndsAt)
        {
            g_Room3Phase = ROOM3_CEILING_UP;
            g_Room3PhaseEndsAt = now + CEILING_UP_SECONDS;

        }
        case ROOM3_CEILING_UP:
        {
            Ceiling_Move(floatmin(1.0, floatmax(0.0, (g_Room3PhaseEndsAt - now) / CEILING_UP_SECONDS)));
            if (now >= g_Room3PhaseEndsAt)
            {
                g_Room3Phase = ROOM3_CEILING_LEAVE;
                g_Room3PhaseEndsAt = now + CEILING_LEAVE_SECONDS;

            }
        }
        case ROOM3_CEILING_LEAVE: if (now >= g_Room3PhaseEndsAt)
        {
            if (!Ceiling_CloseCells()) return false;
            if (g_CeilingRound >= CEILING_ROUNDS) return true;
            Ceiling_BeginRound(now);
        }
    }
    return false;
}
