// Room 3 / safe color. Init, Prepare, Update (true when complete), Reset, CheckHazards.
// Internal include; compile adventure_trust.sma, not this file.

new g_ColorRound, g_ColorSelected, g_ColorScreen[4], g_ColorFrame[4], g_ColorDangerLayer[16];
new g_ColorScreenModel[16][16];
new Float:g_ColorNextBeepAt, Float:g_ColorDangerOrigin[16][3];
new Float:g_ColorScreenOrigin[4][3], Float:g_ColorFrameOrigin[4][3];

stock bool:Color_Init()
{
    new name[64];
    for (new i = 0; i < 16; i++)
    {
        formatex(name, charsmax(name), "boss_screen_template_%d", i);
        new sample = Map_FindEntity(name);
        formatex(name, charsmax(name), "boss_danger_color_%d", i);
        g_ColorDangerLayer[i] = Map_FindEntity(name);
        if (!pev_valid(sample) || !pev_valid(g_ColorDangerLayer[i])) return Room3_FailMissingEntities();
        pev(sample, pev_model, g_ColorScreenModel[i], charsmax(g_ColorScreenModel[]));
        pev(g_ColorDangerLayer[i], pev_origin, g_ColorDangerOrigin[i]);
        if (g_ColorScreenModel[i][0] != '*') return Room3_FailMissingEntities();
        Entity_SetVisible(sample, false);
        Entity_SetVisible(g_ColorDangerLayer[i], false);
    }
    for (new i = 0; i < 4; i++)
    {
        formatex(name, charsmax(name), "boss_screen_frame_%d", i);
        g_ColorFrame[i] = Map_FindEntity(name);
        formatex(name, charsmax(name), "boss_screen_anchor_%d", i);
        new anchor = Map_FindEntity(name);
        if (!pev_valid(g_ColorFrame[i]) || !pev_valid(anchor)) return Room3_FailMissingEntities();
        pev(anchor, pev_origin, g_ColorScreenOrigin[i]);
        pev(g_ColorFrame[i], pev_origin, g_ColorFrameOrigin[i]);
        new Float:angles[3];
        pev(anchor, pev_angles, angles);
        g_ColorScreen[i] = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "info_target"));
        if (!pev_valid(g_ColorScreen[i])) return Room3_FailMissingEntities();
        engfunc(EngFunc_SetModel, g_ColorScreen[i], g_ColorScreenModel[0]);
        set_pev(g_ColorScreen[i], pev_angles, angles);
        set_pev(g_ColorScreen[i], pev_solid, SOLID_NOT);
        set_pev(g_ColorScreen[i], pev_movetype, MOVETYPE_PUSH);
        set_pev(g_ColorScreen[i], pev_scale, 1.0);
        set_pev(g_ColorScreen[i], pev_rendermode, kRenderTransTexture);
        set_pev(g_ColorScreen[i], pev_renderamt, 255.0);
        new Float:white[3] = {255.0, 255.0, 255.0};
        set_pev(g_ColorScreen[i], pev_rendercolor, white);
    }
    return true;
}

stock Color_Prepare(Float:now)
{
    g_ColorRound = 0;
    g_Room3Phase = ROOM3_COLOR_DEPLOY;
    g_Room3PhaseEndsAt = now + COLOR_DEPLOY_SECONDS;
    Room3_PlaySound("buttons/button1.wav");
    for (new i = 0; i < 4; i++) Entity_SetVisible(g_ColorFrame[i], true);
}

stock bool:Color_Update(Float:now)
{
    switch (g_Room3Phase)
    {
        case ROOM3_COLOR_DEPLOY:
        {
            Color_MoveScreens(floatmin(1.0, floatmax(0.0, 1.0 - (g_Room3PhaseEndsAt - now) / COLOR_DEPLOY_SECONDS)));
            if (now >= g_Room3PhaseEndsAt)
            {
                g_Room3Phase = ROOM3_COLOR_WAIT;
                g_Room3PhaseEndsAt = now + COLOR_BLANK_SECONDS;
            }
        }
        case ROOM3_COLOR_WAIT: if (now >= g_Room3PhaseEndsAt) Color_StartRound(now);
        case ROOM3_COLOR_FIND:
        {
            if (now >= g_Room3PhaseEndsAt)
            {
                g_Room3Phase = ROOM3_COLOR_HOLD;
                g_Room3PhaseEndsAt = now + COLOR_DANGER_SECONDS;
                Color_SetDanger(true);
                Room3_PlaySound("buttons/bell1.wav");
            }
            else if (now >= g_ColorNextBeepAt)
            {
                Room3_PlaySound("buttons/blip1.wav");
                g_ColorNextBeepAt += COLOR_BEEP_SECONDS;
            }
        }
        case ROOM3_COLOR_HOLD: if (now >= g_Room3PhaseEndsAt)
        {
            Color_SetDanger(false);
            if (g_ColorRound < COLOR_ROUNDS) Color_StartRound(now);
            else
            {
                for (new i = 0; i < 4; i++) Entity_SetVisible(g_ColorScreen[i], false);
                g_Room3Phase = ROOM3_COLOR_RETRACT;
                g_Room3PhaseEndsAt = now + COLOR_RETRACT_SECONDS;
            }
        }
        case ROOM3_COLOR_RETRACT:
        {
            Color_MoveScreens(floatmin(1.0, floatmax(0.0, (g_Room3PhaseEndsAt - now) / COLOR_RETRACT_SECONDS)));
            if (now >= g_Room3PhaseEndsAt)
            {
                Color_HideScreens();
                return true;
            }
        }
    }
    return false;
}

stock Color_Reset()
{
    Color_HideScreens();
    Color_SetDanger(false);
}

stock Color_MoveScreens(Float:fraction)
{
    new Float:o[3], Float:d =  - 8.0 + 24.0 * fraction;
    for (new i = 0; i < 4; i++)
    {
        for (new a = 0; a < 3; a++) o[a] = g_ColorFrameOrigin[i][a] + COLOR_SCREEN_DIRECTION[i][a] * d;
        engfunc(EngFunc_SetOrigin, g_ColorFrame[i], o);
        for (new a = 0; a < 3; a++) o[a] = g_ColorScreenOrigin[i][a] + COLOR_SCREEN_DIRECTION[i][a] * d;
        engfunc(EngFunc_SetOrigin, g_ColorScreen[i], o);
    }
}

stock Color_HideScreens()
{
    for (new i = 0; i < 4; i++)
    {
        Entity_SetVisible(g_ColorScreen[i], false);
        Entity_SetVisible(g_ColorFrame[i], false);
    }
    Color_MoveScreens(0.0);
}

stock Color_SetDanger(bool:on)
{
    new Float:o[3];
    for (new i = 0; i < 16; i++)
    {
        new bool:active = on && i != g_ColorSelected;
        for (new a = 0; a < 3; a++) o[a] = g_ColorDangerOrigin[i][a];
        if (active) o[2] += 128.0;
        engfunc(EngFunc_SetOrigin, g_ColorDangerLayer[i], o);
        Entity_SetVisible(g_ColorDangerLayer[i], active);
    }
}

stock Color_StartRound(Float:now)
{
    // Independent uniform draw: repeated colors in adjacent rounds are allowed.
    Color_SetDanger(false);
    g_ColorSelected = random_num(0, 15);
    g_ColorRound++;
    g_Room3Phase = ROOM3_COLOR_FIND;
    g_Room3PhaseEndsAt = now + COLOR_SEARCH_SECONDS;
    for (new i = 0; i < 4; i++)
    {
        engfunc(EngFunc_SetModel, g_ColorScreen[i], g_ColorScreenModel[g_ColorSelected]);
        Entity_SetVisible(g_ColorScreen[i], true);
    }
    Room3_PlaySound("buttons/button3.wav");
    g_ColorNextBeepAt = now + COLOR_BEEP_SECONDS;
}

stock bool:Color_IsSafeFootprint(Float:x, Float:y)
{
    // Full footprint must avoid the red tiles when it touches their light layer.
    for (new sx =  - 1; sx <= 1; sx += 2) for (new sy =  - 1; sy <= 1; sy += 2)
    {
        new col = floatround((x + float(sx) * 15.99 + 640.0) / 128.0, floatround_floor);
        new row = floatround((y + float(sy) * 15.99 + 2496.0) / 128.0, floatround_floor);
        if (col < 0 || col >= 10 || row < 0 || row >= 10) return false;
        if (ROOM3_TILE_COLORS[row * 10 + col] != g_ColorSelected) return false;
    }
    return true;
}

stock Color_CheckHazards(id)
{
    new Float:o[3];
    Player_ReadOrigin(id, o);
    new Float:feet[3], Float:head[3];
    Player_ReadMin(id, feet);
    Player_ReadMax(id, head);
    // Touching the visible slab is lethal. Above it, a late jump can still
    // reach the safe tile. Engine bounds are expanded by one unit.
    if (feet[2] + 1.0 <=  - 1330.0 && head[2] - 1.0 >=  - 1331.0 && !Color_IsSafeFootprint(o[0], o[1])) Player_Kill(id);
}
