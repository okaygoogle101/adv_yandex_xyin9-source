// Finale: independent 3x3 cube. First solve unlocks the alcove until new round.
// Further turns remain enabled, with no timer and no participant roster.
new bool:g_FinalCubeMap;
new g_FinalCubeDoor;
// All visible pieces reuse one MDL. The only brush model is a shootable proxy.
// Sticker centers orbit the cube; controls stay fixed; shots are resolved by coordinates.
#include "adventure/final_cube/layout.inc"

new const RFC_MODEL[] = "models/yx_xyin9/yx_rubik3_parts.mdl";
new g_FinalCubeModelIndex, g_FinalCubeHitbox;
const Float:RFC_TURN_SECONDS = 0.45;
new bool:g_FinalCubeReady, bool:g_FinalCubeBusy, bool:g_FinalCubeSolved;
new g_FinalCubeSticker[54], g_FinalCubeControl[48], g_FinalCubeColor[54];
new g_FinalCubeButton, g_FinalCubeGate, g_FinalCubeMove, g_FinalCubeActiveControl;
new Float:g_FinalCubeTurnAt;
new Float:g_FinalCubeUseAt[33];
new Float:g_FinalCubeStickerHome[54][3];
new const Float:RFC_CENTRE[3] = {-1752.0, -2463.0, -3584.0};

stock FinalCube_Precache()
{
    if (!g_FinalCubeMap) return;
    if (file_exists(RFC_MODEL)) g_FinalCubeModelIndex = precache_model(RFC_MODEL);

}

stock bool:FinalCube_Init()
{
    if (!Map_FindEntity("adventure_final_cube_v101")) return true;
    g_FinalCubeButton = Map_FindEntity("final_cube_usp_button");
    g_FinalCubeGate = Map_FindEntity("final_cube_exit_gate");
    if (!g_FinalCubeButton || !g_FinalCubeGate) return FinalCube_Missing();
    g_FinalCubeHitbox = Map_FindEntity("final_cube_cube_hitbox");
    if (!g_FinalCubeHitbox || !g_FinalCubeModelIndex) return FinalCube_Missing();
    set_pev(g_FinalCubeHitbox, pev_effects, pev(g_FinalCubeHitbox, pev_effects) | EF_NODRAW);
    set_pev(g_FinalCubeHitbox, pev_takedamage, DAMAGE_YES);
    set_pev(g_FinalCubeHitbox, pev_health, 10000.0);
    new name[32];
    for (new i = 0; i < 54; i++)
    {
        formatex(name, charsmax(name), "final_cube_sticker_%d", i);
        g_FinalCubeSticker[i] = Map_FindEntity(name);
        if (!g_FinalCubeSticker[i]) return FinalCube_Missing();
        FinalCube_ConfigurePart(g_FinalCubeSticker[i], 0, i / 9, 255.0);
        pev(g_FinalCubeSticker[i], pev_origin, g_FinalCubeStickerHome[i]);
    }
    for (new i = 0; i < 48; i++)
    {
        formatex(name, charsmax(name), "final_cube_control_%d", i);
        g_FinalCubeControl[i] = Map_FindEntity(name);
        if (!g_FinalCubeControl[i]) return FinalCube_Missing();
        FinalCube_ConfigurePart(g_FinalCubeControl[i], RFC_CONTROL_DESC[i][3], 6, 100.0);
    }
    g_FinalCubeDoor = Door_Add(g_FinalCubeGate, 52.0, 80.0);
    if (g_FinalCubeDoor < 0) return false;
    g_FinalCubeReady = true;
    FinalCube_Reset();
    return true;
}

stock bool:FinalCube_Missing()
{
    set_fail_state("Final cube requires map v101 and models/yx_xyin9/yx_rubik3_parts.mdl.");
    return false;
}

stock FinalCube_ApplyMove(move)
{
    new next[54];
    for (new i = 0; i < 54; i++) next[RFC_PERM[move][i]] = g_FinalCubeColor[i];
    for (new i = 0; i < 54; i++) g_FinalCubeColor[i] = next[i];
}

stock bool:FinalCube_IsSolved()
{
    for (new face = 0; face < 6; face++)
        for (new i = 1; i < 9; i++)
            if (g_FinalCubeColor[face * 9 + i] != g_FinalCubeColor[face * 9]) return false;
    return true;
}

// All 102 parts share one precache entry. Body selects geometry, skin selects color.
stock FinalCube_ConfigurePart(ent, body, skin, Float:alpha)
{
    set_pev(ent, pev_movetype, MOVETYPE_NONE);
    set_pev(ent, pev_solid, SOLID_NOT);
    set_pev(ent, pev_takedamage, DAMAGE_NO);
    set_pev(ent, pev_body, body);
    set_pev(ent, pev_skin, skin);
    set_pev(ent, pev_sequence, 0);
    set_pev(ent, pev_frame, 0.0);
    set_pev(ent, pev_framerate, 0.0);
    set_pev(ent, pev_nextthink, 0.0); // Static cycler: animation is driven only during a turn.
    set_pev(ent, pev_rendermode, alpha < 255.0 ? kRenderTransTexture : kRenderNormal);
    set_pev(ent, pev_renderamt, alpha);
    // No client interpolation back across the cube when a completed move commits.
    set_pev(ent, pev_effects, pev(ent, pev_effects) | EF_NOINTERP);
    new Float:lo[3] = {-38.0,-38.0,-38.0}, Float:hi[3] = {38.0,38.0,38.0};
    engfunc(EngFunc_SetSize, ent, lo, hi);
}

stock FinalCube_RotateVector(Float:p[3], axis, Float:c, Float:s)
{
    new u = (axis + 1) % 3, v = (axis + 2) % 3;
    new Float:x = p[u], Float:y = p[v];
    p[u] = x * c - y * s;
    p[v] = x * s + y * c;
}

stock FinalCube_PartAngles(face, axis, Float:c, Float:s, Float:angle[3])
{
    new Float:r[3], Float:u[3], Float:n[3];
    for (new k = 0; k < 3; k++) { r[k] = RFC_RIGHT[face][k]; u[k] = RFC_UP[face][k]; n[k] = RFC_NORMAL[face][k]; }
    FinalCube_RotateVector(r, axis, c, s);
    FinalCube_RotateVector(u, axis, c, s);
    FinalCube_RotateVector(n, axis, c, s);
    // StudioSetUpTransform negates entity pitch before applying AngleMatrix.
    angle[0] = floatasin(floatclamp(r[2], -1.0, 1.0), degrees);
    if (r[0] * r[0] + r[1] * r[1] > 0.000001)
    {
        angle[1] = floatatan2(r[1], r[0], degrees);
        angle[2] = floatatan2(u[2], n[2], degrees);
    }
    else { angle[1] = floatatan2(-u[0], u[1], degrees); angle[2] = 0.0; }
}

stock FinalCube_Paint()
{
    new Float:angle[3];
    for (new i = 0; i < 54; i++)
    {
        if (i % 9 == 0) FinalCube_PartAngles(i / 9, 0, 1.0, 0.0, angle);
        set_pev(g_FinalCubeSticker[i], pev_angles, angle);
        engfunc(EngFunc_SetOrigin, g_FinalCubeSticker[i], g_FinalCubeStickerHome[i]);
        set_pev(g_FinalCubeSticker[i], pev_skin, g_FinalCubeColor[i]);
    }
}

stock FinalCube_Reset()
{
    if (!g_FinalCubeReady) return;
    g_FinalCubeBusy = false;
    g_FinalCubeSolved = false;
    for (new i = 0; i < 54; i++) g_FinalCubeColor[i] = i / 9;
    // Legal quarter turns only: never assign arbitrary sticker colors.
    new previous = -1, move;
    for (new turn = 0; turn < 30; turn++)
    {
        do { move = random_num(0, 17); } while (move / 2 == previous);
        previous = move / 2;
        FinalCube_ApplyMove(move);
    }
    if (FinalCube_IsSolved()) FinalCube_ApplyMove(0);
    FinalCube_Paint();
    for (new i = 0; i < 48; i++)
    {
        set_pev(g_FinalCubeControl[i], pev_skin, 6);
        set_pev(g_FinalCubeControl[i], pev_renderamt, 100.0);
    }
    for (new id = 1; id <= 32; id++) g_FinalCubeUseAt[id] = 0.0;
    Door_Set(g_FinalCubeDoor, false, true);
}

public FinalCube_OnRoundStart()
{
    // Deliberately separate from Adventure_Reset (also used on plugin pause).
    if (g_PluginReady) FinalCube_Reset();
}

stock bool:FinalCube_IsPlayerInside(id)
{
    if (id < 1 || id > g_MaxPlayers || !is_user_alive(id) || pev(id, pev_iuser1) != 0) return false;
    new Float:p[3]; pev(id, pev_origin, p);
    return p[0] > -2056.0 && p[0] < -1448.0 && p[1] > -2815.0 && p[1] < -2047.0 && p[2] > -3800.0 && p[2] < -3320.0;
}

// Determine a triangle on the first cube surface actually hit by the bullet.
// This replaces 48 solid brush entities with a small analytic point-in-triangle test.
stock FinalCube_ControlAt(const Float:hit[3])
{
    new Float:p[3]; for (new k = 0; k < 3; k++) p[k] = hit[k] - RFC_CENTRE[k];
    if (floatabs(p[2]) >= floatmax(floatabs(p[0]), floatabs(p[1]))) return -1;
    new face;
    if (floatabs(p[0]) > floatabs(p[1])) face = p[0] > 0.0 ? 1 : 3;
    else face = p[1] > 0.0 ? 2 : 0;
    new Float:x = p[0] * RFC_RIGHT[face][0] + p[1] * RFC_RIGHT[face][1], Float:y = p[2];
    new col = floatround(x / 80.0), row = floatround(y / 80.0);
    if (col < -1 || col > 1 || row < -1 || row > 1) return -1;
    x -= float(col) * 80.0; y -= float(row) * 80.0;
    if (floatabs(x) > 36.0 || floatabs(y) > 36.0) return -1;
    new body;
    if (col == 1 && x >= floatabs(y)) body = 1;
    else if (col == -1 && -x >= floatabs(y)) body = 2;
    else if (row == 1 && y >= floatabs(x)) body = 3;
    else if (row == -1 && -y >= floatabs(x)) body = 4;
    else return -1;
    for (new i = 0; i < 48; i++) if (RFC_CONTROL_DESC[i][0] == face && RFC_CONTROL_DESC[i][1] == col && RFC_CONTROL_DESC[i][2] == row && RFC_CONTROL_DESC[i][3] == body) return i;
    return -1;
}

public FinalCube_OnTraceAttack(ent, attacker, Float:damage, Float:direction[3], trace, damageBits)
{
    if (!g_PluginReady || !g_FinalCubeReady || ent != g_FinalCubeHitbox) return HAM_IGNORED;
    if (!g_FinalCubeBusy && FinalCube_IsPlayerInside(attacker) &&
        get_user_weapon(attacker) == CSW_USP && (damageBits & DMG_BULLET) && get_tr2(trace, TR_pHit) == ent)
    {
        new Float:hit[3]; get_tr2(trace, TR_vecEndPos, hit);
        new control = FinalCube_ControlAt(hit);
        if (control < 0) return HAM_SUPERCEDE;
        g_FinalCubeMove = RFC_CONTROL_MOVE[control];
        g_FinalCubeActiveControl = control;
        g_FinalCubeBusy = true;
        g_FinalCubeTurnAt = get_gametime();
        set_pev(g_FinalCubeControl[control], pev_skin, 7);
        set_pev(g_FinalCubeControl[control], pev_renderamt, 160.0);
        emit_sound(g_FinalCubeControl[control], CHAN_ITEM, "buttons/button3.wav", 0.6, ATTN_NORM, 0, 100);
    }
    return HAM_SUPERCEDE;
}

public FinalCube_OnTakeDamage(ent, inflictor, attacker, Float:damage, damageBits)
{
    return (g_FinalCubeReady && ent == g_FinalCubeHitbox) ? HAM_SUPERCEDE : HAM_IGNORED;
}

stock FinalCube_Update(Float:now)
{
    if (!g_FinalCubeReady) return;
    if (g_FinalCubeBusy)
    {
        new Float:t = floatmin(1.0, (now - g_FinalCubeTurnAt) / RFC_TURN_SECONDS);
        new axis = RFC_MOVE[g_FinalCubeMove][0], layer = RFC_MOVE[g_FinalCubeMove][1];
        new Float:angle[3], Float:turnAngle = 90.0 * float(RFC_MOVE[g_FinalCubeMove][2]) * t;
        new Float:c = floatcos(turnAngle, degrees), Float:s = floatsin(turnAngle, degrees);
        new lastFace=-1;
        for (new i = 0; i < 54; i++) if (RFC_SLOT_POS[i][axis] == layer)
        {
            new Float:p[3];
            for (new a = 0; a < 3; a++) p[a] = g_FinalCubeStickerHome[i][a];
            for (new a = 0; a < 3; a++) p[a] -= RFC_CENTRE[a];
            FinalCube_RotateVector(p, axis, c, s);
            for (new a = 0; a < 3; a++) p[a] += RFC_CENTRE[a];
            engfunc(EngFunc_SetOrigin, g_FinalCubeSticker[i], p);
            if (lastFace != i / 9) { FinalCube_PartAngles(i / 9, axis, c, s, angle); lastFace=i / 9; }
            set_pev(g_FinalCubeSticker[i], pev_angles, angle);
        }
        if (t >= 1.0)
        {
            FinalCube_ApplyMove(g_FinalCubeMove);
            FinalCube_Paint();
            set_pev(g_FinalCubeControl[g_FinalCubeActiveControl], pev_skin, 6);
            set_pev(g_FinalCubeControl[g_FinalCubeActiveControl], pev_renderamt, 100.0);
            g_FinalCubeBusy = false;
            if (!g_FinalCubeSolved && FinalCube_IsSolved())
            {
                g_FinalCubeSolved = true;
                Door_Set(g_FinalCubeDoor, true);
                emit_sound(g_FinalCubeGate, CHAN_ITEM, "buttons/bell1.wav", 0.9, ATTN_NORM, 0, 100);
            }
        }
    }

}

stock FinalCube_UseButton(id)
{
    if (!FinalCube_IsPlayerInside(id) || !(pev(id, pev_button) & IN_USE) || !Player_IsAimingAt(id, g_FinalCubeButton)) return HAM_SUPERCEDE;
    new Float:now = get_gametime();
    if (now < g_FinalCubeUseAt[id]) return HAM_SUPERCEDE;
    g_FinalCubeUseAt[id] = now + 0.5;
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
    emit_sound(g_FinalCubeButton, CHAN_ITEM, "buttons/button1.wav", 0.7, ATTN_NORM, 0, 100);
    return HAM_SUPERCEDE;
}
