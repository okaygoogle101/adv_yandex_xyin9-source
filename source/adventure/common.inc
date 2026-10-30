// Shared entity/player helpers. Never changes teammate collision settings.
// Internal include; compile adventure_trust.sma, not this file.

stock Map_FindEntity(const name[])
{
    return engfunc(EngFunc_FindEntityByString, - 1, "targetname", name);
}

stock bool:Player_IsAimingAt(id, ent)
{
    new Float:eye[3], Float:offset[3], Float:angle[3], Float:direction[3], Float:end[3];
    pev(id, pev_origin, eye);
    pev(id, pev_view_ofs, offset);
    pev(id, pev_v_angle, angle);
    engfunc(EngFunc_MakeVectors, angle);
    global_get(glb_v_forward, direction);
    for (new a = 0; a < 3; a++)
    {
        eye[a] += offset[a];
        end[a] = eye[a] + direction[a] * 96.0;
    }
    new tr = create_tr2();
    // Do not ignore glass, walls or other players. The first hit must be this button.
    engfunc(EngFunc_TraceLine, eye, end, 0, id, tr);
    new bool:ok = get_tr2(tr, TR_pHit) == ent && !get_tr2(tr, TR_StartSolid);
    free_tr2(tr);
    return ok;
}

stock bool:Bounds_Overlap(const Float:lo[3], const Float:hi[3], const Float:bLo[3], const Float:bHi[3])
{
    for (new a = 0; a < 3; a++) if (hi[a] <= bLo[a] || lo[a] >= bHi[a]) return false;
    return true;
}

stock bool:Player_TryPlace(id, const Float:destination[3], trace = 0)
{
    new tr = trace ? trace : create_tr2();
    engfunc(EngFunc_TraceHull, destination, destination, 0, (pev(id, pev_flags) & FL_DUCKING) ? HULL_HEAD : HULL_HUMAN, id, tr);
    new bool:free = !get_tr2(tr, TR_StartSolid) && !get_tr2(tr, TR_AllSolid);
    if (!trace) free_tr2(tr);
    if (!free) return false;
    new Float:zero[3];
    Player_SetOrigin(id, destination);
    set_pev(id, pev_velocity, zero);
    set_pev(id, pev_basevelocity, zero);
    return true;
}

stock Entity_SetVisible(ent, bool:show)
{
    new effects = pev(ent, pev_effects);
    new wanted = show ? (effects & ~EF_NODRAW) : (effects | EF_NODRAW);
    if (effects != wanted) set_pev(ent, pev_effects, wanted);
}

stock Player_Kill(id)
{
    if (is_user_alive(id)) ExecuteHamB(Ham_Killed, id, 0, 0);
}

// Lazy snapshots live only inside a simulation pass, never across player commands.
new bool:g_PlayerSnapshotActive;
new g_PlayerSnapshotFlags[33], g_PlayerSnapshotUid[33];
new Float:g_PlayerSnapshotOrigin[33][3], Float:g_PlayerSnapshotLo[33][3], Float:g_PlayerSnapshotHi[33][3];
stock Player_ReadOrigin(id, Float:o[3])
{
    if (!g_PlayerSnapshotActive) { pev(id, pev_origin, o); return; }
    if (!(g_PlayerSnapshotFlags[id] & 1))
    {
        pev(id, pev_origin, g_PlayerSnapshotOrigin[id]);
        g_PlayerSnapshotFlags[id] |= 1;
    }
    for (new a = 0; a < 3; a++) o[a] = g_PlayerSnapshotOrigin[id][a];
}
stock Player_ReadMin(id, Float:o[3])
{
    if (!g_PlayerSnapshotActive) { pev(id, pev_absmin, o); return; }
    if (!(g_PlayerSnapshotFlags[id] & 2))
    {
        pev(id, pev_absmin, g_PlayerSnapshotLo[id]);
        g_PlayerSnapshotFlags[id] |= 2;
    }
    for (new a = 0; a < 3; a++) o[a] = g_PlayerSnapshotLo[id][a];
}
stock Player_ReadMax(id, Float:o[3])
{
    if (!g_PlayerSnapshotActive) { pev(id, pev_absmax, o); return; }
    if (!(g_PlayerSnapshotFlags[id] & 4))
    {
        pev(id, pev_absmax, g_PlayerSnapshotHi[id]);
        g_PlayerSnapshotFlags[id] |= 4;
    }
    for (new a = 0; a < 3; a++) o[a] = g_PlayerSnapshotHi[id][a];
}
stock Player_ReadUserId(id)
{
    if (!g_PlayerSnapshotActive) return get_user_userid(id);
    if (!(g_PlayerSnapshotFlags[id] & 8))
    {
        g_PlayerSnapshotUid[id] = get_user_userid(id);
        g_PlayerSnapshotFlags[id] |= 8;
    }
    return g_PlayerSnapshotUid[id];
}
stock Player_SetOrigin(id, const Float:o[3])
{
    engfunc(EngFunc_SetOrigin, id, o);
    g_PlayerSnapshotFlags[id] = 0;
}
stock Player_BeginSnapshot()
{
    for (new id = 1; id <= g_MaxPlayers; id++) g_PlayerSnapshotFlags[id] = 0;
    g_PlayerSnapshotActive = true;
}
