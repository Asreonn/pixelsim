/*
 * fire.c - Deterministic Fire and Gas Physics Implementation
 *
 * OVERHAUL v2.0 - Completely rewritten for deterministic behavior:
 *   - NO probability/randomness - everything is predictable
 *   - Fire ALWAYS rises upward
 *   - Glow gradient based on lifetime
 *   - Consistent heat transfer to neighbors
 *   - Fixed lifetime before death
 */
#include "subsystems/fire.h"
#include "materials/material.h"
#include "world/cell_ops.h"
#include "materials/behavior.h"
#include "world/grid_iter.h"
#include "physics/thermal.h"
#include "physics/physics.h"
#include <math.h>

/* =============================================================================
 * Fire Configuration - DETERMINISTIC VALUES
 * ============================================================================= */

/* Lifetime settings (at 120Hz) */
#define FIRE_LIFETIME_MAX       30      /* 0.25 seconds of fire (very short) */
#define FIRE_SMOKE_THRESHOLD    25      /* After this, dies with fade effect */

/* Heat output */
#define FIRE_HEAT_PER_TICK      20.0f   /* Heat given to each neighbor per tick */
#define FIRE_CORE_TEMP          1000.0f /* Fire's own temperature */

/* Oxygen system */
#define O2_ATMOSPHERIC          0.21f   /* Normal atmospheric O2 */
#define O2_CONSUMPTION_FIRE     0.008f  /* O2 consumed per fire tick */

/* =============================================================================
 * Fire Color Palette - Lifetime-Based Gradient
 * ============================================================================= */

/* 6-stage gradient from hottest (white-yellow) to dying (dark red/transparent) */
static const Color FIRE_GRADIENT[6] = {
    {255, 255, 220, 255},   /* Stage 0: White-yellow (hottest) */
    {255, 220, 100, 255},   /* Stage 1: Bright yellow */
    {255, 150,  50, 255},   /* Stage 2: Orange */
    {255,  80,  25, 230},   /* Stage 3: Red-orange (slight fade) */
    {200,  50,  15, 180},   /* Stage 4: Dark red (fading) */
    {120,  30,  10,  50},   /* Stage 5: Dying ember (mostly transparent) */
};

/* Ember color palette (temperature-based glow) */
static const Color EMBER_PALETTE[4] = {
    { 60,  20,  10, 255},   /* Cool ember (dark) */
    {150,  40,  10, 255},   /* Warm ember */
    {200,  60,  15, 255},   /* Hot ember */
    {255, 100,  30, 255},   /* Glowing ember */
};

/* Get fire color with smooth interpolation based on lifetime */
Color fire_get_color(uint8_t lifetime) {
    if (lifetime >= FIRE_LIFETIME_MAX) {
        return FIRE_GRADIENT[5];
    }
    
    /* Calculate steps per stage dynamically based on max lifetime */
    /* We have 6 stages (0-5), so 5 transitions */
    int steps_per_stage = FIRE_LIFETIME_MAX / 5;
    if (steps_per_stage < 1) steps_per_stage = 1;
    
    int stage = lifetime / steps_per_stage;
    if (stage > 5) stage = 5;
    
    int next_stage = (stage < 5) ? stage + 1 : stage;
    float t = (float)(lifetime % steps_per_stage) / (float)steps_per_stage;
    
    /* Linear interpolation between stages */
    Color result;
    result.r = (uint8_t)(FIRE_GRADIENT[stage].r * (1.0f - t) + FIRE_GRADIENT[next_stage].r * t);
    result.g = (uint8_t)(FIRE_GRADIENT[stage].g * (1.0f - t) + FIRE_GRADIENT[next_stage].g * t);
    result.b = (uint8_t)(FIRE_GRADIENT[stage].b * (1.0f - t) + FIRE_GRADIENT[next_stage].b * t);
    result.a = (uint8_t)(FIRE_GRADIENT[stage].a * (1.0f - t) + FIRE_GRADIENT[next_stage].a * t);
    
    return result;
}

Color ember_get_color(float temp) {
    if (temp < 200.0f) return EMBER_PALETTE[0];
    if (temp < 350.0f) return EMBER_PALETTE[1];
    if (temp < 500.0f) return EMBER_PALETTE[2];
    return EMBER_PALETTE[3];
}

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

static void set_cell_temp(World* world, int x, int y, float temp_c) {
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);

    world->temp[idx] = temp_c;
    world->temp_next[idx] = temp_c;
    world->energy[idx] = thermal_temp_to_energy(temp_c, props->mass, props->heat_capacity);
}

static void add_heat_to_cell(World* world, int x, int y, float delta_c) {
    if (!IN_BOUNDS(x, y)) return;
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);

    world_add_heat(world, x, y, props->thermal_mass * delta_c);
    world->temp_next[idx] = world->temp[idx];
}

/* Check oxygen availability at cell surface */
static float get_surface_oxygen(World* world, int x, int y) {
    float max_o2 = 0.0f;

    for (int i = 0; i < 8; i++) {
        int nx = x + NEIGHBOR8_DX[i];
        int ny = y + NEIGHBOR8_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;

        int nidx = IDX(nx, ny);
        MaterialID nmat = world->mat[nidx];
        
        if (nmat == MAT_FIRE) {
            if (O2_ATMOSPHERIC > max_o2) {
                max_o2 = O2_ATMOSPHERIC;
            }
            continue;
        }
        if (nmat == MAT_AIR || material_state(nmat) == STATE_GAS) {
            if (world->oxygen[nidx] > max_o2) {
                max_o2 = world->oxygen[nidx];
            }
        }
    }

    return max_o2;
}

/* =============================================================================
 * Deterministic Fire Movement
 * 
 * Fire ALWAYS tries to rise. No randomness.
 * Priority: Up > Diagonal-Up > Horizontal
 * Alternates left/right based on lifetime (deterministic)
 * ============================================================================= */

static bool fire_move_deterministic(World* world, int x, int y) {
    int idx = IDX(x, y);
    uint8_t lifetime = world->lifetime[idx];
    
    /* Priority 1: Straight up */
    if (cell_gas_can_enter(world, x, y - 1)) {
        cell_move(world, x, y, x, y - 1);
        return true;
    }
    
    /* Priority 2: Diagonal up - alternate based on lifetime (deterministic) */
    int primary_side = (lifetime % 2 == 0) ? -1 : 1;
    int secondary_side = -primary_side;
    
    if (cell_gas_can_enter(world, x + primary_side, y - 1)) {
        cell_move(world, x, y, x + primary_side, y - 1);
        return true;
    }
    
    if (cell_gas_can_enter(world, x + secondary_side, y - 1)) {
        cell_move(world, x, y, x + secondary_side, y - 1);
        return true;
    }
    
    /* Priority 3: Horizontal spread - alternate based on lifetime */
    if (cell_gas_can_enter(world, x + primary_side, y)) {
        cell_move(world, x, y, x + primary_side, y);
        return true;
    }
    
    if (cell_gas_can_enter(world, x + secondary_side, y)) {
        cell_move(world, x, y, x + secondary_side, y);
        return true;
    }
    
    /* Cannot move - stay in place */
    return false;
}

/* =============================================================================
 * Fire Ignition (for external use)
 * ============================================================================= */

bool fire_try_ignite(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return false;

    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);

    /* Check if flammable */
    if (!bhv_is_flammable(mat)) return false;
    
    /* Check ignition temperature */
    float temp = world->temp[idx];
    if (temp < props->ignition_temp) return false;
    
    /* Check oxygen */
    float surface_o2 = get_surface_oxygen(world, x, y);
    if (surface_o2 < 0.05f) return false;

    /* IGNITE! */
    world_set_mat(world, x, y, MAT_FIRE);
    set_cell_temp(world, x, y, FIRE_CORE_TEMP);
    world->lifetime[idx] = 0;
    world_add_flag(world, x, y, FLAG_BURNING);
    return true;
}

/* =============================================================================
 * Fire Cell Update - DETERMINISTIC
 * ============================================================================= */

bool fire_update_cell(Simulation* sim, World* world, int x, int y) {
    (void)sim; /* Not using simulation randomness anymore */
    
    MaterialID mat = world_get_mat(world, x, y);
    if (mat != MAT_FIRE) {
        return false;
    }

    int idx = IDX(x, y);

    /* 1. INCREMENT LIFETIME (always) */
    if (world->lifetime[idx] < 255) {
        world->lifetime[idx]++;
    }

    /* 2. CONSUME OXYGEN */
    world->oxygen[idx] -= O2_CONSUMPTION_FIRE;
    if (world->oxygen[idx] < 0.0f) {
        world->oxygen[idx] = 0.0f;
    }

    /* 3. CHECK DEATH CONDITIONS */
    bool should_die = false;
    
    /* Die from lifetime expiration */
    if (world->lifetime[idx] >= FIRE_LIFETIME_MAX) {
        should_die = true;
    }
    
    /* Die from lack of oxygen */
    float surface_o2 = get_surface_oxygen(world, x, y);
    if (surface_o2 < 0.03f) {
        should_die = true;
    }

    if (should_die) {
        /* Fire simply disappears - no smoke production */
        /* Smoke was causing visual issues at ground level */
        world_set_mat(world, x, y, MAT_AIR);
        set_cell_temp(world, x, y, 80.0f);

        world->lifetime[idx] = 0;
        world_remove_flag(world, x, y, FLAG_BURNING);
        cell_mark_updated(world, x, y);
        world->cells_updated++;
        return true;
    }

    /* 4. MAINTAIN FIRE TEMPERATURE */
    set_cell_temp(world, x, y, FIRE_CORE_TEMP);

    /* 5. HEAT TRANSFER TO ALL NEIGHBORS (deterministic, every tick) */
    for (int i = 0; i < 8; i++) {
        int nx = x + NEIGHBOR8_DX[i];
        int ny = y + NEIGHBOR8_DY[i];
        if (!IN_BOUNDS(nx, ny)) continue;

        add_heat_to_cell(world, nx, ny, FIRE_HEAT_PER_TICK);
    }

    /* 6. MOVEMENT - ALWAYS TRY TO RISE */
    fire_move_deterministic(world, x, y);

    /* Note: Smoke is only produced when fire dies (see death section above) */
    /* This prevents ghost smoke appearing below fire */

    cell_mark_updated(world, x, y);
    return true;
}

/* =============================================================================
 * Ember Cell Update
 * ============================================================================= */

bool ember_update_cell(Simulation* sim, World* world, int x, int y) {
    if (cell_skip_if_updated(world, x, y)) {
        return false;
    }

    MaterialID mat = world_get_mat(world, x, y);
    if (mat != MAT_EMBER) {
        return false;
    }

    int idx = IDX(x, y);
    float temp = world->temp[idx];
    float o2 = world->oxygen[idx];

    /* Increment lifetime */
    if (world->lifetime[idx] < 255) {
        world->lifetime[idx]++;
    }

    /* Check for reignition - deterministic */
    if (temp >= 500.0f && o2 >= 0.1f) {
        world_set_mat(world, x, y, MAT_FIRE);
        set_cell_temp(world, x, y, FIRE_CORE_TEMP);
        world->lifetime[idx] = 0;
        cell_mark_updated(world, x, y);
        world->cells_updated++;
        return true;
    }

    /* Smoldering heat generation */
    if (o2 >= 0.03f && temp >= 200.0f) {
        add_heat_to_cell(world, x, y, 2.0f);
        world->oxygen[idx] -= 0.003f;
        if (world->oxygen[idx] < 0.0f) world->oxygen[idx] = 0.0f;
        
        /* NOTE: Smoke production removed - was causing issues */
    }

    /* Cooling */
    float cool_rate = (o2 < 0.03f) ? 1.0f : 0.5f;
    add_heat_to_cell(world, x, y, -cool_rate);

    /* Heat radiation to neighbors */
    if (temp > 100.0f) {
        for (int i = 0; i < 4; i++) {
            int nx = x + NEIGHBOR4_DX[i];
            int ny = y + NEIGHBOR4_DY[i];
            if (!IN_BOUNDS(nx, ny)) continue;

            float neighbor_temp = world->temp[IDX(nx, ny)];
            if (neighbor_temp < temp) {
                float transfer = (temp - neighbor_temp) * 0.05f;
                add_heat_to_cell(world, nx, ny, transfer);
            }
        }
    }

    /* Ember death - becomes ash when cold or old */
    if (world->temp[idx] < 50.0f || world->lifetime[idx] > 200) {
        world_set_mat(world, x, y, MAT_ASH);
        set_cell_temp(world, x, y, 25.0f);
        world->lifetime[idx] = 0;
        cell_mark_updated(world, x, y);
        world->cells_updated++;
        return true;
    }

    cell_mark_updated(world, x, y);
    return true;
}

/* =============================================================================
 * Gas/Smoke Cell Update (simplified, less random)
 * ============================================================================= */

bool gas_update_cell(Simulation* sim, World* world, int x, int y) {
    if (cell_skip_if_updated(world, x, y)) {
        return false;
    }

    MaterialID mat = world_get_mat(world, x, y);
    MaterialState state = material_state(mat);

    if (state != STATE_GAS || mat == MAT_FIRE) {
        return false;
    }

    int idx = IDX(x, y);

    /* Increment lifetime */
    if (world->lifetime[idx] < 255) {
        world->lifetime[idx]++;
    }

    /* Smoke dissipation - deterministic based on lifetime */
    if (mat == MAT_SMOKE) {
        /* Dissipate every 150 ticks (about 1.25 seconds) */
        if (world->lifetime[idx] >= 150) {
            world_set_mat(world, x, y, MAT_AIR);
            world->lifetime[idx] = 0;
            world->oxygen[idx] = O2_ATMOSPHERIC;
            cell_mark_updated(world, x, y);
            world->cells_updated++;
            return true;
        }
    }

    /* Steam condensation - deterministic based on temperature */
    if (mat == MAT_STEAM) {
        float temp = world->temp[idx];
        if (temp < 70.0f) {
            world_set_mat(world, x, y, MAT_WATER);
            world->lifetime[idx] = 0;
            cell_mark_updated(world, x, y);
            world->cells_updated++;
            return true;
        }
    }

    /* Gas movement - ALWAYS try to rise */
    int new_x = x, new_y = y;
    bool moved = false;
    
    /* Alternate direction based on lifetime */
    int primary_side = (world->lifetime[idx] % 2 == 0) ? -1 : 1;

    /* Priority 1: Straight up */
    if (cell_gas_can_enter(world, x, y - 1)) {
        new_y = y - 1;
        moved = true;
    }
    /* Priority 2: Diagonal up */
    else if (cell_gas_can_enter(world, x + primary_side, y - 1)) {
        new_x = x + primary_side;
        new_y = y - 1;
        moved = true;
    }
    else if (cell_gas_can_enter(world, x - primary_side, y - 1)) {
        new_x = x - primary_side;
        new_y = y - 1;
        moved = true;
    }
    /* Priority 3: Horizontal */
    else if (cell_gas_can_enter(world, x + primary_side, y)) {
        new_x = x + primary_side;
        moved = true;
    }
    else if (cell_gas_can_enter(world, x - primary_side, y)) {
        new_x = x - primary_side;
        moved = true;
    }
    /* Priority 4: Bubble through fluid (Straight Up) */
    else if (cell_is_fluid(world, x, y - 1)) {
        new_y = y - 1;
        moved = true;
    }
    /* Priority 5: Bubble through fluid (Diagonal - helps escape trapped corners) */
    else if (cell_is_fluid(world, x + primary_side, y - 1)) {
        new_x = x + primary_side;
        new_y = y - 1;
        moved = true;
    }
    else if (cell_is_fluid(world, x - primary_side, y - 1)) {
        new_x = x - primary_side;
        new_y = y - 1;
        moved = true;
    }
    /* Priority 6: Slip Horizontal in fluid (rare but prevents clogging) */
    else if (cell_is_fluid(world, x + primary_side, y)) {
        new_x = x + primary_side;
        moved = true;
    }

    if (moved) {
        cell_move(world, x, y, new_x, new_y);
        return true;
    }

    return false;
}

/* =============================================================================
 * Grid Update Callbacks
 * ============================================================================= */

static bool fire_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    fire_update_cell(sim, world, x, y);
    return true;
}

static bool ember_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    ember_update_cell(sim, world, x, y);
    return true;
}

static bool gas_cell_callback(Simulation* sim, World* world, int x, int y, void* userdata) {
    (void)userdata;
    gas_update_cell(sim, world, x, y);
    return true;
}

/* =============================================================================
 * Main Update Functions
 * ============================================================================= */

void fire_update(Simulation* sim, World* world) {
    /* Fire RISES, so process top to bottom */
    grid_iterate_rising(sim, world, fire_cell_callback, NULL);
    
    /* Update embers (they fall slowly, process bottom to top) */
    grid_iterate_falling(sim, world, ember_cell_callback, NULL);
}

void gas_update(Simulation* sim, World* world) {
    /* Gas rises, process top to bottom */
    grid_iterate_rising(sim, world, gas_cell_callback, NULL);
}

/* =============================================================================
 * Oxygen System Update
 * ============================================================================= */

void oxygen_update(World* world) {
    /* Replenish oxygen at world boundaries */
    for (int x = 0; x < GRID_WIDTH; x++) {
        int idx_top = IDX(x, 0);
        if (world->mat[idx_top] == MAT_AIR) {
            world->oxygen[idx_top] = O2_ATMOSPHERIC;
        }
    }
    
    /* Slow global oxygen diffusion for air cells */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = IDX(x, y);
            if (world->mat[idx] == MAT_AIR) {
                float diff = O2_ATMOSPHERIC - world->oxygen[idx];
                world->oxygen[idx] += diff * 0.01f;
            }
        }
    }
}
