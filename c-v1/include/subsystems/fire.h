/*
 * fire.h - Fire, ember, and smoke physics
 *
 * Oxygen-based combustion system with Arrhenius kinetics
 */
#ifndef FIRE_H
#define FIRE_H

#include "core/types.h"
#include "world/world.h"
#include "engine/simulation.h"

/* =============================================================================
 * Fire and Gas Update System
 * 
 * Fire behavior:
 * - Burns fuel (wood, oil, coal), produces heat
 * - Consumes oxygen (fire dies in low-O2 environments)
 * - Spreads based on temperature and oxygen availability
 * - Produces smoke and embers
 * - Uses Arrhenius kinetics for realistic burn rates
 * 
 * Ember behavior:
 * - Smoldering remains of burned materials
 * - Can reignite with sufficient heat and oxygen
 * - Slowly cools and becomes ash
 * - Glows based on temperature
 * 
 * Smoke/Gas behavior:
 * - Rises upward (opposite of gravity)
 * - Spreads horizontally when blocked
 * - Dissipates over time
 * ============================================================================= */

/* Main fire update function (includes ember updates) */
void fire_update(Simulation* sim, World* world);

/* Main gas/smoke update function */
void gas_update(Simulation* sim, World* world);

/* Oxygen diffusion and replenishment */
void oxygen_update(World* world);

/* Update a single fire cell */
bool fire_update_cell(Simulation* sim, World* world, int x, int y);

/* Update a single ember cell */
bool ember_update_cell(Simulation* sim, World* world, int x, int y);

/* Update a single gas/smoke cell */
bool gas_update_cell(Simulation* sim, World* world, int x, int y);

/* Try to ignite a cell (if flammable and has oxygen) */
bool fire_try_ignite(World* world, int x, int y);

/* Check if material is flammable */
bool material_is_flammable(MaterialID mat);

/* Get fire color based on lifetime (for animated fire) */
Color fire_get_color(uint8_t lifetime);

/* Get ember color based on temperature (glowing effect) */
Color ember_get_color(float temp);

#endif /* FIRE_H */
