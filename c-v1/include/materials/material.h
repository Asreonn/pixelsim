/*
 * material.h - Data-driven material system with engineering-level physics
 */
#ifndef MATERIAL_H
#define MATERIAL_H

#include "core/types.h"

/* =============================================================================
 * Material Properties Structure (data-driven)
 * 
 * Contains all physical properties needed for realistic simulation:
 * - Mechanical properties (density, viscosity, etc.)
 * - Thermal properties (conductivity, heat capacity, latent heat)
 * - Chemical properties (combustion, corrosion, reactions)
 * - Visual properties (color, glow)
 * ============================================================================= */

typedef struct {
    /* =========================================================================
     * Identity
     * ========================================================================= */
    MaterialID id;
    const char* name;
    MaterialState state;
    
    /* =========================================================================
     * Visual Properties
     * ========================================================================= */
    Color base_color;
    uint8_t color_variation;      /* Random variation in color (0-255) */
    
    /* =========================================================================
     * Mechanical Properties (SI units)
     * ========================================================================= */
    float density;                /* kg/m^3 */
    float mass;                   /* kg per cell (derived from density) */
    float friction;               /* 0-1, for solids/powders */
    float restitution;            /* bounce factor 0-1 */
    float cohesion;               /* 0-1, clumpiness for powders */
    float viscosity;              /* Pa*s (dynamic viscosity) */
    float surface_tension;        /* N/m */
    float porosity;               /* 0-1 (void fraction) */
    float permeability;           /* m^2 (Darcy permeability) */
    float bulk_modulus;           /* Pa (compressibility) */
    float thermal_expansion;      /* 1/K (volumetric) */
    
    /* =========================================================================
     * Motion Properties
     * ========================================================================= */
    float gravity_scale;          /* multiplier (1.0 = normal, -1.0 = rises) */
    float drag_coeff;             /* 0-1, air/fluid resistance */
    float terminal_velocity;      /* cells/tick (max speed) */
    float flow_rate;              /* 0-1, horizontal flow probability */
    
    /* Fixed-point cached values (8.8 format) */
    Fixed8 gravity_step_fixed;
    Fixed8 drag_factor_fixed;
    Fixed8 terminal_velocity_fixed;
    
    /* Powder-specific */
    float settle_probability;     /* 0-1, chance to stop jittering */
    float slide_bias;             /* 0-1, randomness in left/right slide */
    
    /* =========================================================================
     * Thermal Properties (SI units)
     * ========================================================================= */
    float conductivity;           /* W/(m*K) - thermal conductivity */
    float heat_capacity;          /* J/(kg*K) - specific heat capacity */
    float emissivity;             /* 0-1 - Stefan-Boltzmann radiation */
    
    /* Phase change temperatures (Celsius) */
    float melting_temp;           /* Solid -> Liquid */
    float boiling_temp;           /* Liquid -> Gas */
    float freezing_temp;          /* Liquid -> Solid (can differ from melting) */
    
    /* Latent heat (J/kg) - energy absorbed/released during phase change */
    float latent_heat_fusion;     /* Melting/Freezing */
    float latent_heat_vaporization; /* Boiling/Condensation */
    
    /* =========================================================================
     * Combustion Properties
     * ========================================================================= */
    float flammability;           /* 0-1: 0=non-flammable (metal/stone), 1=highly flammable (oil) */
    float ignition_temp;          /* C - temperature to catch fire */
    float heat_of_combustion;     /* J/kg - energy released when burned */
    float activation_energy;      /* J/mol - Arrhenius activation energy */
    float oxygen_consumption;     /* mol O2 / mol fuel per tick */
    float stoichiometric_ratio;   /* kg air / kg fuel for complete burn */
    float burn_rate;              /* Base consumption rate (derived) */
    float smoke_rate;             /* 0-1, smoke production probability */
    
    /* =========================================================================
     * Gas Properties (for ideal gas law)
     * ========================================================================= */
    float molar_mass;             /* kg/mol */
    float specific_gas_constant;  /* J/(kg*K) = R/M */
    float adiabatic_index;        /* gamma = Cp/Cv (typically 1.4 for air) */
    float compressibility;        /* 1/Pa - isothermal compressibility */
    
    /* =========================================================================
     * Surface Interaction Properties
     * ========================================================================= */
    float cohesion_strength;      /* N/m - same material attraction */
    float adhesion_water;         /* 0-1 - attraction to water */
    float adhesion_oil;           /* 0-1 - attraction to oil */
    
    /* =========================================================================
     * Chemical Properties
     * ========================================================================= */
    float solubility_water;       /* kg/L - solubility in water */
    float corrosion_resistance;   /* 0-1 - resistance to acid */
    float reactivity;             /* 0-1 - general chemical reactivity */
    float freezing_point_depression; /* K*kg/mol - for salt-like materials */
    
    /* =========================================================================
     * Electrical Properties (future use)
     * ========================================================================= */
    float electrical_conductivity; /* S/m */
    
    /* =========================================================================
     * Cached/Derived Values (computed at init)
     * ========================================================================= */
    float thermal_mass;           /* m * c (J/K per cell) */
    float inv_thermal_mass;       /* 1 / (m * c) for fast division */
    
} MaterialProps;

/* =============================================================================
 * Material Table Access
 * ============================================================================= */

/* Initialize material table with all properties */
void material_init(void);

/* Get material properties by ID */
const MaterialProps* material_get(MaterialID id);

/* Get material state by ID (convenience) */
MaterialState material_state(MaterialID id);

/* =============================================================================
 * Material State Queries (LUT-based for performance)
 * ============================================================================= */

bool material_is_powder(MaterialID id);
bool material_is_fluid(MaterialID id);
bool material_is_solid(MaterialID id);
bool material_is_gas(MaterialID id);

/* Check if material is air (for empty space checks) */
static inline bool material_is_empty(MaterialID id) {
    return id == MAT_AIR;
}

/* Check if material is air (explicit name) */
static inline bool material_is_air(MaterialID id) {
    return id == MAT_AIR;
}

/* =============================================================================
 * Color Functions
 * ============================================================================= */

/* Get base color for material (with optional variation) */
Color material_color(MaterialID id, uint32_t seed);

/* Get color with temperature-based glow effect */
Color material_color_with_temp(MaterialID id, uint32_t seed, float temp);

/* Get fire/glow color based on temperature (blackbody approximation) */
Color material_temp_to_glow_color(float temp_celsius);

/* =============================================================================
 * Material Property Helpers
 * ============================================================================= */

/* Check if material can burn (has flammability > 0) */
static inline bool material_can_burn(MaterialID id) {
    const MaterialProps* props = material_get(id);
    return props->flammability > 0.0f && props->ignition_temp < 9000.0f;
}

/* Check if material is highly flammable (flammability >= 0.5) */
static inline bool material_is_flammable(MaterialID id) {
    const MaterialProps* props = material_get(id);
    return props->flammability >= 0.5f;
}

/* Get flammability value */
static inline float material_get_flammability(MaterialID id) {
    const MaterialProps* props = material_get(id);
    return props->flammability;
}

/* Check if material can melt */
static inline bool material_can_melt(MaterialID id) {
    const MaterialProps* props = material_get(id);
    return props->melting_temp < 9000.0f; /* 9999 = can't melt */
}

/* Check if material can boil */
static inline bool material_can_boil(MaterialID id) {
    const MaterialProps* props = material_get(id);
    return props->boiling_temp < 9000.0f;
}

/* Check if material can freeze */
static inline bool material_can_freeze(MaterialID id) {
    const MaterialProps* props = material_get(id);
    return props->freezing_temp > -9000.0f;
}

/* Check if material conducts heat well */
static inline bool material_conducts_heat(MaterialID id) {
    const MaterialProps* props = material_get(id);
    return props->conductivity > 0.1f;
}

/* Check if material is affected by buoyancy */
static inline bool material_has_buoyancy(MaterialID id) {
    MaterialState state = material_state(id);
    return state == STATE_GAS || state == STATE_FLUID;
}

/* Get effective density at temperature */
static inline float material_effective_density(MaterialID id, float temp_celsius) {
    const MaterialProps* props = material_get(id);
    float dT = temp_celsius - 20.0f; /* Reference at 20C */
    return props->density / (1.0f + props->thermal_expansion * dT);
}

#endif /* MATERIAL_H */
