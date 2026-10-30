new bool:g_SpinnerTrigValid;
new Float:g_SpinnerTrigAngle, Float:g_SpinnerTrigCos, Float:g_SpinnerTrigSin;
// Room 3 / spinner. Init, Prepare, Update (true when complete), Reset, CheckHazards.
// Internal include; compile adventure_trust.sma, not this file.

new g_SpinnerPad[4], g_SpinnerRotor[3], g_SpinnerPadMarks, g_SpinnerSpeedIndex;
new Float:g_SpinnerLastStepAt, Float:g_SpinnerSpeedChangeAt, Float:g_SpinnerAngle, Float:g_SpinnerLiftFraction;
new Float:g_SpinnerPadOrigin[4][3], Float:g_SpinnerRotorOrigin[3][3];

stock bool:Spinner_Init()
{
    new name[64];
    g_SpinnerPadMarks = Map_FindEntity("boss_pad_marks");
    if (!pev_valid(g_SpinnerPadMarks)) return Room3_FailMissingEntities();
    for (new i = 0; i < 4; i++)
    {
        formatex(name, charsmax(name), "boss_spinner_pad_%d_%d", i < 2 ? 3:6, (i % 2) ? 6:3);
        g_SpinnerPad[i] = Map_FindEntity(name);
        if (!pev_valid(g_SpinnerPad[i])) return Room3_FailMissingEntities();
        pev(g_SpinnerPad[i], pev_origin, g_SpinnerPadOrigin[i]);
    }
    g_SpinnerRotor[0] = Map_FindEntity("boss_spinner_hub");
    g_SpinnerRotor[1] = Map_FindEntity("boss_spinner_arms");
    g_SpinnerRotor[2] = Map_FindEntity("boss_spinner_glow");
    for (new i = 0; i < 3; i++)
    {
        if (!pev_valid(g_SpinnerRotor[i])) return Room3_FailMissingEntities();
        pev(g_SpinnerRotor[i], pev_origin, g_SpinnerRotorOrigin[i]);
        set_pev(g_SpinnerRotor[i], pev_solid, SOLID_NOT);
    }
    return true;
}

stock Spinner_Prepare(Float:now)
{
    g_Room3Phase = ROOM3_SPINNER_LOW_RISE;
    g_Room3PhaseEndsAt = now + SPINNER_LOW_RISE_SECONDS;
    Room3_PlaySound("buttons/button1.wav");
}

stock bool:Spinner_Update(Float:now)
{
    switch (g_Room3Phase)
    {
        case ROOM3_SPINNER_LOW_RISE:
        {
            Spinner_MoveLift(floatmin(0.5, floatmax(0.0, 0.5 - 0.5 * (g_Room3PhaseEndsAt - now) / SPINNER_LOW_RISE_SECONDS)));
            if (now >= g_Room3PhaseEndsAt)
            {
                g_Room3Phase = ROOM3_SPINNER_PREPARE;
                g_Room3PhaseEndsAt = now + SPINNER_PREPARE_SECONDS;
            }
        }
        case ROOM3_SPINNER_PREPARE: if (now >= g_Room3PhaseEndsAt)
        {
            g_Room3Phase = ROOM3_SPINNER_RISE;
            g_Room3PhaseEndsAt = now + SPINNER_RISE_SECONDS;
            Room3_PlaySound("buttons/button1.wav");
            for (new i = 0; i < 3; i++) Entity_SetVisible(g_SpinnerRotor[i], true);
        }
        case ROOM3_SPINNER_RISE:
        {
            Spinner_MoveLift(floatmin(1.0, floatmax(0.5, 1.0 - 0.5 * (g_Room3PhaseEndsAt - now) / SPINNER_RISE_SECONDS)));
            if (now >= g_Room3PhaseEndsAt)
            {
                g_Room3Phase = ROOM3_SPINNER_ACTIVE;
                g_Room3PhaseEndsAt = now + SPINNER_ACTIVE_SECONDS;
                g_SpinnerAngle = 0.0;
                g_SpinnerLastStepAt = now;
                g_SpinnerSpeedIndex = 2;
                g_SpinnerSpeedChangeAt = now + random_float(SPINNER_SPEED_MIN_SECONDS, SPINNER_SPEED_MAX_SECONDS);
                Room3_PlaySound("buttons/bell1.wav");
            }
        }
        case ROOM3_SPINNER_ACTIVE:
        {
            new Float:next = Spinner_AdvanceAngle(now);
            for (new id = 1; id <= g_MaxPlayers; id++) Room3_CheckHazards(id, g_SpinnerAngle, next);
            g_SpinnerAngle = next;
            new Float:angles[3];
            angles[1] = g_SpinnerAngle;
            for (new i = 1; i < 3; i++) set_pev(g_SpinnerRotor[i], pev_angles, angles);
            if (now >= g_Room3PhaseEndsAt)
            {
                g_Room3Phase = ROOM3_SPINNER_LOWER;
                g_Room3PhaseEndsAt = now + SPINNER_LOWER_SECONDS;
            }
        }
        case ROOM3_SPINNER_LOWER:
        {
            Spinner_MoveLift(floatmin(1.0, floatmax(0.0, (g_Room3PhaseEndsAt - now) / SPINNER_LOWER_SECONDS)));
            if (now >= g_Room3PhaseEndsAt)
            {
                return true;
            }
        }
    }
    return false;
}

stock Spinner_Reset()
{
    for (new i = 0; i < 3; i++) Entity_SetVisible(g_SpinnerRotor[i], false);
    Entity_SetVisible(g_SpinnerPadMarks, false);
    Spinner_MoveLift(0.0);
    g_SpinnerAngle = 0.0;
    new Float:zero[3];
    for (new i = 0; i < 3; i++) set_pev(g_SpinnerRotor[i], pev_angles, zero);
}

stock Float:Spinner_AdvanceAngle(Float:now)
{
    new Float:end = floatmin(now, g_Room3PhaseEndsAt), Float:angle = g_SpinnerAngle;
    while (g_SpinnerLastStepAt < end)
    {
        new Float:t = floatmin(end, g_SpinnerSpeedChangeAt);
        angle += (t - g_SpinnerLastStepAt) * (g_SpinnerSpeedIndex == 4 ? 216.0 : float(g_SpinnerSpeedIndex) * SPINNER_SPEED_STEP);
        g_SpinnerLastStepAt = t;
        if (t >= g_SpinnerSpeedChangeAt)
        {
            new speed = random_num(2, 3);
            if (speed >= g_SpinnerSpeedIndex) speed++;
            g_SpinnerSpeedIndex = speed;
            g_SpinnerSpeedChangeAt = t + random_float(SPINNER_SPEED_MIN_SECONDS, SPINNER_SPEED_MAX_SECONDS);
            Room3_PlaySound("buttons/blip1.wav", 120);
        }
    }
    return angle;
}

stock Spinner_MoveLift(Float:fraction)
{
    // Use current origin + hull mins: linked absmin and FL_ONGROUND can be stale.
    // Carry surface passengers before relocating the brush, including edge/duck riders.
    new Float:dz = (fraction - g_SpinnerLiftFraction) * 64.0, Float:o[3], Float:lo[3], Float:hi[3], Float:v[3];
    new Float:oldTop =  - 1336.0 + g_SpinnerLiftFraction * 64.0, Float:newTop =  - 1336.0 + fraction * 64.0;
    for (new id = 1; id <= g_MaxPlayers; id++) if (Room3_IsInsideArena(id) && floatabs(dz) > 0.0001)
    {
        Player_ReadOrigin(id, o);
        pev(id, pev_mins, lo);
        pev(id, pev_maxs, hi);
        pev(id, pev_velocity, v);
        // Let a genuine upward jump leave the lift freely.
        if (v[2] > 1.0) continue;
        new Float:feet = o[2] + lo[2];
        if (floatabs(feet - oldTop) > 3.0 && !(dz > 0.0 && feet >= oldTop && feet <= newTop)) continue;
        for (new p = 0; p < 4; p++) if (o[0] + hi[0] > SPINNER_PAD_XY[p][0] - 64.0 && o[0] + lo[0] < SPINNER_PAD_XY[p][0] + 64.0 && o[1] + hi[1] > SPINNER_PAD_XY[p][1] - 64.0 && o[1] + lo[1] < SPINNER_PAD_XY[p][1] + 64.0)
        {
            o[2] = newTop - lo[2] + 0.1;
            Player_SetOrigin(id, o);
            v[2] = 0.0;
            set_pev(id, pev_velocity, v);
            set_pev(id, pev_groundentity, g_SpinnerPad[p]);
            set_pev(id, pev_flags, pev(id, pev_flags) | FL_ONGROUND);
            break;
        }
    }
    g_SpinnerLiftFraction = fraction;
    for (new p = 0; p < 4; p++)
    {
        for (new a = 0; a < 3; a++) o[a] = g_SpinnerPadOrigin[p][a];
        o[2] += 64.0 * fraction;
        engfunc(EngFunc_SetOrigin, g_SpinnerPad[p], o);
    }
    for (new p = 0; p < 3; p++)
    {
        for (new a = 0; a < 3; a++) o[a] = g_SpinnerRotorOrigin[p][a];
        o[2] += 128.0 * fraction;
        engfunc(EngFunc_SetOrigin, g_SpinnerRotor[p], o);
    }
}

stock Spinner_CheckHazards(id, Float:oldAngle, Float:newAngle)
{
    new Float:o[3];
    Player_ReadOrigin(id, o);
    new Float:lo[3], Float:hi[3];
    Player_ReadMin(id, lo);
    Player_ReadMax(id, hi);
    if (lo[2] + 1.0 <  - 1274.0)
    {
        Player_Kill(id);
        return;
    }
    // Floor below the raised pads.
    // Hub has a lethal cap; no safe standing spot on the center mechanism.
    if (floatabs(o[0]) < 40.0 && floatabs(o[1] + 1856.0) < 40.0 && lo[2] <=  - 1222.0)
    {
        Player_Kill(id);
        return;
    }
    if (lo[2] >  - 1234.0 || hi[2] <  - 1244.0) return;
    // Sweep the rotating rectangle, rather than testing just the final angle.
    // A 2-degree step at radius 368 is under 13 units (less than a player hull).
    new steps = newAngle == oldAngle ? 0 : max(1, floatround((newAngle - oldAngle) / 2.0, floatround_ceil));
    for (new i = 0; i <= steps; i++)
    {
        new Float:a = steps ? oldAngle + (newAngle - oldAngle) * float(i) / float(steps) : oldAngle;
        new Float:cs, Float:sn;
        if (!steps)
        {
            if (!g_SpinnerTrigValid || a != g_SpinnerTrigAngle)
            {
                g_SpinnerTrigAngle = a;
                g_SpinnerTrigCos = floatcos(a, degrees);
                g_SpinnerTrigSin = floatsin(a, degrees);
                g_SpinnerTrigValid = true;
            }
            cs = g_SpinnerTrigCos; sn = g_SpinnerTrigSin;
        }
        else { cs = floatcos(a, degrees); sn = floatsin(a, degrees); }
        new Float:x = o[0] * cs + (o[1] + 1856.0) * sn, Float:y =  - o[0] * sn + (o[1] + 1856.0) * cs;
        new Float:extent = 16.0 * (floatabs(cs) + floatabs(sn));
        if (floatabs(x) <= 368.0 + extent && floatabs(y) <= 5.0 + extent)
        {
            Player_Kill(id);
            return;
        }
    }
}
