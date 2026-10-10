/*
 * types.h - Core types and constants for Pixel-Cell Physics Simulator
 */
#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* =============================================================================
 * Configuration Constants
 * ============================================================================= */

/* Grid dimensions (can be changed at compile time) */
#ifndef GRID_WIDTH
#define GRID_WIDTH 512
#endif

#ifndef GRID_HEIGHT
#define GRID_HEIGHT 512
#endif

#define GRID_SIZE (GRID_WIDTH * GRID_HEIGHT)

/* Window dimensions (pixels) */
#define WINDOW_WIDTH  GRID_WIDTH
#define WINDOW_HEIGHT GRID_HEIGHT

/* Simulation tick rate (Hz) - decoupled from FPS */
#define TICK_HZ 120
#define TICK_DT (1.0 / TICK_HZ)

/* Game time multiplier for accelerated simulation
 * Lower values = slower, more realistic heat propagation
 * Higher values = faster, more arcade-like feel */
#define GAME_TIME_MULTIPLIER 10.0f  /* Reduced from 60 for visible heat propagation */

/* Target frame rate for rendering */
#define TARGET_FPS 144
#define TARGET_FRAME_TIME (1.0 / TARGET_FPS)

/* Gravity acceleration in cells/tick^2 (scaled from 9.81 m/s^2) */
#define GRAVITY_ACCEL 0.08f

/* Chunk size for dirty region tracking */
#define CHUNK_SIZE 32
#define CHUNKS_X ((GRID_WIDTH + CHUNK_SIZE - 1) / CHUNK_SIZE)
#define CHUNKS_Y ((GRID_HEIGHT + CHUNK_SIZE - 1) / CHUNK_SIZE)
#define CHUNK_COUNT (CHUNKS_X * CHUNKS_Y)

/* =============================================================================
 * Physics Constants (Engineering Level)
 * ============================================================================= */

/* Universal constants */
#define STEFAN_BOLTZMANN    5.67e-8f    /* W/(m^2*K^4) */
#define GAS_CONSTANT        8.314f      /* J/(mol*K) */
#define GRAVITY_MPS2        9.81f       /* m/s^2 */
#define ATM_PRESSURE        101325.0f   /* Pa (1 atm) */
#define AIR_DENSITY         1.225f      /* kg/m^3 at sea level, 20C */

/* Simulation scaling */
#define CELL_SIZE_M         0.01f       /* 1 cm per cell */
#define CELL_VOLUME_M3      1.0e-6f     /* 1 cm^3 */
#define CELL_AREA_M2        1.0e-4f     /* 1 cm^2 */
#define TIME_SCALE          0.5f        /* Heat time per tick (scaled by GAME_TIME_MULTIPLIER) */
#define HEAT_TIME_SCALE     (TIME_SCALE * GAME_TIME_MULTIPLIER)  /* Effective: 30 seconds of heat per tick */

/* Temperature conversion macros */
#define C_TO_K(c)           ((c) + 273.15f)
#define K_TO_C(k)           ((k) - 273.15f)

/* Physics thresholds */
#define BUOYANCY_THRESHOLD      0.001f      /* m/s^2 min for movement */
#define CONVECTION_STRENGTH     0.5f        /* Convection multiplier */
#define RADIATION_SCALE         1.0e-8f     /* Stefan-Boltzmann scaling */
#define PRESSURE_THRESHOLD      100.0f      /* Pa min for gas movement */
#define PRESSURE_MOVE_FACTOR    0.001f      /* Pressure->velocity factor */
#define OXYGEN_MIN_COMBUSTION   0.05f       /* Min O2 for burning */
#define OXYGEN_DIFFUSION_RATE   0.1f        /* O2 spread rate */
#define OXYGEN_REPLENISH_RATE   0.001f      /* Atmospheric O2 restore */
#define ARRHENIUS_A             1.0e8f      /* Pre-exponential factor */
#define ENERGY_TOLERANCE        1000.0f     /* J - conservation error limit */

/* =============================================================================
 * Cell Flags (per-cell overlay states)
 * ============================================================================= */

typedef uint16_t CellFlags;

#define FLAG_NONE           0x0000
#define FLAG_UPDATED        0x0001  /* Cell was updated this tick (prevent double-update) */
#define FLAG_STATIC         0x0002  /* Cell is static/immovable */
#define FLAG_BURNING        0x0004  /* Cell is on fire */
#define FLAG_WET            0x0008  /* Cell is wet */
#define FLAG_HOT            0x0010  /* Cell is hot */
#define FLAG_ACTIVE         0x0020  /* Cell is active (needs processing) */
#define FLAG_CORRODING      0x0040  /* Cell is being corroded by acid */
#define FLAG_FROZEN         0x0080  /* Cell is frozen */
#define FLAG_MELTING        0x0100  /* Cell is in melting phase transition */
#define FLAG_BOILING        0x0200  /* Cell is in boiling phase transition */
#define FLAG_GLOWING        0x0400  /* Cell is hot enough to glow */

/* =============================================================================
 * Material States
 * ============================================================================= */

typedef enum {
    STATE_GAS = 0,      /* Gases rise, fill space */
    STATE_FLUID,        /* Liquids flow, fall */
    STATE_POWDER,       /* Powders pile, slide */
    STATE_SOLID         /* Solids are immovable */
} MaterialState;

/* =============================================================================
 * Material IDs
 * 
 * Note: MAT_EMPTY replaced with MAT_AIR
 *       MAT_ASH replaced with MAT_EMBER
 * ============================================================================= */

typedef uint8_t MaterialID;

#define MAT_AIR         0   /* Atmospheric air (was EMPTY) */
#define MAT_SAND        1
#define MAT_STONE       2
#define MAT_WATER       3
#define MAT_WOOD        4
#define MAT_FIRE        5
#define MAT_SMOKE       6
#define MAT_SOIL        7
#define MAT_ICE         8
#define MAT_STEAM       9
#define MAT_EMBER       10  /* Hot remnant of burned material (was ASH) */
#define MAT_ACID        11
#define MAT_METAL       12
#define MAT_MUD         13
#define MAT_LAVA        14
#define MAT_OIL         15
#define MAT_WET_WOOD    16
#define MAT_GLASS       17  /* NEW - formed from melted sand */
#define MAT_OBSIDIAN    18  /* NEW - formed from water + lava */
#define MAT_COAL        19  /* NEW - slow burning fuel */
#define MAT_SALT        20  /* NEW - soluble, lowers freezing point */
#define MAT_MOLTEN_METAL 21 /* NEW - melted metal */

#define MAT_COUNT       22  /* Total number of materials */

/* Backwards compatibility aliases */
#define MAT_EMPTY       MAT_AIR
#define MAT_ASH         MAT_EMBER

/* =============================================================================
 * Color Types
 * ============================================================================= */

typedef struct {
    uint8_t r, g, b, a;
} Color;

/* =============================================================================
 * Fixed-Point Types (8.8)
 * ============================================================================= */

typedef int16_t Fixed8;

#define FIXED_SHIFT 8
#define FIXED_ONE (1 << FIXED_SHIFT)
#define FIXED_FROM_FLOAT(x) ((Fixed8)((x) * (float)FIXED_ONE))
#define FIXED_TO_FLOAT(x) ((float)(x) / (float)FIXED_ONE)
#define FIXED_MUL(a, b) ((Fixed8)(((int32_t)(a) * (int32_t)(b)) >> FIXED_SHIFT))
#define FIXED_ABS(x) ((x) < 0 ? -(x) : (x))

/* =============================================================================
 * Utility Macros
 * ============================================================================= */

/* Convert 2D coordinates to 1D index */
#define IDX(x, y) ((y) * GRID_WIDTH + (x))

/* Check if coordinates are within grid bounds */
#define IN_BOUNDS(x, y) ((x) >= 0 && (x) < GRID_WIDTH && (y) >= 0 && (y) < GRID_HEIGHT)

/* Get chunk index from cell coordinates */
#define CHUNK_IDX(x, y) (((y) / CHUNK_SIZE) * CHUNKS_X + ((x) / CHUNK_SIZE))

/* Min/Max macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) (MIN(MAX(x, lo), hi))

/* =============================================================================
 * Neighbor Direction Arrays
 * ============================================================================= */

/* 4-directional neighbors (NSEW) */
static const int NEIGHBOR4_DX[4] = { 0, 0, -1, 1 };
static const int NEIGHBOR4_DY[4] = { -1, 1, 0, 0 };

/* 8-directional neighbors (includes diagonals) */
static const int NEIGHBOR8_DX[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
static const int NEIGHBOR8_DY[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };

#endif /* TYPES_H */
