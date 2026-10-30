// Room 1: four independent cabin pairs. Owns doors, choices, gas and trap floors.
// Internal include; compile adventure_trust.sma, not this file.

enum { CABIN_WAITING, CABIN_COUNTDOWN, CABIN_CLOSING, CABIN_CHOOSING, CABIN_RESULT, CABIN_RESETTING };
new g_CabinPairState[CABIN_PAIR_COUNT], g_CabinPlayer[CABIN_COUNT], g_CabinUserId[CABIN_COUNT], g_CabinChoice[CABIN_COUNT];
new g_CabinDoor[CABIN_COUNT], g_CabinFloor[CABIN_COUNT], g_CabinButton[CABIN_COUNT][2];
new g_CabinPlayerCount[CABIN_COUNT], g_CabinOccupant[CABIN_COUNT], g_CabinGasTicks[CABIN_COUNT];
new Float:g_CabinMin[CABIN_COUNT][3], Float:g_CabinMax[CABIN_COUNT][3];
new Float:g_CabinDoorDrop[CABIN_COUNT], Float:g_CabinPairDeadline[CABIN_PAIR_COUNT], Float:g_CabinNextGasAt[CABIN_COUNT];
new  bool:g_CabinWantClosed[CABIN_COUNT], bool:g_CabinFloorOpen[CABIN_COUNT], bool:g_CabinGasVisible[CABIN_COUNT];

new g_CabinMotion[CABIN_COUNT];
new g_CabinGasSound[CABIN_COUNT];

stock bool:Cabins_Init()
{
    new name[64], prefix[48], zone;
    for (new c = 0; c < CABIN_COUNT; c++)
    {
        formatex(prefix, charsmax(prefix), "pair_%d_%s", c / 2 + 1, (c % 2) ? "right" : "left");
        formatex(name, charsmax(name), "%s_door", prefix);
        g_CabinDoor[c] = Map_FindEntity(name);
        formatex(name, charsmax(name), "%s_floor", prefix);
        g_CabinFloor[c] = Map_FindEntity(name);
        formatex(name, charsmax(name), "%s_zone", prefix);
        zone = Map_FindEntity(name);
        formatex(name, charsmax(name), "%s_green", prefix);
        g_CabinButton[c][0] = Map_FindEntity(name);
        formatex(name, charsmax(name), "%s_red", prefix);
        g_CabinButton[c][1] = Map_FindEntity(name);
        if (!pev_valid(g_CabinDoor[c]) || !pev_valid(g_CabinFloor[c]) || !pev_valid(zone) || !pev_valid(g_CabinButton[c][0]) || !pev_valid(g_CabinButton[c][1]))
        {
            set_fail_state("Adventure v15 cabin entities missing; recompile the matching map.");
            return false;
        }
        g_CabinMotion[c] = Door_Add(g_CabinDoor[c], CABIN_DOOR_TRAVEL, CABIN_DOOR_SPEED);
        // Cache only our ambient entities; other map sounds keep native restart behavior.
        formatex(name, charsmax(name), "%s_gas_fx", prefix);
        new sound = -1, classname[32];
        while ((sound = engfunc(EngFunc_FindEntityByString, sound, "targetname", name)) > 0)
        {
            pev(sound, pev_classname, classname, charsmax(classname));
            if (equal(classname, "ambient_generic")) g_CabinGasSound[c] = sound;
        }
        // GoldSrc expands absolute bounds by one unit on each side.
        pev(zone, pev_absmin, g_CabinMin[c]);
        pev(zone, pev_absmax, g_CabinMax[c]);
        for (new a = 0; a < 3; a++)
        {
            g_CabinMin[c][a] += 1.0;
            g_CabinMax[c][a] -= 1.0;
        }
        set_pev(zone, pev_solid, SOLID_NOT);
    }
    return true;
}

stock bool:Cabins_IsInside(id, c)
{
    if (id < 1 || id > g_MaxPlayers || !is_user_alive(id)) return false;
    new Float:o[3];
    pev(id, pev_origin, o);
    for (new a = 0; a < 3; a++) if (o[a] < g_CabinMin[c][a] || o[a] > g_CabinMax[c][a]) return false;
    return true;
}

stock Cabins_ScanPlayers()
{
    for (new c = 0; c < CABIN_COUNT; c++) { g_CabinPlayerCount[c] = 0; g_CabinOccupant[c] = 0; }
    new Float:o[3];
    for (new id = 1; id <= g_MaxPlayers; id++) if (is_user_alive(id))
    {
        pev(id, pev_origin, o);
        for (new c = 0; c < CABIN_COUNT; c++)
        {
            if (o[0] < g_CabinMin[c][0] || o[0] > g_CabinMax[c][0]
                || o[1] < g_CabinMin[c][1] || o[1] > g_CabinMax[c][1]
                || o[2] < g_CabinMin[c][2] || o[2] > g_CabinMax[c][2]) continue;
            g_CabinPlayerCount[c]++;
            g_CabinOccupant[c] = id;
        }
    }
}

stock bool:Cabins_IsParticipant(c)
{
    return Cabins_IsInside(g_CabinPlayer[c], c) && get_user_userid(g_CabinPlayer[c]) == g_CabinUserId[c];
}

stock bool:Cabins_IsPairValid(p)
{
    new c = p * 2;
    return g_CabinPlayerCount[c] == 1 && g_CabinPlayerCount[c + 1] == 1 && Cabins_IsParticipant(c) && Cabins_IsParticipant(c + 1);
}

stock Cabins_SetFloorOpen(c, bool:open)
{
    ExecuteHamB(Ham_Use, g_CabinFloor[c], 0, 0, open ? 0 : 1, 0.0);
    // USE_OFF / USE_ON
    g_CabinFloorOpen[c] = open;
}

stock Cabins_SetGasVisible(c, bool:on)
{
    // OFF must reach the engine even when our cached state already says OFF.
    if (on && g_CabinGasVisible[c]) return;
    g_CabinGasVisible[c] = on;
    new name[64], ent =  - 1;
    formatex(name, charsmax(name), "pair_%d_%s_gas_fx", c / 2 + 1, (c % 2) ? "right" : "left");
    while ((ent = engfunc(EngFunc_FindEntityByString, ent, "targetname", name)) > 0)
    ExecuteHamB(Ham_Use, ent, 0, 0, on ? 1 : 0, 0.0);
}

stock Cabins_CancelPair(p)
{
    // Wait for both cabins to empty before rearming: no automatic retrapping.
    g_CabinPairState[p] = CABIN_RESETTING;
    for (new c = p * 2; c < p * 2 + 2; c++)
    {
        g_CabinWantClosed[c] = false;
        g_CabinGasTicks[c] = 0;
        Cabins_SetGasVisible(c, false);
    }
}

stock Cabins_ResolveChoices(p, Float:now)
{
    // Latch CABIN_RESULT before any damage/death callbacks. Neither departure nor
    // death after this point may cancel the other cabin's committed result.
    g_CabinPairState[p] = CABIN_RESULT;
    for (new c = p * 2; c < p * 2 + 2; c++)
    {
        if (g_CabinChoice[c ^ 1] == CABIN_CHOICE_GREEN) Cabins_SetFloorOpen(c, true);
        else
        {
            g_CabinGasTicks[c] = CABIN_GAS_TICKS;
            g_CabinNextGasAt[c] = now;
            Cabins_SetGasVisible(c, true);
        }
    }
}

stock Cabins_Update(Float:now)
{
    Cabins_ScanPlayers();
    for (new p = 0; p < CABIN_PAIR_COUNT; p++)
    {
        new c = p * 2;
        switch (g_CabinPairState[p])
        {
            case CABIN_WAITING:
            {
                if (g_CabinPlayerCount[c] == 1 && g_CabinPlayerCount[c + 1] == 1)
                {
                    for (new k = c; k < c + 2; k++)
                    {
                        g_CabinPlayer[k] = g_CabinOccupant[k];
                        g_CabinUserId[k] = get_user_userid(g_CabinPlayer[k]);
                        g_CabinChoice[k] = 0;
                    }
                    g_CabinPairState[p] = CABIN_COUNTDOWN;
                    g_CabinPairDeadline[p] = now + CABIN_CLOSE_DELAY;
                }
            }
            case CABIN_COUNTDOWN:
            {
                if (!Cabins_IsPairValid(p))
                {
                    g_CabinPairState[p] = CABIN_WAITING;
                    g_CabinPairDeadline[p] = 0.0;
                }
                else if (now >= g_CabinPairDeadline[p])
                {
                    g_CabinWantClosed[c] = true;
                    g_CabinWantClosed[c + 1] = true;
                    g_CabinPairState[p] = CABIN_CLOSING;
                }
            }
            case CABIN_CLOSING:
            {
                if (!Cabins_IsPairValid(p)) Cabins_CancelPair(p);
                else if (g_CabinDoorDrop[c] >= CABIN_DOOR_TRAVEL && g_CabinDoorDrop[c + 1] >= CABIN_DOOR_TRAVEL)
                {
                    g_CabinPairState[p] = CABIN_CHOOSING;
                    g_CabinPairDeadline[p] = now + CABIN_CHOICE_SECONDS;
                }
            }
            case CABIN_CHOOSING:
            {
                if (!Cabins_IsPairValid(p) || now >= g_CabinPairDeadline[p]) Cabins_CancelPair(p);
            }
            case CABIN_RESULT:
            {
                // Each door opens independently once that cabin is empty.
                for (new k = c; k < c + 2; k++) if (!g_CabinPlayerCount[k]) g_CabinWantClosed[k] = false;
                if (!g_CabinPlayerCount[c] && !g_CabinPlayerCount[c + 1] && !g_CabinGasTicks[c] && !g_CabinGasTicks[c + 1])
                g_CabinPairState[p] = CABIN_RESETTING;
            }
            case CABIN_RESETTING:
            {
                if (!g_CabinPlayerCount[c] && !g_CabinPlayerCount[c + 1] && g_CabinDoorDrop[c] == 0.0 && g_CabinDoorDrop[c + 1] == 0.0 && Cabins_IsFloorClear(c) && Cabins_IsFloorClear(c + 1))
                {
                    for (new k = c; k < c + 2; k++)
                    {
                        Cabins_SetFloorOpen(k, false);
                        g_CabinChoice[k] = 0;
                        g_CabinPlayer[k] = 0;
                        g_CabinUserId[k] = 0;
                        set_pev(g_CabinButton[k][0], pev_frame, 0.0);
                        set_pev(g_CabinButton[k][1], pev_frame, 0.0);
                    }
                    g_CabinPairState[p] = CABIN_WAITING;
                }
            }
        }
    }
    for (new c = 0; c < CABIN_COUNT; c++)
    {
        if (g_CabinGasTicks[c] && now >= g_CabinNextGasAt[c])
        {
            g_CabinGasTicks[c]--;
            g_CabinNextGasAt[c] = now + CABIN_GAS_INTERVAL;
            // Damage the locked participant only; userid rejects reconnect reuse.
            // DMG_DROWN causes immediate 15 HP damage without armor absorption
            // or poison's additional damage-over-time tail in stock CS.
            if (Cabins_IsParticipant(c)) ExecuteHamB(Ham_TakeDamage, g_CabinPlayer[c], g_CabinFloor[c], 0, CABIN_GAS_DAMAGE, DMG_DROWN);
            if (!g_CabinGasTicks[c]) Cabins_SetGasVisible(c, false);
        }
    }
}

stock Cabins_OnButtonUse(ent, activator)
{
    for (new c = 0; c < CABIN_COUNT; c++) for (new color = 0; color < 2; color++)
    {
        if (ent != g_CabinButton[c][color]) continue;
        // Suppress native Use even when invalid: no frames, sounds or outputs.
        if (g_CabinPairState[c / 2] != CABIN_CHOOSING || g_CabinChoice[c] || activator != g_CabinPlayer[c] || !Cabins_IsParticipant(c) || !(pev(activator, pev_button) & IN_USE)) return HAM_SUPERCEDE;
        Cabins_ScanPlayers();
        new Float:now = get_gametime();
        if (!Cabins_IsPairValid(c / 2) || now >= g_CabinPairDeadline[c / 2])
        {
            Cabins_CancelPair(c / 2);
            return HAM_SUPERCEDE;
        }
        if (!Player_IsAimingAt(activator, ent)) return HAM_SUPERCEDE;
        g_CabinChoice[c] = color + 1;
        set_pev(ent, pev_frame, 1.0);
        if (g_CabinChoice[c ^ 1]) Cabins_ResolveChoices(c / 2, now);
        return HAM_SUPERCEDE;
    }
    return HAM_IGNORED;
}

stock bool:Cabins_IsFloorClear(c)
{
    if (!g_CabinFloorOpen[c]) return true;
    new Float:lo[3], Float:hi[3], Float:fl[3], Float:fh[3];
    for (new a = 0; a < 3; a++)
    {
        fl[a] = g_CabinMin[c][a];
        fh[a] = g_CabinMax[c][a];
    }
    fl[2] =  - 18.0;
    fh[2] = 2.0;
    for (new id = 1; id <= g_MaxPlayers; id++) if (is_user_alive(id))
    {
        pev(id, pev_absmin, lo);
        pev(id, pev_absmax, hi);
        if (Bounds_Overlap(lo, hi, fl, fh)) return false;
    }
    return true;
}



stock Cabins_MoveDoor(c)
{
    Door_Set(g_CabinMotion[c], !g_CabinWantClosed[c]);
    // Pair state must wait for the actual native pusher, not a wall-clock timer.
    new Float:remaining = floatabs(Door_Distance(g_CabinMotion[c]));
    g_CabinDoorDrop[c] = g_CabinWantClosed[c] ? CABIN_DOOR_TRAVEL-remaining : remaining;
    if (remaining < 0.02) g_CabinDoorDrop[c] = g_CabinWantClosed[c] ? CABIN_DOOR_TRAVEL : 0.0;
}

stock Cabins_Reset()
{
    for (new p = 0; p < CABIN_PAIR_COUNT; p++)
    {
        g_CabinPairState[p] = CABIN_RESETTING;
        g_CabinPairDeadline[p] = 0.0;
    }
    for (new c = 0; c < CABIN_COUNT; c++)
    {
        g_CabinGasTicks[c] = 0;
        Cabins_SetGasVisible(c, false);
        g_CabinWantClosed[c] = false;
        g_CabinDoorDrop[c] = 0.0;
        g_CabinPlayer[c] = 0;
        g_CabinUserId[c] = 0;
        g_CabinChoice[c] = 0;
        Door_Set(g_CabinMotion[c], true, true);
        set_pev(g_CabinButton[c][0], pev_frame, 0.0);
        set_pev(g_CabinButton[c][1], pev_frame, 0.0);
        // Floor restoration is deferred to CABIN_RESETTING until nobody intersects it.
    }
}

// ReGameDLL's ambient Restart starts looping sounds without honoring Start Silent.
// Stop ours and suppress that restart; global round reset handles the rest of the pair.
public Cabins_OnGasSoundRestart(ent)
{
    if (!g_PluginReady) return HAM_IGNORED;
    for (new c = 0; c < CABIN_COUNT; c++)
    {
        if (!g_CabinGasSound[c] || ent != g_CabinGasSound[c]) continue;
        g_CabinGasTicks[c] = 0;
        Cabins_SetGasVisible(c, false);
        return HAM_SUPERCEDE;
    }
    return HAM_IGNORED;
}

stock Cabins_AnimateDoors()
{
    for (new c = 0; c < CABIN_COUNT; c++) Cabins_MoveDoor(c);
}
