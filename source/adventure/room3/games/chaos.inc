// Room 3 / Chaos: reuse three games with independent phase clocks.
// Context is restored before returning to the room controller or player hooks.
#define CHAOS_COMPONENTS 3
new const CHAOS_SOUND[] = "yx_xyin9/yx_chaos_start.wav";
const Float:CHAOS_INTRO_SECONDS = 4.374;
new bool:g_ChaosSoundAvailable, bool:g_ChaosSoundPlaying;
new bool:g_ChaosActive[CHAOS_COMPONENTS];
new g_ChaosPhase[CHAOS_COMPONENTS];
new Float:g_ChaosEndsAt[CHAOS_COMPONENTS];

stock Chaos_StopSound()
{
    if (!g_ChaosSoundPlaying) return;
    emit_sound(g_Room3Speaker, CHAN_VOICE, CHAOS_SOUND, 0.0, ATTN_NORM, SND_STOP, 100);
    g_ChaosSoundPlaying = false;
}

stock Chaos_Reset()
{
    Chaos_StopSound();
    for (new i = 0; i < CHAOS_COMPONENTS; i++)
    {
        g_ChaosActive[i] = false;
        g_ChaosPhase[i] = ROOM3_IDLE;
        g_ChaosEndsAt[i] = 0.0;
    }
    // The room reset owns child Reset calls, including on empty-room abort.
}

stock Chaos_Prepare(Float:now)
{
    Chaos_Reset();
    g_Room3Phase = ROOM3_CHAOS_INTRO;
    g_Room3PhaseEndsAt = now + CHAOS_INTRO_SECONDS;
    emit_sound(g_Room3Speaker, CHAN_VOICE, CHAOS_SOUND, 1.0, ATTN_NORM, 0, 100);
    g_ChaosSoundPlaying = true;
}

stock Chaos_Start(Float:now)
{
    Chaos_StopSound();
    Color_Reset();
    for (new i = 0; i < CHAOS_COMPONENTS; i++)
    {
        if (i == 0) Color_Prepare(now);
        else if (i == 1) Laser_Prepare(now, true);
        else Meteor_Prepare(now, true);
        g_ChaosPhase[i] = g_Room3Phase;
        g_ChaosEndsAt[i] = g_Room3PhaseEndsAt;
        g_ChaosActive[i] = true;
    }
    g_Room3Phase = ROOM3_CHAOS_ACTIVE;
    g_Room3PhaseEndsAt = 0.0;
}

stock bool:Chaos_Update(Float:now)
{
    if (g_Room3Phase == ROOM3_CHAOS_INTRO)
    {
        if (now >= g_Room3PhaseEndsAt) Chaos_Start(now);
        return false;
    }
    new parentPhase = g_Room3Phase;
    new Float:parentEnd = g_Room3PhaseEndsAt;
    new remaining;
    for (new i = 0; i < CHAOS_COMPONENTS; i++) if (g_ChaosActive[i])
    {
        g_Room3Phase = g_ChaosPhase[i];
        g_Room3PhaseEndsAt = g_ChaosEndsAt[i];
        new bool:done;
        if (i == 0)
        {
            done = Color_Update(now);
            if (g_Room3Phase == ROOM3_COLOR_HOLD)
                for (new id = 1; id <= g_MaxPlayers; id++)
                    if (Room3_IsInsideArena(id)) Color_CheckHazards(id);
        }
        else if (i == 1) done = Laser_Update(now);
        else done = Meteor_Update(now);
        g_ChaosPhase[i] = g_Room3Phase;
        g_ChaosEndsAt[i] = g_Room3PhaseEndsAt;
        if (done)
        {
            g_ChaosActive[i] = false;
            if (i == 0) Color_Reset();
            else if (i == 1) Laser_Reset();
            else Meteor_Reset();
        }
        else remaining++;
    }
    g_Room3Phase = parentPhase;
    g_Room3PhaseEndsAt = parentEnd;
    return remaining == 0;
}

stock Chaos_CheckHazards(id)
{
    new parentPhase = g_Room3Phase;
    new Float:parentEnd = g_Room3PhaseEndsAt;
    for (new i = 0; i < CHAOS_COMPONENTS; i++) if (g_ChaosActive[i] && Room3_IsInsideArena(id))
    {
        g_Room3Phase = g_ChaosPhase[i];
        g_Room3PhaseEndsAt = g_ChaosEndsAt[i];
        if (i == 0 && g_Room3Phase == ROOM3_COLOR_HOLD) Color_CheckHazards(id);
        else if (i == 1 && g_Room3Phase == ROOM3_LASER_ACTIVE) Laser_CheckHazards(id);
        else if (i == 2 && g_Room3Phase == ROOM3_METEOR_ACTIVE) Meteor_CheckHazards(id);
    }
    g_Room3Phase = parentPhase;
    g_Room3PhaseEndsAt = parentEnd;
}
