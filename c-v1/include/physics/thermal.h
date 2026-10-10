/*
 * thermal.h - Energy-Based Thermal Simulation System
 * 
 * Implements realistic heat transfer using:
 * - Fourier's law for conduction
 * - Stefan-Boltzmann law for radiation
 * - Latent heat for phase transitions
 * - Energy conservation tracking
 */
#ifndef THERMAL_H
#define THERMAL_H

#include "core/types.h"
#include "world/world.h"
#include "engine/simulation.h"

/* =============================================================================
 * Thermal Constants
 * ============================================================================= */

/* Temperature limits (Celsius) */
#define AMBIENT_TEMP            20.0f       /* Room temperature */
#define MIN_TEMPERATURE         -273.15f    /* Absolute zero */
#define MAX_TEMPERATURE         3000.0f     /* Simulation limit */

/* Heat transfer constants */
#define HEAT_DIFFUSION_RATE     0.05f   /* Very slow spread - prevents heat leaking through walls */
#define AMBIENT_COOLING_RATE    0.0005f /* Fraction of temp diff lost to air per tick (very slow) */
#define CONDUCTION_BOOST_FACTOR 3.0f        /* Extra boost for high conductivity materials */
/* Note: RADIATION_SCALE is defined in core/types.h */

/* Fire/Heat source temperatures */
#define FIRE_TEMPERATURE        900.0f      /* Fire core temperature (C) */
#define LAVA_TEMPERATURE        1000.0f     /* Lava temperature (C) */
#define EMBER_TEMPERATURE       600.0f      /* Hot ember temperature (C) */

/* Phase change thresholds */
#define PHASE_CHANGE_TOLERANCE  1.0f        /* Degrees C tolerance */

/* =============================================================================
 * Phase Transition State
 * ============================================================================= */

typedef enum {
    PHASE_NONE = 0,
    PHASE_MELTING,      /* Solid -> Liquid */
    PHASE_FREEZING,     /* Liquid -> Solid */
    PHASE_BOILING,      /* Liquid -> Gas */
    PHASE_CONDENSING    /* Gas -> Liquid */
} PhaseTransitionType;

typedef struct {
    PhaseTransitionType type;
    MaterialID from_mat;
    MaterialID to_mat;
    float required_energy;  /* Total energy needed (J) */
    float current_energy;   /* Energy accumulated so far (J) */
    float progress;         /* 0.0 to 1.0 */
} PhaseTransition;

/* =============================================================================
 * Main Thermal Update
 * ============================================================================= */

/* Main thermal simulation update */
void thermal_update(Simulation* sim, World* world);

/* =============================================================================
 * Heat Transfer Functions
 * ============================================================================= */

/* Calculate heat transfer between two cells (Fourier's Law) */
float thermal_calc_conduction(World* world, int x1, int y1, int x2, int y2);

/* Apply heat diffusion to a single cell */
void thermal_apply_conduction(World* world, int x, int y);

/* Apply radiative heat loss (Stefan-Boltzmann) */
void thermal_apply_radiation(World* world, int x, int y);

/* Apply cooling towards ambient temperature */
void thermal_apply_ambient_cooling(World* world, int x, int y);

/* =============================================================================
 * Phase Transitions
 * ============================================================================= */

/* Check and process phase changes for a cell */
void thermal_check_phase_change(Simulation* sim, World* world, int x, int y);

/* Get the phase transition type for a material at given temperature */
PhaseTransitionType thermal_get_transition_type(MaterialID mat, float temp);

/* Get the target material for a phase transition */
MaterialID thermal_get_transition_target(MaterialID mat, PhaseTransitionType type);

/* Calculate energy required for phase transition */
float thermal_calc_transition_energy(MaterialID mat, PhaseTransitionType type);

/* =============================================================================
 * Auto-Ignition System
 * ============================================================================= */

/* Check if a flammable material should catch fire based on temperature */
void thermal_check_ignition(Simulation* sim, World* world, int x, int y);

/* =============================================================================
 * Heat Sources
 * ============================================================================= */

/* Update heat sources (fire, lava, etc.) */
void thermal_update_heat_sources(World* world);

/* Check if material is a heat source */
static inline bool thermal_is_heat_source(MaterialID mat) {
    return mat == MAT_FIRE || mat == MAT_LAVA || mat == MAT_EMBER;
}

/* Get heat source temperature */
static inline float thermal_get_source_temp(MaterialID mat) {
    switch (mat) {
        case MAT_FIRE: return FIRE_TEMPERATURE;
        case MAT_LAVA: return LAVA_TEMPERATURE;
        case MAT_EMBER: return EMBER_TEMPERATURE;
        default: return AMBIENT_TEMP;
    }
}

/* =============================================================================
 * Energy Conservation
 * ============================================================================= */

/* Calculate total thermal energy in world (for debugging) */
double thermal_calc_total_energy(const World* world);

/* Validate energy conservation */
bool thermal_validate_conservation(const World* world, double prev_energy, double tolerance);

/* =============================================================================
 * Utility Functions
 * ============================================================================= */

/* Convert between temperature and energy */
static inline float thermal_temp_to_energy(float temp_c, float mass, float heat_capacity) {
    /* E = m * c * T (in Kelvin) */
    return mass * heat_capacity * C_TO_K(temp_c);
}

static inline float thermal_energy_to_temp(float energy, float mass, float heat_capacity) {
    /* T = E / (m * c) - 273.15 */
    if (mass * heat_capacity < 0.0001f) return AMBIENT_TEMP;
    return K_TO_C(energy / (mass * heat_capacity));
}

/* Clamp temperature to valid range */
static inline float thermal_clamp_temp(float temp) {
    return CLAMP(temp, MIN_TEMPERATURE, MAX_TEMPERATURE);
}

/* Check if cell is hot enough to glow */
static inline bool thermal_is_glowing(float temp) {
    return temp > 500.0f; /* ~773K - starts to glow red */
}

#endif /* THERMAL_H */
