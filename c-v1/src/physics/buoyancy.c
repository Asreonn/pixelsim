/*
 * buoyancy.c - Buoyancy and Convection Implementation
 * 
 * Implements Archimedes' principle and natural convection
 */
#include "physics/buoyancy.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "world/grid_iter.h"
#include <math.h>

/* =============================================================================
 * Density Calculations
 * ============================================================================= */

float buoyancy_calc_effective_density(MaterialID mat, float temp) {
    const MaterialProps* props = material_get(mat);
    
    /* Thermal expansion: ρ(T) = ρ₀ / (1 + β × ΔT) */
    float dT = temp - AMBIENT_TEMP;
    float expansion = 1.0f + props->thermal_expansion * dT;
    
    if (expansion > 0.01f) {
        return props->density / expansion;
    }
    return props->density;
}

void buoyancy_update_densities(World* world) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = IDX(x, y);
            MaterialID mat = world->mat[idx];
            float temp = world->temp[idx];
            
            world->effective_density[idx] = buoyancy_calc_effective_density(mat, temp);
        }
    }
}

float buoyancy_get_surrounding_density(const World* world, int x, int y) {
    float total = 0.0f;
    int count = 0;
    
    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];
        
        if (IN_BOUNDS(nx, ny)) {
            int nidx = IDX(nx, ny);
            total += world->effective_density[nidx];
            count++;
        }
    }
    
    return (count > 0) ? (total / count) : AIR_DENSITY;
}

/* =============================================================================
 * Buoyancy Force Calculation
 * ============================================================================= */

float buoyancy_calc_acceleration(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return 0.0f;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    
    /* Only fluids and gases experience buoyancy */
    if (!buoyancy_affects(mat)) return 0.0f;
    
    float my_density = world->effective_density[idx];
    float surrounding_density = buoyancy_get_surrounding_density(world, x, y);
    
    /* Archimedes: F = (ρ_fluid - ρ_particle) × V × g */
    /* Acceleration: a = F/m = (ρ_fluid - ρ_particle) × g / ρ_particle */
    
    if (my_density < 0.001f) return 0.0f;
    
    float accel = (surrounding_density - my_density) * GRAVITY_MPS2 / my_density;
    
    /* Scale for simulation */
    return accel * CONVECTION_VELOCITY;
}

bool buoyancy_should_rise(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    
    if (!buoyancy_affects(mat)) return false;
    
    float my_density = world->effective_density[idx];
    
    /* Check cell above */
    if (y > 0) {
        int above_idx = IDX(x, y - 1);
        float above_density = world->effective_density[above_idx];
        
        /* Rise if we're lighter than what's above */
        return (above_density - my_density) > BUOYANCY_MIN_THRESHOLD;
    }
    
    return false;
}

bool buoyancy_should_sink(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    
    if (!buoyancy_affects(mat)) return false;
    
    float my_density = world->effective_density[idx];
    
    /* Check cell below */
    if (y < GRID_HEIGHT - 1) {
        int below_idx = IDX(x, y + 1);
        float below_density = world->effective_density[below_idx];
        
        /* Sink if we're heavier than what's below */
        return (my_density - below_density) > BUOYANCY_MIN_THRESHOLD;
    }
    
    return false;
}

/* =============================================================================
 * Convection Movement
 * ============================================================================= */

bool buoyancy_try_density_swap(Simulation* sim, World* world, int x1, int y1, int x2, int y2) {
    if (!IN_BOUNDS(x1, y1) || !IN_BOUNDS(x2, y2)) return false;
    
    int idx1 = IDX(x1, y1);
    int idx2 = IDX(x2, y2);
    
    /* Both must be gas or fluid */
    if (!buoyancy_can_exchange(world, x1, y1)) return false;
    if (!buoyancy_can_exchange(world, x2, y2)) return false;
    
    float density1 = world->effective_density[idx1];
    float density2 = world->effective_density[idx2];
    
    /* Check if density difference is significant */
    float diff = density1 - density2;
    
    /* If cell 1 is above cell 2, cell 1 should be lighter */
    if (y1 < y2) {
        /* Cell 1 is above, should be lighter */
        if (diff > BUOYANCY_MIN_THRESHOLD) {
            /* Cell 1 is heavier - swap */
            if (simulation_randf(sim) < DENSITY_SWAP_CHANCE) {
                world_swap_cells(world, x1, y1, x2, y2);
                return true;
            }
        }
    } else if (y1 > y2) {
        /* Cell 1 is below, should be heavier */
        if (diff < -BUOYANCY_MIN_THRESHOLD) {
            /* Cell 1 is lighter - swap */
            if (simulation_randf(sim) < DENSITY_SWAP_CHANCE) {
                world_swap_cells(world, x1, y1, x2, y2);
                return true;
            }
        }
    }
    
    return false;
}

bool buoyancy_try_move(Simulation* sim, World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    
    if (!buoyancy_affects(mat)) return false;
    
    float my_density = world->effective_density[idx];
    
    /* Try to rise */
    if (y > 0) {
        int above_idx = IDX(x, y - 1);
        MaterialID above_mat = world->mat[above_idx];
        
        if (buoyancy_can_exchange(world, x, y - 1)) {
            float above_density = world->effective_density[above_idx];
            
            /* Rise if lighter */
            if ((above_density - my_density) > BUOYANCY_MIN_THRESHOLD) {
                float rise_chance = (above_density - my_density) / above_density;
                rise_chance = CLAMP(rise_chance, 0.1f, DENSITY_SWAP_CHANCE);
                
                if (simulation_randf(sim) < rise_chance) {
                    world_swap_cells(world, x, y, x, y - 1);
                    return true;
                }
            }
        }
    }
    
    /* Try to sink */
    if (y < GRID_HEIGHT - 1) {
        int below_idx = IDX(x, y + 1);
        
        if (buoyancy_can_exchange(world, x, y + 1)) {
            float below_density = world->effective_density[below_idx];
            
            /* Sink if heavier */
            if ((my_density - below_density) > BUOYANCY_MIN_THRESHOLD) {
                float sink_chance = (my_density - below_density) / my_density;
                sink_chance = CLAMP(sink_chance, 0.1f, DENSITY_SWAP_CHANCE);
                
                if (simulation_randf(sim) < sink_chance) {
                    world_swap_cells(world, x, y, x, y + 1);
                    return true;
                }
            }
        }
    }
    
    /* Try diagonal movement for convection currents */
    if (simulation_randf(sim) < 0.3f) {
        int dx = (simulation_randf(sim) < 0.5f) ? -1 : 1;
        int dy = (buoyancy_should_rise(world, x, y)) ? -1 : 1;
        
        if (buoyancy_can_exchange(world, x + dx, y + dy)) {
            float other_density = world->effective_density[IDX(x + dx, y + dy)];
            
            bool should_swap = false;
            if (dy < 0 && my_density < other_density) should_swap = true;
            if (dy > 0 && my_density > other_density) should_swap = true;
            
            if (should_swap) {
                world_swap_cells(world, x, y, x + dx, y + dy);
                return true;
            }
        }
    }
    
    return false;
}

/* =============================================================================
 * Grid Iteration Callback
 * ============================================================================= */

static bool buoyancy_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    
    /* Skip if already updated this tick */
    if (cell_skip_if_updated(world, x, y)) return true;
    
    /* Try buoyancy movement */
    if (buoyancy_try_move(sim, world, x, y)) {
        cell_mark_updated(world, x, y);
    }
    
    return true;
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void buoyancy_update(Simulation* sim, World* world) {
    /* First, update all effective densities */
    buoyancy_update_densities(world);
    
    /* Process from bottom to top (so rising happens correctly) */
    grid_iterate_rising(sim, world, buoyancy_cell_callback, NULL);
}
