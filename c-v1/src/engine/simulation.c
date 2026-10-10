/*
 * simulation.c - Fixed timestep simulation loop implementation
 * 
 * Updated with new physics systems:
 * - Buoyancy and convection
 * - Enhanced thermal system
 * - Oxygen diffusion (for combustion)
 */
#include "engine/simulation.h"
#include "core/utils.h"
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

/* High-resolution timer for profiling */
static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
}

/* Forward declarations for subsystem updates */
void powder_update(Simulation* sim, World* world);
void fluid_update(Simulation* sim, World* world);
void acid_update(Simulation* sim, World* world);
void fire_update(Simulation* sim, World* world);
void gas_update(Simulation* sim, World* world);
void oxygen_update(World* world);
void thermal_update(Simulation* sim, World* world);
void buoyancy_update(Simulation* sim, World* world);

Simulation* simulation_create(double tick_hz) {
    Simulation* sim = calloc(1, sizeof(Simulation));
    if (!sim) return NULL;
    
    sim->tick_hz = tick_hz;
    sim->dt = 1.0 / tick_hz;
    sim->accumulator = 0.0;
    sim->tick_count = 0;
    
    /* Initialize RNG with time-based seed */
    sim->rng_state = (uint32_t)time(NULL);
    sim->tick_seed = xorshift32(&sim->rng_state);
    
    sim->tick_time_ms = 0.0;
    sim->avg_tick_time_ms = 0.0;
    sim->paused = false;
    sim->step_once = false;
    
    return sim;
}

void simulation_destroy(Simulation* sim) {
    free(sim);
}

void simulation_update(Simulation* sim, World* world, double real_dt) {
    if (sim->paused && !sim->step_once) {
        return;
    }
    
    if (sim->step_once) {
        /* Single step mode */
        simulation_tick(sim, world);
        sim->step_once = false;
        return;
    }
    
    /* Accumulate real time */
    sim->accumulator += real_dt;
    
    /* Cap accumulator to prevent spiral of death (max 3 ticks catch-up) */
    double max_accumulator = sim->dt * 3.0;
    if (sim->accumulator > max_accumulator) {
        sim->accumulator = max_accumulator;
    }
    
    /* Run fixed timestep simulation */
    while (sim->accumulator >= sim->dt) {
        simulation_tick(sim, world);
        sim->accumulator -= sim->dt;
    }
}

void simulation_tick(Simulation* sim, World* world) {
    /* Generate per-tick seed for determinism */
    sim->tick_seed = xorshift32(&sim->rng_state);
    
    /* Clear per-tick flags */
    world_clear_tick_flags(world);
    
    /* =========================================================================
     * SIMULATION PIPELINE
     * 
     * Order matters! Each step builds on the previous:
     * 1. Update densities (needed for buoyancy)
     * 2. Buoyancy/convection (moves hot air up)
     * 3. Powder physics
     * 4. Fluid physics
     * 5. Fire/combustion
     * 6. Gas physics
     * 7. Acid corrosion
     * 8. Thermal simulation (uses results from fire, etc.)
     * ========================================================================= */
    
    world->cells_updated = 0;
    
    double t0, t1;
    
    /* 1. Update effective densities for buoyancy calculations */
    t0 = get_time_us();
    world_update_all_densities(world);
    t1 = get_time_us();
    sim->profile_density_us = t1 - t0;
    
    /* 2. Buoyancy and convection (hot air rises, cold air sinks) */
    t0 = get_time_us();
    buoyancy_update(sim, world);
    t1 = get_time_us();
    sim->profile_buoyancy_us = t1 - t0;
    
    /* 3. Powder step (sand/soil) - falls and piles */
    t0 = get_time_us();
    powder_update(sim, world);
    t1 = get_time_us();
    sim->profile_powder_us = t1 - t0;
    
    /* 4. Fluid step (water, oil, acid, lava, molten metal) */
    t0 = get_time_us();
    fluid_update(sim, world);
    t1 = get_time_us();
    sim->profile_fluid_us = t1 - t0;
    
    /* 5. Fire step - burns, spreads, consumes oxygen */
    t0 = get_time_us();
    fire_update(sim, world);
    t1 = get_time_us();
    sim->profile_fire_us = t1 - t0;

    /* 6. Gas step (smoke, steam) - rises, dissipates */
    t0 = get_time_us();
    gas_update(sim, world);
    t1 = get_time_us();
    sim->profile_gas_us = t1 - t0;

    /* 7. Oxygen refresh (air flow + diffusion) */
    oxygen_update(world);

    /* 8. Acid step - corrosion */
    acid_update(sim, world);

    /* 9. Thermal step - heat diffusion, radiation, phase changes */
    t0 = get_time_us();
    thermal_update(sim, world);
    t1 = get_time_us();
    sim->profile_thermal_us = t1 - t0;
    
    /* Calculate total */
    sim->profile_total_us = sim->profile_density_us + sim->profile_buoyancy_us +
                            sim->profile_powder_us + sim->profile_fluid_us + 
                            sim->profile_fire_us + sim->profile_gas_us +
                            sim->profile_thermal_us;
    
    /* 9. Update chunk activation */
    world_update_chunk_activation(world);
    
    /* Update tick count */
    sim->tick_count++;
    
    /* Update tick time for profiling */
    sim->tick_time_ms = sim->profile_total_us / 1000.0;
    
    /* Running average */
    sim->avg_tick_time_ms = sim->avg_tick_time_ms * 0.95 + sim->tick_time_ms * 0.05;
}

void simulation_set_paused(Simulation* sim, bool paused) {
    sim->paused = paused;
}

void simulation_toggle_pause(Simulation* sim) {
    sim->paused = !sim->paused;
}

void simulation_step_once(Simulation* sim) {
    sim->step_once = true;
}

uint32_t simulation_rand(Simulation* sim) {
    return xorshift32(&sim->tick_seed);
}

float simulation_randf(Simulation* sim) {
    return (float)simulation_rand(sim) / (float)0xFFFFFFFF;
}

int simulation_rand_range(Simulation* sim, int min, int max) {
    if (min >= max) return min;
    uint32_t range = (uint32_t)(max - min + 1);
    return min + (int)(simulation_rand(sim) % range);
}

void simulation_reset(Simulation* sim) {
    sim->accumulator = 0.0;
    sim->tick_count = 0;
    sim->rng_state = (uint32_t)time(NULL);
    sim->tick_seed = xorshift32(&sim->rng_state);
    sim->paused = false;
    sim->step_once = false;
}
