// Finale gallery awards: shared per-round wall labels, no persistent database.
// Only reached-finish detection; this is not an audit of every previous challenge.
#define GALLERY_PLACES 20
new const GALLERY_FONT_MODEL[] = "sprites/yx_xyin9/yx_gallery_font.spr";
new const GALLERY_GLYPH_CLASS[] = "adventure_gallery_glyph";
new bool:g_GalleryMap, bool:g_GalleryReady;
new g_GalleryFontModel;
new bool:g_GalleryOccupied[GALLERY_PLACES];
new g_GalleryOwnerUid[GALLERY_PLACES], g_GalleryOwnerAuth[GALLERY_PLACES][40];
new g_GalleryDoneUid[33], g_GalleryGlyphs[GALLERY_PLACES][GALLERY_NAME_LIMIT];
new Float:g_GalleryOrigin[GALLERY_PLACES][3], Float:g_GalleryAngles[GALLERY_PLACES][3];
new Float:g_GalleryRetryAt;

stock Gallery_Precache()
{
    if (!g_GalleryMap) return;
    if (!file_exists(GALLERY_FONT_MODEL)) {
        log_amx("Gallery disabled: install %s from plugin 3.35.0.", GALLERY_FONT_MODEL);
        return;
    }
    new file = fopen(GALLERY_FONT_MODEL, "rb"), frames;
    if (!file) return;
    fseek(file, 28, SEEK_SET);
    new readBytes = fread(file, frames, BLOCK_INT);
    fclose(file);
    if (readBytes != BLOCK_INT || frames != 102) {
        log_amx("Gallery disabled: replace old gallery_font.spr with the 102-frame font from 3.35.0.");
        return;
    }
    g_GalleryFontModel = precache_model(GALLERY_FONT_MODEL);
}

stock Gallery_Init()
{
    if (!g_GalleryMap || !g_GalleryFontModel) return;
    new name[32];
    for (new i = 0; i < GALLERY_PLACES; i++) {
        formatex(name, charsmax(name), "gallery_label_%02d", i + 1);
        new marker = Map_FindEntity(name);
        if (!pev_valid(marker)) { log_amx("Gallery disabled: missing %s.", name); return; }
        pev(marker, pev_origin, g_GalleryOrigin[i]);
        pev(marker, pev_angles, g_GalleryAngles[i]);
        // Map markers face out of the wall; oriented sprite angles face into it.
        // Turning 180 degrees makes texture-right point right from the corridor.
        g_GalleryAngles[i][1] += 180.0;
        if (g_GalleryAngles[i][1] >= 360.0) g_GalleryAngles[i][1] -= 360.0;
    }
    g_GalleryReady = true;
    register_forward(FM_PlayerPostThink, "Gallery_OnPostThink");
    Gallery_NewRound();
}

stock Gallery_ClearSlot(slot)
{
    new classname[32];
    for (new j = 0; j < GALLERY_NAME_LIMIT; j++) {
        new ent = g_GalleryGlyphs[slot][j];
        if (pev_valid(ent)) {
            pev(ent, pev_classname, classname, charsmax(classname));
            if (equal(classname, GALLERY_GLYPH_CLASS)) engfunc(EngFunc_RemoveEntity, ent);
        }
        g_GalleryGlyphs[slot][j] = 0;
    }
    g_GalleryOccupied[slot] = false;
    g_GalleryOwnerUid[slot] = 0;
    g_GalleryOwnerAuth[slot][0] = 0;
}

public Gallery_NewRound()
{
    if (!g_GalleryReady) return;
    for (new i = 0; i < GALLERY_PLACES; i++) Gallery_ClearSlot(i);
    arrayset(g_GalleryDoneUid, 0, sizeof g_GalleryDoneUid);
    g_GalleryRetryAt = 0.0;
}

stock bool:Gallery_InFinish(const Float:p[3])
{
    return p[0] >= -2620.0 && p[0] <= -2580.0
        && p[1] >= -3016.0 && p[1] <= -2864.0
        && p[2] >= -3784.0 && p[2] <= -3700.0;
}

stock bool:Gallery_StableAuth(const auth[])
{
    // Shared LAN/PENDING/BOT identifiers must never merge different players.
    return (contain(auth, "STEAM_") == 0 || contain(auth, "VALVE_") == 0)
        && contain(auth, ":") != -1 && contain(auth, "LAN") == -1
        && contain(auth, "PENDING") == -1;
}

stock bool:Gallery_HasAward(uid, const auth[])
{
    for (new i = 0; i < GALLERY_PLACES; i++) {
        if (!g_GalleryOccupied[i]) continue;
        if (g_GalleryOwnerUid[i] == uid) return true;
        if (auth[0] && equal(g_GalleryOwnerAuth[i], auth)) return true;
    }
    return false;
}

stock Gallery_RandomFree()
{
    new freeSlots[GALLERY_PLACES], count;
    for (new i = 0; i < GALLERY_PLACES; i++)
        if (!g_GalleryOccupied[i]) freeSlots[count++] = i;
    if (!count) return -1;
    return freeSlots[random_num(0, count - 1)];
}

stock bool:Gallery_DrawName(slot, const frames[], count)
{
    // 15 letters: 14 * 8.5 advance + 8 width = 127; inner plate width is 132.
    new Float:direction = (g_GalleryAngles[slot][1] > 90.0) ? 1.0 : -1.0;
    new Float:offset = -float(count - 1) * 8.5 / 2.0;
    new Float:p[3], Float:lo[3] = {-4.0, -4.0, -6.0}, Float:hi[3] = {4.0, 4.0, 6.0};
    new Float:white[3] = {255.0, 255.0, 255.0};
    for (new j = 0; j < count; j++) {
        if (frames[j] == 1) continue; // Blank space needs no entity.
        new ent = engfunc(EngFunc_CreateNamedEntity, engfunc(EngFunc_AllocString, "info_target"));
        if (!pev_valid(ent)) { Gallery_ClearSlot(slot); return false; }
        g_GalleryGlyphs[slot][j] = ent;
        set_pev(ent, pev_classname, GALLERY_GLYPH_CLASS);
        engfunc(EngFunc_SetModel, ent, GALLERY_FONT_MODEL);
        set_pev(ent, pev_solid, SOLID_NOT);
        set_pev(ent, pev_movetype, MOVETYPE_NONE);
        set_pev(ent, pev_takedamage, DAMAGE_NO);
        set_pev(ent, pev_angles, g_GalleryAngles[slot]);
        set_pev(ent, pev_frame, float(frames[j]));
        set_pev(ent, pev_framerate, 0.0);
        set_pev(ent, pev_nextthink, 0.0);
        set_pev(ent, pev_scale, 0.25);
        set_pev(ent, pev_rendermode, kRenderNormal);
        set_pev(ent, pev_renderamt, 255.0);
        set_pev(ent, pev_rendercolor, white);
        set_pev(ent, pev_effects, EF_NOINTERP);
        engfunc(EngFunc_SetSize, ent, lo, hi);
        p[0] = g_GalleryOrigin[slot][0];
        p[1] = g_GalleryOrigin[slot][1] + direction * (offset + float(j) * 8.5);
        p[2] = g_GalleryOrigin[slot][2];
        engfunc(EngFunc_SetOrigin, ent, p);
    }
    return true;
}

stock bool:Gallery_Award(id, uid)
{
    new auth[40]; get_user_authid(id, auth, charsmax(auth));
    if (!Gallery_StableAuth(auth)) auth[0] = 0;
    if (Gallery_HasAward(uid, auth)) return true;
    new slot = Gallery_RandomFree();
    if (slot < 0) return true; // Full: never replace previous winners.
    new name[128], frames[GALLERY_NAME_LIMIT]; get_user_name(id, name, charsmax(name));
    new count = GalleryFont_Frames(name, frames, sizeof frames), bool:visible;
    for (new j = 0; j < count; j++) if (frames[j] != 1) visible = true;
    if (!visible) { frames[0] = 0; count = 1; }
    new needed;
    for (new j = 0; j < count; j++) if (frames[j] != 1) needed++;
    // Keep edict headroom for weapons and other server plugins.
    if (engfunc(EngFunc_NumberOfEntities) + needed + 96 >= global_get(glb_maxEntities)) return false;
    if (!Gallery_DrawName(slot, frames, count)) return false;
    g_GalleryOccupied[slot] = true;
    g_GalleryOwnerUid[slot] = uid;
    copy(g_GalleryOwnerAuth[slot], charsmax(g_GalleryOwnerAuth[]), auth);
    return true;
}

stock Gallery_CheckPlayer(id, Float:now)
{
    if (!g_PluginReady || !g_GalleryReady || now < g_GalleryRetryAt) return;
    if (!is_user_alive(id) || is_user_bot(id)) return;
    if (pev(id, pev_movetype) != MOVETYPE_WALK || pev(id, pev_iuser1)) return;
    new uid = get_user_userid(id);
    if (uid <= 0 || g_GalleryDoneUid[id] == uid) return;
    new Float:p[3]; pev(id, pev_origin, p);
    if (!Gallery_InFinish(p)) return;
    if (Gallery_Award(id, uid)) g_GalleryDoneUid[id] = uid;
    else {
        g_GalleryRetryAt = now + 1.0;
        log_amx("Gallery: not enough free entities for a name; keeping all existing labels.");
    }
}
public Gallery_OnPostThink(id)
{
    Gallery_CheckPlayer(id, get_gametime());
    return FMRES_IGNORED;
}
stock Gallery_Update(Float:now)
{
    for (new id = 1; id <= g_MaxPlayers; id++) Gallery_CheckPlayer(id, now);
}
