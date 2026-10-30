// One continuous USE rectangle per picture+nameplate. No additional BSP models.
#define GALLERY_AUDIO_COUNT 10
new g_GalleryAudioMarker[GALLERY_AUDIO_COUNT];
new bool:g_GalleryAudioAvailable[GALLERY_AUDIO_COUNT], bool:g_GalleryAudioReady;
new g_GalleryAudioCurrent = -1;
new Float:g_GalleryAudioNextUse;

stock GalleryAudio_Precache()
{
    if (!g_GalleryMap) return;
    new sample[64],path[80];
    for (new i=0;i<GALLERY_AUDIO_COUNT;i++)
    {
        formatex(sample,charsmax(sample),"yx_xyin9/gallery/yx_z%d.wav",i+1);
        formatex(path,charsmax(path),"sound/%s",sample);
        g_GalleryAudioAvailable[i]=bool:file_exists(path);
        if (g_GalleryAudioAvailable[i]) precache_sound(sample);
        else log_amx("Gallery audio missing: %s",path);
    }
}
stock GalleryAudio_Init()
{
    if (!g_GalleryMap) return;
    new name[32];
    for (new i=0;i<GALLERY_AUDIO_COUNT;i++)
    {
        formatex(name,charsmax(name),"gallery_label_%02d",i+1);
        g_GalleryAudioMarker[i]=Map_FindEntity(name);
        if (!pev_valid(g_GalleryAudioMarker[i])) return;
    }
    g_GalleryAudioReady=true;
    register_forward(FM_PlayerPreThink,"GalleryAudio_OnPreThink");
    register_event("HLTV","GalleryAudio_NewRound","a","1=0","2=0");
}
stock GalleryAudio_Stop()
{
    if (g_GalleryAudioCurrent>=0 && pev_valid(g_GalleryAudioMarker[g_GalleryAudioCurrent]))
    {
        new sample[64];formatex(sample,charsmax(sample),"yx_xyin9/gallery/yx_z%d.wav",g_GalleryAudioCurrent+1);
        emit_sound(g_GalleryAudioMarker[g_GalleryAudioCurrent],CHAN_STATIC,sample,0.0,ATTN_NORM,SND_STOP,100);
    }
    g_GalleryAudioCurrent=-1;
}
public GalleryAudio_NewRound()
{
    GalleryAudio_Stop();
    g_GalleryAudioNextUse=0.0;
}
stock GalleryAudio_Hit(const Float:eye[3],const Float:direction[3],Float:hit[3])
{
    if (direction[0]>=-0.001) return -1;
    // Stay near the wall, but let a standing player aim at the full-height picture.
    if (eye[0]<-2757.5 || eye[0]>-2661.5) return -1;
    new Float:distance=(-2757.5-eye[0])/direction[0];
    if (distance<0.0 || distance>224.0) return -1;
    for (new axis=0;axis<3;axis++) hit[axis]=eye[axis]+direction[axis]*distance;
    if (hit[2]<-3728.0 || hit[2]>-3552.0) return -1;
    for (new i=0;i<GALLERY_AUDIO_COUNT;i++)
    {
        new Float:center=-1739.0+float(i)*160.0;
        if (hit[1]>=center-72.0 && hit[1]<=center+72.0) return i;
    }
    return -1;
}
public GalleryAudio_OnPreThink(id)
{
    if (!g_PluginReady || !g_GalleryAudioReady || !is_user_alive(id) || pev(id,pev_iuser1)!=0) return FMRES_IGNORED;
    if (!(pev(id,pev_button)&IN_USE) || (pev(id,pev_oldbuttons)&IN_USE)) return FMRES_IGNORED;
    new Float:now=get_gametime();if(now<g_GalleryAudioNextUse) return FMRES_IGNORED;
    new Float:eye[3];pev(id,pev_origin,eye);
    if (eye[0]<-2757.0 || eye[0]>-2442.0 || eye[1]<-1830.0 || eye[1]>-200.0 || eye[2]<-3800.0 || eye[2]>-3500.0) return FMRES_IGNORED;
    new Float:offset[3],Float:angle[3],Float:direction[3],Float:hit[3];
    pev(id,pev_view_ofs,offset);for(new axis=0;axis<3;axis++)eye[axis]+=offset[axis];
    pev(id,pev_v_angle,angle);engfunc(EngFunc_MakeVectors,angle);global_get(glb_v_forward,direction);
    new slot=GalleryAudio_Hit(eye,direction,hit);
    if(slot<0 || !g_GalleryAudioAvailable[slot])return FMRES_IGNORED;
    // Only a clear ray to the near face is usable: no interaction through walls/players.
    new trace=create_tr2();engfunc(EngFunc_TraceLine,eye,hit,0,id,trace);
    new Float:fraction;get_tr2(trace,TR_flFraction,fraction);
    new bool:blocked=get_tr2(trace,TR_StartSolid) || get_tr2(trace,TR_AllSolid) || fraction<0.999;
    free_tr2(trace);if(blocked)return FMRES_IGNORED;
    GalleryAudio_Stop();
    new sample[64];formatex(sample,charsmax(sample),"yx_xyin9/gallery/yx_z%d.wav",slot+1);
    emit_sound(g_GalleryAudioMarker[slot],CHAN_STATIC,sample,0.8,ATTN_NORM,0,100);
    g_GalleryAudioCurrent=slot;g_GalleryAudioNextUse=now+0.4;
    return FMRES_IGNORED;
}
