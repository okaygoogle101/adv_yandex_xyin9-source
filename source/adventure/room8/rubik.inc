new g_Room8DoorMotion;
// Room 8: shared 2x2 cube; a solved cube opens the exit for 10 seconds.
// All visible pieces reuse one MDL. The only brush model is a shootable proxy.
// Sticker centers orbit the cube; controls stay fixed; shots are resolved by coordinates.
#include "adventure/room8/layout.inc"
#include "adventure/room8/scrambles.inc"

new const R8_MODEL[] = "models/yx_xyin9/yx_rubik2_parts.mdl";
new g_Room8ModelIndex, g_Room8Hitbox, g_Room8ExplosionSprite;
const Float:R8_TURN_SECONDS = 0.45;
const Float:R8_DOOR_SECONDS = 0.8;
const Float:R8_EXIT_HOLD_SECONDS = 10.0;
const Float:R8_SCRAMBLE_TURN_SECONDS = 0.12;
const Float:R8_TRAP_RADIUS = 200.0;
const Float:R8_TRAP_DAMAGE = 50.0;
enum { R8_GATE_CLOSED, R8_GATE_OPENING, R8_GATE_OPEN, R8_GATE_CLOSING };
new g_Room8GatePhase, g_Room8Scramble = -1, g_Room8ScrambleStep;
new bool:g_Room8Shuffling, bool:g_Room8UseHeld[33];
new Float:g_Room8TurnDuration;
new Float:g_Room8TrapOrigin[3];
new bool:g_Room8Ready, bool:g_Room8Busy, bool:g_Room8Solved;
new g_Room8Sticker[24], g_Room8Control[32], g_Room8Color[24];
new g_Room8Button, g_Room8Gate, g_Room8Move, g_Room8ActiveControl;
new Float:g_Room8TurnAt, Float:g_Room8DoorAt;
new Float:g_Room8UseAt[33];
new Float:g_Room8StickerHome[24][3];
new const Float:R8_CENTRE[3] = {-2200.0, 2200.0, 1240.0};

stock Room8_Precache()
{
    if (file_exists(R8_MODEL)) g_Room8ModelIndex = precache_model(R8_MODEL);
    g_Room8ExplosionSprite = precache_model("sprites/zerogxplode.spr");
    precache_sound("weapons/explode3.wav");
    // Use the weapon's own precache list, including its firing event.
    new ent = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "weapon_usp"));
    if (pev_valid(ent)) { ExecuteHam(Ham_Precache, ent); engfunc(EngFunc_RemoveEntity, ent); }
}

stock bool:Room8_Init()
{
    if (!Map_FindEntity("adventure_room8_v63"))
    {
        if (Map_FindEntity("adventure_room8_v61")) return Room8_Missing();
        return true;
    }
    g_Room8Button = Map_FindEntity("room8_usp_button");
    g_Room8Gate = Map_FindEntity("room8_exit_gate");
    if (!g_Room8Button || !g_Room8Gate) return Room8_Missing();
    g_Room8Hitbox = Map_FindEntity("room8_cube_hitbox");
    if (!g_Room8Hitbox || !g_Room8ModelIndex) return Room8_Missing();
    set_pev(g_Room8Hitbox, pev_effects, pev(g_Room8Hitbox, pev_effects) | EF_NODRAW);
    set_pev(g_Room8Hitbox, pev_takedamage, DAMAGE_YES);
    set_pev(g_Room8Hitbox, pev_health, 10000.0);
    new name[32];
    for (new i = 0; i < 24; i++)
    {
        formatex(name, charsmax(name), "room8_sticker_%d", i);
        g_Room8Sticker[i] = Map_FindEntity(name);
        if (!g_Room8Sticker[i]) return Room8_Missing();
        Room8_ConfigurePart(g_Room8Sticker[i], 0, i / 4, 255.0);
        pev(g_Room8Sticker[i], pev_origin, g_Room8StickerHome[i]);
    }
    for (new i = 0; i < 32; i++)
    {
        formatex(name, charsmax(name), "room8_control_%d", i);
        g_Room8Control[i] = Map_FindEntity(name);
        if (!g_Room8Control[i]) return Room8_Missing();
        Room8_ConfigurePart(g_Room8Control[i], R8_CONTROL_DESC[i][3], 6, 100.0);
    }
    g_Room8DoorMotion = Door_Add(g_Room8Gate, 160.0, 160.0 / R8_DOOR_SECONDS);
    new Float:buttonLo[3], Float:buttonHi[3];
    pev(g_Room8Button, pev_absmin, buttonLo); pev(g_Room8Button, pev_absmax, buttonHi);
    for (new k = 0; k < 3; k++)
    {
        g_Room8TrapOrigin[k] = (buttonLo[k] + buttonHi[k]) * 0.5;
    }
    g_Room8TrapOrigin[0] = buttonHi[0] + 7.0; // Just in front of the west-wall button.
    g_Room8Ready = true;
    Room8_Reset();
    return true;
}

stock bool:Room8_Missing()
{
    set_fail_state("Room 8 requires map v63 and models/yx_xyin9/yx_rubik2_parts.mdl.");
    return false;
}

stock Room8_ApplyMove(move)
{
    new next[24];
    for (new i = 0; i < 24; i++) next[R8_PERM[move][i]] = g_Room8Color[i];
    for (new i = 0; i < 24; i++) g_Room8Color[i] = next[i];
}

stock bool:Room8_IsSolved()
{
    for (new face = 0; face < 6; face++)
        for (new i = 1; i < 4; i++)
            if (g_Room8Color[face * 4 + i] != g_Room8Color[face * 4]) return false;
    return true;
}

// All 56 parts share one precache entry. Body selects geometry, skin selects color.
stock Room8_ConfigurePart(ent, body, skin, Float:alpha)
{
    set_pev(ent, pev_movetype, MOVETYPE_NONE);
    set_pev(ent, pev_solid, SOLID_NOT);
    set_pev(ent, pev_takedamage, DAMAGE_NO);
    set_pev(ent, pev_body, body);
    set_pev(ent, pev_skin, skin);
    set_pev(ent, pev_sequence, 0);
    set_pev(ent, pev_frame, 0.0);
    set_pev(ent, pev_framerate, 0.0);
    set_pev(ent, pev_nextthink, 0.0);
    set_pev(ent, pev_rendermode, alpha < 255.0 ? kRenderTransTexture : kRenderNormal);
    set_pev(ent, pev_renderamt, alpha);
    // No client interpolation back across the cube when a completed move commits.
    set_pev(ent, pev_effects, pev(ent, pev_effects) | EF_NOINTERP);
    new Float:lo[3] = {-57.0,-57.0,-57.0}, Float:hi[3] = {57.0,57.0,57.0};
    engfunc(EngFunc_SetSize, ent, lo, hi);
}

stock Room8_RotateVector(Float:p[3], axis, Float:c, Float:s)
{
    new u = (axis + 1) % 3, v = (axis + 2) % 3;
    new Float:x = p[u], Float:y = p[v];
    p[u] = x * c - y * s;
    p[v] = x * s + y * c;
}

stock Room8_PartAngles(face, axis, Float:c, Float:s, Float:angle[3])
{
    new Float:r[3], Float:u[3], Float:n[3];
    for (new k = 0; k < 3; k++) { r[k] = R8_RIGHT[face][k]; u[k] = R8_UP[face][k]; n[k] = R8_NORMAL[face][k]; }
    Room8_RotateVector(r, axis, c, s);
    Room8_RotateVector(u, axis, c, s);
    Room8_RotateVector(n, axis, c, s);
    // StudioSetUpTransform negates entity pitch before applying AngleMatrix.
    angle[0] = floatasin(floatclamp(r[2], -1.0, 1.0), degrees);
    if (r[0] * r[0] + r[1] * r[1] > 0.000001)
    {
        angle[1] = floatatan2(r[1], r[0], degrees);
        angle[2] = floatatan2(u[2], n[2], degrees);
    }
    else { angle[1] = floatatan2(-u[0], u[1], degrees); angle[2] = 0.0; }
}

stock Room8_Paint()
{
    new Float:angle[3];
    for (new i = 0; i < 24; i++)
    {
        if (i % 4 == 0) Room8_PartAngles(i / 4, 0, 1.0, 0.0, angle);
        set_pev(g_Room8Sticker[i], pev_angles, angle);
        engfunc(EngFunc_SetOrigin, g_Room8Sticker[i], g_Room8StickerHome[i]);
        set_pev(g_Room8Sticker[i], pev_skin, g_Room8Color[i]);
    }
}

stock Room8_SetControls(skin, Float:alpha)
{
    for (new i = 0; i < 32; i++)
    {
        set_pev(g_Room8Control[i], pev_skin, skin);
        set_pev(g_Room8Control[i], pev_renderamt, alpha);
    }
}

stock Room8_StartTurn(move, control, Float:now, Float:duration)
{
    g_Room8Move = move;
    g_Room8ActiveControl = control;
    g_Room8Busy = true;
    g_Room8TurnAt = now;
    g_Room8TurnDuration = duration;
    if (control >= 0)
    {
        set_pev(g_Room8Control[control], pev_skin, 7);
        set_pev(g_Room8Control[control], pev_renderamt, 160.0);
        emit_sound(g_Room8Control[control], CHAN_ITEM, "buttons/button3.wav", 0.6, ATTN_NORM, 0, 100);
    }
}

stock Room8_BeginShuffle(Float:now)
{
    // The catalogue was exhaustively verified offline. No expensive solver here.
    new previous = g_Room8Scramble;
    do { g_Room8Scramble = random_num(0, sizeof R8_SCRAMBLES - 1); } while (g_Room8Scramble == previous);
    g_Room8ScrambleStep = 0;
    g_Room8Shuffling = true;
    g_Room8Solved = false;
    Room8_SetControls(7, 160.0);
    Room8_StartTurn(R8_SCRAMBLES[g_Room8Scramble][0], -1, now, R8_SCRAMBLE_TURN_SECONDS);
}

stock Room8_Reset()
{
    if (!g_Room8Ready) return;
    g_Room8Busy = false;
    g_Room8Solved = false;
    g_Room8Shuffling = false;
    g_Room8DoorAt = 0.0;
    g_Room8GatePhase = R8_GATE_CLOSED;
    for (new i = 0; i < 24; i++) g_Room8Color[i] = i / 4;
    Room8_Paint();
    for (new i = 0; i < 24; i++) set_pev(g_Room8Sticker[i], pev_nextthink, 0.0);
    for (new i = 0; i < 32; i++) set_pev(g_Room8Control[i], pev_nextthink, 0.0);
    for (new id = 1; id <= 32; id++) { g_Room8UseAt[id] = 0.0; g_Room8UseHeld[id] = false; }
    Door_Set(g_Room8DoorMotion, false, true);
    Room8_BeginShuffle(get_gametime());
}

public Room8_OnRoundStart()
{
    // Deliberately separate from Adventure_Reset (also used on plugin pause).
    if (g_PluginReady) Room8_Reset();
}

stock bool:Room8_IsPlayerInside(id)
{
    if (id < 1 || id > g_MaxPlayers || !is_user_alive(id)) return false;
    new Float:p[3]; pev(id, pev_origin, p);
    return p[0] > -2936.0 && p[0] < -1464.0 && p[1] > 1464.0 && p[1] < 2936.0 && p[2] > 984.0 && p[2] < 1728.0;
}

// Determine a triangle on the first cube surface actually hit by the bullet.
// This replaces 32 solid brush entities with a small analytic point-in-triangle test.
stock Room8_ControlAt(const Float:hit[3])
{
    new Float:p[3]; for (new k = 0; k < 3; k++) p[k] = hit[k] - R8_CENTRE[k];
    if (floatabs(p[2]) >= floatmax(floatabs(p[0]), floatabs(p[1]))) return -1;
    new face;
    if (floatabs(p[0]) > floatabs(p[1])) face = p[0] > 0.0 ? 1 : 3;
    else face = p[1] > 0.0 ? 2 : 0;
    new Float:x = p[0] * R8_RIGHT[face][0] + p[1] * R8_RIGHT[face][1], Float:y = p[2];
    new col = x >= 0.0 ? 1 : -1, row = y >= 0.0 ? 1 : -1;
    x -= float(col) * 60.0; y -= float(row) * 60.0;
    if (floatabs(x) > 54.0 || floatabs(y) > 54.0) return -1;
    new body;
    if (col == 1 && x >= floatabs(y)) body = 1;
    else if (col == -1 && -x >= floatabs(y)) body = 2;
    else if (row == 1 && y >= floatabs(x)) body = 3;
    else if (row == -1 && -y >= floatabs(x)) body = 4;
    else return -1;
    for (new i = 0; i < 32; i++) if (R8_CONTROL_DESC[i][0] == face && R8_CONTROL_DESC[i][1] == col && R8_CONTROL_DESC[i][2] == row && R8_CONTROL_DESC[i][3] == body) return i;
    return -1;
}

public Room8_OnTraceAttack(ent, attacker, Float:damage, Float:direction[3], trace, damageBits)
{
    if (!g_PluginReady || !g_Room8Ready || ent != g_Room8Hitbox) return HAM_IGNORED;
    if (!g_Room8Busy && !g_Room8Shuffling && !g_Room8Solved && Room8_IsPlayerInside(attacker) &&
        get_user_weapon(attacker) == CSW_USP && (damageBits & DMG_BULLET) && get_tr2(trace, TR_pHit) == ent)
    {
        new Float:hit[3]; get_tr2(trace, TR_vecEndPos, hit);
        new control = Room8_ControlAt(hit);
        if (control < 0) return HAM_SUPERCEDE;
        Room8_StartTurn(R8_CONTROL_MOVE[control], control, get_gametime(), R8_TURN_SECONDS);
    }
    return HAM_SUPERCEDE;
}

public Room8_OnTakeDamage(ent, inflictor, attacker, Float:damage, damageBits)
{
    return (g_Room8Ready && ent == g_Room8Hitbox) ? HAM_SUPERCEDE : HAM_IGNORED;
}

// Shared pusher controller handles movement, sound and lethal blocking.


stock Room8_UpdateGate(Float:now)
{
    if (g_Room8GatePhase == R8_GATE_CLOSED) return;
    if (g_Room8GatePhase == R8_GATE_OPEN)
    {
        if (now < g_Room8DoorAt) return;
        g_Room8GatePhase = R8_GATE_CLOSING;
        Door_Set(g_Room8DoorMotion, false);
    }
    if (!Door_Ready(g_Room8DoorMotion)) return;
    if (g_Room8GatePhase == R8_GATE_CLOSING)
    {
        g_Room8GatePhase = R8_GATE_CLOSED;
        g_Room8DoorAt = 0.0;
        Room8_BeginShuffle(now);
    }
    else
    {
        g_Room8GatePhase = R8_GATE_OPEN;
        g_Room8DoorAt = now + R8_EXIT_HOLD_SECONDS;
    }
}

stock Room8_Update(Float:now)
{
    if (!g_Room8Ready) return;
    Room8_UpdateGate(now);
    if (!g_Room8Busy) return;
    new Float:t = floatclamp((now - g_Room8TurnAt) / g_Room8TurnDuration, 0.0, 1.0);
    new axis = R8_MOVE[g_Room8Move][0], layer = R8_MOVE[g_Room8Move][1];
    new Float:angle[3], Float:turnAngle = 90.0 * float(R8_MOVE[g_Room8Move][2]) * t;
    new Float:c = floatcos(turnAngle, degrees), Float:s = floatsin(turnAngle, degrees);
    new lastFace = -1;
    for (new i = 0; i < 24; i++) if (R8_SLOT_POS[i][axis] == layer)
    {
        new Float:p[3];
        for (new a = 0; a < 3; a++) p[a] = g_Room8StickerHome[i][a] - R8_CENTRE[a];
        Room8_RotateVector(p, axis, c, s);
        for (new a = 0; a < 3; a++) p[a] += R8_CENTRE[a];
        engfunc(EngFunc_SetOrigin, g_Room8Sticker[i], p);
        if (lastFace != i / 4) { Room8_PartAngles(i / 4, axis, c, s, angle); lastFace = i / 4; }
        set_pev(g_Room8Sticker[i], pev_angles, angle);
    }
    if (t < 1.0) return;
    Room8_ApplyMove(g_Room8Move);
    Room8_Paint();
    g_Room8Busy = false;
    if (g_Room8Shuffling)
    {
        if (++g_Room8ScrambleStep < R8_SCRAMBLE_LENGTH[g_Room8Scramble])
            Room8_StartTurn(R8_SCRAMBLES[g_Room8Scramble][g_Room8ScrambleStep], -1, now, R8_SCRAMBLE_TURN_SECONDS);
        else { g_Room8Shuffling = false; Room8_SetControls(6, 100.0); }
        return;
    }
    set_pev(g_Room8Control[g_Room8ActiveControl], pev_skin, 6);
    set_pev(g_Room8Control[g_Room8ActiveControl], pev_renderamt, 100.0);
    if (Room8_IsSolved())
    {
        g_Room8Solved = true;
        Door_Set(g_Room8DoorMotion, true);
        g_Room8GatePhase = R8_GATE_OPENING;
        g_Room8DoorAt = now;
        emit_sound(g_Room8Gate, CHAN_ITEM, "buttons/bell1.wav", 0.9, ATTN_NORM, 0, 100);
    }
}

stock Room8_ButtonTrap()
{
    engfunc(EngFunc_MessageBegin, MSG_PVS, SVC_TEMPENTITY, g_Room8TrapOrigin, 0);
    write_byte(TE_EXPLOSION);
    engfunc(EngFunc_WriteCoord, g_Room8TrapOrigin[0]);
    engfunc(EngFunc_WriteCoord, g_Room8TrapOrigin[1]);
    engfunc(EngFunc_WriteCoord, g_Room8TrapOrigin[2]);
    write_short(g_Room8ExplosionSprite); write_byte(30); write_byte(15); write_byte(4); // No duplicate default explosion sound.
    message_end();
    emit_sound(g_Room8Button, CHAN_AUTO, "weapons/explode3.wav", 1.0, ATTN_NORM, 0, 100);
    for (new id = 1; id <= g_MaxPlayers; id++) if (Room8_IsPlayerInside(id))
    {
        new Float:p[3], Float:distanceSquared;
        pev(id, pev_origin, p);
        for (new k = 0; k < 3; k++) distanceSquared += (p[k] - g_Room8TrapOrigin[k]) * (p[k] - g_Room8TrapOrigin[k]);
        if (distanceSquared <= R8_TRAP_RADIUS * R8_TRAP_RADIUS)
            // DMG_FALL bypasses CS armor absorption: base health damage stays 50.
            ExecuteHamB(Ham_TakeDamage, id, g_Room8Button, 0, R8_TRAP_DAMAGE, DMG_BLAST | DMG_FALL);
    }
}

stock Room8_UseButton(id)
{
    if (!Room8_IsPlayerInside(id) || !(pev(id, pev_button) & IN_USE) || !Player_IsAimingAt(id, g_Room8Button)) return HAM_SUPERCEDE;
    new Float:now = get_gametime();
    if (g_Room8UseHeld[id]) return HAM_SUPERCEDE;
    g_Room8UseHeld[id] = true;
    if (now < g_Room8UseAt[id]) return HAM_SUPERCEDE;
    g_Room8UseAt[id] = now + 0.5;
    // One roll per accepted press, before either weapon or ammunition is given.
    if (random_num(1, 100) <= 10) { Room8_ButtonTrap(); return HAM_SUPERCEDE; }
    if (!user_has_weapon(id, CSW_USP))
    {
        // Native CS inventory allows one pistol. Replace it through the normal
        // drop command rather than private-data offsets or another AMXX module.
        new weapons[32], count, name[24];
        get_user_weapons(id, weapons, count);
        for (new i = 0; i < count; i++)
        {
            switch (weapons[i])
            {
                case CSW_GLOCK18, CSW_P228, CSW_DEAGLE, CSW_ELITE, CSW_FIVESEVEN:
                {
                    get_weaponname(weapons[i], name, charsmax(name));
                    engclient_cmd(id, "drop", name);
                }
            }
        }
        new ent = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "weapon_usp"));
        if (pev_valid(ent))
        {
            set_pev(ent, pev_spawnflags, 1 << 30); // SF_NORESPAWN: no dropped weapon respawn loop.
            ExecuteHamB(Ham_Spawn, ent);
            new Float:p[3]; pev(id, pev_origin, p);
            engfunc(EngFunc_SetOrigin, ent, p);
            ExecuteHamB(Ham_Touch, ent, id);
            // Failed pickup must not leave an entity on the floor.
            if (pev_valid(ent) && pev(ent, pev_owner) != id) engfunc(EngFunc_RemoveEntity, ent);
        }
    }
    ExecuteHamB(Ham_GiveAmmo, id, 100, "45acp", 100);
    return HAM_SUPERCEDE;
}

stock Room8_UpdateUseKeys()
{
    if (!g_Room8Ready) return;
    for (new id = 1; id <= g_MaxPlayers; id++)
        if (g_Room8UseHeld[id] && (!is_user_alive(id) || !(pev(id, pev_button) & IN_USE)))
            g_Room8UseHeld[id] = false;
}
