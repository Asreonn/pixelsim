/*
 * world.c - Grid/World model implementation
 */
#include "world/world.h"
#include "core/utils.h"
#include "physics/thermal.h"  /* For AMBIENT_TEMP */
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* =============================================================================
 * World Creation and Destruction
 * ============================================================================= */

World* world_create(int width, int height) {
    World* world = calloc(1, sizeof(World));
    if (!world) return NULL;
    
    world->width = width;
    world->height = height;
    
    size_t grid_size = (size_t)width * height;
    size_t chunk_count = (size_t)CHUNKS_X * CHUNKS_Y;
    
    /* Allocate primary arrays */
    world->mat = calloc(grid_size, sizeof(MaterialID));
    world->mat_next = calloc(grid_size, sizeof(MaterialID));
    world->flags = calloc(grid_size, sizeof(CellFlags));
    world->color_seed = calloc(grid_size, sizeof(uint32_t));
    world->lifetime = calloc(grid_size, sizeof(uint8_t));
    
    /* Allocate thermal arrays */
    world->temp = calloc(grid_size, sizeof(float));
    world->temp_next = calloc(grid_size, sizeof(float));
    world->energy = calloc(grid_size, sizeof(float));
    world->phase_progress = calloc(grid_size, sizeof(float));
    
    /* Allocate pressure/density arrays */
    world->pressure = calloc(grid_size, sizeof(float));
    world->density = calloc(grid_size, sizeof(float));
    world->effective_density = calloc(grid_size, sizeof(float));
    
    /* Allocate chemical arrays */
    world->oxygen = calloc(grid_size, sizeof(float));
    world->fuel = calloc(grid_size, sizeof(float));
    world->reaction = calloc(grid_size, sizeof(float));
    
    /* Allocate fluid arrays */
    world->moisture = calloc(grid_size, sizeof(float));
    world->fill = calloc(grid_size, sizeof(float));
    
    /* Allocate velocity arrays */
    world->vel_x = calloc(grid_size, sizeof(Fixed8));
    world->vel_y = calloc(grid_size, sizeof(Fixed8));
    
    /* Allocate chunk arrays */
    world->chunk_active = calloc(chunk_count, sizeof(bool));
    world->chunk_active_next = calloc(chunk_count, sizeof(bool));
    
    /* Check all allocations */
    if (!world->mat || !world->mat_next || !world->flags || 
        !world->color_seed || !world->lifetime ||
        !world->temp || !world->temp_next || !world->energy || !world->phase_progress ||
        !world->pressure || !world->density || !world->effective_density ||
        !world->oxygen || !world->fuel || !world->reaction ||
        !world->moisture || !world->fill ||
        !world->vel_x || !world->vel_y ||
        !world->chunk_active || !world->chunk_active_next) {
        world_destroy(world);
        return NULL;
    }
    
    /* Initialize color seeds with random values */
    uint32_t seed = 12345;
    for (size_t i = 0; i < grid_size; i++) {
        world->color_seed[i] = xorshift32(&seed);
    }
    
    /* Initialize to ambient conditions */
    world_reset(world);
    
    return world;
}

void world_destroy(World* world) {
    if (!world) return;
    
    free(world->mat);
    free(world->mat_next);
    free(world->flags);
    free(world->color_seed);
    free(world->lifetime);
    free(world->temp);
    free(world->temp_next);
    free(world->energy);
    free(world->phase_progress);
    free(world->pressure);
    free(world->density);
    free(world->effective_density);
    free(world->oxygen);
    free(world->fuel);
    free(world->reaction);
    free(world->moisture);
    free(world->fill);
    free(world->vel_x);
    free(world->vel_y);
    free(world->chunk_active);
    free(world->chunk_active_next);
    free(world);
}

/* =============================================================================
 * World Clearing and Reset
 * ============================================================================= */

void world_clear(World* world) {
    size_t grid_size = (size_t)world->width * world->height;
    
    /* All cells become air */
    memset(world->mat, MAT_AIR, grid_size * sizeof(MaterialID));
    memset(world->mat_next, MAT_AIR, grid_size * sizeof(MaterialID));
    memset(world->flags, 0, grid_size * sizeof(CellFlags));
    memset(world->lifetime, 0, grid_size * sizeof(uint8_t));
    
    /* Reset physics state */
    memset(world->pressure, 0, grid_size * sizeof(float));
    memset(world->moisture, 0, grid_size * sizeof(float));
    memset(world->fill, 0, grid_size * sizeof(float));
    memset(world->fuel, 0, grid_size * sizeof(float));
    memset(world->reaction, 0, grid_size * sizeof(float));
    memset(world->phase_progress, 0, grid_size * sizeof(float));
    memset(world->vel_x, 0, grid_size * sizeof(Fixed8));
    memset(world->vel_y, 0, grid_size * sizeof(Fixed8));
    
    /* Reset to ambient conditions */
    for (size_t i = 0; i < grid_size; i++) {
        world->temp[i] = 20.0f;           /* Room temperature */
        world->temp_next[i] = 20.0f;
        world->oxygen[i] = 0.21f;         /* Atmospheric O2 */
        world->density[i] = AIR_DENSITY;
        world->effective_density[i] = AIR_DENSITY;
        world->pressure[i] = ATM_PRESSURE;
        
        /* Calculate air energy at ambient temp (must convert to Kelvin!) */
        const MaterialProps* air = material_get(MAT_AIR);
        world->energy[i] = air->mass * air->heat_capacity * (20.0f + 273.15f);
    }
    
    world->total_thermal_energy = 0.0;
    world->total_chemical_energy = 0.0;
}

void world_reset(World* world) {
    world_clear(world);
}

/* =============================================================================
 * Material Access
 * ============================================================================= */

MaterialID world_get_mat(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return MAT_AIR;
    return world->mat[IDX(x, y)];
}

void world_set_mat(World* world, int x, int y, MaterialID mat) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);
    
    MaterialID old_mat = world->mat[idx];
    world->mat[idx] = mat;
    
    /* Reset velocity */
    world->vel_x[idx] = 0;
    world->vel_y[idx] = 0;
    
    /* Update material-dependent properties */
    const MaterialProps* props = material_get(mat);
    world->density[idx] = props->density;
    world->pressure[idx] = (props->state == STATE_GAS) ? ATM_PRESSURE : 0.0f;
    world->phase_progress[idx] = 0.0f;
    
    /* Set moisture for wet materials */
    world->moisture[idx] = (mat == MAT_MUD || mat == MAT_WET_WOOD) ? 1.0f : 0.0f;
    
    /* Set fill for fluids/gases */
    world->fill[idx] = (props->state == STATE_FLUID || props->state == STATE_GAS) ? 1.0f : 0.0f;
    
    /* Set fuel for combustibles */
    world->fuel[idx] = (props->heat_of_combustion > 0.0f) ? 1.0f : 0.0f;
    
    /* Reset reaction progress */
    world->reaction[idx] = 0.0f;
    
    /* Set oxygen for air cells */
    if (mat == MAT_AIR) {
        world->oxygen[idx] = 0.21f;
    } else if (props->state != STATE_GAS) {
        world->oxygen[idx] = 0.0f;  /* Solids/liquids don't hold O2 */
    }
    
    /* Set appropriate initial temperature based on material */
    float temp;
    if (mat == MAT_FIRE) {
        temp = 900.0f;      /* Fire is hot */
    } else if (mat == MAT_LAVA) {
        temp = 1200.0f;     /* Lava is very hot */
    } else if (mat == MAT_MOLTEN_METAL) {
        temp = 1500.0f;     /* Molten metal is extremely hot */
    } else if (mat == MAT_EMBER) {
        temp = 500.0f;      /* Embers are warm */
    } else if (mat == MAT_ICE) {
        temp = -10.0f;      /* Ice is cold */
    } else if (mat == MAT_STEAM) {
        temp = 110.0f;      /* Steam is above boiling */
    } else {
        temp = AMBIENT_TEMP; /* Default: room temperature (20°C) */
    }
    world->temp[idx] = temp;
    world->temp_next[idx] = temp;
    
    /* Calculate initial energy from temperature */
    world->energy[idx] = props->mass * props->heat_capacity * (temp + 273.15f);
    
    /* Update effective density */
    world_update_effective_density(world, x, y);
    
    /* Activate chunk */
    world_activate_chunk_at(world, x, y);
}

void world_set_mat_next(World* world, int x, int y, MaterialID mat) {
    if (!IN_BOUNDS(x, y)) return;
    world->mat_next[IDX(x, y)] = mat;
}

/* =============================================================================
 * Flag Operations
 * ============================================================================= */

CellFlags world_get_flags(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return FLAG_NONE;
    return world->flags[IDX(x, y)];
}

void world_set_flags(World* world, int x, int y, CellFlags flags) {
    if (!IN_BOUNDS(x, y)) return;
    world->flags[IDX(x, y)] = flags;
}

void world_add_flag(World* world, int x, int y, CellFlags flag) {
    if (!IN_BOUNDS(x, y)) return;
    world->flags[IDX(x, y)] |= flag;
}

void world_remove_flag(World* world, int x, int y, CellFlags flag) {
    if (!IN_BOUNDS(x, y)) return;
    world->flags[IDX(x, y)] &= ~flag;
}

bool world_has_flag(const World* world, int x, int y, CellFlags flag) {
    if (!IN_BOUNDS(x, y)) return false;
    return (world->flags[IDX(x, y)] & flag) != 0;
}

/* =============================================================================
 * State Queries
 * ============================================================================= */

bool world_is_empty(const World* world, int x, int y) {
    return world_get_mat(world, x, y) == MAT_AIR;
}

bool world_is_air(const World* world, int x, int y) {
    return world_get_mat(world, x, y) == MAT_AIR;
}

bool world_is_solid(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return true;  /* Out of bounds treated as solid */
    return material_is_solid(world_get_mat(world, x, y));
}

/* =============================================================================
 * Cell Operations
 * ============================================================================= */

void world_swap_cells(World* world, int x1, int y1, int x2, int y2) {
    if (!IN_BOUNDS(x1, y1) || !IN_BOUNDS(x2, y2)) return;
    
    int idx1 = IDX(x1, y1);
    int idx2 = IDX(x2, y2);
    
    /* Swap material */
    MaterialID tmp_mat = world->mat[idx1];
    world->mat[idx1] = world->mat[idx2];
    world->mat[idx2] = tmp_mat;
    
    /* Swap color seed */
    uint32_t tmp_seed = world->color_seed[idx1];
    world->color_seed[idx1] = world->color_seed[idx2];
    world->color_seed[idx2] = tmp_seed;
    
    /* Swap velocity */
    Fixed8 tmp_vx = world->vel_x[idx1];
    Fixed8 tmp_vy = world->vel_y[idx1];
    world->vel_x[idx1] = world->vel_x[idx2];
    world->vel_y[idx1] = world->vel_y[idx2];
    world->vel_x[idx2] = tmp_vx;
    world->vel_y[idx2] = tmp_vy;
    
    /* Swap temperature */
    float tmp_temp = world->temp[idx1];
    world->temp[idx1] = world->temp[idx2];
    world->temp[idx2] = tmp_temp;
    
    /* Swap energy */
    float tmp_energy = world->energy[idx1];
    world->energy[idx1] = world->energy[idx2];
    world->energy[idx2] = tmp_energy;
    
    /* Swap oxygen */
    float tmp_o2 = world->oxygen[idx1];
    world->oxygen[idx1] = world->oxygen[idx2];
    world->oxygen[idx2] = tmp_o2;
    
    /* Swap moisture */
    float tmp_moist = world->moisture[idx1];
    world->moisture[idx1] = world->moisture[idx2];
    world->moisture[idx2] = tmp_moist;
    
    /* Swap fill */
    float tmp_fill = world->fill[idx1];
    world->fill[idx1] = world->fill[idx2];
    world->fill[idx2] = tmp_fill;
    
    /* Swap fuel */
    float tmp_fuel = world->fuel[idx1];
    world->fuel[idx1] = world->fuel[idx2];
    world->fuel[idx2] = tmp_fuel;
    
    /* Swap reaction */
    float tmp_react = world->reaction[idx1];
    world->reaction[idx1] = world->reaction[idx2];
    world->reaction[idx2] = tmp_react;
    
    /* Swap phase progress */
    float tmp_phase = world->phase_progress[idx1];
    world->phase_progress[idx1] = world->phase_progress[idx2];
    world->phase_progress[idx2] = tmp_phase;
    
    /* Swap lifetime */
    uint8_t tmp_life = world->lifetime[idx1];
    world->lifetime[idx1] = world->lifetime[idx2];
    world->lifetime[idx2] = tmp_life;
    
    /* Swap effective density */
    float tmp_eff_dens = world->effective_density[idx1];
    world->effective_density[idx1] = world->effective_density[idx2];
    world->effective_density[idx2] = tmp_eff_dens;
    
    /* Swap density */
    float tmp_dens = world->density[idx1];
    world->density[idx1] = world->density[idx2];
    world->density[idx2] = tmp_dens;
    
    /* Activate both chunks */
    world_activate_chunk_at(world, x1, y1);
    world_activate_chunk_at(world, x2, y2);
}

void world_copy_cell(World* world, int src_x, int src_y, int dst_x, int dst_y) {
    if (!IN_BOUNDS(src_x, src_y) || !IN_BOUNDS(dst_x, dst_y)) return;
    
    int src = IDX(src_x, src_y);
    int dst = IDX(dst_x, dst_y);
    
    world->mat[dst] = world->mat[src];
    world->color_seed[dst] = world->color_seed[src];
    world->vel_x[dst] = world->vel_x[src];
    world->vel_y[dst] = world->vel_y[src];
    world->temp[dst] = world->temp[src];
    world->energy[dst] = world->energy[src];
    world->oxygen[dst] = world->oxygen[src];
    world->moisture[dst] = world->moisture[src];
    world->fill[dst] = world->fill[src];
    world->fuel[dst] = world->fuel[src];
    world->reaction[dst] = world->reaction[src];
    world->phase_progress[dst] = world->phase_progress[src];
    world->lifetime[dst] = world->lifetime[src];
    world->density[dst] = world->density[src];
    world->effective_density[dst] = world->effective_density[src];
    
    world_activate_chunk_at(world, dst_x, dst_y);
}

/* =============================================================================
 * Heat Operations
 * ============================================================================= */

void world_add_heat(World* world, int x, int y, float joules) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);
    
    world->energy[idx] += joules;
    
    /* Update temperature from energy */
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    
    if (props->thermal_mass > 0.0001f) {
        /* T = E / (m * c) - 273.15 (convert from K to C) */
        world->temp[idx] = (world->energy[idx] / props->thermal_mass) - 273.15f;
    }
    
    world_activate_chunk_at(world, x, y);
}

/* =============================================================================
 * Effective Density
 * ============================================================================= */

void world_update_effective_density(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    int idx = IDX(x, y);
    
    MaterialID mat = world->mat[idx];
    const MaterialProps* props = material_get(mat);
    float temp = world->temp[idx];
    
    /* rho_eff = rho_0 / (1 + beta * dT) */
    float dT = temp - 20.0f;  /* Reference at 20°C */
    float expansion = 1.0f + props->thermal_expansion * dT;
    
    if (expansion > 0.01f) {
        world->effective_density[idx] = props->density / expansion;
    } else {
        world->effective_density[idx] = props->density;
    }
}

void world_update_all_densities(World* world) {
    for (int y = 0; y < world->height; y++) {
        for (int x = 0; x < world->width; x++) {
            world_update_effective_density(world, x, y);
        }
    }
}

/* =============================================================================
 * Chunk Management
 * ============================================================================= */

void world_activate_chunk(World* world, int chunk_x, int chunk_y) {
    if (chunk_x < 0 || chunk_x >= CHUNKS_X || chunk_y < 0 || chunk_y >= CHUNKS_Y) return;
    int idx = chunk_y * CHUNKS_X + chunk_x;
    world->chunk_active[idx] = true;
    world->chunk_active_next[idx] = true;
}

void world_activate_chunk_at(World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) return;
    int chunk_x = x / CHUNK_SIZE;
    int chunk_y = y / CHUNK_SIZE;
    
    world_activate_chunk(world, chunk_x, chunk_y);
    
    /* Activate neighbor chunks if near boundary */
    int local_x = x % CHUNK_SIZE;
    int local_y = y % CHUNK_SIZE;
    
    if (local_x < 2) {
        world_activate_chunk(world, chunk_x - 1, chunk_y);
        if (local_y < 2) world_activate_chunk(world, chunk_x - 1, chunk_y - 1);
        if (local_y >= CHUNK_SIZE - 2) world_activate_chunk(world, chunk_x - 1, chunk_y + 1);
    }
    
    if (local_x >= CHUNK_SIZE - 2) {
        world_activate_chunk(world, chunk_x + 1, chunk_y);
        if (local_y < 2) world_activate_chunk(world, chunk_x + 1, chunk_y - 1);
        if (local_y >= CHUNK_SIZE - 2) world_activate_chunk(world, chunk_x + 1, chunk_y + 1);
    }
    
    if (local_y < 2) {
        world_activate_chunk(world, chunk_x, chunk_y - 1);
    }
    
    if (local_y >= CHUNK_SIZE - 2) {
        world_activate_chunk(world, chunk_x, chunk_y + 1);
    }
}

bool world_is_chunk_active(const World* world, int chunk_x, int chunk_y) {
    if (chunk_x < 0 || chunk_x >= CHUNKS_X || chunk_y < 0 || chunk_y >= CHUNKS_Y) return false;
    int idx = chunk_y * CHUNKS_X + chunk_x;
    return world->chunk_active[idx];
}

void world_swap_buffers(World* world) {
    /* For materials, we typically don't double-buffer in cellular automata */
    (void)world;
}

void world_clear_tick_flags(World* world) {
    size_t grid_size = (size_t)world->width * world->height;
    for (size_t i = 0; i < grid_size; i++) {
        world->flags[i] &= ~FLAG_UPDATED;
    }
}

void world_clear_chunk_activation(World* world) {
    memset(world->chunk_active_next, 0, CHUNK_COUNT * sizeof(bool));
}

void world_update_chunk_activation(World* world) {
    bool* tmp = world->chunk_active;
    world->chunk_active = world->chunk_active_next;
    world->chunk_active_next = tmp;
    
    world->active_chunks = 0;
    for (int i = 0; i < CHUNK_COUNT; i++) {
        if (world->chunk_active[i]) {
            world->active_chunks++;
        }
    }
}

/* =============================================================================
 * Painting Functions
 * ============================================================================= */

void world_paint_circle(World* world, int cx, int cy, int radius, MaterialID mat) {
    int r2 = radius * radius;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int x = cx + dx;
                int y = cy + dy;
                if (IN_BOUNDS(x, y)) {
                    world_set_mat(world, x, y, mat);
                }
            }
        }
    }
}

void world_paint_circle_temp(World* world, int cx, int cy, int radius, MaterialID mat, float temp) {
    int r2 = radius * radius;
    
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= r2) {
                int x = cx + dx;
                int y = cy + dy;
                if (IN_BOUNDS(x, y)) {
                    world_set_mat(world, x, y, mat);
                    world_set_temp(world, x, y, temp);
                    
                    /* Update energy from new temperature */
                    int idx = IDX(x, y);
                    const MaterialProps* props = material_get(mat);
                    world->energy[idx] = props->mass * props->heat_capacity * (temp + 273.15f);
                    
                    world_update_effective_density(world, x, y);
                }
            }
        }
    }
}

void world_paint_line(World* world, int x0, int y0, int x1, int y1, int radius, MaterialID mat) {
    /* Bresenham's line algorithm with thickness */
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    
    while (1) {
        world_paint_circle(world, x0, y0, radius, mat);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/* =============================================================================
 * Rendering Support
 * ============================================================================= */

Color world_get_cell_color(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) {
        return (Color){0, 0, 0, 255};
    }
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    uint32_t seed = world->color_seed[idx];
    
    return material_color(mat, seed);
}

Color world_get_cell_color_thermal(const World* world, int x, int y) {
    if (!IN_BOUNDS(x, y)) {
        return (Color){0, 0, 0, 255};
    }
    
    int idx = IDX(x, y);
    MaterialID mat = world->mat[idx];
    uint32_t seed = world->color_seed[idx];
    float temp = world->temp[idx];
    
    return material_color_with_temp(mat, seed, temp);
}

/* =============================================================================
 * Energy Conservation
 * ============================================================================= */

double world_calculate_total_energy(const World* world) {
    double total = 0.0;
    size_t grid_size = (size_t)world->width * world->height;
    
    for (size_t i = 0; i < grid_size; i++) {
        total += world->energy[i];
    }
    
    return total;
}

bool world_validate_energy(const World* world, double expected, double tolerance) {
    double actual = world_calculate_total_energy(world);
    double error = fabs(actual - expected);
    return error <= tolerance;
}
