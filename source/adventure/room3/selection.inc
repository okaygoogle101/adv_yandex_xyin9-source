// Room 3 run selection. One queue is drawn per successful crystal activation.
// Eight ordinary games are sampled uniformly without replacement.
// Chaos has one independent 15/100 draw and may replace ONLY slot four.
enum { ROOM3_GAME_ISLANDS, ROOM3_GAME_SCANNER, ROOM3_GAME_METEORS,
       ROOM3_GAME_COLOR, ROOM3_GAME_SPINNER, ROOM3_GAME_CEILING,
       ROOM3_GAME_LASERS, ROOM3_GAME_BLOCKS, ROOM3_GAME_CHAOS };
new g_Room3GameQueue[4];
new g_Room3GameIndex = -1;

stock Room3_ClearRun()
{
    g_Room3GameIndex = -1;
    for (new i = 0; i < 4; i++) g_Room3GameQueue[i] = -1;
}

stock Room3_SelectRun()
{
    Room3_ClearRun();
    new pool[8];
    for (new i = 0; i < 8; i++) pool[i] = i;
    // Partial Fisher-Yates shuffle: each remaining game has equal probability.
    for (new slot = 0; slot < 4; slot++)
    {
        new pick = random_num(slot, 7), temp = pool[slot];
        pool[slot] = pool[pick];
        pool[pick] = temp;
        g_Room3GameQueue[slot] = pool[slot];
    }
    if (random_num(1, 100) <= 15) g_Room3GameQueue[3] = ROOM3_GAME_CHAOS;
}

stock Room3_AdvanceRun(Float:now)
{
    // Capture before cleanup: child games and aborts must never unlock this branch.
    new bool:chaosFinished = g_Room3GameIndex == 3
        && g_Room3GameQueue[3] == ROOM3_GAME_CHAOS && g_Room3Phase == ROOM3_CHAOS_ACTIVE;
    // Every transition cleans the previous game's entities, sound and hazards.
    // This also isolates ordinary games reused inside Chaos.
    Room3_ResetGames();
    new alive;
    for (new id = 1; id <= g_MaxPlayers; id++) if (Room3_IsInsideArena(id)) alive++;
    if (!alive)
    {
        Room3_Abort(now);
        return;
    }
    g_Room3GameIndex++;
    if (g_Room3GameIndex >= 4)
    {
        Room3_SetGateClosed(1, false);
        emit_sound(g_Room3Gate[1], CHAN_ITEM, "buttons/bell1.wav", 1.0, ATTN_NORM, 0, 80);
        g_Room3Phase = ROOM3_EXIT_OPEN;
        ChaosSecret_SetEnabled(chaosFinished);
        g_Room3PhaseEndsAt = 0.0; // Start hold only once the door is fully open.
        return;
    }
    switch (g_Room3GameQueue[g_Room3GameIndex])
    {
        case ROOM3_GAME_ISLANDS: Islands_Prepare(now);
        case ROOM3_GAME_SCANNER: Scanner_Prepare(now);
        case ROOM3_GAME_METEORS: Meteor_Prepare(now);
        case ROOM3_GAME_COLOR: Color_Prepare(now);
        case ROOM3_GAME_SPINNER: Spinner_Prepare(now);
        case ROOM3_GAME_CEILING: Ceiling_Prepare(now);
        case ROOM3_GAME_LASERS: Laser_Prepare(now);
        case ROOM3_GAME_BLOCKS: Blocks_Prepare(now);
        case ROOM3_GAME_CHAOS: Chaos_Prepare(now);
        default: Room3_Abort(now);
    }
}
