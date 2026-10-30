// Gameplay settings; defaults preserve version 3.2.1.
// Internal include; compile adventure_trust.sma, not this file.

#define CABIN_COUNT 8
#define CABIN_PAIR_COUNT 4
#define TASK_ADVENTURE_POLL 85150
#define CABIN_CHOICE_GREEN 1
#define CABIN_CHOICE_RED 2
const Float:CABIN_CLOSE_DELAY = 3.0;
const Float:CABIN_CHOICE_SECONDS = 20.0;
const Float:CABIN_DOOR_TRAVEL = 112.0;
const Float:CABIN_DOOR_SPEED = 112.0;

const Float:WATERDROP_ACTIVE_SECONDS = 30.0;
const Float:WATERDROP_COOLDOWN_SECONDS = 300.0;
const Float:ADVENTURE_POLL_SECONDS = 0.1;
const Float:ADVENTURE_MOTION_SECONDS = 0.05;
const CABIN_GAS_TICKS = 7;
const Float:CABIN_GAS_DAMAGE = 15.0;
const Float:CABIN_GAS_INTERVAL = 1.0;
const Float:ROOM3_CRYSTAL_SECONDS = 20.0;
const Float:ROOM3_START_SECONDS = 3.0;
const Float:ROOM3_EMPTY_RESET_SECONDS = 3.0;
const Float:ROOM3_EXIT_SECONDS = 7.0;
const COLOR_ROUNDS = 4;
const Float:COLOR_DEPLOY_SECONDS = 1.0;
const Float:COLOR_BLANK_SECONDS = 2.0;
const Float:COLOR_SEARCH_SECONDS = 4.0;
const Float:COLOR_DANGER_SECONDS = 2.0;
const Float:COLOR_RETRACT_SECONDS = 1.0;
const Float:COLOR_BEEP_SECONDS = 1.0;
const Float:SPINNER_LOW_RISE_SECONDS = 0.5;
const Float:SPINNER_PREPARE_SECONDS = 4.0;
const Float:SPINNER_RISE_SECONDS = 1.0;
const Float:SPINNER_ACTIVE_SECONDS = 15.0;
const Float:SPINNER_LOWER_SECONDS = 1.0;
const Float:SPINNER_SPEED_MIN_SECONDS = 3.0;
const Float:SPINNER_SPEED_MAX_SECONDS = 5.0;
const Float:SPINNER_SPEED_STEP = 36.0;


// Game 3: warnings, descending ceiling and wall shelters.
const CEILING_ROUNDS = 3;
const Float:CEILING_WARNING_SECONDS = 3.0;
const Float:CEILING_DOWN_SECONDS = 5.0;
const Float:CEILING_HOLD_SECONDS = 2.0;
const Float:CEILING_UP_SECONDS = 2.0;
const Float:CEILING_LEAVE_SECONDS = 2.0;

// Game 4: full damage per unique beam contact.
const Float:LASER_DAMAGE = 100.0;
const Float:LASER_WARNING_SECONDS = 3.0;
const Float:LASER_WALL_STAGGER_SECONDS = 1.0;
const Float:LASER_PREVIEW_SECONDS = 1.0;
const Float:LASER_INTERVAL_MIN = 3.0;
const Float:LASER_INTERVAL_MAX = 5.0;
const Float:LASER_TRAVEL_SECONDS = 6.0;

// Game 5: falling blocks, test mode. Wave sizes and timing arrays live in blocks.inc.
const Float:BLOCK_DAMAGE = 100.0;
const Float:BLOCK_COUNTDOWN_SECONDS = 3.0;
const Float:BLOCK_FALL_SECONDS = 0.5;
const Float:BLOCK_FLASH_SECONDS = 0.1;
const Float:BLOCK_START_Z = -1064.0;
const Float:BLOCK_HEIGHT = 96.0;

// Stepping stones: ten cardinal transitions, first five get one extra second.
const ISLAND_TRANSITIONS = 10;
const Float:ISLAND_PREPARE_SECONDS = 5.0;
const Float:ISLAND_STEP_SECONDS = 3.0;
const Float:ISLAND_FINISH_SECONDS = 2.0;

// Scanning curtain: 150-unit opening; base 7/7/6/6/5/5 durations divided by 1.5, then minus one second.
const SCAN_WAVES = 6;
const Float:SCAN_WARNING_SECONDS = 3.0;
const Float:SCAN_PREVIEW_SECONDS = 2.0;
const Float:SCAN_GAP_HALF = 75.0;

// Meteors: radius 112, no homing, new arrivals at 7 and 15 seconds.
const Float:METEOR_RADIUS = 112.0;
const Float:METEOR_SPEED = 384.0;
const Float:METEOR_ACTIVE_SECONDS = 20.0;
