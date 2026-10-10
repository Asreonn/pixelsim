/*
 * fluid.c - Fluid Physics Implementation (water, oil, lava, acid, molten metal)
 *
 * Implements fluid behavior using the modular system:
 *   - cell_ops.h for movement operations
 *   - physics.h for gravity and pressure
 *   - behavior.h for material classification
 *   - grid_iter.h for iteration patterns
 *   - Density-based displacement (buoyancy)
 *   - Temperature-dependent density
 */
#include "subsystems/fluid.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "physics/physics.h"
#include "physics/thermal.h"
#include "materials/behavior.h"
#include "world/grid_iter.h"
#include <stdlib.h>

/* =============================================================================
 * Fluid Configuration
 * ============================================================================= */

#define FLUID_DISPERSION_PASSES  2       /* Number of update passes */
#define FLUID_PRESSURE_THRESHOLD 1       /* Minimum pressure diff to flow */
#define FLUID_PRESSURE_CHANCE    0.3f    /* Chance to equalize pressure */
#define WATER_LATERAL_BIAS       0.45f   /* Water prefers sideways before falling */
#define WATER_SPLIT_CHANCE       0.15f   /* Chance water breaks into a droplet */
#define MAX_WET_DEPTH            8       /* Max wetting radius */
#define WET_DIAMETER             (MAX_WET_DEPTH * 2 + 1)
#define WET_MAX_CELLS            (WET_DIAMETER * WET_DIAMETER)
#define WET_RADIUS_SQ            (MAX_WET_DEPTH * MAX_WET_DEPTH)

/* Buoyancy configuration */
#define BUOYANCY_SWAP_CHANCE     0.4f    /* Chance for density-based swap */
#define DENSITY_DIFF_THRESHOLD   50.0f   /* Minimum density difference for swap */

/* =============================================================================
 * Movement Helpers
 * ============================================================================= */

bool fluid_can_move_to(const World* world, int x, int y) {
    return cell_fluid_can_enter(world, x, y);
}

static bool fluid_has_adjacent_except(const World* world, int x, int y, MaterialID mat,
                                      int ex, int ey) {
    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];
        if (nx == ex && ny == ey) continue;
        if (!IN_BOUNDS(nx, ny)) continue;
        if (world_get_mat(world, nx, ny) == mat) {
            return true;
        }
    }
    return false;
}

/* =============================================================================
 * Density-Based Displacement (Buoyancy)
 * ============================================================================= */

/*
 * Check if this fluid should swap with the cell below based on density.
 * Lighter fluids rise, heavier fluids sink.
 * Uses effective_density which accounts for temperature.
 */
static bool fluid_should_swap_down(World* world, int x, int y, Simulation* sim) {
    if (!IN_BOUNDS(x, y + 1)) return false;
    
    int idx = IDX(x, y);
    int idx_below = IDX(x, y + 1);
    
    MaterialID mat = world->mat[idx];
    MaterialID mat_below = world->mat[idx_below];
    
    /* Can only swap with another fluid */
    if (!material_is_fluid(mat_below)) return false;
    
    /* Compare effective densities (temperature-adjusted) */
    float my_density = world->effective_density[idx];
    float below_density = world->effective_density[idx_below];
    
    /* I'm heavier - I should sink */
    if (my_density > below_density + DENSITY_DIFF_THRESHOLD) {
        if (simulation_randf(sim) < BUOYANCY_SWAP_CHANCE) {
            return true;
        }
    }
    
    return false;
}

/*
 * Check if this fluid should swap with the cell above based on density.
 * This makes lighter fluids rise through heavier ones.
 */
static bool fluid_should_swap_up(World* world, int x, int y, Simulation* sim) {
    if (!IN_BOUNDS(x, y - 1)) return false;
    
    int idx = IDX(x, y);
    int idx_above = IDX(x, y - 1);
    
    MaterialID mat = world->mat[idx];
    MaterialID mat_above = world->mat[idx_above];
    
    /* Can only swap with another fluid */
    if (!material_is_fluid(mat_above)) return false;
    
    /* Compare effective densities (temperature-adjusted) */
    float my_density = world->effective_density[idx];
    float above_density = world->effective_density[idx_above];
    
    /* I'm lighter - I should rise */
    if (my_density + DENSITY_DIFF_THRESHOLD < above_density) {
        if (simulation_randf(sim) < BUOYANCY_SWAP_CHANCE) {
            return true;
        }
    }
    
    return false;
}

/* =============================================================================
 * Fluid Temperature Effects
 * ============================================================================= */

/* NOTE: Phase transitions (freezing, boiling, melting) are now handled by
 * thermal.c:thermal_check_phase_change() to avoid duplicate processing.
 * This ensures consistent temperature-based state changes across all materials. */

/* =============================================================================
 * Cell Update Logic
 * ============================================================================= */

bool fluid_update_cell(Simulation* sim, World* world, int x, int y) {
    if (cell_skip_if_updated(world, x, y)) {
        return false;
    }

    MaterialID mat = world_get_mat(world, x, y);

    if (!material_is_fluid(mat)) {
        return false;
    }

    const MaterialProps* props = material_get(mat);
    int idx = IDX(x, y);

    /* NOTE: Temperature-based state changes (freezing, boiling, etc.) are now
     * handled by thermal.c:thermal_check_phase_change() for consistency */

    /* =========================================================================
     * Apply Gravity
     * ========================================================================= */
    phys_apply_gravity_fixed(world, x, y, props);

    Fixed8 vy = world->vel_y[idx];

    MovementSteps steps = phys_calc_fall_steps(world, x, y, 2);

    int new_x = x, new_y = y;
    bool moved = false;

    /* =========================================================================
     * Density-Based Displacement (Buoyancy)
     * ========================================================================= */
    /* Heavy fluid sinks through lighter fluid */
    if (fluid_should_swap_down(world, x, y, sim)) {
        world_swap_cells(world, x, y, x, y + 1);
        cell_mark_updated(world, x, y);
        cell_mark_updated(world, x, y + 1);
        return true;
    }
    
    /* Light fluid rises through heavier fluid */
    if (fluid_should_swap_up(world, x, y, sim)) {
        world_swap_cells(world, x, y, x, y - 1);
        cell_mark_updated(world, x, y);
        cell_mark_updated(world, x, y - 1);
        return true;
    }

    /* =========================================================================
     * Water Interactions
     * ========================================================================= */
    if (mat == MAT_WATER) {
        MaterialID wet_mat = MAT_AIR;
        MaterialID dry_a = MAT_AIR;
        MaterialID dry_b = MAT_AIR;
        bool has_dry_b = false;

        /* Determine if we are touching an absorbent material */
        for (int i = 0; i < 4; i++) {
            int nx = x + NEIGHBOR4_DX[i];
            int ny = y + NEIGHBOR4_DY[i];
            if (!IN_BOUNDS(nx, ny)) continue;

            MaterialID neighbor = world_get_mat(world, nx, ny);
            if (neighbor == MAT_WOOD || neighbor == MAT_WET_WOOD) {
                wet_mat = MAT_WET_WOOD;
                dry_a = MAT_WOOD;
                break;
            }
            if (neighbor == MAT_SAND || neighbor == MAT_SOIL || neighbor == MAT_MUD) {
                wet_mat = MAT_MUD;
                dry_a = MAT_SAND;
                dry_b = MAT_SOIL;
                has_dry_b = true;
                break;
            }
        }

        if (wet_mat != MAT_AIR) {
            /* BFS radial spread from contact point */
            int qx[WET_MAX_CELLS];
            int qy[WET_MAX_CELLS];
            int head = 0;
            int tail = 0;

            qx[tail] = x;
            qy[tail] = y;
            tail++;

            bool visited[WET_MAX_CELLS];
            for (int i = 0; i < WET_MAX_CELLS; i++) {
                visited[i] = false;
            }

            while (head < tail) {
                int cx = qx[head];
                int cy = qy[head];
                head++;

                int dx = cx - x;
                int dy = cy - y;
                if (dx * dx + dy * dy > WET_RADIUS_SQ) {
                    continue;
                }

                for (int n = 0; n < 4; n++) {
                    int nx = cx + NEIGHBOR4_DX[n];
                    int ny = cy + NEIGHBOR4_DY[n];
                    if (!IN_BOUNDS(nx, ny)) continue;

                    int vx = nx - (x - MAX_WET_DEPTH);
                    int local_vy = ny - (y - MAX_WET_DEPTH);
                    int vidx = local_vy * WET_DIAMETER + vx;
                    if (vidx < 0 || vidx >= WET_MAX_CELLS) {
                        continue;
                    }
                    if (visited[vidx]) {
                        continue;
                    }
                    visited[vidx] = true;

                    MaterialID neighbor = world_get_mat(world, nx, ny);
                    if (neighbor == wet_mat) {
                        qx[tail] = nx;
                        qy[tail] = ny;
                        tail++;
                        continue;
                    }
                    if (neighbor == dry_a || (has_dry_b && neighbor == dry_b)) {
                        world_set_mat(world, nx, ny, wet_mat);
                        world_set_mat(world, x, y, MAT_AIR);
                        world->cells_updated++;
                        return true;
                    }
                }
            }
        }

        /* Check neighbors for interactions */
        for (int i = 0; i < 4; i++) {
            int nx = x + NEIGHBOR4_DX[i];
            int ny = y + NEIGHBOR4_DY[i];
            if (!IN_BOUNDS(nx, ny)) continue;
            
            MaterialID neighbor = world_get_mat(world, nx, ny);
            int nidx = IDX(nx, ny);
            
            /* Water + Lava = Steam + Obsidian (water evaporates, lava cools) */
            if (neighbor == MAT_LAVA && simulation_randf(sim) < 0.5f) {
                world_set_mat(world, x, y, MAT_STEAM);
                world->lifetime[idx] = 0;
                world->temp[nidx] -= 200.0f;  /* Cool the lava */
                /* Lava solidifies if cooled enough */
                if (world->temp[nidx] < 700.0f) {
                    world_set_mat(world, nx, ny, MAT_OBSIDIAN);
                }
                world->cells_updated++;
                return true;
            }
            
            /* Water + Molten Metal = Steam + Metal */
            if (neighbor == MAT_MOLTEN_METAL && simulation_randf(sim) < 0.3f) {
                world_set_mat(world, x, y, MAT_STEAM);
                world->lifetime[idx] = 0;
                world->temp[nidx] -= 300.0f;
                if (world->temp[nidx] < 1000.0f) {
                    world_set_mat(world, nx, ny, MAT_METAL);
                }
                world->cells_updated++;
                return true;
            }
            
            /* Water evaporates on hot metal */
            if (neighbor == MAT_METAL && world->temp[nidx] > 100.0f) {
                if (simulation_randf(sim) < 0.3f) {
                    world_set_mat(world, x, y, MAT_STEAM);
                    world->lifetime[idx] = 0;
                    world->temp[nidx] -= 50.0f;  /* Cool the metal */
                    world->cells_updated++;
                    return true;
                }
            }
            
            /* Water dissolves salt */
            if (neighbor == MAT_SALT && simulation_randf(sim) < 0.1f) {
                world_set_mat(world, nx, ny, MAT_AIR);
                /* TODO: Could create "salt water" material */
                world->cells_updated++;
            }
        }
    }
    
    /* =========================================================================
     * Lava Interactions
     * ========================================================================= */
    if (mat == MAT_LAVA) {
        /* Lava radiates heat */
        for (int i = 0; i < 4; i++) {
            int nx = x + NEIGHBOR4_DX[i];
            int ny = y + NEIGHBOR4_DY[i];
            if (!IN_BOUNDS(nx, ny)) continue;
            
            int nidx = IDX(nx, ny);
            MaterialID neighbor = world->mat[nidx];
            
            /* Heat neighbors */
            world->temp[nidx] += 10.0f;
            if (world->temp[nidx] > MAX_TEMPERATURE) {
                world->temp[nidx] = MAX_TEMPERATURE;
            }
            
            /* Lava ignites flammable materials */
            if (bhv_is_flammable(neighbor) && simulation_randf(sim) < 0.1f) {
                world_set_mat(world, nx, ny, MAT_FIRE);
                world->temp[nidx] = 800.0f;
            }
            
            /* Lava melts ice */
            if (neighbor == MAT_ICE) {
                world_set_mat(world, nx, ny, MAT_WATER);
                world->temp[nidx] = 20.0f;
                world->temp[idx] -= 100.0f;  /* Lava cools */
            }
        }
        
        /* Maintain high temperature */
        if (world->temp[idx] < 900.0f) {
            world->temp[idx] = 900.0f;
        }
    }
    
    /* =========================================================================
     * Molten Metal Interactions
     * ========================================================================= */
    if (mat == MAT_MOLTEN_METAL) {
        /* Molten metal radiates heat (less than lava) */
        for (int i = 0; i < 4; i++) {
            int nx = x + NEIGHBOR4_DX[i];
            int ny = y + NEIGHBOR4_DY[i];
            if (!IN_BOUNDS(nx, ny)) continue;
            
            int nidx = IDX(nx, ny);
            world->temp[nidx] += 5.0f;
            if (world->temp[nidx] > MAX_TEMPERATURE) {
                world->temp[nidx] = MAX_TEMPERATURE;
            }
        }
        
        /* Maintain high temperature */
        if (world->temp[idx] < 1200.0f) {
            world->temp[idx] += 2.0f;
        }
    }

    /* =========================================================================
     * Movement: Priority 1 - Lateral Flow (water bias)
     * ========================================================================= */
    if (mat == MAT_WATER && simulation_randf(sim) < WATER_LATERAL_BIAS) {
        bool can_left = fluid_can_move_to(world, x - 1, y);
        bool can_right = fluid_can_move_to(world, x + 1, y);

        if (can_left && can_right) {
            new_x = (simulation_randf(sim) < 0.5f) ? (x - 1) : (x + 1);
            moved = true;
        } else if (can_left) {
            new_x = x - 1;
            moved = true;
        } else if (can_right) {
            new_x = x + 1;
            moved = true;
        }
    }

    /* =========================================================================
     * Movement: Priority 2 - Fall Down
     * ========================================================================= */
    if (!moved && vy > 0) {
        for (int i = 0; i < steps.steps; i++) {
            if (fluid_can_move_to(world, new_x, new_y + 1)) {
                new_y++;
                moved = true;
            } else {
                phys_collision_vertical(world, x, y, COLLISION_STOP, 0);
                break;
            }
        }
    }

    /* =========================================================================
     * Movement: Priority 3 - Horizontal Flow
     * ========================================================================= */
    if (!moved || vy <= 0) {
        if (simulation_randf(sim) < props->flow_rate) {
            bool can_left = fluid_can_move_to(world, x - 1, y);
            bool can_right = fluid_can_move_to(world, x + 1, y);

            if (can_left && can_right) {
                new_x = (simulation_randf(sim) < 0.5f) ? (x - 1) : (x + 1);
                moved = true;
            } else if (can_left) {
                new_x = x - 1;
                moved = true;
            } else if (can_right) {
                new_x = x + 1;
                moved = true;
            }
        }
    }

    /* =========================================================================
     * Movement: Priority 4 - Pressure Equalization
     * ========================================================================= */
    if (!moved && simulation_randf(sim) < FLUID_PRESSURE_CHANCE) {
        int my_pressure = phys_column_height(world, x, y, mat);

        /* Check left neighbor */
        if (fluid_can_move_to(world, x - 1, y)) {
            int left_pressure = phys_column_height(world, x - 1, y, mat);
            if (left_pressure < my_pressure - FLUID_PRESSURE_THRESHOLD) {
                new_x = x - 1;
                moved = true;
            }
        }

        /* Check right neighbor if not moved */
        if (!moved && fluid_can_move_to(world, x + 1, y)) {
            int right_pressure = phys_column_height(world, x + 1, y, mat);
            if (right_pressure < my_pressure - FLUID_PRESSURE_THRESHOLD) {
                new_x = x + 1;
                moved = true;
            }
        }
    }

    /* =========================================================================
     * Apply Horizontal Drag
     * ========================================================================= */
    world->vel_x[idx] = FIXED_MUL(world->vel_x[idx], props->drag_factor_fixed);

    /* =========================================================================
     * Execute Movement
     * ========================================================================= */
    if (moved && (new_x != x || new_y != y)) {
        /* Keep water cohesive: avoid splitting into single droplets */
        if (mat == MAT_WATER) {
            bool had_neighbor = fluid_has_adjacent_except(world, x, y, mat, -1, -1);
            bool will_neighbor = fluid_has_adjacent_except(world, new_x, new_y, mat, x, y);
            if (had_neighbor && !will_neighbor && simulation_randf(sim) > WATER_SPLIT_CHANCE) {
                return false;
            }
        }
        cell_move(world, x, y, new_x, new_y);
        return true;
    }

    return false;
}

/* =============================================================================
 * Grid Update Callback
 * ============================================================================= */

static bool fluid_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    int* pass = (int*)userdata;

    /* Clear updated flag on subsequent passes */
    if (*pass > 0) {
        world_remove_flag(world, x, y, FLAG_UPDATED);
    }

    fluid_update_cell(sim, world, x, y);
    return true;
}

/* =============================================================================
 * Main Update Function
 * ============================================================================= */

void fluid_update(Simulation* sim, World* world) {
    /* Multiple passes for better dispersion */
    for (int pass = 0; pass < FLUID_DISPERSION_PASSES; pass++) {
        grid_iterate_falling(sim, world, fluid_cell_callback, &pass);
    }
}
