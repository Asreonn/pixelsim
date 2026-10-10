/*
 * world.h - Grid/World model with SoA (Structure of Arrays) layout
 * 
 * Enhanced for engineering-level physics simulation with:
 * - Oxygen tracking for combustion
 * - Energy grid for thermal simulation
 * - Phase transition tracking
 * - Effective density for buoyancy
 */
#ifndef WORLD_H
#define WORLD_H

#include "core/types.h"
#include "materials/material.h"

/* =============================================================================
 * World State Structure (SoA layout for performance)
 * ============================================================================= */

typedef struct {
    /* =========================================================================
     * Primary Material Grid
     * ========================================================================= */
    MaterialID* mat;          /* Current frame material */
    MaterialID* mat_next;     /* Double buffer for next frame */
    
    /* =========================================================================
     * Per-Cell Flags and Metadata
     * ========================================================================= */
    CellFlags* flags;         /* Per-cell state flags */
    uint32_t* color_seed;     /* Color variation seed */
    uint8_t* lifetime;        /* Particle age (fire, smoke) */
    
    /* =========================================================================
     * Thermal System
     * ========================================================================= */
    float* temp;              /* Temperature (Celsius) - current */
    float* temp_next;         /* Temperature - next frame buffer */
    float* energy;            /* Thermal energy (Joules) */
    float* phase_progress;    /* Phase transition progress (0-1) */
    
    /* =========================================================================
     * Pressure and Density
     * ========================================================================= */
    float* pressure;          /* Pressure (Pascals) */
    float* density;           /* Base density from material */
    float* effective_density; /* Temperature-adjusted density */
    
    /* =========================================================================
     * Chemical Properties
     * ========================================================================= */
    float* oxygen;            /* O2 level (0-1, 0.21 = atmospheric) */
    float* fuel;              /* Remaining fuel for combustion */
    float* reaction;          /* Reaction progress (corrosion, etc.) */
    
    /* =========================================================================
     * Fluid Properties
     * ========================================================================= */
    float* moisture;          /* Moisture content (0-1) */
    float* fill;              /* Fill fraction for fluids (0-1) */
    
    /* =========================================================================
     * Velocity (Fixed-point 8.8)
     * ========================================================================= */
    Fixed8* vel_x;
    Fixed8* vel_y;
    
    /* =========================================================================
     * Chunk Activation Tracking
     * ========================================================================= */
    bool* chunk_active;
    bool* chunk_active_next;
    
    /* =========================================================================
     * Grid Dimensions
     * ========================================================================= */
    int width;
    int height;
    
    /* =========================================================================
     * Statistics
     * ========================================================================= */
    uint32_t cells_updated;
    uint32_t active_chunks;
    
    /* Energy tracking for conservation checks */
    double total_thermal_energy;
    double total_chemical_energy;
    
} World;

/* =============================================================================
 * World Lifecycle
 * ============================================================================= */

/* Create and initialize a new world */
World* world_create(int width, int height);

/* Destroy and free world resources */
void world_destroy(World* world);

/* Clear the entire world to air */
void world_clear(World* world);

/* Reset world to initial state (air everywhere, ambient temp) */
void world_reset(World* world);

/* =============================================================================
 * Material Access
 * ============================================================================= */

/* Get material at position (returns MAT_AIR if out of bounds) */
MaterialID world_get_mat(const World* world, int x, int y);

/* Set material at position (no-op if out of bounds) */
void world_set_mat(World* world, int x, int y, MaterialID mat);

/* Set material in the next buffer */
void world_set_mat_next(World* world, int x, int y, MaterialID mat);

/* =============================================================================
 * Flag Operations
 * ============================================================================= */

CellFlags world_get_flags(const World* world, int x, int y);
void world_set_flags(World* world, int x, int y, CellFlags flags);
void world_add_flag(World* world, int x, int y, CellFlags flag);
void world_remove_flag(World* world, int x, int y, CellFlags flag);
bool world_has_flag(const World* world, int x, int y, CellFlags flag);

/* =============================================================================
 * State Queries
 * ============================================================================= */

/* Check if position is air/empty */
bool world_is_empty(const World* world, int x, int y);
bool world_is_air(const World* world, int x, int y);

/* Check if position is solid (blocking) */
bool world_is_solid(const World* world, int x, int y);

/* =============================================================================
 * Cell Operations
 * ============================================================================= */

/* Swap cell contents (for movement) - swaps ALL properties */
void world_swap_cells(World* world, int x1, int y1, int x2, int y2);

/* Copy cell to another location */
void world_copy_cell(World* world, int src_x, int src_y, int dst_x, int dst_y);

/* =============================================================================
 * Temperature Operations
 * ============================================================================= */

/* Get temperature at position (Celsius) */
static inline float world_get_temp(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return 20.0f; /* Ambient */
    return world->temp[IDX(x, y)];
}

/* Set temperature at position */
static inline void world_set_temp(World* world, int x, int y, float temp) {
    if (!IN_BOUNDS(x, y)) return;
    world->temp[IDX(x, y)] = temp;
}

/* Add heat to cell (in Joules) */
void world_add_heat(World* world, int x, int y, float joules);

/* =============================================================================
 * Oxygen Operations
 * ============================================================================= */

/* Get oxygen level (0-1) */
static inline float world_get_oxygen(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return 0.21f; /* Atmospheric */
    return world->oxygen[IDX(x, y)];
}

/* Set oxygen level */
static inline void world_set_oxygen(World* world, int x, int y, float level) {
    if (!IN_BOUNDS(x, y)) return;
    world->oxygen[IDX(x, y)] = CLAMP(level, 0.0f, 1.0f);
}

/* Consume oxygen (for combustion) */
static inline void world_consume_oxygen(World* world, int x, int y, float amount) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);
    world->oxygen[idx] = MAX(0.0f, world->oxygen[idx] - amount);
}

/* =============================================================================
 * Pressure Operations
 * ============================================================================= */

static inline float world_get_pressure(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return ATM_PRESSURE;
    return world->pressure[IDX(x, y)];
}

static inline void world_set_pressure(World* world, int x, int y, float pressure) {
    if (!IN_BOUNDS(x, y)) return;
    world->pressure[IDX(x, y)] = pressure;
}

/* =============================================================================
 * Effective Density (for buoyancy)
 * ============================================================================= */

static inline float world_get_effective_density(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return AIR_DENSITY;
    return world->effective_density[IDX(x, y)];
}

/* Update effective density based on temperature */
void world_update_effective_density(World* world, int x, int y);

/* Update all effective densities */
void world_update_all_densities(World* world);

/* =============================================================================
 * Phase Transition
 * ============================================================================= */

static inline float world_get_phase_progress(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return 0.0f;
    return world->phase_progress[IDX(x, y)];
}

static inline void world_set_phase_progress(World* world, int x, int y, float progress) {
    if (!IN_BOUNDS(x, y)) return;
    world->phase_progress[IDX(x, y)] = CLAMP(progress, 0.0f, 1.0f);
}

/* =============================================================================
 * Chunk Management
 * ============================================================================= */

void world_activate_chunk(World* world, int chunk_x, int chunk_y);
void world_activate_chunk_at(World* world, int x, int y);
bool world_is_chunk_active(const World* world, int chunk_x, int chunk_y);
void world_swap_buffers(World* world);
void world_clear_tick_flags(World* world);
void world_clear_chunk_activation(World* world);
void world_update_chunk_activation(World* world);

/* =============================================================================
 * Painting / Drawing
 * ============================================================================= */

void world_paint_circle(World* world, int cx, int cy, int radius, MaterialID mat);
void world_paint_line(World* world, int x0, int y0, int x1, int y1, int radius, MaterialID mat);

/* Paint with specific temperature */
void world_paint_circle_temp(World* world, int cx, int cy, int radius, MaterialID mat, float temp);

/* =============================================================================
 * Rendering Support
 * ============================================================================= */

Color world_get_cell_color(const World* world, int x, int y);

/* Get color with temperature-based effects */
Color world_get_cell_color_thermal(const World* world, int x, int y);

/* =============================================================================
 * Energy Conservation (Debug/Validation)
 * ============================================================================= */

/* Calculate total thermal energy in world */
double world_calculate_total_energy(const World* world);

/* Validate energy conservation */
bool world_validate_energy(const World* world, double expected, double tolerance);

#endif /* WORLD_H */
