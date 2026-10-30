// Game 7: six full-height sweeps, one visible 150-unit opening per wave.
// Two reusable visual entities; analytical swept collision cannot be body-blocked.
new bool:g_ScannerReady;
new g_ScannerEntity[2], g_ScannerModel[8][2][16];
new g_ScannerWave, g_ScannerWall, g_ScannerVariant;
new Float:g_ScannerDistance, Float:g_ScannerDuration, Float:g_ScannerBeepAt;

stock bool:Scanner_Init()
{
    new name[48];
    for (new i = 0; i < 8; i++) for (new part = 0; part < 2; part++)
    {
        if (part) formatex(name, charsmax(name), "room3_scan_green_%d", i);
        else formatex(name, charsmax(name), "room3_scan_red_%d", i);
        new sample = Map_FindEntity(name);
        if (!pev_valid(sample))
        {
            set_fail_state("Scanner game requires adventure_start_v34.map.");
            return false;
        }
        pev(sample, pev_model, g_ScannerModel[i][part], 15);
        if (g_ScannerModel[i][part][0] != '*') return Room3_FailMissingEntities();
        Entity_SetVisible(sample, false);
    }
    for (new part = 0; part < 2; part++)
    {
        new ent = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "info_target"));
        if (!pev_valid(ent)) return Room3_FailMissingEntities();
        g_ScannerEntity[part] = ent;
        engfunc(EngFunc_SetModel, ent, g_ScannerModel[0][part]);
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_movetype, MOVETYPE_PUSH);
        set_pev(ent, pev_rendermode, kRenderTransColor);
        set_pev(ent, pev_renderamt, part ? 230.0 : 115.0);
        new Float:color[3];
        color[0] = part ? 40.0 : 255.0;
        color[1] = part ? 255.0 : 35.0;
        color[2] = part ? 90.0 : 45.0;
        set_pev(ent, pev_rendercolor, color);
        Entity_SetVisible(ent, false);
    }
    g_ScannerReady = true;
    Scanner_Reset();
    return true;
}

stock Scanner_Reset()
{
    if (!g_ScannerReady) return;
    for (new part = 0; part < 2; part++) Entity_SetVisible(g_ScannerEntity[part], false);
    g_ScannerWave = 0;
    g_ScannerDistance = 0.0;
    g_ScannerBeepAt = 0.0;
}

stock Float:Scanner_Coordinate(Float:distance)
{
    if (g_ScannerWall == 0) return -1216.0 - distance;
    if (g_ScannerWall == 1) return -2496.0 + distance;
    if (g_ScannerWall == 2) return -640.0 + distance;
    return 640.0 - distance;
}

stock Scanner_Move(Float:distance)
{
    new Float:o[3];
    o[1] = -1856.0;
    o[2] = -1336.0;
    o[g_ScannerWall < 2 ? 1 : 0] = Scanner_Coordinate(distance);
    for (new part = 0; part < 2; part++) engfunc(EngFunc_SetOrigin, g_ScannerEntity[part], o);
    g_ScannerDistance = distance;
}

stock Scanner_Prepare(Float:now)
{
    Scanner_Reset();
    g_Room3Phase = ROOM3_SCAN_WARNING;
    g_Room3PhaseEndsAt = now + SCAN_WARNING_SECONDS;
    g_ScannerBeepAt = now + 1.0;
    Room3_PlaySound("buttons/blip1.wav", 90);
}

stock Scanner_BeginWave(Float:now)
{
    g_ScannerWave++;
    g_ScannerWall = random_num(0, 3);
    g_ScannerVariant = random_num(0, 7);
    g_ScannerDuration = (g_ScannerWave <= 2 ? 7.0 : (g_ScannerWave <= 4 ? 6.0 : 5.0)) / 1.5 - 1.0;
    new Float:angles[3];
    angles[1] = g_ScannerWall < 2 ? 90.0 : 0.0;
    for (new part = 0; part < 2; part++)
    {
        engfunc(EngFunc_SetModel, g_ScannerEntity[part], g_ScannerModel[g_ScannerVariant][part]);
        set_pev(g_ScannerEntity[part], pev_angles, angles);
        Entity_SetVisible(g_ScannerEntity[part], true);
    }
    Scanner_Move(0.0);
    g_Room3Phase = ROOM3_SCAN_PREVIEW;
    g_Room3PhaseEndsAt = now + SCAN_PREVIEW_SECONDS;
    Room3_PlaySound("buttons/button1.wav", 80);
}

stock Scanner_CheckSegment(id, Float:from, Float:to)
{
    if (!Room3_IsInsideArena(id)) return;
    new Float:lo[3], Float:hi[3];
    Player_ReadMin(id, lo);
    Player_ReadMax(id, hi);
    for (new a = 0; a < 3; a++) { lo[a] += 1.0; hi[a] -= 1.0; }
    if (hi[2] <= -1336.0 || lo[2] >= -952.0) return;
    new axis = g_ScannerWall < 2 ? 1 : 0, cross = 1 - axis;
    new Float:start = Scanner_Coordinate(from), Float:end = Scanner_Coordinate(to);
    if (hi[axis] < floatmin(start, end) - 4.0 || lo[axis] > floatmax(start, end) + 4.0) return;
    new Float:gap = -448.0 + float(g_ScannerVariant) * 128.0;
    // Rotation by 90 degrees maps local +Y onto world -X.
    if (g_ScannerWall < 2) gap = -gap;
    else gap -= 1856.0;
    if (lo[cross] < gap - SCAN_GAP_HALF || hi[cross] > gap + SCAN_GAP_HALF) Player_Kill(id);
}

stock Scanner_CheckHazards(id)
{
    Scanner_CheckSegment(id, g_ScannerDistance, g_ScannerDistance);
}

stock bool:Scanner_Update(Float:now)
{
    if (g_Room3Phase == ROOM3_SCAN_WARNING)
    {
        if (now >= g_Room3PhaseEndsAt) Scanner_BeginWave(now);
        else if (now >= g_ScannerBeepAt)
        {
            Room3_PlaySound("buttons/blip1.wav", 90);
            g_ScannerBeepAt = now + 1.0;
        }
    }
    else if (g_Room3Phase == ROOM3_SCAN_PREVIEW && now >= g_Room3PhaseEndsAt)
    {
        g_Room3Phase = ROOM3_SCAN_ACTIVE;
        g_Room3PhaseEndsAt = now + g_ScannerDuration;
        Room3_PlaySound("buttons/button3.wav", 140);
        for (new id = 1; id <= g_MaxPlayers; id++) Scanner_CheckHazards(id);
    }
    else if (g_Room3Phase == ROOM3_SCAN_ACTIVE)
    {
        new Float:distance = 1280.0 * floatmin(1.0, floatmax(0.0, 1.0 - (g_Room3PhaseEndsAt - now) / g_ScannerDuration));
        for (new id = 1; id <= g_MaxPlayers; id++) Scanner_CheckSegment(id, g_ScannerDistance, distance);
        Scanner_Move(distance);
        if (now >= g_Room3PhaseEndsAt)
        {
            if (g_ScannerWave >= SCAN_WAVES)
            {
                // If the last sweep killed everybody, let the controller abort next frame.
                for (new id = 1; id <= g_MaxPlayers; id++) if (Room3_IsInsideArena(id)) return true;
                return false;
            }
            Scanner_BeginWave(now);
        }
    }
    return false;
}
