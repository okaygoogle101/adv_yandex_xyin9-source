// Room 5 / harder long jumps. Two nonsolid moving visuals become solid only
// at the fully raised position. No player can block the descent or get crushed.
new bool:g_Room5Ready, bool:g_Room5Hard, bool:g_Room5Moving, bool:g_Room5NeedSolid, bool:g_Room5ButtonLocked;
new g_Room5Button, g_Room5Edge[2];
new Float:g_Room5Origin[2][3], Float:g_Room5Min[2][3], Float:g_Room5Max[2][3];
new Float:g_Room5EndsAt, Float:g_Room5ReadyAt, Float:g_Room5MoveAt;
new Float:g_Room5Fraction, Float:g_Room5From, Float:g_Room5Target;
const Float:ROOM5_HARD_SECONDS = 60.0;
const Float:ROOM5_COOLDOWN_SECONDS = 300.0;
const Float:ROOM5_MOVE_SECONDS = 0.4;
const Float:ROOM5_DROP = 128.0;

stock bool:Room5_Init()
{
    if (!Map_FindEntity("adventure_room5_v45")) return true;
    g_Room5Button = Map_FindEntity("room5_lj_button");
    if (!pev_valid(g_Room5Button)) return Room5_Missing();
    new name[32];
    for (new i = 0; i < 2; i++)
    {
        formatex(name, charsmax(name), "room5_lj_edge_%d", i);
        g_Room5Edge[i] = Map_FindEntity(name);
        if (!pev_valid(g_Room5Edge[i])) return Room5_Missing();
        pev(g_Room5Edge[i], pev_origin, g_Room5Origin[i]);
        pev(g_Room5Edge[i], pev_absmin, g_Room5Min[i]);
        pev(g_Room5Edge[i], pev_absmax, g_Room5Max[i]);
        for (new a = 0; a < 3; a++)
        {
            g_Room5Min[i][a] += 1.0;
            g_Room5Max[i][a] -= 1.0;
        }
    }
    g_Room5Ready = true;
    return true;
}

stock bool:Room5_Missing()
{
    set_fail_state("Room 5 landing edges/button missing (layout v45).");
    return false;
}

stock Room5_Move(Float:fraction)
{
    g_Room5Fraction = fraction;
    new Float:o[3];
    for (new i = 0; i < 2; i++)
    {
        for (new a = 0; a < 3; a++) o[a] = g_Room5Origin[i][a];
        o[2] -= ROOM5_DROP * fraction;
        engfunc(EngFunc_SetOrigin, g_Room5Edge[i], o);
    }
}

stock Room5_Begin(bool:hard, Float:now)
{
    g_Room5Hard = hard;
    g_Room5NeedSolid = false;
    g_Room5From = g_Room5Fraction;
    g_Room5Target = hard ? 1.0 : 0.0;
    g_Room5MoveAt = now;
    g_Room5Moving = true;
    for (new i = 0; i < 2; i++) set_pev(g_Room5Edge[i], pev_solid, SOLID_NOT);
    Room5_Move(g_Room5Fraction);
}

stock Room5_Use(id)
{
    if (!g_Room5Ready || id < 1 || id > g_MaxPlayers || !is_user_alive(id)) return HAM_SUPERCEDE;
    if (!(pev(id, pev_button) & IN_USE) || !Player_IsAimingAt(id, g_Room5Button)) return HAM_SUPERCEDE;
    new Float:now = get_gametime();
    if (now < g_Room5ReadyAt) return HAM_SUPERCEDE;
    // One shared cooldown; neither repeated uses nor round resets shorten it.
    g_Room5ReadyAt = now + ROOM5_COOLDOWN_SECONDS;
    g_Room5EndsAt = now + ROOM5_HARD_SECONDS;
    g_Room5ButtonLocked = true;
    set_pev(g_Room5Button, pev_frame, 1.0);
    Room5_Begin(true, now);
    emit_sound(g_Room5Button, CHAN_ITEM, "buttons/button3.wav", 0.8, ATTN_NORM, 0, 100);
    return HAM_SUPERCEDE;
}

stock bool:Room5_RestoreCollision()
{
    // Raise a falling player's feet above the returned surface before enabling
    // collision. Ignore other players in traces, matching server semiclip.
    new bool:blocked;
    for (new id = 1; id <= g_MaxPlayers; id++) if (is_user_alive(id) && pev(id, pev_movetype) == MOVETYPE_WALK)
    {
        new Float:o[3], Float:mins[3], Float:maxs[3], Float:lo[3], Float:hi[3];
        pev(id, pev_origin, o);
        pev(id, pev_mins, mins);
        pev(id, pev_maxs, maxs);
        for (new a = 0; a < 3; a++) { lo[a] = o[a] + mins[a]; hi[a] = o[a] + maxs[a]; }
        for (new i = 0; i < 2; i++) if (Bounds_Overlap(lo, hi, g_Room5Min[i], g_Room5Max[i]))
        {
            o[2] = g_Room5Max[i][2] + 1.0 - mins[2];
            new tr = create_tr2();
            engfunc(EngFunc_TraceHull, o, o, 1, (pev(id, pev_flags) & FL_DUCKING) ? HULL_HEAD : HULL_HUMAN, id, tr);
            new bool:free = !get_tr2(tr, TR_StartSolid) && !get_tr2(tr, TR_AllSolid);
            free_tr2(tr);
            if (!free) { blocked = true; break; }
            Player_SetOrigin(id, o);
            new Float:v[3]; pev(id, pev_velocity, v);
            if (v[2] < 0.0) { v[2] = 0.0; set_pev(id, pev_velocity, v); }
            break;
        }
    }
    if (blocked) return false;
    for (new i = 0; i < 2; i++)
    {
        set_pev(g_Room5Edge[i], pev_solid, SOLID_BSP);
        engfunc(EngFunc_SetOrigin, g_Room5Edge[i], g_Room5Origin[i]);
    }
    return true;
}

stock Room5_Update(Float:now)
{
    if (!g_Room5Ready) return;
    if (g_Room5Hard && now >= g_Room5EndsAt) Room5_Begin(false, now);
    if (g_Room5Moving)
    {
        new Float:t = floatmin(1.0, floatmax(0.0, (now - g_Room5MoveAt) / ROOM5_MOVE_SECONDS));
        Room5_Move(g_Room5From + (g_Room5Target - g_Room5From) * t);
        if (t >= 1.0)
        {
            g_Room5Moving = false;
            g_Room5NeedSolid = !g_Room5Hard;
        }
    }
    if (g_Room5NeedSolid && Room5_RestoreCollision()) g_Room5NeedSolid = false;
    if (g_Room5ButtonLocked && now >= g_Room5ReadyAt)
    {
        g_Room5ButtonLocked = false;
        set_pev(g_Room5Button, pev_frame, 0.0);
    }
}

stock Room5_Reset()
{
    if (!g_Room5Ready) return;
    g_Room5Hard = false;
    g_Room5Moving = false;
    for (new i = 0; i < 2; i++) set_pev(g_Room5Edge[i], pev_solid, SOLID_NOT);
    Room5_Move(0.0);
    g_Room5NeedSolid = !Room5_RestoreCollision();
    // Preserve ReadyAt. A restart cannot bypass the 300-second cooldown.
}
