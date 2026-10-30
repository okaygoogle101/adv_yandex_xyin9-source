new g_Room3DoorMotion[2];
new bool:g_Room3CrystalShown[4];
// Room 3: arena AND exit landing. Controller owns crystals, gates and game order.
// Internal include; compile adventure_trust.sma, not this file.

stock bool:Room3_Init()
{
    g_Room3Speaker = Map_FindEntity("adventure_boss_v24");
    if (!g_Room3Speaker)
    {
        if (Map_FindEntity("adventure_boss_v21") || Map_FindEntity("adventure_boss_v23")) return Room3_FailMissingEntities();
        return true;
    }
    if (!Map_FindEntity("adventure_balance_v35")) return Room3_FailMissingEntities();
    if (!g_ChaosSoundAvailable)
    {
        set_fail_state("Missing sound/yx_xyin9/yx_chaos_start.wav: install the Chaos intro sound.");
        return false;
    }
    new name[64];
    g_Room3Gate[0] = Map_FindEntity("boss_entry_gate");
    g_Room3Gate[1] = Map_FindEntity("boss_exit_gate");
    for (new i = 0; i < 4; i++)
    {
        formatex(name, charsmax(name), "boss_crystal_%d_off", i);
        g_Room3Crystal[i][0] = Map_FindEntity(name);
        formatex(name, charsmax(name), "boss_crystal_%d_on", i);
        g_Room3Crystal[i][1] = Map_FindEntity(name);
        formatex(name, charsmax(name), "boss_crystal_zone_%d", i);
        new zone = Map_FindEntity(name);
        if (!pev_valid(zone)) return Room3_FailMissingEntities();
        set_pev(zone, pev_solid, SOLID_NOT);
        // Frame scan supplies entry-only activation.
        if (!pev_valid(g_Room3Crystal[i][0]) || !pev_valid(g_Room3Crystal[i][1])) return Room3_FailMissingEntities();
    }
    if (!pev_valid(g_Room3Gate[0]) || !pev_valid(g_Room3Gate[1])) return Room3_FailMissingEntities();
    g_Room3DoorMotion[0] = Door_Add(g_Room3Gate[0], 128.0, 128.0, false, true);
    g_Room3DoorMotion[1] = Door_Add(g_Room3Gate[1], 152.0, 152.0, false, true);
    if (!Spinner_Init() || !Color_Init() || !Ceiling_Init() || !Laser_Init() || !Blocks_Init() || !Islands_Init() || !Scanner_Init() || !Meteor_Init()) return false;
    g_Room3Ready = true;
    log_amx("Room 3 ready: four random games without repeats, Chaos only last with 5 percent chance; legacy boss_* map entity names preserved.");
    return true;
}

stock bool:Room3_FailMissingEntities()
{
    set_fail_state("Room 3 entities missing: expected layout v35 (nine room3 games).");
    return false;
}

stock Room3_PlaySound(const sample[], pitch = 100)
{
    emit_sound(g_Room3Speaker, CHAN_ITEM, sample, 0.8, ATTN_NORM, 0, pitch);
}

stock bool:Room3_IsInsideArena(id)
{
    if (!is_user_alive(id)) return false;
    new Float:o[3];
    Player_ReadOrigin(id, o);
    if (o[0] > -640.0 && o[0] < 640.0 && o[1] > -2496.0 && o[1] < -1216.0 && o[2] > -1336.0 && o[2] < -952.0) return true;
    return Ceiling_IsInsideCell(id);
}



stock Room3_SetGateClosed(which, bool:closed)
{
    Door_Set(g_Room3DoorMotion[which], !closed);
}

stock Room3_ResetGames()
{
    Chaos_Reset();
    Color_Reset();
    Spinner_Reset();
    Ceiling_Reset();
    Laser_Reset();
    Blocks_Reset();
    Islands_Reset();
    Scanner_Reset();
    Meteor_Reset();
}

stock Room3_Reset()
{
    ChaosSecret_SetEnabled(false);
    if (!g_Room3Ready) return;
    Room3_ClearRun();
    g_Room3Phase = ROOM3_IDLE;
    g_Room3PhaseEndsAt = 0.0;
    g_ColorNextBeepAt = 0.0;
    g_ColorRound = 0;
    Room3_ResetGames();
    Room3_SetGateClosed(1, true);
    Room3_SetGateClosed(0, false);
    for (new i = 0; i < 4; i++)
    {
        g_Room3CrystalActiveUntil[i] = 0.0;
        g_Room3CrystalShown[i] = false;
        Entity_SetVisible(g_Room3Crystal[i][0], true);
        Entity_SetVisible(g_Room3Crystal[i][1], false);
    }
    for (new id = 1; id <= 32; id++)
    {
        g_Room3CrystalTouchMask[id] = 0;
        g_Room3SlotUserId[id] = 0;
    }
}

stock Room3_Abort(Float:now)
{
    ChaosSecret_SetEnabled(false);
    Room3_ClearRun();
    g_Room3Phase = ROOM3_RESET_DELAY;
    g_Room3PhaseEndsAt = now + ROOM3_EMPTY_RESET_SECONDS;
    Room3_ResetGames();
    Room3_SetGateClosed(1, true);
    for (new i = 0; i < 4; i++)
    {
        Entity_SetVisible(g_Room3Crystal[i][0], false);
        Entity_SetVisible(g_Room3Crystal[i][1], false);
    }
}

stock Room3_CheckHazards(id, Float:oldAngle, Float:newAngle)
{
    if (g_Room3Phase != ROOM3_CHAOS_ACTIVE && g_Room3Phase != ROOM3_COLOR_HOLD
        && g_Room3Phase != ROOM3_SPINNER_ACTIVE && g_Room3Phase != ROOM3_METEOR_ACTIVE
        && g_Room3Phase != ROOM3_SCAN_ACTIVE && g_Room3Phase != ROOM3_BLOCK_ACTIVE
        && g_Room3Phase != ROOM3_ISLAND_ACTIVE && g_Room3Phase != ROOM3_ISLAND_FINISH
        && g_Room3Phase != ROOM3_LASER_ACTIVE
        && !(g_Room3Phase >= ROOM3_CEILING_DOWN && g_Room3Phase <= ROOM3_CEILING_UP)) return;
    if (!Room3_IsInsideArena(id)) return;
    if (g_Room3Phase == ROOM3_CHAOS_ACTIVE) Chaos_CheckHazards(id);
    else if (g_Room3Phase == ROOM3_COLOR_HOLD) Color_CheckHazards(id);
    else if (g_Room3Phase == ROOM3_SPINNER_ACTIVE) Spinner_CheckHazards(id, oldAngle, newAngle);
    else if (g_Room3Phase == ROOM3_METEOR_ACTIVE) Meteor_CheckHazards(id);
    else if (g_Room3Phase == ROOM3_SCAN_ACTIVE) Scanner_CheckHazards(id);
    else if (g_Room3Phase == ROOM3_BLOCK_ACTIVE) Blocks_CheckHazards(id);
    else if (g_Room3Phase == ROOM3_ISLAND_ACTIVE || g_Room3Phase == ROOM3_ISLAND_FINISH) Islands_CheckHazards(id);
    else if (g_Room3Phase == ROOM3_LASER_ACTIVE) Laser_CheckHazards(id);
    else if (g_Room3Phase >= ROOM3_CEILING_DOWN && g_Room3Phase <= ROOM3_CEILING_UP) Ceiling_CheckHazards(id);
}

public Room3_OnPlayerPostThink(id)
{
    if (g_Room3Ready && g_PluginReady)
    {
        // A fresh command snapshot: never reuse coordinates from StartFrame.
        g_PlayerSnapshotFlags[id] = 0;
        g_PlayerSnapshotActive = true;
        Room3_CheckHazards(id, g_SpinnerAngle, g_SpinnerAngle);
        g_PlayerSnapshotActive = false;
    }
    return HAM_IGNORED;
}

stock Room3_UpdateCrystals(Float:now)
{
    for (new id = 1; id <= g_MaxPlayers; id++)
    {
        if (!Room3_IsInsideArena(id))
        {
            g_Room3CrystalTouchMask[id] = 0;
            continue;
        }
        new uid = Player_ReadUserId(id);
        if (g_Room3SlotUserId[id] != uid)
        {
            g_Room3CrystalTouchMask[id] = 0;
            g_Room3SlotUserId[id] = uid;
        }
        new mask, Float:o[3];
        Player_ReadOrigin(id, o);
        for (new i = 0; i < 4; i++) if (floatabs(o[0] - ROOM3_CRYSTAL_XY[i][0]) <= 32.0 && floatabs(o[1] - ROOM3_CRYSTAL_XY[i][1]) <= 32.0 && o[2] <  - 1232.0)
        {
            mask |= (1 << i);
            if (!(g_Room3CrystalTouchMask[id] & (1 << i))) g_Room3CrystalActiveUntil[i] = now + ROOM3_CRYSTAL_SECONDS;
        }
        g_Room3CrystalTouchMask[id] = mask;
    }
    new count;
    for (new i = 0; i < 4; i++)
    {
        new bool:active = g_Room3CrystalActiveUntil[i] > now;
        if (g_Room3CrystalShown[i] != active)
        {
            g_Room3CrystalShown[i] = active;
            Entity_SetVisible(g_Room3Crystal[i][0], !active);
            Entity_SetVisible(g_Room3Crystal[i][1], active);
        }
        if (active) count++;
    }
    if (count == 4)
    {
        for (new i = 0; i < 4; i++)
        {
            Entity_SetVisible(g_Room3Crystal[i][0], false);
            Entity_SetVisible(g_Room3Crystal[i][1], false);
        }
        Room3_SetGateClosed(0, true);
        Room3_SelectRun();
        g_ColorRound = 0;
        g_Room3Phase = ROOM3_ARMING;
        g_Room3PhaseEndsAt = now + ROOM3_START_SECONDS;
        Room3_PlaySound("buttons/button1.wav");
    }
}

stock Room3_FrameUpdate()
{
    if (!g_PluginReady || !g_Room3Ready) return FMRES_IGNORED;
    new Float:now = get_gametime();

    if (g_Room3Phase >= ROOM3_ARMING && g_Room3Phase <= ROOM3_CHAOS_ACTIVE)
    {
        new alive;
        for (new id = 1; id <= g_MaxPlayers; id++) if (Room3_IsInsideArena(id)) { alive=1; break; }
        if (!alive)
        {
            Room3_Abort(now);
            return FMRES_IGNORED;
        }
    }
    switch (g_Room3Phase)
    {
        case ROOM3_IDLE: Room3_UpdateCrystals(now);
        case ROOM3_ARMING: if (now >= g_Room3PhaseEndsAt) Room3_AdvanceRun(now);
        case ROOM3_COLOR_DEPLOY, ROOM3_COLOR_WAIT, ROOM3_COLOR_FIND, ROOM3_COLOR_HOLD, ROOM3_COLOR_RETRACT:
        {
            if (Color_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_SPINNER_LOW_RISE, ROOM3_SPINNER_PREPARE, ROOM3_SPINNER_RISE, ROOM3_SPINNER_ACTIVE, ROOM3_SPINNER_LOWER:
        {
            if (Spinner_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_CEILING_WARNING, ROOM3_CEILING_DOWN, ROOM3_CEILING_HOLD, ROOM3_CEILING_UP, ROOM3_CEILING_LEAVE:
        {
            if (Ceiling_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_LASER_WARNING, ROOM3_LASER_ACTIVE:
        {
            if (Laser_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_BLOCK_WARNING, ROOM3_BLOCK_ACTIVE:
        {
            if (Blocks_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_ISLAND_PREPARE, ROOM3_ISLAND_ACTIVE, ROOM3_ISLAND_FINISH:
        {
            if (Islands_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_SCAN_WARNING, ROOM3_SCAN_PREVIEW, ROOM3_SCAN_ACTIVE:
        {
            if (Scanner_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_METEOR_INTRO, ROOM3_METEOR_ACTIVE:
        {
            if (Meteor_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_CHAOS_INTRO, ROOM3_CHAOS_ACTIVE:
        {
            if (Chaos_Update(now)) Room3_AdvanceRun(now);
        }
        case ROOM3_EXIT_OPEN:
        {
            if (g_Room3PhaseEndsAt == 0.0 && Door_Ready(g_Room3DoorMotion[1])) g_Room3PhaseEndsAt = now + ROOM3_EXIT_SECONDS;
            else if (g_Room3PhaseEndsAt > 0.0 && now >= g_Room3PhaseEndsAt) Room3_Reset();
        }
        case ROOM3_RESET_DELAY: if (now >= g_Room3PhaseEndsAt) Room3_Reset();
    }
    // Damage on the first HOLD/SPIN frame too; cannot dodge a phase boundary.
    if (g_Room3Phase == ROOM3_COLOR_HOLD || g_Room3Phase == ROOM3_SPINNER_ACTIVE
        || g_Room3Phase == ROOM3_ISLAND_ACTIVE || g_Room3Phase == ROOM3_ISLAND_FINISH
        || (g_Room3Phase >= ROOM3_CEILING_DOWN && g_Room3Phase <= ROOM3_CEILING_UP)) for (new id = 1; id <= g_MaxPlayers; id++) Room3_CheckHazards(id, g_SpinnerAngle, g_SpinnerAngle);
    return FMRES_IGNORED;
}

stock Room3_OnFrame()
{
    if (!g_PluginReady || !g_Room3Ready) return;
    Player_BeginSnapshot();
    Room3_FrameUpdate();
    g_PlayerSnapshotActive = false;
}
