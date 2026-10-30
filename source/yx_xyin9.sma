// Adventure 3.45.2 — modular source for AMX Mod X 1.9.
// Compile this entry point with the adjacent adventure/ directory intact.
// v123 optimization of v122. Challenge rules and assets preserved.
// Door layout v94, any BSP filename. Install models/yx_xyin9/.
#include <amxmodx>
#include <fakemeta>
#include <hamsandwich>

new bool:g_PluginReady;
new bool:g_AdventureMap;
new g_MaxPlayers;
new Float:g_LastMotionAt;

#include "adventure/settings.inc"
#include "adventure/map_layout.inc"
#include "adventure/common.inc"
#include "adventure/doors.inc"
#include "adventure/exit_weapons.inc"
#include "adventure/room1_cabins.inc"
#include "adventure/room2_waterdrop.inc"
#include "adventure/room3/state.inc"
#include "adventure/room3/games/safe_color.inc"
#include "adventure/room3/games/spinner.inc"
#include "adventure/room3/games/ceiling.inc"
#include "adventure/room3/games/lasers.inc"
#include "adventure/room3/games/blocks.inc"
#include "adventure/room3/games/islands.inc"
#include "adventure/room3/games/scanner.inc"
#include "adventure/room3/games/meteors.inc"
#include "adventure/room3/games/chaos.inc"
#include "adventure/room3/selection.inc"
#include "adventure/room3/secret.inc"
#include "adventure/room3/secret_audio.inc"
#include "adventure/room3/controller.inc"
#include "adventure/room4_layout.inc"
#include "adventure/room4_maze.inc"
#include "adventure/room5_longjumps.inc"
#include "adventure/room6_decor.inc"
#include "adventure/room8/rubik.inc"
#include "adventure/room8/ladder_step.inc"
#include "adventure/static_decor.inc"
#include "adventure/toys.inc"
#include "adventure/room9_train.inc"
#include "adventure/room10/golf.inc"
#include "adventure/room10/corridor_lasers.inc"
#include "adventure/room11/recoil.inc"
#include "adventure/room11/flight.inc"
#include "adventure/room11/contact.inc"
#include "adventure/room11/silent_m249.inc"
#include "adventure/finale.inc"
#include "gallery/gallery_font.inc"
#include "adventure/gallery.inc"
#include "adventure/gallery_audio.inc"
#include "adventure/round_recovery.inc"
#include "adventure/final_cube/rubik.inc"

public plugin_precache()
{
    g_AdventureMap = Adventure_HasOptimizedLayout();
    if (!g_AdventureMap) return;
    SecretAudio_Precache();
    Room8_Precache();
    FinalCube_Precache();
    Gallery_Precache();
    GalleryAudio_Precache();
    Room10_Precache();
    Room11_Precache();
    Room11Flight_Precache();
    // Do not fail unrelated maps when this map-specific sound is absent.
    g_ChaosSoundAvailable = bool:file_exists("sound/yx_xyin9/yx_chaos_start.wav");
    if (g_ChaosSoundAvailable) precache_sound(CHAOS_SOUND);
    precache_sound("plats/heavymove1.wav");
    precache_sound("doors/doormove1.wav");
    precache_sound("doors/doorstop1.wav");
    precache_sound("buttons/button1.wav");
    precache_sound("buttons/button3.wav");
    precache_sound("buttons/blip1.wav");
    precache_sound("buttons/bell1.wav");
    precache_sound("debris/bustcrate1.wav");
}

public plugin_init()
{
    register_plugin("Adventure Trust", "3.45.2", "Adventure project");
    if (!g_AdventureMap)
    {
        if (Map_FindEntity("adventure_trust_v15"))
            set_fail_state("Adventure map detected but BSP resource detection failed; check AMXX log.");
        return;
    }
    g_MaxPlayers = get_maxplayers();
    register_event("HLTV", "RoundRecovery_HLTV", "a", "1=0", "2=0");
    register_logevent("RoundRecovery_Start", 2, "1=Round_Start");
    RegisterHam(Ham_Blocked, "func_wall", "Doors_Blocked");
    RegisterHam(Ham_TraceAttack, "func_wall", "Room8_OnTraceAttack");
    RegisterHam(Ham_TraceAttack, "func_wall", "FinalCube_OnTraceAttack");
    RegisterHam(Ham_TakeDamage, "func_wall", "FinalCube_OnTakeDamage");
    register_event("HLTV", "FinalCube_OnRoundStart", "a", "1=0", "2=0");
    RegisterHam(Ham_TakeDamage, "func_wall", "Room8_OnTakeDamage");
    register_event("HLTV", "Room8_OnRoundStart", "a", "1=0", "2=0");
    RegisterHam(Ham_CS_Restart, "ambient_generic", "Cabins_OnGasSoundRestart");
    RegisterHam(Ham_Use, "func_button", "Adventure_OnButtonUse");
    RegisterHam(Ham_Player_PostThink, "player", "Waterdrop_OnPlayerPostThink", 1);
    register_forward(FM_StartFrame, "Adventure_OnFrame");
    register_forward(FM_PlayerPreThink, "Room4_OnPreThink");
    RegisterHam(Ham_Player_PostThink, "player", "Room4_OnPostThink", 1);
    register_event("HLTV", "Room4_OnRoundStart", "a", "1=0", "2=0");
    RegisterHam(Ham_Player_PostThink, "player", "Room3_OnPlayerPostThink", 1);
    register_event("HLTV", "Adventure_Reset", "a", "1=0", "2=0");
}

public plugin_cfg()
{
    if (!g_AdventureMap) return;
    if (!Map_FindEntity("adventure_trust_v15")) return;
    // No map marker: do not activate any mechanics.
    if (!Cabins_Init() || !Waterdrop_Init() || !Room3_Init() || !Room4_Init() || !Room5_Init()) return;
    if (!ChaosSecret_Init()) return;
    SecretAudio_Init();
    Room6_Init();
    Toys_Init();
    if (!Room8_Init() || !Room8Ladder_Init()) return;
    if (!StaticDecor_Init()) return;
    if (!Room9_Init()) return;
    if (!Room10_Init()) return;
    if (!Room10Corridor_Init()) return;
    if (!Room11_Init()) return;
    if (!Room11Flight_Init()) return;
    Room11Contact_Init();
    Room11Silent_Init();
    ExitWeapons_Init();
    Finale_Init();
    if (!FinalCube_Init()) return;
    Gallery_Init();
    GalleryAudio_Init();
    g_PluginReady = true;
    Adventure_Reset();
    set_task(ADVENTURE_POLL_SECONDS, "Adventure_Poll", TASK_ADVENTURE_POLL, "", 0, "b");
    log_amx("Adventure 3.45.2 ready; Chaos completion secret branch; existing games preserved.");
}

public Adventure_Poll()
{
    if (!g_PluginReady) return;
    new Float:now = get_gametime();
    SecretAudio_Update();
    Gallery_Update(now);
    Waterdrop_Update(now);
    Room8_UpdateUseKeys();
    Room8Ladder_Update(now);
    Cabins_Update(now);
}

public Adventure_OnButtonUse(ent, activator, caller, useType, Float:value)
{
    if (!g_PluginReady) return HAM_IGNORED;
    new flightResult=Room11Flight_Use(ent,activator);
    if(flightResult!=HAM_IGNORED) return flightResult;
    new recoilResult=Room11_Use(ent,activator);
    if(recoilResult!=HAM_IGNORED) return recoilResult;
    new corridorResult=Room10Corridor_Use(ent,activator);
    if(corridorResult!=HAM_IGNORED) return corridorResult;
    new golfResult=Room10_Use(ent,activator);
    if(golfResult!=HAM_IGNORED) return golfResult;
    if (g_R8StepReady && ent == g_R8StepButton) return Room8Ladder_Use(activator);
    if (g_FinalCubeReady && ent == g_FinalCubeButton) return FinalCube_UseButton(activator);
    if (g_Room8Ready && ent == g_Room8Button) return Room8_UseButton(activator);
    if (g_Room5Ready && ent == g_Room5Button) return Room5_Use(activator);
    if (g_Room4Ready && ent == g_Room4Button) return Room4_UseButton(activator);
    if (g_WaterdropButton && ent == g_WaterdropButton) return Waterdrop_Use(activator);
    return Cabins_OnButtonUse(ent, activator);
}

public Adventure_Reset()
{
    if (!g_PluginReady) return;
    Room9_Reset();
    Room10_Reset();
    Room10Corridor_Reset();
    Room11_Reset();
    Room11Flight_Reset();
    Room3_Reset();
    Room5_Reset();
    Room6_Reset();
    Toys_Reset();
    Room8Ladder_Reset();
    // A round restart never bypasses the waterdrop cooldown.
    Waterdrop_SetActive(false);
    Cabins_Reset();
    StaticDecor_Reset();
    Doors_SnapAll();
    g_LastMotionAt = get_gametime();
}

public plugin_pause()
{
    GalleryAudio_Stop();
    Room4_StopWind();
    Doors_Stop();
    Adventure_Reset();
    Room11_Stop();
    Room11Flight_Stop();
}

public plugin_unpause()
{
    Adventure_Reset();
}

public plugin_end()
{
    GalleryAudio_Stop();
    Gallery_NewRound();
    Room4_Reset();
    Adventure_Reset();
    Room11_Stop();
    Room11Flight_Stop();
    Room10_Shutdown();
    Doors_Stop();
}

// Read only the BSP entity lump before entity spawning/precache has completed.
// Existing optimized-layout markers work even if JACK saves the map under a new name.
// No resources or gameplay hooks are registered on unrelated maps.
stock bool:Adventure_HasOptimizedLayout()
{
    new map[64], path[96];
    get_mapname(map, charsmax(map));
    formatex(path, charsmax(path), "maps/%s.bsp", map);
    // Valve search paths also cover downloaded maps and custom game directories.
    new file = fopen(path, "rb", true);
    if (!file) file = fopen(path, "rb");
    if (!file) { log_amx("Cannot read BSP for layout detection: %s", path); return false; }
    // AMXX 1.9 fread returns BYTES, not the element count for BLOCK_INT.
    new version, offset, length;
    if (fread(file, version, BLOCK_INT) != BLOCK_INT || version != 30
        || fread(file, offset, BLOCK_INT) != BLOCK_INT || fread(file, length, BLOCK_INT) != BLOCK_INT)
    { fclose(file); log_amx("Invalid BSP header: %s", path); return false; }
    fseek(file, 0, SEEK_END);
    new size = ftell(file);
    if (offset < 124 || offset > size || length < 1 || length > size - offset)
    { fclose(file); log_amx("Invalid BSP entity bounds: %s", path); return false; }
    fseek(file, offset, SEEK_SET);
    new line[512], bool:baseMarker, bool:starsMarker;
    while (!feof(file) && ftell(file) < offset + length)
    {
        if (!fgets(file, line, charsmax(line))) break;
        trim(line);
        if (equal(line, "^"targetname^" ^"adventure_trust_v15^"")) baseMarker = true;
        if (equal(line, "^"targetname^" ^"room4_stars_0^"")) starsMarker = true;
        if (equal(line, "^"targetname^" ^"gallery_label_01^"")) g_GalleryMap = true;
        if (equal(line, "^"targetname^" ^"adventure_room10_v84^"")) g_R10Map = true;
        if (equal(line, "^"targetname^" ^"adventure_final_cube_v101^"")) g_FinalCubeMap = true;
        if (equal(line, "^"targetname^" ^"adventure_room11_v88^"")) g_R11Map = true;
        if (equal(line, "^"targetname^" ^"adventure_room11_flight_v89^"")) g_R11FMap = true;
    }
    fclose(file);
    if (baseMarker && !starsMarker) log_amx("Adventure BSP lacks optimized star marker: %s", path);
    return baseMarker && starsMarker;
}

public Adventure_OnFrame()
{
    if (!g_PluginReady) return FMRES_IGNORED;
    new Float:now = get_gametime();
    if (now - g_LastMotionAt >= ADVENTURE_MOTION_SECONDS)
    {
        g_LastMotionAt = now;
        Room5_Update(now);
    }
    Doors_BeginSnapshot();
    Cabins_AnimateDoors();
    Room8_Update(now);
    FinalCube_Update(now);
    Room9_OnFrame(now);
    Room10_OnFrame(now);
    Room10Corridor_Update(now);
    Room11_OnFrame(now);
    Room11Flight_OnFrame(now);
    Room3_OnFrame();
    Doors_Frame();
    g_DoorSnapshotActive = false;
    return FMRES_IGNORED;
}

