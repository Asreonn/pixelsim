/*
 * buoyancy.h - Buoyancy and Convection System
 * 
 * Implements:
 * - Archimedes' principle for density-based movement
 * - Natural convection (hot air rises)
 * - Temperature-dependent density
 */
#ifndef BUOYANCY_H
#define BUOYANCY_H

#include "core/types.h"
#include "world/world.h"
#include "engine/simulation.h"
#include "physics/thermal.h"  /* For AMBIENT_TEMP */

/* =============================================================================
 * Buoyancy Constants
 * ============================================================================= */

#define BUOYANCY_MIN_THRESHOLD  0.0005f     /* Minimum density diff for movement */
#define CONVECTION_VELOCITY     0.5f        /* Base convection speed */
#define DENSITY_SWAP_CHANCE     0.8f        /* Probability of density-based swap */

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

/* Main buoyancy and convection update */
void buoyancy_update(Simulation* sim, World* world);

/* =============================================================================
 * Density Calculations
 * ============================================================================= */

/* Update effective density for all cells (call once per tick) */
void buoyancy_update_densities(World* world);

/* Calculate effective density considering temperature */
float buoyancy_calc_effective_density(MaterialID mat, float temp);

/* Get average density of surrounding cells */
float buoyancy_get_surrounding_density(const World* world, int x, int y);

/* =============================================================================
 * Buoyancy Force
 * ============================================================================= */

/* Calculate buoyancy acceleration for a cell */
/* Positive = upward (rises), Negative = downward (sinks) */
float buoyancy_calc_acceleration(const World* world, int x, int y);

/* Check if cell should rise due to buoyancy */
bool buoyancy_should_rise(const World* world, int x, int y);

/* Check if cell should sink due to buoyancy */
bool buoyancy_should_sink(const World* world, int x, int y);

/* =============================================================================
 * Convection Movement
 * ============================================================================= */

/* Try to move cell up/down based on buoyancy */
bool buoyancy_try_move(Simulation* sim, World* world, int x, int y);

/* Swap cells if density difference warrants it */
bool buoyancy_try_density_swap(Simulation* sim, World* world, int x1, int y1, int x2, int y2);

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

/* Check if material is affected by buoyancy */
static inline bool buoyancy_affects(MaterialID mat) {
    /* Fire has its own movement logic in fire.c (gas_try_move), 
     * so exclude it from buoyancy to prevent double-update conflicts */
    if (mat == MAT_FIRE) return false;
    
    MaterialState state = material_state(mat);
    return state == STATE_GAS || state == STATE_FLUID;
}

/* Check if position can participate in buoyancy exchange */
static inline bool buoyancy_can_exchange(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    MaterialID mat = world->mat[IDX(x, y)];
    MaterialState state = material_state(mat);
    return state == STATE_GAS || state == STATE_FLUID;
}

#endif /* BUOYANCY_H */
