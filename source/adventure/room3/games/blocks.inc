// Room 3 / game 5: 18 waves (20 blocks each), warnings followed by falling blocks.
// Fixed pool: the pool retains capacity for future changes; current waves contain 20 blocks.
#define BLOCK_POOL_SIZE 60
#define BLOCK_GRID_SIZE 100
new const BLOCK_WAVE_SIZE[3] = {20, 20, 20};
new const Float:BLOCK_WAVE_INTERVAL[3] = {1.5, 1.0, 0.75};
new const Float:BLOCK_WAVE_WARNING[3] = {1.2, 0.9, 0.7};
new bool:g_BlocksReady;
new g_BlockEntity[BLOCK_POOL_SIZE], g_BlockWarning[BLOCK_GRID_SIZE], g_BlockModel[2][16];
new g_BlockStage[BLOCK_POOL_SIZE], g_BlockTile[BLOCK_POOL_SIZE], g_BlockHitUserId[BLOCK_POOL_SIZE][33];
new g_BlockWave, g_BlockWarningLevel[BLOCK_GRID_SIZE];
new bool:g_BlockPlayerInRoom[33];
new g_BlockPlayerUserId[33];
new Float:g_BlockPlayerMin[33][3], Float:g_BlockPlayerMax[33][3];
new Float:g_BlockFallAt[BLOCK_POOL_SIZE], Float:g_BlockBottom[BLOCK_POOL_SIZE];
new Float:g_BlockNextWaveAt, Float:g_BlockNextBeepAt, Float:g_BlockImpactSoundAt;

stock bool:Blocks_Init()
{
    if (!Map_FindEntity("adventure_blocks_v28"))
    {
        set_fail_state("Falling blocks require adventure_start_v28.map and plugin 3.6.0.");
        return false;
    }
    new sample = Map_FindEntity("room3_block_template");
    new warning = Map_FindEntity("room3_block_warning_template");
    if (!pev_valid(sample) || !pev_valid(warning)) return Room3_FailMissingEntities();
    pev(sample, pev_model, g_BlockModel[0], 15);
    pev(warning, pev_model, g_BlockModel[1], 15);
    if (g_BlockModel[0][0] != '*' || g_BlockModel[1][0] != '*') return Room3_FailMissingEntities();
    Entity_SetVisible(sample, false);
    Entity_SetVisible(warning, false);
    new allocatedClass = engfunc(EngFunc_AllocString, "info_target");
    for (new slot = 0; slot < BLOCK_POOL_SIZE; slot++)
    {
        new ent = engfunc(EngFunc_CreateNamedEntity, allocatedClass);
        if (!pev_valid(ent)) return Room3_FailMissingEntities();
        g_BlockEntity[slot] = ent;
        engfunc(EngFunc_SetModel, ent, g_BlockModel[0]);
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_movetype, MOVETYPE_NONE);
        Entity_SetVisible(ent, false);
    }
    for (new tile = 0; tile < BLOCK_GRID_SIZE; tile++)
    {
        new ent = engfunc(EngFunc_CreateNamedEntity, allocatedClass);
        if (!pev_valid(ent)) return Room3_FailMissingEntities();
        g_BlockWarning[tile] = ent;
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_movetype, MOVETYPE_NONE);
        Entity_SetVisible(ent, false);
    }
    Blocks_ConfigureWarnings();
    g_BlocksReady = true;
    Blocks_Reset();
    return true;
}

stock Blocks_Reset()
{
    if (!g_BlocksReady) return;
    for (new slot = 0; slot < BLOCK_POOL_SIZE; slot++)
    {
        g_BlockStage[slot] = 0;
        g_BlockFallAt[slot] = 0.0;
        Entity_SetVisible(g_BlockEntity[slot], false);
    }
    for (new tile = 0; tile < BLOCK_GRID_SIZE; tile++)
    {
        g_BlockWarningLevel[tile] = 0;
        Entity_SetVisible(g_BlockWarning[tile], false);
    }
    g_BlockWave = 0;
    g_BlockNextWaveAt = 0.0;
    g_BlockNextBeepAt = 0.0;
    g_BlockImpactSoundAt = 0.0;
}

// GoldSrc ScreenShake: amplitude/duration are 4.12 fixed point, frequency is 8.8.
// One message per living human participant; no movement, damage or recurring tasks.
stock Blocks_ShakeIntro()
{
    new message = get_user_msgid("ScreenShake");
    if (!message) return;
    for (new id = 1; id <= g_MaxPlayers; id++)
    {
        if (!Room3_IsInsideArena(id) || is_user_bot(id)) continue;
        message_begin(MSG_ONE, message, _, id);
        write_short(4 * 4096); // Amplitude: noticeable but brief.
        write_short(4096);     // Duration: exactly one second.
        write_short(12 * 256); // Frequency: 12 Hz.
        message_end();
    }
}

stock Blocks_Prepare(Float:now)
{
    Blocks_Reset();
    Blocks_ConfigureWarnings();
    Blocks_ShakeIntro();
    g_Room3Phase = ROOM3_BLOCK_WARNING;
    g_Room3PhaseEndsAt = now + BLOCK_COUNTDOWN_SECONDS;
    g_BlockNextBeepAt = now + 1.0;
    Room3_PlaySound("buttons/blip1.wav");
}

stock Blocks_Tier()
{
    if (g_BlockWave < 4) return 0;
    if (g_BlockWave < 10) return 1;
    return 2;
}

stock Blocks_PlayerTile(id)
{
    if (!is_user_alive(id)) return -1;
    new Float:o[3];
    Player_ReadOrigin(id, o);
    if (o[0] < -640.0 || o[0] >= 640.0 || o[1] < -2496.0 || o[1] >= -1216.0
        || o[2] < -1336.0 || o[2] >= -952.0) return -1;
    new col = floatround((o[0] + 640.0) / 128.0, floatround_floor);
    new row = floatround((o[1] + 2496.0) / 128.0, floatround_floor);
    return row * 10 + col;
}

stock Blocks_Start(slot, tile, Float:now, Float:warning)
{
    g_BlockTile[slot] = tile;
    g_BlockStage[slot] = 1; // harmless warning/hover
    g_BlockFallAt[slot] = now + warning;
    g_BlockBottom[slot] = BLOCK_START_Z;
    for (new id = 1; id <= 32; id++) g_BlockHitUserId[slot][id] = 0;
    new Float:o[3];
    o[0] = -576.0 + float(tile % 10) * 128.0;
    o[1] = -2432.0 + float(tile / 10) * 128.0;
    o[2] = BLOCK_START_Z;
    engfunc(EngFunc_SetOrigin, g_BlockEntity[slot], o);
    Entity_SetVisible(g_BlockEntity[slot], true);
}

stock bool:Blocks_LaunchWave(Float:now)
{
    new tier = Blocks_Tier(), count = BLOCK_WAVE_SIZE[tier], freeSlots;
    for (new slot = 0; slot < BLOCK_POOL_SIZE; slot++) if (!g_BlockStage[slot]) freeSlots++;
    // If custom timings ever exhaust the pool, postpone instead of skipping warnings.
    if (freeSlots < count) return false;
    new occupied[100], marked[100], selected[100], choices[50], pool[100];
    new occupiedCount, chosen, remaining;
    for (new id = 1; id <= g_MaxPlayers; id++)
    {
        new tile = Blocks_PlayerTile(id);
        if (tile < 0 || marked[tile]) continue;
        marked[tile] = 1;
        occupied[occupiedCount++] = tile;
    }
    // Up to half the targets use DISTINCT occupied cells; freeze targets before warning.
    while (chosen < count / 2 && occupiedCount > 0)
    {
        new pick = random_num(0, occupiedCount - 1), tile = occupied[pick];
        occupiedCount--;
        occupied[pick] = occupied[occupiedCount];
        selected[tile] = 1;
        choices[chosen++] = tile;
    }
    for (new tile = 0; tile < BLOCK_GRID_SIZE; tile++) if (!selected[tile]) pool[remaining++] = tile;
    while (chosen < count)
    {
        new pick = random_num(0, remaining - 1), tile = pool[pick];
        remaining--;
        pool[pick] = pool[remaining];
        choices[chosen++] = tile;
    }
    new next;
    for (new slot = 0; slot < BLOCK_POOL_SIZE && next < count; slot++) if (!g_BlockStage[slot])
        Blocks_Start(slot, choices[next++], now, BLOCK_WAVE_WARNING[tier]);
    g_BlockWave++;
    g_BlockNextWaveAt = now + BLOCK_WAVE_INTERVAL[tier];
    Room3_PlaySound("buttons/button3.wav");
    return true;
}

stock Blocks_HitPlayer(id, slot)
{
    new uid = Player_ReadUserId(id);
    if (g_BlockHitUserId[slot][id] == uid) return;
    g_BlockHitUserId[slot][id] = uid;
    ExecuteHamB(Ham_TakeDamage, id, g_BlockEntity[slot], 0, BLOCK_DAMAGE, DMG_DROWN);
    if (!is_user_alive(id)) g_BlockPlayerInRoom[id] = false;
}

// Cache engine values once per update/player command, not once per falling block.
stock Blocks_CachePlayer(id)
{
    g_BlockPlayerInRoom[id] = Room3_IsInsideArena(id);
    if (!g_BlockPlayerInRoom[id]) return;
    g_BlockPlayerUserId[id] = Player_ReadUserId(id);
    Player_ReadMin(id, g_BlockPlayerMin[id]);
    Player_ReadMax(id, g_BlockPlayerMax[id]);
    for (new a = 0; a < 3; a++) { g_BlockPlayerMin[id][a] += 1.0; g_BlockPlayerMax[id][a] -= 1.0; }
}

stock Blocks_CheckCached(id, slot, Float:oldBottom, Float:newBottom)
{
    if (!g_BlockPlayerInRoom[id] || g_BlockHitUserId[slot][id] == g_BlockPlayerUserId[id]) return;
    new tile = g_BlockTile[slot];
    new Float:x = -640.0 + float(tile % 10) * 128.0, Float:y = -2496.0 + float(tile / 10) * 128.0;
    if (g_BlockPlayerMax[id][0] > x && g_BlockPlayerMin[id][0] < x + 128.0
        && g_BlockPlayerMax[id][1] > y && g_BlockPlayerMin[id][1] < y + 128.0
        && g_BlockPlayerMax[id][2] >= newBottom && g_BlockPlayerMin[id][2] <= oldBottom + BLOCK_HEIGHT)
        Blocks_HitPlayer(id, slot);
}

stock Blocks_CheckSegment(id, slot, Float:oldBottom, Float:newBottom)
{
    Blocks_CachePlayer(id);
    Blocks_CheckCached(id, slot, oldBottom, newBottom);
}

stock Blocks_CheckHazards(id)
{
    if (!g_BlocksReady || g_Room3Phase != ROOM3_BLOCK_ACTIVE) return;
    Blocks_CachePlayer(id);
    for (new slot = 0; slot < BLOCK_POOL_SIZE; slot++) if (g_BlockStage[slot] == 2)
        Blocks_CheckCached(id, slot, g_BlockBottom[slot], g_BlockBottom[slot]);
}

stock Blocks_Advance(Float:now)
{
    for (new id = 1; id <= g_MaxPlayers; id++) Blocks_CachePlayer(id);
    for (new slot = 0; slot < BLOCK_POOL_SIZE; slot++)
    {
        if (!g_BlockStage[slot] || now < g_BlockFallAt[slot]) continue;
        if (g_BlockStage[slot] == 3)
        {
            if (now >= g_BlockFallAt[slot] + BLOCK_FALL_SECONDS + BLOCK_FLASH_SECONDS)
            {
                g_BlockStage[slot] = 0;
            }
            continue;
        }
        g_BlockStage[slot] = 2;
        new Float:fraction = floatmin(1.0, floatmax(0.0, (now - g_BlockFallAt[slot]) / BLOCK_FALL_SECONDS));
        new Float:bottom = BLOCK_START_Z + (-1336.0 - BLOCK_START_Z) * fraction;
        for (new id = 1; id <= g_MaxPlayers; id++) Blocks_CheckCached(id, slot, g_BlockBottom[slot], bottom);
        g_BlockBottom[slot] = bottom;
        new Float:o[3];
        pev(g_BlockEntity[slot], pev_origin, o);
        o[2] = bottom;
        engfunc(EngFunc_SetOrigin, g_BlockEntity[slot], o);
        if (fraction >= 1.0)
        {
            Entity_SetVisible(g_BlockEntity[slot], false);
            g_BlockStage[slot] = 3; // brief impact flash, no lingering collision or damage
            if (now >= g_BlockImpactSoundAt)
            {
                emit_sound(g_BlockEntity[slot], CHAN_ITEM, "debris/bustcrate1.wav", 0.5, ATTN_NORM, 0, 100);
                g_BlockImpactSoundAt = now + 0.15;
            }
        }
    }
}

// One warning entity per TILE, not per falling block. Overlapping waves never
// create coincident translucent surfaces or hide another wave's warning.
stock Blocks_RefreshWarnings()
{
    new level[100];
    for (new slot = 0; slot < BLOCK_POOL_SIZE; slot++) if (g_BlockStage[slot])
    {
        new tile = g_BlockTile[slot], value = g_BlockStage[slot] == 3 ? 2 : 1;
        if (value > level[tile]) level[tile] = value;
    }
    for (new tile = 0; tile < BLOCK_GRID_SIZE; tile++) if (level[tile] != g_BlockWarningLevel[tile])
    {
        g_BlockWarningLevel[tile] = level[tile];
        set_pev(g_BlockWarning[tile], pev_renderamt, level[tile] == 2 ? 255.0 : 180.0);
        Entity_SetVisible(g_BlockWarning[tile], level[tile] != 0);
    }
}

stock bool:Blocks_Update(Float:now)
{
    if (g_Room3Phase == ROOM3_BLOCK_WARNING)
    {
        if (now < g_Room3PhaseEndsAt)
        {
            if (now >= g_BlockNextBeepAt)
            {
                Room3_PlaySound("buttons/blip1.wav");
                g_BlockNextBeepAt += 1.0;
            }
            return false;
        }
        g_Room3Phase = ROOM3_BLOCK_ACTIVE;
        g_BlockNextWaveAt = now;
    }
    Blocks_Advance(now);
    if (g_BlockWave < 18 && now >= g_BlockNextWaveAt) Blocks_LaunchWave(now);
    Blocks_RefreshWarnings();
    if (g_BlockWave < 18) return false;
    for (new slot = 0; slot < BLOCK_POOL_SIZE; slot++) if (g_BlockStage[slot]) return false;
    return true;
}

stock Blocks_ConfigureWarnings()
{
    new Float:o[3], Float:white[3] = {255.0, 255.0, 255.0};
    for (new tile = 0; tile < BLOCK_GRID_SIZE; tile++)
    {
        new ent = g_BlockWarning[tile];
        engfunc(EngFunc_SetModel, ent, g_BlockModel[1]);
        set_pev(ent, pev_rendermode, kRenderTransTexture);
        set_pev(ent, pev_renderamt, 180.0);
        set_pev(ent, pev_rendercolor, white);
        o[0] = -576.0 + float(tile % 10) * 128.0;
        o[1] = -2432.0 + float(tile / 10) * 128.0;
        o[2] = -1334.0;
        engfunc(EngFunc_SetOrigin, ent, o);
    }
}
