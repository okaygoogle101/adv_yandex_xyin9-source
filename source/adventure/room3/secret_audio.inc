// Shared once-per-round reveal; independent of Room3's ordinary game resets.
new g_SecretAudioRoom, g_SecretAudioExit;
new bool:g_SecretAudioPlayed, bool:g_SecretMusicAvailable, bool:g_SecretBackAvailable;
new g_SecretExitTouch[33];
new Float:g_SecretExitBefore[33][3];
new const SECRET_MUSIC[] = "yx_xyin9/secret/yx_dexter_shipyard.wav";
new const SECRET_BACK[] = "yx_xyin9/secret/yx_back.wav";

stock SecretAudio_Precache()
{
    g_SecretMusicAvailable = bool:file_exists("sound/yx_xyin9/secret/yx_dexter_shipyard.wav");
    g_SecretBackAvailable = bool:file_exists("sound/yx_xyin9/secret/yx_back.wav");
    if (g_SecretMusicAvailable) precache_sound(SECRET_MUSIC);
    if (g_SecretBackAvailable) precache_sound(SECRET_BACK);
}
stock SecretAudio_Bind()
{
    g_SecretAudioRoom = Map_FindEntity("secret_audio_room");
    g_SecretAudioExit = Map_FindEntity("chaos_secret_exit");
}
stock SecretAudio_Init()
{
    SecretAudio_Bind();
    // Engine touch dispatch survives game-DLL Restart and does not depend on a Ham vtable hook.
    register_forward(FM_Touch, "SecretAudio_ExitPre");
    register_forward(FM_Touch, "SecretAudio_ExitPost", 1);
}
public SecretAudio_NewRound()
{
    SecretAudio_Bind();
    if (g_SecretMusicAvailable && pev_valid(g_SecretAudioRoom))
        emit_sound(g_SecretAudioRoom,CHAN_STATIC,SECRET_MUSIC,0.0,ATTN_NORM,SND_STOP,100);
    g_SecretAudioPlayed = false;
    arrayset(g_SecretExitTouch,0,sizeof g_SecretExitTouch);
}
stock SecretAudio_Update()
{
    if (g_SecretAudioPlayed || !g_SecretMusicAvailable || !pev_valid(g_SecretAudioRoom)) return;
    for (new id=1; id<=g_MaxPlayers; id++)
    {
        if (!is_user_alive(id) || pev(id,pev_iuser1)!=0) continue;
        new Float:p[3]; pev(id,pev_origin,p);
        if (p[0]<-1100.0 || p[0]>-460.0 || p[1]<2152.0 || p[1]>2728.0 || p[2]<2600.0 || p[2]>2824.0) continue;
        g_SecretAudioPlayed = true;
        emit_sound(g_SecretAudioRoom,CHAN_STATIC,SECRET_MUSIC,0.65,ATTN_NORM,0,100);
        return;
    }
}
public SecretAudio_ExitPre(ent,id)
{
    if (ent!=g_SecretAudioExit || id<1 || id>g_MaxPlayers) return FMRES_IGNORED;
    g_SecretExitTouch[id]=0;
    if (!g_PluginReady || !is_user_alive(id) || pev(id,pev_iuser1)!=0 || pev(ent,pev_solid)!=SOLID_TRIGGER) return FMRES_IGNORED;
    pev(id,pev_origin,g_SecretExitBefore[id]);
    g_SecretExitTouch[id]=ent;
    return FMRES_IGNORED;
}
public SecretAudio_ExitPost(ent,id)
{
    if (ent!=g_SecretAudioExit || id<1 || id>g_MaxPlayers || g_SecretExitTouch[id]!=ent) return FMRES_IGNORED;
    g_SecretExitTouch[id]=0;
    if (!g_PluginReady || !is_user_alive(id) || !g_SecretBackAvailable) return FMRES_IGNORED;
    new Float:p[3]; pev(id,pev_origin,p);
    if (get_distance_f(p,g_SecretExitBefore[id])>1.0
        && p[0]>-128.0 && p[0]<128.0 && p[1]>-1248.0 && p[1]<-992.0
        && p[2]>-1320.0 && p[2]<-1080.0)
        client_cmd(id,"spk yx_xyin9/secret/yx_back.wav");
    return FMRES_IGNORED;
}
