/*
 * thermal.c - Energy-Based Thermal Simulation Implementation
 * 
 * Implements realistic thermal physics:
 * - Fourier's Law heat conduction
 * - Stefan-Boltzmann radiation
 * - Latent heat phase transitions
 * - Energy conservation
 */
#include "physics/thermal.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "world/grid_iter.h"
#include <math.h>

/* =============================================================================
 * Phase Transition Targets
 * ============================================================================= */

/* Get what material becomes when it changes phase */
MaterialID thermal_get_transition_target(MaterialID mat, PhaseTransitionType type) {
    switch (type) {
        case PHASE_MELTING:
            switch (mat) {
                case MAT_ICE: return MAT_WATER;
                case MAT_STONE: return MAT_LAVA;
                case MAT_METAL: return MAT_MOLTEN_METAL;
                case MAT_SAND: return MAT_GLASS;
                case MAT_GLASS: return MAT_LAVA;  /* Re-melting */
                case MAT_OBSIDIAN: return MAT_LAVA;
                case MAT_SALT: return MAT_WATER;  /* Dissolves as liquid */
                default: return mat;
            }
        
        case PHASE_FREEZING:
            switch (mat) {
                case MAT_WATER: return MAT_ICE;
                case MAT_LAVA: return MAT_STONE;
                case MAT_MOLTEN_METAL: return MAT_METAL;
                default: return mat;
            }
        
        case PHASE_BOILING:
            switch (mat) {
                case MAT_WATER: return MAT_STEAM;
                case MAT_MUD: return MAT_SOIL;       /* Dries */
                case MAT_WET_WOOD: return MAT_WOOD;  /* Dries */
                default: return mat;
            }
        
        case PHASE_CONDENSING:
            switch (mat) {
                case MAT_STEAM: return MAT_WATER;
                default: return mat;
            }
        
        default:
            return mat;
    }
}

/* Get required energy for phase transition */
float thermal_calc_transition_energy(MaterialID mat, PhaseTransitionType type) {
    const MaterialProps* props = material_get(mat);
    
    switch (type) {
        case PHASE_MELTING:
        case PHASE_FREEZING:
            return props->latent_heat_fusion * props->mass;
        
        case PHASE_BOILING:
        case PHASE_CONDENSING:
            return props->latent_heat_vaporization * props->mass;
        
        default:
            return 0.0f;
    }
}

/* Determine what transition should happen at given temperature */
PhaseTransitionType thermal_get_transition_type(MaterialID mat, float temp) {
    const MaterialProps* props = material_get(mat);
    
    /* Check melting (solid -> liquid) */
    if (props->state == STATE_SOLID && props->melting_temp < 9000.0f) {
        if (temp > props->melting_temp + PHASE_CHANGE_TOLERANCE) {
            return PHASE_MELTING;
        }
    }
    
    /* Check freezing (liquid -> solid) */
    if (props->state == STATE_FLUID && props->freezing_temp > -9000.0f) {
        if (temp < props->freezing_temp - PHASE_CHANGE_TOLERANCE) {
            return PHASE_FREEZING;
        }
    }
    
    /* Check boiling (liquid -> gas) */
    if ((props->state == STATE_FLUID || mat == MAT_MUD || mat == MAT_WET_WOOD) 
        && props->boiling_temp < 9000.0f) {
        if (temp > props->boiling_temp + PHASE_CHANGE_TOLERANCE) {
            return PHASE_BOILING;
        }
    }
    
    /* Check condensation (gas -> liquid) */
    if (props->state == STATE_GAS && mat == MAT_STEAM) {
        if (temp < props->boiling_temp - PHASE_CHANGE_TOLERANCE) {
            return PHASE_CONDENSING;
        }
    }
    
    return PHASE_NONE;
}

/* =============================================================================
 * Heat Transfer Functions
 * ============================================================================= */

/* Calculate heat transfer between two adjacent cells (Fourier's Law) */
float thermal_calc_conduction(World* world, int x1, int y1, int x2, int y2) {
    if (!IN_BOUNDS(x1, y1) || !IN_BOUNDS(x2, y2)) return 0.0f;
    
    int idx1 = IDX(x1, y1);
    int idx2 = IDX(x2, y2);
    
    MaterialID mat1 = world->mat[idx1];
    MaterialID mat2 = world->mat[idx2];
    
    const MaterialProps* props1 = material_get(mat1);
    const MaterialProps* props2 = material_get(mat2);
    
    float k1 = props1->conductivity;
    float k2 = props2->conductivity;
    
    /* Skip if either has no conductivity */
    if (k1 < 0.001f || k2 < 0.001f) return 0.0f;
    
    /* Harmonic mean of conductivities for interface */
    float k_eff = (2.0f * k1 * k2) / (k1 + k2);
    
    /* Temperature difference */
    float T1 = world->temp[idx1];
    float T2 = world->temp[idx2];
    float dT = T2 - T1;
    
    /* Fourier's Law: Q = k * A * dT * dt / dx */
    /* Simplified for grid: Q = k_eff * dT * rate */
    float Q = k_eff * dT * HEAT_DIFFUSION_RATE * CELL_AREA_M2 / CELL_SIZE_M;
    
    return Q;
}

/* Apply heat diffusion to a single cell */
void thermal_apply_conduction(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    /* Skip if non-conductive */
    if (props->conductivity < 0.001f) return;
    
    /* Sum heat from all 4 neighbors */
    float total_heat = 0.0f;
    
    for (int i = 0; i < 4; i++) {
        int nx = x + NEIGHBOR4_DX[i];
        int ny = y + NEIGHBOR4_DY[i];
        
        total_heat += thermal_calc_conduction(world, x, y, nx, ny);
    }
    
    /* Add heat to energy pool (scaled for game time: 1 sec = 1 min real time) */
    world->energy[idx] += total_heat * HEAT_TIME_SCALE;
    
    /* Update temperature from energy */
    if (props->thermal_mass > 0.0001f) {
        world->temp_next[idx] = thermal_energy_to_temp(
            world->energy[idx], 
            props->mass, 
            props->heat_capacity
        );
    }
}

/* Apply radiative heat loss (Stefan-Boltzmann Law) */
void thermal_apply_radiation(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    /* Skip low emissivity materials */
    if (props->emissivity < 0.1f) return;
    
    float temp = world->temp[idx];
    
    /* Only radiate if above ambient */
    if (temp <= AMBIENT_TEMP + 10.0f) return;
    
    /* Stefan-Boltzmann: P = ε * σ * A * (T^4 - T_amb^4) */
    float T_K = C_TO_K(temp);
    float T_amb_K = C_TO_K(AMBIENT_TEMP);
    
    /* Calculate T^4 terms (careful with large numbers) */
    float T4 = T_K * T_K * T_K * T_K;
    float T_amb4 = T_amb_K * T_amb_K * T_amb_K * T_amb_K;
    
    /* Power radiated */
    float P_rad = props->emissivity * STEFAN_BOLTZMANN * CELL_AREA_M2 * (T4 - T_amb4);
    
    /* Apply scaling factor (T^4 produces huge numbers) */
    P_rad *= RADIATION_SCALE;
    
    /* Remove energy (scaled for game time) */
    world->energy[idx] -= P_rad * HEAT_TIME_SCALE;
    
    /* Ensure energy doesn't go negative */
    if (world->energy[idx] < 0.0f) {
        world->energy[idx] = 0.0f;
    }
}

/* Apply cooling towards ambient */
void thermal_apply_ambient_cooling(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    
    int idx = IDX(x, y);
    float temp = world->temp[idx];
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    /* Skip heat sources - they maintain their own temperature */
    if (mat == MAT_FIRE || mat == MAT_LAVA) return;
    
    /* Gradual cooling to ambient - energy-driven */
    float dT = (AMBIENT_TEMP - temp) * AMBIENT_COOLING_RATE;
    
    /* Update energy first (this is the source of truth) */
    float energy_change = props->mass * props->heat_capacity * dT;
    world->energy[idx] += energy_change;
    
    /* Ensure energy doesn't go negative */
    if (world->energy[idx] < 0.0f) {
        world->energy[idx] = 0.0f;
    }
    
    /* Recalculate temperature from energy for consistency */
    if (props->thermal_mass > 0.0001f) {
        world->temp_next[idx] = thermal_energy_to_temp(
            world->energy[idx], 
            props->mass, 
            props->heat_capacity
        );
    }
}

/* =============================================================================
 * Phase Change Processing
 * ============================================================================= */

void thermal_check_phase_change(Simulation* sim, World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    float temp = world->temp[idx];
    
    /* Determine transition type */
    PhaseTransitionType trans_type = thermal_get_transition_type(mat, temp);
    
    if (trans_type == PHASE_NONE) {
        /* No transition - reset progress */
        world->phase_progress[idx] = 0.0f;
        return;
    }
    
    /* Get required energy for transition */
    float required_energy = thermal_calc_transition_energy(mat, trans_type);
    
    if (required_energy <= 0.0f) {
        /* Instant transition (no latent heat defined) */
        MaterialID target = thermal_get_transition_target(mat, trans_type);
        world_set_mat(world, x, y, target);
        world->phase_progress[idx] = 0.0f;
        return;
    }
    
    const MaterialProps* props = material_get(mat);
    
    /* Calculate available energy for phase change */
    float available_energy = 0.0f;
    
    if (trans_type == PHASE_MELTING || trans_type == PHASE_BOILING) {
        /* Energy above transition point */
        float trans_temp = (trans_type == PHASE_MELTING) ? props->melting_temp : props->boiling_temp;
        float excess_temp = temp - trans_temp;
        if (excess_temp > 0.0f) {
            available_energy = props->mass * props->heat_capacity * excess_temp;
        }
    } else {
        /* Energy below transition point (releasing heat) */
        float trans_temp = (trans_type == PHASE_FREEZING) ? props->freezing_temp : props->boiling_temp;
        float deficit_temp = trans_temp - temp;
        if (deficit_temp > 0.0f) {
            available_energy = props->mass * props->heat_capacity * deficit_temp;
        }
    }
    
    /* Limit energy consumed per tick */
    float max_energy_per_tick = required_energy * 0.02f;  /* Max 2% per tick (slower transition/evaporation) */
    float energy_this_tick = fminf(available_energy, max_energy_per_tick);
    
    /* Accumulate phase progress */
    world->phase_progress[idx] += energy_this_tick / required_energy;
    
    /* Consume/release energy */
    if (trans_type == PHASE_MELTING || trans_type == PHASE_BOILING) {
        /* Absorb heat (endothermic) */
        world->energy[idx] -= energy_this_tick;
    } else {
        /* Release heat (exothermic) */
        world->energy[idx] += energy_this_tick;
    }
    
    /* Hold temperature at transition point during phase change */
    if (trans_type == PHASE_MELTING) {
        world->temp_next[idx] = props->melting_temp;
    } else if (trans_type == PHASE_BOILING) {
        world->temp_next[idx] = props->boiling_temp;
    } else if (trans_type == PHASE_FREEZING) {
        world->temp_next[idx] = props->freezing_temp;
    } else if (trans_type == PHASE_CONDENSING) {
        world->temp_next[idx] = props->boiling_temp;
    }
    
    /* Check if transition complete */
    if (world->phase_progress[idx] >= 1.0f) {
        MaterialID target = thermal_get_transition_target(mat, trans_type);
        
        /* Special case: Water + Lava = Obsidian handled elsewhere */
        
        world_set_mat(world, x, y, target);
        world->phase_progress[idx] = 0.0f;
        
        /* Set appropriate initial temperature for new material */
        const MaterialProps* new_props = material_get(target);
        if (trans_type == PHASE_MELTING) {
            world->temp[idx] = new_props->melting_temp + 1.0f;
        } else if (trans_type == PHASE_BOILING) {
            world->temp[idx] = new_props->boiling_temp + 1.0f;
        } else if (trans_type == PHASE_FREEZING) {
            world->temp[idx] = new_props->freezing_temp - 1.0f;
        } else if (trans_type == PHASE_CONDENSING) {
            world->temp[idx] = new_props->boiling_temp - 1.0f;
        }
        
        /* Recalculate energy */
        world->energy[idx] = thermal_temp_to_energy(
            world->temp[idx],
            new_props->mass,
            new_props->heat_capacity
        );
        
        world_activate_chunk_at(world, x, y);
    }
    
    /* Set visual flag */
    if (trans_type == PHASE_MELTING || trans_type == PHASE_BOILING) {
        world_add_flag(world, x, y, (trans_type == PHASE_MELTING) ? FLAG_MELTING : FLAG_BOILING);
    }
}

/* =============================================================================
 * Auto-Ignition System
 * 
 * Flammable materials catch fire when they reach ignition temperature.
 * This replaces direct ignition in fire.c - fire only produces heat now.
 * ============================================================================= */

/* Oxygen availability check - same as in fire.c */
#define O2_ATMOSPHERIC      0.21f
#define O2_MIN_COMBUSTION   0.05f

static float thermal_get_surface_oxygen(World* world, int x, int y) {
    float max_o2 = 0.0f;
    
    for (int i = 0; i < 8; i++) {
        int nx = x + NEIGHBOR8_DX[i];
        int ny = y + NEIGHBOR8_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;
        
        int nidx = IDX(nx, ny);
        MaterialID nmat = world->mat[nidx];
        
        /* Fire provides atmospheric oxygen for combustion */
        if (nmat == MAT_FIRE) {
            if (O2_ATMOSPHERIC > max_o2) {
                max_o2 = O2_ATMOSPHERIC;
            }
            continue;
        }
        
        /* Air and gas cells have oxygen */
        if (nmat == MAT_AIR || material_state(nmat) == STATE_GAS) {
            if (world->oxygen[nidx] > max_o2) {
                max_o2 = world->oxygen[nidx];
            }
        }
    }
    
    return max_o2;
}

/* Check if a flammable material should catch fire based on temperature */
/* DETERMINISTIC VERSION - No randomness, ignition at threshold */
void thermal_check_ignition(Simulation* sim, World* world, int x, int y) {
    (void)sim; /* Not using simulation randomness anymore */
    
    if (!IN_BOUNDS(x, y)) return;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    /* Skip if not flammable or no ignition temp defined */
    if (props->flammability <= 0.0f) return;
    if (props->ignition_temp >= 9000.0f) return;
    
    float temp = world->temp[idx];
    
    /* DETERMINISTIC: Ignite when temperature reaches ignition point */
    if (temp < props->ignition_temp) return;
    
    /* Check oxygen availability */
    float surface_o2 = thermal_get_surface_oxygen(world, x, y);
    if (surface_o2 < O2_MIN_COMBUSTION) return;
    
    /* IGNITE! Temperature reached, oxygen available */
    
    /* Special handling for different materials */
    if (mat == MAT_WOOD || mat == MAT_COAL) {
        /* Solid fuels: at exact ignition temp -> ember, higher -> fire */
        if (temp >= props->ignition_temp * 1.2f) {
            world_set_mat(world, x, y, MAT_FIRE);
            world->temp[idx] = FIRE_TEMPERATURE;
            world->temp_next[idx] = FIRE_TEMPERATURE;
        } else {
            world_set_mat(world, x, y, MAT_EMBER);
            world->temp[idx] = 500.0f;
            world->temp_next[idx] = 500.0f;
        }
    } else if (mat == MAT_OIL) {
        /* Oil ignites directly to fire */
        world_set_mat(world, x, y, MAT_FIRE);
        world->temp[idx] = FIRE_TEMPERATURE;
        world->temp_next[idx] = FIRE_TEMPERATURE;
    } else if (mat == MAT_EMBER) {
        /* Ember reignites to fire */
        world_set_mat(world, x, y, MAT_FIRE);
        world->temp[idx] = FIRE_TEMPERATURE;
        world->temp_next[idx] = FIRE_TEMPERATURE;
    } else {
        /* Generic flammable material */
        world_set_mat(world, x, y, MAT_FIRE);
        world->temp[idx] = FIRE_TEMPERATURE;
        world->temp_next[idx] = FIRE_TEMPERATURE;
    }
    
    world->lifetime[idx] = 0;
    world_add_flag(world, x, y, FLAG_BURNING);
    world_activate_chunk_at(world, x, y);
}

/* =============================================================================
 * Heat Source Updates
 * ============================================================================= */

void thermal_update_heat_sources(World* world) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = IDX(x, y);
            MaterialID mat = world->mat[idx];
            
            /* Fire maintains constant high temperature */
            if (mat == MAT_FIRE) {
                world->temp[idx] = FIRE_TEMPERATURE;
                world->temp_next[idx] = FIRE_TEMPERATURE;
                const MaterialProps* props = material_get(mat);
                world->energy[idx] = thermal_temp_to_energy(
                    FIRE_TEMPERATURE, props->mass, props->heat_capacity
                );
            }
            
            /* Lava maintains temperature (cools slowly) */
            if (mat == MAT_LAVA) {
                if (world->temp[idx] < LAVA_TEMPERATURE * 0.7f) {
                    world->temp[idx] = LAVA_TEMPERATURE * 0.7f;
                }
            }
            
            /* Ember maintains warmth but can cool */
            if (mat == MAT_EMBER) {
                if (world->temp[idx] < 300.0f) {
                    /* Cold ember becomes smoke/disappears */
                    /* Handled in fire subsystem */
                }
            }
        }
    }
}

/* =============================================================================
 * Grid Iteration Callbacks
 * ============================================================================= */

static bool thermal_conduction_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)sim;
    (void)userdata;
    thermal_apply_conduction(world, x, y);
    return true;
}

static bool thermal_radiation_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)sim;
    (void)userdata;
    thermal_apply_radiation(world, x, y);
    thermal_apply_ambient_cooling(world, x, y);
    return true;
}

static bool thermal_phase_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    thermal_check_phase_change(sim, world, x, y);
    return true;
}

static bool thermal_ignition_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    thermal_check_ignition(sim, world, x, y);
    return true;
}

/* =============================================================================
 * Main Thermal Update
 * ============================================================================= */

void thermal_update(Simulation* sim, World* world) {
    /* Pass 1: Update heat sources */
    thermal_update_heat_sources(world);
    
    /* Pass 2: Heat conduction */
    grid_iterate(sim, world, ITER_TOP_DOWN, ITER_LEFT_RIGHT,
                 thermal_conduction_callback, NULL);
    
    /* Pass 3: Radiation and ambient cooling */
    grid_iterate(sim, world, ITER_TOP_DOWN, ITER_LEFT_RIGHT,
                 thermal_radiation_callback, NULL);
    
    /* Pass 4: Phase transitions (melting, boiling, freezing) */
    grid_iterate(sim, world, ITER_TOP_DOWN, ITER_LEFT_RIGHT,
                 thermal_phase_callback, NULL);
    
    /* Pass 5: Auto-ignition (flammable materials catch fire at ignition temp) */
    grid_iterate(sim, world, ITER_TOP_DOWN, ITER_LEFT_RIGHT,
                 thermal_ignition_callback, NULL);
    
    /* Swap temperature buffers */
    float* tmp = world->temp;
    world->temp = world->temp_next;
    world->temp_next = tmp;
    
    /* Clamp all temperatures */
    size_t grid_size = (size_t)world->width * world->height;
    for (size_t i = 0; i < grid_size; i++) {
        world->temp[i] = thermal_clamp_temp(world->temp[i]);
        world->temp_next[i] = world->temp[i];
    }
}

/* =============================================================================
 * Energy Conservation
 * ============================================================================= */

double thermal_calc_total_energy(const World* world) {
    double total = 0.0;
    size_t grid_size = (size_t)world->width * world->height;
    
    for (size_t i = 0; i < grid_size; i++) {
        total += (double)world->energy[i];
    }
    
    return total;
}

bool thermal_validate_conservation(const World* world, double prev_energy, double tolerance) {
    double current = thermal_calc_total_energy(world);
    double diff = fabs(current - prev_energy);
    return diff <= tolerance;
}
