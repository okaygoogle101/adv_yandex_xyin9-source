// Room 8 entrance: a normally open 64x10x10 landing recess.
// The shared button restores the old flush wall/floor for 60 seconds.
const Float:R8_STEP_DURATION = 60.0;
const Float:R8_STEP_COOLDOWN = 300.0;
new bool:g_R8StepReady, bool:g_R8StepClosed, bool:g_R8StepLocked;
new g_R8StepFill, g_R8StepButton;
new Float:g_R8StepEndsAt, Float:g_R8StepReadyAt;
new Float:g_R8StepLo[3], Float:g_R8StepHi[3];

stock bool:Room8Ladder_Init()
{
    if (!Map_FindEntity("adventure_room8_ladder_v64")) return true;
    g_R8StepFill = Map_FindEntity("room8_ladder_fill");
    g_R8StepButton = Map_FindEntity("room8_ladder_button");
    if (!g_R8StepFill || !g_R8StepButton)
    {
        set_fail_state("Room 8 ladder recess/button missing (map v64).");
        return false;
    }
    pev(g_R8StepFill, pev_absmin, g_R8StepLo);
    pev(g_R8StepFill, pev_absmax, g_R8StepHi);
    for (new a = 0; a < 3; a++) { g_R8StepLo[a] += 1.0; g_R8StepHi[a] -= 1.0; }
    g_R8StepReady = true;
    return true;
}

stock bool:Room8Ladder_ClearRecess()
{
    // Do not entomb or kill a player standing on the step. Validate all
    // destinations first, then move overlapping players a few units into the shaft.
    new bool:movePlayer[33], Float:destination[33][3];
    for (new id = 1; id <= g_MaxPlayers; id++) if (is_user_alive(id))
    {
        new Float:p[3], Float:mins[3], Float:maxs[3], Float:lo[3], Float:hi[3];
        pev(id, pev_origin, p); pev(id, pev_mins, mins); pev(id, pev_maxs, maxs);
        for (new a = 0; a < 3; a++) { lo[a] = p[a] + mins[a]; hi[a] = p[a] + maxs[a]; }
        if (!Bounds_Overlap(lo, hi, g_R8StepLo, g_R8StepHi)) continue;
        for (new a = 0; a < 3; a++) destination[id][a] = p[a];
        destination[id][1] = g_R8StepLo[1] - maxs[1] - 0.25;
        new tr = create_tr2();
        engfunc(EngFunc_TraceHull, destination[id], destination[id], 1,
            (pev(id, pev_flags) & FL_DUCKING) ? HULL_HEAD : HULL_HUMAN, id, tr);
        new bool:free = !get_tr2(tr, TR_StartSolid) && !get_tr2(tr, TR_AllSolid);
        free_tr2(tr);
        if (!free) return false;
        movePlayer[id] = true;
    }
    for (new id = 1; id <= g_MaxPlayers; id++) if (movePlayer[id])
        Player_SetOrigin(id, destination[id]);
    return true;
}

stock Room8Ladder_Use(id)
{
    if (!g_R8StepReady || id < 1 || id > g_MaxPlayers || !is_user_alive(id)) return HAM_SUPERCEDE;
    if (!(pev(id, pev_button) & IN_USE) || !Player_IsAimingAt(id, g_R8StepButton)) return HAM_SUPERCEDE;
    new Float:now = get_gametime();
    if (now < g_R8StepReadyAt || !Room8Ladder_ClearRecess()) return HAM_SUPERCEDE;
    ExecuteHamB(Ham_Use, g_R8StepFill, 0, 0, 1, 0.0); // USE_ON: fill the recess.
    g_R8StepClosed = true;
    g_R8StepEndsAt = now + R8_STEP_DURATION;
    g_R8StepReadyAt = now + R8_STEP_COOLDOWN;
    g_R8StepLocked = true;
    set_pev(g_R8StepButton, pev_frame, 1.0);
    emit_sound(g_R8StepButton, CHAN_ITEM, "buttons/button3.wav", 0.7, ATTN_NORM, 0, 100);
    return HAM_SUPERCEDE;
}

stock Room8Ladder_Update(Float:now)
{
    if (!g_R8StepReady) return;
    if (g_R8StepClosed && now >= g_R8StepEndsAt)
    {
        ExecuteHamB(Ham_Use, g_R8StepFill, 0, 0, 0, 0.0);
        g_R8StepClosed = false;
    }
    if (g_R8StepLocked && now >= g_R8StepReadyAt)
    {
        g_R8StepLocked = false;
        set_pev(g_R8StepButton, pev_frame, 0.0);
    }
}

stock Room8Ladder_Reset()
{
    if (!g_R8StepReady) return;
    ExecuteHamB(Ham_Use, g_R8StepFill, 0, 0, 0, 0.0);
    g_R8StepClosed = false;
    // Match the other difficulty buttons: a new round opens the recess but
    // cannot bypass the shared 300-second cooldown measured from activation.
    g_R8StepLocked = get_gametime() < g_R8StepReadyAt;
    set_pev(g_R8StepButton, pev_frame, g_R8StepLocked ? 1.0 : 0.0);
}
