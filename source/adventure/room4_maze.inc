// Room 4: one route reveal per round and jump-activated wind shaft.
// No per-star entities or pathfinding at runtime; no player gravity/team changes.
new bool:g_Room4Ready, bool:g_Room4RouteRevealed;
new g_Room4Button, g_Room4Beam;
new g_Room4WindStage[33], g_Room4WindUserId[33];
const Float:ROOM4_WIND_SPEED = 150.0;

stock bool:Room4_Init()
{
    if (!Map_FindEntity("adventure_room4_v42")) return true;
    g_Room4Button = Map_FindEntity("room4_route_button");
    if (!pev_valid(g_Room4Button))
    {
        set_fail_state("Room 4 route button missing (layout v42).");
        return false;
    }
    g_Room4Beam = Map_FindEntity("room4_route_beam");
    if (!pev_valid(g_Room4Beam))
    {
        set_fail_state("Room 4 route beam missing (layout v42).");
        return false;
    }
    g_Room4Ready = true;
    Room4_Reset();
    return true;
}

stock Room4_StopWind()
{
    for (new id = 1; id <= 32; id++)
    {
        g_Room4WindStage[id] = 0;
        g_Room4WindUserId[id] = 0;
    }
}

stock Room4_Reset()
{
    Room4_StopWind();
    if (!g_Room4Ready) return;
    g_Room4RouteRevealed = false;
    Entity_SetVisible(g_Room4Beam, false);
    set_pev(g_Room4Beam, pev_renderamt, 0.0);
    set_pev(g_Room4Button, pev_frame, 0.0);
}

public Room4_OnRoundStart()
{
    if (g_PluginReady) Room4_Reset();
}

stock Room4_UseButton(id)
{
    if (!g_Room4Ready || !is_user_alive(id) || g_Room4RouteRevealed) return HAM_SUPERCEDE;
    new Float:o[3];
    pev(id, pev_origin, o);
    // CLIP maze walls do not block point traces. Require the actual button cell
    // as well as a direct short use trace, preventing activation through them.
    for (new a = 0; a < 3; a++)
        if (o[a] <= ROOM4_BUTTON_MIN[a] || o[a] >= ROOM4_BUTTON_MAX[a]) return HAM_SUPERCEDE;
    if (!Player_IsAimingAt(id, g_Room4Button)) return HAM_SUPERCEDE;
    g_Room4RouteRevealed = true;
    set_pev(g_Room4Beam, pev_renderamt, 235.0);
    Entity_SetVisible(g_Room4Beam, true);
    set_pev(g_Room4Button, pev_frame, 1.0);
    emit_sound(g_Room4Button, CHAN_ITEM, "buttons/button3.wav", 0.8, ATTN_NORM, 0, 100);
    return HAM_SUPERCEDE;
}

public Room4_OnPreThink(id)
{
    if (!g_PluginReady || !g_Room4Ready) return FMRES_IGNORED;
    if (!is_user_alive(id) || pev(id, pev_movetype) != MOVETYPE_WALK)
    {
        g_Room4WindStage[id] = 0;
        return FMRES_IGNORED;
    }
    new uid = get_user_userid(id);
    if (g_Room4WindUserId[id] != uid)
    {
        g_Room4WindUserId[id] = uid;
        g_Room4WindStage[id] = 0;
    }
    if (!g_Room4WindStage[id] && (pev(id, pev_button) & IN_JUMP)
        && !(pev(id, pev_oldbuttons) & IN_JUMP) && (pev(id, pev_flags) & FL_ONGROUND))
    {
        new Float:o[3], Float:mins[3];
        pev(id, pev_origin, o);
        pev(id, pev_mins, mins);
        if (o[0] > ROOM4_TUBE_MIN[0] && o[0] < ROOM4_TUBE_MAX[0]
            && o[1] > ROOM4_TUBE_MIN[1] && o[1] < ROOM4_TUBE_MAX[1]
            && floatabs(o[2] + mins[2] - ROOM4_TUBE_MIN[2]) < 4.0)
            g_Room4WindStage[id] = 1;
    }
    if (g_Room4WindStage[id]) Room4_ApplyWind(id);
    return FMRES_IGNORED;
}

public Room4_OnPostThink(id)
{
    if (g_PluginReady && g_Room4Ready && g_Room4WindStage[id])
    {
        if (!is_user_alive(id) || pev(id, pev_movetype) != MOVETYPE_WALK
            || g_Room4WindUserId[id] != get_user_userid(id)) g_Room4WindStage[id] = 0;
        else Room4_ApplyWind(id);
    }
    return HAM_IGNORED;
}

stock Room4_ApplyWind(id)
{
    new Float:o[3], Float:mins[3], Float:v[3];
    pev(id, pev_origin, o);
    pev(id, pev_mins, mins);
    new Float:feet = o[2] + mins[2];
    // Cancel on leaving the shaft at its base, death, noclip or another teleport.
    if (o[0] <= ROOM4_TUBE_MIN[0] || o[0] >= ROOM4_TUBE_MAX[0]
        || o[1] <= ROOM4_TUBE_MIN[1] || o[1] > ROOM4_CORRIDOR_START_Y + 64.0
        || feet < ROOM4_TUBE_MIN[2] - 4.0 || feet > ROOM4_TUBE_MAX[2] + 128.0
        || (o[1] >= ROOM4_TUBE_MAX[1] && feet < ROOM4_TUBE_MAX[2]))
    {
        g_Room4WindStage[id] = 0;
        return;
    }
    pev(id, pev_velocity, v);
    if (feet >= ROOM4_TUBE_MAX[2] + 2.0) g_Room4WindStage[id] = 2;
    if (g_Room4WindStage[id] == 2)
    {
        // Clear the corridor lip with the entire standing hull before release.
        if (o[1] >= ROOM4_CORRIDOR_START_Y + 18.0 && feet >= ROOM4_TUBE_MAX[2])
        {
            g_Room4WindStage[id] = 0;
            v[2] = 0.0;
            set_pev(id, pev_velocity, v);
            return;
        }
        v[0] = floatmax(-100.0, floatmin(100.0, ((ROOM4_TUBE_MIN[0] + ROOM4_TUBE_MAX[0]) * 0.5 - o[0]) * 4.0));
        v[1] = 130.0;
        v[2] = feet >= ROOM4_TUBE_MAX[2] + 4.0 ? 0.0 : ROOM4_WIND_SPEED;
    }
    else v[2] = ROOM4_WIND_SPEED;
    set_pev(id, pev_velocity, v);
}
