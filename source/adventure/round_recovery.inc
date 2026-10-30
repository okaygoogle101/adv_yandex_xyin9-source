// Restore round-owned labels/audio after the game DLL finishes map cleanup.
#define TASK_ROUND_RECOVERY 85151
new bool:g_RoundRecoveryHLTV;

stock RoundRecovery_Schedule()
{
    if (!g_PluginReady) return;
    remove_task(TASK_ROUND_RECOVERY);
    set_task(0.2, "RoundRecovery_Apply", TASK_ROUND_RECOVERY);
}
public RoundRecovery_HLTV()
{
    g_RoundRecoveryHLTV = true;
    RoundRecovery_Schedule();
}
public RoundRecovery_Start()
{
    // Servers which omit the standard HLTV notification still reset these systems.
    if (!g_RoundRecoveryHLTV) RoundRecovery_Schedule();
    g_RoundRecoveryHLTV = false;
}
public RoundRecovery_Apply()
{
    if (!g_PluginReady) return;
    SecretAudio_NewRound();
    Gallery_NewRound();
    GalleryAudio_NewRound();
    // Rebind persistent map markers rather than trusting cached edict indices.
    for (new i=0; i<GALLERY_AUDIO_COUNT; i++) {
        new name[32]; formatex(name, charsmax(name), "gallery_label_%02d", i+1);
        g_GalleryAudioMarker[i] = Map_FindEntity(name);
    }
    g_ChaosSecretTrigger = Map_FindEntity("chaos_secret_trigger");
    g_ChaosSecretLight = Map_FindEntity("chaos_secret_light");
    ChaosSecret_SetEnabled(false);
    if (g_Room3Ready) for (new i=0; i<2; i++) {
        new door = g_Room3DoorMotion[i];
        Door_Set(door, g_DoorOpen[door], true);
    }
}
