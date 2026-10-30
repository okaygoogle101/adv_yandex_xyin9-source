// Game 8: three reusable meteors, no homing. Fixed small physics steps.
new bool:g_MeteorReady, bool:g_MeteorSingle;
new g_MeteorEdge;
new Float:g_MeteorEdgeOrigin[3];
new g_MeteorEntity[3][2], g_MeteorStage[3]; // 0 hidden, 1 warning, 2 falling, 3 moving
new Float:g_MeteorPos[3][3], Float:g_MeteorVelocity[3][3];
new Float:g_MeteorLandAt[3], Float:g_MeteorFlashUntil[3], Float:g_MeteorRenderAmount[3];
new Float:g_MeteorStart, Float:g_MeteorLastUpdate, Float:g_MeteorBeepAt, Float:g_MeteorSoundAt;
new bool:g_MeteorPlayerInside[33];
new Float:g_MeteorPlayerMin[33][3], Float:g_MeteorPlayerMax[33][3];

stock bool:Meteor_Init()
{
    new sample = Map_FindEntity("room3_meteor_template"), ring = Map_FindEntity("room3_meteor_ring");
    if (!pev_valid(sample) || !pev_valid(ring))
    {
        set_fail_state("Meteor game requires adventure_start_v34.map.");
        return false;
    }
    new model[2][16];
    pev(sample, pev_model, model[0], 15);
    pev(ring, pev_model, model[1], 15);
    if (model[0][0] != '*' || model[1][0] != '*') return Room3_FailMissingEntities();
    Entity_SetVisible(sample, false);
    Entity_SetVisible(ring, false);
    for (new i = 0; i < 3; i++) for (new part = 0; part < 2; part++)
    {
        new ent = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "info_target"));
        if (!pev_valid(ent)) return Room3_FailMissingEntities();
        g_MeteorEntity[i][part] = ent;
        engfunc(EngFunc_SetModel, ent, model[part]);
        set_pev(ent, pev_movetype, MOVETYPE_PUSH);
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_rendermode, part ? kRenderTransColor : kRenderTransTexture);
        set_pev(ent, pev_renderamt, 200.0);
        if (!part) g_MeteorRenderAmount[i] = 200.0;
        new Float:color[3]; color[0] = 255.0; color[1] = 150.0; color[2] = 40.0;
        if (part) set_pev(ent, pev_rendercolor, color);
        Entity_SetVisible(ent, false);
    }
    g_MeteorEdge = Map_FindEntity("room3_meteor_edge");
    if (!pev_valid(g_MeteorEdge)) return Room3_FailMissingEntities();
    pev(g_MeteorEdge, pev_origin, g_MeteorEdgeOrigin);
    g_MeteorReady = true;
    Meteor_Reset();
    return true;
}

stock Meteor_Reset()
{
    if (!g_MeteorReady) return;
    for (new i = 0; i < 3; i++)
    {
        g_MeteorStage[i] = 0;
        g_MeteorLandAt[i] = 0.0;
        g_MeteorFlashUntil[i] = 0.0;
        for (new part = 0; part < 2; part++) Entity_SetVisible(g_MeteorEntity[i][part], false);
    }
    Entity_SetVisible(g_MeteorEdge, false);
    engfunc(EngFunc_SetOrigin, g_MeteorEdge, g_MeteorEdgeOrigin);
    g_MeteorSingle = false;
    g_MeteorStart = 0.0;
    g_MeteorBeepAt = 0.0;
    g_MeteorSoundAt = 0.0;
}

stock Meteor_Warn(i, Float:now, Float:lead)
{
    g_MeteorPos[i][0] = i ? random_float(-448.0, 448.0) : 0.0;
    g_MeteorPos[i][1] = i ? random_float(-2304.0, -1408.0) : -1856.0;
    g_MeteorPos[i][2] = -1072.0;
    g_MeteorLandAt[i] = now + lead;
    g_MeteorStage[i] = 1;
    engfunc(EngFunc_SetOrigin, g_MeteorEntity[i][0], g_MeteorPos[i]);
    new Float:o[3];
    o[0] = g_MeteorPos[i][0]; o[1] = g_MeteorPos[i][1]; o[2] = -1333.0;
    engfunc(EngFunc_SetOrigin, g_MeteorEntity[i][1], o);
    for (new part = 0; part < 2; part++) Entity_SetVisible(g_MeteorEntity[i][part], true);
    Room3_PlaySound("buttons/button1.wav", 70);
}

stock Meteor_Prepare(Float:now, bool:single = false)
{
    Meteor_Reset();
    g_MeteorSingle = single;
    new Float:edge[3];
    for (new a = 0; a < 3; a++) edge[a] = g_MeteorEdgeOrigin[a];
    edge[2] += 128.0;
    engfunc(EngFunc_SetOrigin, g_MeteorEdge, edge);
    set_pev(g_MeteorEdge, pev_renderamt, 45.0); // Preview during the existing intro grace period.
    Entity_SetVisible(g_MeteorEdge, true);
    g_Room3Phase = ROOM3_METEOR_INTRO;
    g_Room3PhaseEndsAt = now + 3.5;
    g_MeteorLastUpdate = now;
    g_MeteorBeepAt = now + 1.0;
    Meteor_Warn(0, now, 3.5);
    Room3_PlaySound("buttons/blip1.wav", 80);
}

stock Meteor_Shake()
{
    new message = get_user_msgid("ScreenShake");
    if (!message) return;
    for (new id = 1; id <= g_MaxPlayers; id++) if (Room3_IsInsideArena(id) && !is_user_bot(id))
    {
        message_begin(MSG_ONE, message, _, id);
        write_short(3 * 4096);
        write_short(2048); // Half a second, no movement or damage.
        write_short(12 * 256);
        message_end();
    }
}

stock Meteor_SetDirection(i, Float:angle)
{
    g_MeteorVelocity[i][0] = floatcos(angle, degrees) * METEOR_SPEED;
    g_MeteorVelocity[i][1] = floatsin(angle, degrees) * METEOR_SPEED;
    g_MeteorVelocity[i][2] = 0.0;
}

stock Meteor_BounceSound(i, Float:now)
{
    g_MeteorFlashUntil[i] = now + 0.15;
    if (now < g_MeteorSoundAt) return;
    emit_sound(g_MeteorEntity[i][0], CHAN_ITEM, "buttons/button3.wav", 0.5, ATTN_NORM, 0, 85);
    g_MeteorSoundAt = now + 0.12;
}

stock Meteor_CachePlayer(id)
{
    g_MeteorPlayerInside[id] = Room3_IsInsideArena(id);
    if (!g_MeteorPlayerInside[id]) return;
    Player_ReadMin(id, g_MeteorPlayerMin[id]);
    Player_ReadMax(id, g_MeteorPlayerMax[id]);
    for (new a = 0; a < 3; a++) { g_MeteorPlayerMin[id][a] += 1.0; g_MeteorPlayerMax[id][a] -= 1.0; }
    Meteor_CheckEdge(id);
}

stock Meteor_CheckEdge(id)
{
    if (g_Room3Phase != ROOM3_METEOR_ACTIVE || !g_MeteorPlayerInside[id]) return;
    // Outer 128-unit row/column. Only contact with the visible 5-6 unit slab is lethal.
    if (g_MeteorPlayerMin[id][2] > -1330.0 || g_MeteorPlayerMax[id][2] < -1331.0) return;
    if (g_MeteorPlayerMin[id][0] < -512.0 || g_MeteorPlayerMax[id][0] > 512.0
        || g_MeteorPlayerMin[id][1] < -2368.0 || g_MeteorPlayerMax[id][1] > -1344.0)
    {
        g_MeteorPlayerInside[id] = false;
        Player_Kill(id);
    }
}

stock Meteor_HitPoint(id, Float:x, Float:y)
{
    if (!g_MeteorPlayerInside[id]) return;
    new Float:p[3]; p[0] = x; p[1] = y; p[2] = -1224.0;
    new Float:distance;
    for (new a = 0; a < 3; a++)
    {
        new Float:d = floatmax(g_MeteorPlayerMin[id][a] - p[a], floatmax(0.0, p[a] - g_MeteorPlayerMax[id][a]));
        distance += d * d;
    }
    if (distance <= METEOR_RADIUS * METEOR_RADIUS)
    {
        g_MeteorPlayerInside[id] = false;
        Player_Kill(id);
    }
}

stock Meteor_HitSweep(i, Float:x, Float:y)
{
    new Float:dx = g_MeteorPos[i][0] - x, Float:dy = g_MeteorPos[i][1] - y;
    new steps = max(1, floatround(floatsqroot(dx * dx + dy * dy) / 4.0, floatround_ceil));
    // Conservative broad phase; exact original sampled sphere test remains unchanged.
    new candidates[32], count;
    new Float:x0 = floatmin(x, g_MeteorPos[i][0]) - METEOR_RADIUS - 0.01;
    new Float:x1 = floatmax(x, g_MeteorPos[i][0]) + METEOR_RADIUS + 0.01;
    new Float:y0 = floatmin(y, g_MeteorPos[i][1]) - METEOR_RADIUS - 0.01;
    new Float:y1 = floatmax(y, g_MeteorPos[i][1]) + METEOR_RADIUS + 0.01;
    for (new id = 1; id <= g_MaxPlayers; id++)
        if (g_MeteorPlayerInside[id] && g_MeteorPlayerMax[id][0] >= x0
            && g_MeteorPlayerMin[id][0] <= x1 && g_MeteorPlayerMax[id][1] >= y0
            && g_MeteorPlayerMin[id][1] <= y1) candidates[count++] = id;
    if (!count) return;
    for (new step = 0; step <= steps; step++)
    {
        new Float:px = x + dx * float(step) / float(steps);
        new Float:py = y + dy * float(step) / float(steps);
        for (new c = 0; c < count; c++) Meteor_HitPoint(candidates[c], px, py);
    }
}

stock Meteor_CheckHazards(id)
{
    Meteor_CachePlayer(id);
    for (new i = 0; i < 3; i++) if (g_MeteorStage[i] == 3) Meteor_HitPoint(id, g_MeteorPos[i][0], g_MeteorPos[i][1]);
}

stock Meteor_Walls(i, Float:now)
{
    new bool:left = g_MeteorPos[i][0] < -528.0, bool:right = g_MeteorPos[i][0] > 528.0;
    new bool:bottom = g_MeteorPos[i][1] < -2384.0, bool:top = g_MeteorPos[i][1] > -1328.0;
    if (!left && !right && !bottom && !top) return;
    g_MeteorPos[i][0] = floatmax(-528.0, floatmin(528.0, g_MeteorPos[i][0]));
    g_MeteorPos[i][1] = floatmax(-2384.0, floatmin(-1328.0, g_MeteorPos[i][1]));
    new Float:angle;
    if ((left || right) && (bottom || top))
    {
        angle = random_float(25.0, 65.0);
        if (right) angle = 180.0 - angle;
        if (top) angle = -angle;
    }
    else if (left) angle = random_float(-60.0, 60.0);
    else if (right) angle = random_float(120.0, 240.0);
    else if (bottom) angle = random_float(30.0, 150.0);
    else angle = random_float(210.0, 330.0);
    Meteor_SetDirection(i, angle);
    Meteor_BounceSound(i, now);
}

stock Meteor_Pairs(Float:now)
{
    for (new i = 0; i < 3; i++) if (g_MeteorStage[i] == 3) for (new j = i + 1; j < 3; j++) if (g_MeteorStage[j] == 3)
    {
        new Float:dx = g_MeteorPos[j][0] - g_MeteorPos[i][0], Float:dy = g_MeteorPos[j][1] - g_MeteorPos[i][1];
        new Float:distance = floatsqroot(dx * dx + dy * dy);
        if (distance >= 224.0) continue;
        if (distance < 0.001) { dx = 1.0; dy = 0.0; distance = 0.001; }
        else { dx /= distance; dy /= distance; }
        new Float:push = (224.1 - distance) * 0.5;
        new Float:ix = g_MeteorPos[i][0], Float:iy = g_MeteorPos[i][1];
        new Float:jx = g_MeteorPos[j][0], Float:jy = g_MeteorPos[j][1];
        g_MeteorPos[i][0] -= dx * push; g_MeteorPos[i][1] -= dy * push;
        g_MeteorPos[j][0] += dx * push; g_MeteorPos[j][1] += dy * push;
        // Equal-mass elastic collision: exchange normal velocity only when approaching.
        new Float:approach = (g_MeteorVelocity[i][0] - g_MeteorVelocity[j][0]) * dx + (g_MeteorVelocity[i][1] - g_MeteorVelocity[j][1]) * dy;
        if (approach > 0.0)
        {
            g_MeteorVelocity[i][0] -= approach * dx; g_MeteorVelocity[i][1] -= approach * dy;
            g_MeteorVelocity[j][0] += approach * dx; g_MeteorVelocity[j][1] += approach * dy;
            // Separate with fixed speed; a tiny/zero outgoing vector points away from contact.
            for (new ball = i; ball <= j; ball += j - i)
            {
                new Float:length = floatsqroot(g_MeteorVelocity[ball][0] * g_MeteorVelocity[ball][0] + g_MeteorVelocity[ball][1] * g_MeteorVelocity[ball][1]);
                if (length < 1.0) { g_MeteorVelocity[ball][0] = (ball == i ? -dx : dx) * METEOR_SPEED; g_MeteorVelocity[ball][1] = (ball == i ? -dy : dy) * METEOR_SPEED; }
                else { g_MeteorVelocity[ball][0] *= METEOR_SPEED / length; g_MeteorVelocity[ball][1] *= METEOR_SPEED / length; }
            }
            Meteor_BounceSound(i, now); g_MeteorFlashUntil[j] = now + 0.15;
        }
        Meteor_Walls(i, now); Meteor_Walls(j, now);
        Meteor_HitSweep(i, ix, iy); Meteor_HitSweep(j, jx, jy);
    }
}

stock bool:Meteor_Update(Float:now)
{
    for (new id = 1; id <= g_MaxPlayers; id++) Meteor_CachePlayer(id);
    // Slow simulation during a severe stall instead of teleporting through players/walls.
    new Float:remaining = floatmin(0.25, floatmax(0.0, now - g_MeteorLastUpdate));
    g_MeteorLastUpdate = now;
    while (remaining > 0.00001)
    {
        new Float:dt = floatmin(0.02, remaining);
        for (new i = 0; i < 3; i++) if (g_MeteorStage[i] == 3)
        {
            new Float:x = g_MeteorPos[i][0], Float:y = g_MeteorPos[i][1];
            g_MeteorPos[i][0] += g_MeteorVelocity[i][0] * dt;
            g_MeteorPos[i][1] += g_MeteorVelocity[i][1] * dt;
            Meteor_Walls(i, now);
            Meteor_HitSweep(i, x, y);
        }
        Meteor_Pairs(now);
        remaining -= dt;
    }
    if (g_Room3Phase == ROOM3_METEOR_ACTIVE)
    {
        if (!g_MeteorSingle && !g_MeteorStage[1] && now >= g_MeteorStart + 5.0) Meteor_Warn(1, now, 2.0);
        if (!g_MeteorSingle && !g_MeteorStage[2] && now >= g_MeteorStart + 13.0) Meteor_Warn(2, now, 2.0);
    }
    else if (now >= g_MeteorBeepAt && now < g_MeteorLandAt[0] - 0.5)
    {
        Room3_PlaySound("buttons/blip1.wav", now < g_MeteorLandAt[0] - 1.5 ? 100 : 120);
        g_MeteorBeepAt = now + 1.0;
    }
    for (new i = 0; i < 3; i++) if (g_MeteorStage[i])
    {
        if (g_MeteorStage[i] < 3)
        {
            new Float:f = floatmin(1.0, floatmax(0.0, 1.0 - (g_MeteorLandAt[i] - now) / 0.5));
            g_MeteorPos[i][2] = -1072.0 - 152.0 * f * f;
            if (f > 0.0) g_MeteorStage[i] = 2;
            if (now >= g_MeteorLandAt[i])
            {
                g_MeteorStage[i] = 3;
                Meteor_SetDirection(i, random_float(0.0, 360.0));
                Entity_SetVisible(g_MeteorEntity[i][1], false);
                engfunc(EngFunc_SetOrigin, g_MeteorEntity[i][0], g_MeteorPos[i]);
                emit_sound(g_MeteorEntity[i][0], CHAN_ITEM, "debris/bustcrate1.wav", 0.9, ATTN_NORM, 0, 70);
                g_MeteorFlashUntil[i] = now + 0.2;
                Meteor_Shake();
                Meteor_HitSweep(i, g_MeteorPos[i][0], g_MeteorPos[i][1]);
                if (!i)
                {
                    g_Room3Phase = ROOM3_METEOR_ACTIVE;
                    set_pev(g_MeteorEdge, pev_renderamt, 105.0);
                    for (new id = 1; id <= g_MaxPlayers; id++) Meteor_CheckEdge(id);
                    g_MeteorStart = now;
                    g_Room3PhaseEndsAt = now + METEOR_ACTIVE_SECONDS;
                }
            }
        }
        new Float:amount = g_MeteorStage[i] == 3 ? (now < g_MeteorFlashUntil[i] ? 255.0 : 200.0) : (60.0 + 160.0 * floatmin(1.0, floatmax(0.0, 1.0 - (g_MeteorLandAt[i] - now) / (i ? 2.0 : 3.5))) + 20.0 * floatsin(now * 180.0, degrees));
        if (g_MeteorRenderAmount[i] != amount)
        {
            g_MeteorRenderAmount[i] = amount;
            set_pev(g_MeteorEntity[i][0], pev_renderamt, amount);
        }
        engfunc(EngFunc_SetOrigin, g_MeteorEntity[i][0], g_MeteorPos[i]);
    }
    if (g_Room3Phase == ROOM3_METEOR_ACTIVE && now >= g_Room3PhaseEndsAt)
    {
        for (new id = 1; id <= g_MaxPlayers; id++) if (Room3_IsInsideArena(id)) return true;
    }
    return false;
}
