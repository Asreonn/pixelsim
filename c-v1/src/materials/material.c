/*
 * material.c - Data-driven material system implementation
 * 
 * Contains all material definitions with engineering-level physical properties.
 * Values are based on real-world measurements where applicable.
 */
#include "materials/material.h"
#include "core/utils.h"
#include <string.h>
#include <math.h>

/* =============================================================================
 * Material Table
 * ============================================================================= */

static MaterialProps g_materials[MAT_COUNT];

/* =============================================================================
 * Lookup Tables (LUTs) for fast material queries
 * ============================================================================= */

static MaterialState g_material_state_lut[MAT_COUNT];
static bool g_material_is_powder_lut[MAT_COUNT];
static bool g_material_is_fluid_lut[MAT_COUNT];
static bool g_material_is_solid_lut[MAT_COUNT];
static bool g_material_is_gas_lut[MAT_COUNT];

/* =============================================================================
 * Helper: Finalize derived values
 * ============================================================================= */

static void material_finalize(MaterialProps* mat) {
    /* Calculate mass from density */
    mat->mass = mat->density * CELL_VOLUME_M3;
    
    /* Calculate thermal mass: m * c */
    mat->thermal_mass = mat->mass * mat->heat_capacity;
    mat->inv_thermal_mass = (mat->thermal_mass > 0.0001f) 
                           ? (1.0f / mat->thermal_mass) 
                           : 0.0f;
    
    /* Set default bulk modulus if not specified */
    if (mat->bulk_modulus <= 0.0f) {
        if (mat->state == STATE_GAS) mat->bulk_modulus = 1.4e5f;
        else if (mat->state == STATE_FLUID) mat->bulk_modulus = 2.2e9f;
        else mat->bulk_modulus = 1.0e10f;
    }
    
    /* Set default thermal expansion if not specified */
    if (mat->thermal_expansion <= 0.0f) {
        if (mat->state == STATE_GAS) mat->thermal_expansion = 0.00341f; /* 1/293K */
        else if (mat->state == STATE_FLUID) mat->thermal_expansion = 0.00021f;
        else mat->thermal_expansion = 0.000012f;
    }
    
    /* Calculate gas constant if molar mass is set */
    if (mat->molar_mass > 0.0f && mat->specific_gas_constant <= 0.0f) {
        mat->specific_gas_constant = GAS_CONSTANT / mat->molar_mass;
    }
    
    /* Calculate fixed-point motion parameters */
    float effective_drag = mat->drag_coeff / (1.0f + mat->mass * 100.0f);
    mat->gravity_step_fixed = FIXED_FROM_FLOAT(GRAVITY_ACCEL * mat->gravity_scale);
    mat->drag_factor_fixed = FIXED_FROM_FLOAT(1.0f - effective_drag);
    mat->terminal_velocity_fixed = FIXED_FROM_FLOAT(mat->terminal_velocity);
    
    /* Default freezing temp to melting temp if not set */
    if (mat->freezing_temp < -9000.0f && mat->melting_temp < 9000.0f) {
        mat->freezing_temp = mat->melting_temp;
    }
}

/* =============================================================================
 * Material Initialization
 * ============================================================================= */

void material_init(void) {
    memset(g_materials, 0, sizeof(g_materials));
    
    /* =========================================================================
     * MAT_AIR - Atmospheric air (replaces EMPTY)
     * ========================================================================= */
    g_materials[MAT_AIR] = (MaterialProps){
        .id = MAT_AIR,
        .name = "Air",
        .state = STATE_GAS,
        .base_color = {0, 0, 0, 0},           /* Transparent */
        .color_variation = 0,
        .density = 1.225f,                     /* kg/m³ at 20°C, 1 atm */
        .viscosity = 0.0000181f,               /* Pa·s */
        .thermal_expansion = 0.00341f,         /* 1/K (≈1/T for ideal gas) */
        .gravity_scale = 0.0f,                 /* Statik - konveksiyon ayrı */
        .drag_coeff = 0.0f,
        .terminal_velocity = 0.0f,
        .flow_rate = 0.0f,
        .conductivity = 0.026f,                /* W/(m·K) */
        .heat_capacity = 1005.0f,              /* J/(kg·K) */
        .emissivity = 0.0f,                    /* Air doesn't radiate */
        .melting_temp = -9999.0f,
        .boiling_temp = -9999.0f,
        .freezing_temp = -9999.0f,
        .molar_mass = 0.029f,                  /* kg/mol */
        .adiabatic_index = 1.4f,               /* γ = Cp/Cv */
        .compressibility = 7.14e-6f,           /* 1/Pa */
        .flammability = 0.0f,                  /* Air doesn't burn */
    };
    
    /* =========================================================================
     * MAT_SAND - Silicon dioxide grains
     * ========================================================================= */
    g_materials[MAT_SAND] = (MaterialProps){
        .id = MAT_SAND,
        .name = "Sand",
        .state = STATE_POWDER,
        .base_color = {220, 190, 130, 255},
        .color_variation = 25,
        .density = 1600.0f,                    /* kg/m³ (loose) */
        .friction = 0.7f,
        .restitution = 0.0f,
        .cohesion = 0.15f,
        .porosity = 0.38f,
        .permeability = 2.0e-12f,
        .gravity_scale = 1.2f,
        .drag_coeff = 0.35f,
        .terminal_velocity = 2.8f,
        .flow_rate = 0.0f,
        .settle_probability = 0.25f,
        .slide_bias = 0.5f,
        .conductivity = 0.3f,                  /* W/(m·K) */
        .heat_capacity = 830.0f,               /* J/(kg·K) */
        .emissivity = 0.9f,
        .melting_temp = 1700.0f,               /* °C - forms glass */
        .boiling_temp = 9999.0f,
        .latent_heat_fusion = 50000.0f,        /* J/kg */
        .flammability = 0.0f,                  /* Sand doesn't burn */
        .ignition_temp = 9999.0f,
        .corrosion_resistance = 0.8f,
    };
    
    /* =========================================================================
     * MAT_STONE - Generic rock
     * ========================================================================= */
    g_materials[MAT_STONE] = (MaterialProps){
        .id = MAT_STONE,
        .name = "Stone",
        .state = STATE_SOLID,
        .base_color = {80, 80, 90, 255},
        .color_variation = 20,
        .density = 2600.0f,                    /* kg/m³ (granite-ish) */
        .friction = 0.9f,
        .restitution = 0.1f,
        .cohesion = 1.0f,
        .porosity = 0.02f,
        .gravity_scale = 0.0f,
        .conductivity = 2.5f,                  /* W/(m·K) */
        .heat_capacity = 1000.0f,              /* J/(kg·K) - Harder to heat */
        .emissivity = 0.93f,
        .melting_temp = 1000.0f,               /* °C - Harder to melt */
        .boiling_temp = 9999.0f,
        .latent_heat_fusion = 420000.0f,       /* J/kg */
        .flammability = 0.0f,                  /* Stone doesn't burn, only melts */
        .ignition_temp = 9999.0f,
        .corrosion_resistance = 0.6f,
    };
    
    /* =========================================================================
     * MAT_WATER - Liquid water (H₂O)
     * ========================================================================= */
    g_materials[MAT_WATER] = (MaterialProps){
        .id = MAT_WATER,
        .name = "Water",
        .state = STATE_FLUID,
        .base_color = {50, 100, 200, 180},
        .color_variation = 15,
        .density = 1000.0f,                    /* kg/m³ at 4°C */
        .viscosity = 0.001f,                   /* Pa·s at 20°C */
        .surface_tension = 0.0728f,            /* N/m */
        .thermal_expansion = 0.00021f,         /* 1/K */
        .gravity_scale = 1.0f,
        .drag_coeff = 0.15f,
        .terminal_velocity = 4.0f,
        .flow_rate = 0.7f,
        .conductivity = 0.6f,                  /* W/(m·K) */
        .heat_capacity = 4186.0f,              /* J/(kg·K) - very high! */
        .emissivity = 0.95f,
        .melting_temp = 0.0f,                  /* °C */
        .boiling_temp = 100.0f,                /* °C */
        .freezing_temp = 0.0f,
        .latent_heat_fusion = 334000.0f,       /* J/kg */
        .latent_heat_vaporization = 2260000.0f, /* J/kg */
        .cohesion_strength = 0.0728f,
        .flammability = 0.0f,                  /* Water doesn't burn */
        .ignition_temp = 9999.0f,
        .molar_mass = 0.018f,                  /* kg/mol */
    };
    
    /* =========================================================================
     * MAT_WOOD - Organic combustible solid
     * ========================================================================= */
    g_materials[MAT_WOOD] = (MaterialProps){
        .id = MAT_WOOD,
        .name = "Wood",
        .state = STATE_SOLID,
        .base_color = {139, 90, 43, 255},
        .color_variation = 25,
        .density = 600.0f,                     /* kg/m³ (pine) */
        .friction = 0.7f,
        .restitution = 0.1f,
        .cohesion = 1.0f,
        .porosity = 0.6f,
        .gravity_scale = 0.0f,
        .conductivity = 0.12f,                 /* W/(m·K) - poor conductor */
        .heat_capacity = 1700.0f,              /* J/(kg·K) */
        .emissivity = 0.9f,
        .melting_temp = 9999.0f,               /* Doesn't melt, burns */
        .boiling_temp = 9999.0f,
        .flammability = 0.8f,                  /* Highly flammable */
        .ignition_temp = 300.0f,               /* °C */
        .heat_of_combustion = 15000000.0f,     /* J/kg (15 MJ/kg) */
        .activation_energy = 125000.0f,        /* J/mol */
        .oxygen_consumption = 1.0f,
        .stoichiometric_ratio = 6.0f,
        .burn_rate = 0.15f,                    /* Base burn probability per tick */
        .smoke_rate = 0.3f,
        .corrosion_resistance = 0.3f,
    };
    
    /* =========================================================================
     * MAT_FIRE - Combustion plasma/hot gas
     * ========================================================================= */
    g_materials[MAT_FIRE] = (MaterialProps){
        .id = MAT_FIRE,
        .name = "Fire",
        .state = STATE_GAS,
        .base_color = {255, 150, 50, 220},
        .color_variation = 40,
        .density = 0.3f,                       /* kg/m³ (hot gas) */
        .viscosity = 0.00003f,
        .thermal_expansion = 0.003f,
        .gravity_scale = -1.0f,                /* Always rises (deterministic) */
        .drag_coeff = 0.2f,
        .terminal_velocity = 4.0f,
        .flow_rate = 0.6f,
        .conductivity = 0.05f,                 /* W/(m·K) */
        .heat_capacity = 1000.0f,              /* J/(kg·K) */
        .emissivity = 0.95f,                   /* High radiation */
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
        .flammability = 0.0f,                  /* Fire itself */
        .ignition_temp = 9999.0f,
        .oxygen_consumption = 0.1f,            /* Consumes O₂ */
        .smoke_rate = 0.2f,
    };
    
    /* =========================================================================
     * MAT_SMOKE - Combustion products (CO₂, particulates)
     * ========================================================================= */
    g_materials[MAT_SMOKE] = (MaterialProps){
        .id = MAT_SMOKE,
        .name = "Smoke",
        .state = STATE_GAS,
        .base_color = {60, 60, 60, 150},
        .color_variation = 20,
        .density = 1.1f,                       /* kg/m³ (slightly heavy) */
        .viscosity = 0.00002f,
        .thermal_expansion = 0.003f,
        .gravity_scale = -0.2f,                /* Rises slowly */
        .drag_coeff = 0.3f,
        .terminal_velocity = 2.0f,
        .flow_rate = 0.7f,
        .conductivity = 0.02f,                 /* W/(m·K) */
        .heat_capacity = 1000.0f,              /* J/(kg·K) */
        .emissivity = 0.1f,
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
        .flammability = 0.0f,                  /* Smoke doesn't burn */
        .molar_mass = 0.044f,                  /* kg/mol (CO₂) */
    };
    
    /* =========================================================================
     * MAT_SOIL - Organic earth material
     * ========================================================================= */
    g_materials[MAT_SOIL] = (MaterialProps){
        .id = MAT_SOIL,
        .name = "Soil",
        .state = STATE_POWDER,
        .base_color = {100, 70, 40, 255},
        .color_variation = 20,
        .density = 1800.0f,                    /* kg/m³ */
        .friction = 0.85f,
        .cohesion = 0.4f,
        .porosity = 0.45f,
        .gravity_scale = 1.1f,
        .drag_coeff = 0.4f,
        .terminal_velocity = 2.0f,
        .settle_probability = 0.4f,
        .slide_bias = 0.5f,
        .conductivity = 0.5f,                  /* W/(m·K) */
        .heat_capacity = 1000.0f,              /* J/(kg·K) */
        .emissivity = 0.92f,
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
        .flammability = 0.0f,                  /* Soil doesn't burn */
        .ignition_temp = 9999.0f,
        .corrosion_resistance = 0.4f,
    };
    
    /* =========================================================================
     * MAT_ICE - Solid water (H₂O)
     * ========================================================================= */
    g_materials[MAT_ICE] = (MaterialProps){
        .id = MAT_ICE,
        .name = "Ice",
        .state = STATE_SOLID,
        .base_color = {180, 220, 255, 200},
        .color_variation = 15,
        .density = 917.0f,                     /* kg/m³ - lighter than water! */
        .friction = 0.05f,                     /* Very slippery */
        .restitution = 0.2f,
        .cohesion = 1.0f,
        .gravity_scale = 0.0f,
        .conductivity = 2.2f,                  /* W/(m·K) - good conductor */
        .heat_capacity = 2090.0f,              /* J/(kg·K) */
        .emissivity = 0.97f,
        .melting_temp = 0.0f,                  /* °C */
        .boiling_temp = 100.0f,
        .freezing_temp = 0.0f,
        .latent_heat_fusion = 334000.0f,       /* J/kg */
        .latent_heat_vaporization = 2260000.0f,
        .flammability = 0.0f,                  /* Ice doesn't burn */
        .ignition_temp = 9999.0f,
    };
    
    /* =========================================================================
     * MAT_STEAM - Water vapor (H₂O gas)
     * ========================================================================= */
    g_materials[MAT_STEAM] = (MaterialProps){
        .id = MAT_STEAM,
        .name = "Steam",
        .state = STATE_GAS,
        .base_color = {220, 220, 230, 80},
        .color_variation = 10,
        .density = 0.6f,                       /* kg/m³ (hot steam) */
        .viscosity = 0.000012f,
        .thermal_expansion = 0.003f,
        .gravity_scale = -0.7f,                /* Rises fast */
        .drag_coeff = 0.2f,
        .terminal_velocity = 3.5f,
        .flow_rate = 0.8f,
        .conductivity = 0.02f,                 /* W/(m·K) */
        .heat_capacity = 2010.0f,              /* J/(kg·K) */
        .emissivity = 0.1f,
        .melting_temp = 0.0f,
        .boiling_temp = 100.0f,                /* Condenses below this */
        .freezing_temp = 0.0f,
        .latent_heat_vaporization = 2260000.0f,
        .flammability = 0.0f,                  /* Steam doesn't burn */
        .molar_mass = 0.018f,                  /* kg/mol */
    };
    
    /* =========================================================================
     * MAT_EMBER - Hot combustion remnant (replaces ASH)
     * ========================================================================= */
    g_materials[MAT_EMBER] = (MaterialProps){
        .id = MAT_EMBER,
        .name = "Ember",
        .state = STATE_POWDER,
        .base_color = {180, 80, 20, 255},
        .color_variation = 30,
        .density = 400.0f,                     /* kg/m³ (light char) */
        .friction = 0.4f,
        .cohesion = 0.1f,
        .gravity_scale = 0.5f,                 /* Falls slowly */
        .drag_coeff = 0.6f,
        .terminal_velocity = 1.5f,
        .settle_probability = 0.3f,
        .slide_bias = 0.5f,
        .conductivity = 0.1f,                  /* W/(m·K) */
        .heat_capacity = 800.0f,               /* J/(kg·K) */
        .emissivity = 0.85f,                   /* Glows! */
        .melting_temp = 9999.0f,
        .boiling_temp = 9999.0f,
        .flammability = 0.4f,                  /* Can reignite when hot */
        .ignition_temp = 250.0f,               /* °C - can reignite! */
        .heat_of_combustion = 5000000.0f,      /* J/kg (remaining energy) */
        .activation_energy = 80000.0f,
        .oxygen_consumption = 0.05f,
        .smoke_rate = 0.1f,
    };
    
    /* =========================================================================
     * MAT_ACID - Corrosive liquid (HCl/H₂SO₄ approximation)
     * ========================================================================= */
    g_materials[MAT_ACID] = (MaterialProps){
        .id = MAT_ACID,
        .name = "Acid",
        .state = STATE_FLUID,
        .base_color = {100, 255, 50, 200},
        .color_variation = 20,
        .density = 1100.0f,                    /* kg/m³ */
        .viscosity = 0.002f,
        .surface_tension = 0.05f,
        .gravity_scale = 1.0f,
        .drag_coeff = 0.15f,
        .terminal_velocity = 3.5f,
        .flow_rate = 0.7f,
        .conductivity = 0.5f,                  /* W/(m·K) */
        .heat_capacity = 3000.0f,              /* J/(kg·K) */
        .emissivity = 0.9f,
        .melting_temp = -20.0f,
        .boiling_temp = 120.0f,
        .flammability = 0.0f,                  /* Acid doesn't burn */
        .ignition_temp = 9999.0f,
        .reactivity = 0.9f,
    };
    
    /* =========================================================================
     * MAT_METAL - Steel/Iron approximation
     * ========================================================================= */
    g_materials[MAT_METAL] = (MaterialProps){
        .id = MAT_METAL,
        .name = "Metal",
        .state = STATE_SOLID,
        .base_color = {160, 160, 170, 255},
        .color_variation = 10,
        .density = 7800.0f,                    /* kg/m³ (steel) */
        .friction = 0.6f,
        .restitution = 0.3f,
        .cohesion = 1.0f,
        .gravity_scale = 0.0f,
        .conductivity = 50.0f,                 /* W/(m·K) - good conductor */
        .heat_capacity = 800.0f,               /* J/(kg·K) - Harder to heat */
        .emissivity = 0.3f,                    /* Low when cold, high when hot */
        .melting_temp = 9999.0f,               /* °C - Practically doesn't melt */
        .boiling_temp = 9999.0f,
        .latent_heat_fusion = 270000.0f,       /* J/kg */
        .flammability = 0.0f,                  /* Metal doesn't burn, only melts */
        .ignition_temp = 9999.0f,
        .corrosion_resistance = 0.5f,
    };
    
    /* =========================================================================
     * MAT_MUD - Wet soil mixture
     * ========================================================================= */
    g_materials[MAT_MUD] = (MaterialProps){
        .id = MAT_MUD,
        .name = "Mud",
        .state = STATE_POWDER,
        .base_color = {80, 55, 35, 255},
        .color_variation = 15,
        .density = 1900.0f,                    /* kg/m³ */
        .friction = 0.95f,
        .cohesion = 0.7f,
        .viscosity = 0.5f,
        .gravity_scale = 0.6f,
        .drag_coeff = 0.5f,
        .terminal_velocity = 1.5f,
        .flow_rate = 0.1f,
        .settle_probability = 0.6f,
        .slide_bias = 0.5f,
        .conductivity = 0.6f,                  /* W/(m·K) */
        .heat_capacity = 2000.0f,              /* J/(kg·K) */
        .emissivity = 0.92f,
        .melting_temp = 9999.0f,
        .boiling_temp = 80.0f,                 /* Dries to soil */
        .latent_heat_vaporization = 500000.0f, /* Water evaporation */
        .flammability = 0.0f,                  /* Mud doesn't burn */
        .ignition_temp = 9999.0f,
    };
    
    /* =========================================================================
     * MAT_LAVA - Molten rock
     * ========================================================================= */
    g_materials[MAT_LAVA] = (MaterialProps){
        .id = MAT_LAVA,
        .name = "Lava",
        .state = STATE_FLUID,
        .base_color = {255, 80, 0, 255},
        .color_variation = 40,
        .density = 2500.0f,                    /* kg/m³ */
        .viscosity = 100.0f,                   /* Pa·s - very viscous */
        .gravity_scale = 0.5f,
        .drag_coeff = 0.3f,
        .terminal_velocity = 1.5f,
        .flow_rate = 0.05f,                    /* Very slow viscous flow */
        .conductivity = 1.0f,                  /* W/(m·K) */
        .heat_capacity = 1000.0f,              /* J/(kg·K) */
        .emissivity = 0.95f,                   /* Very high radiation */
        .melting_temp = 700.0f,                /* Below this -> stone */
        .boiling_temp = 9999.0f,
        .freezing_temp = 700.0f,
        .latent_heat_fusion = 420000.0f,       /* J/kg */
        .flammability = 0.0f,                  /* Lava doesn't burn, it IS molten rock */
        .ignition_temp = 9999.0f,
        .smoke_rate = 0.1f,
    };
    
    /* =========================================================================
     * MAT_OIL - Flammable hydrocarbon liquid
     * ========================================================================= */
    g_materials[MAT_OIL] = (MaterialProps){
        .id = MAT_OIL,
        .name = "Oil",
        .state = STATE_FLUID,
        .base_color = {200, 180, 60, 220},
        .color_variation = 15,
        .density = 800.0f,                     /* kg/m³ - floats on water */
        .viscosity = 0.08f,
        .surface_tension = 0.03f,
        .gravity_scale = 0.95f,
        .drag_coeff = 0.18f,
        .terminal_velocity = 3.0f,
        .flow_rate = 0.45f,
        .conductivity = 0.15f,                 /* W/(m·K) */
        .heat_capacity = 2000.0f,              /* J/(kg·K) */
        .emissivity = 0.9f,
        .melting_temp = -20.0f,
        .boiling_temp = 300.0f,
        .flammability = 1.0f,                  /* HIGHLY flammable! */
        .ignition_temp = 200.0f,               /* °C - flammable */
        .heat_of_combustion = 45000000.0f,     /* J/kg (45 MJ/kg) */
        .activation_energy = 100000.0f,
        .oxygen_consumption = 3.5f,
        .stoichiometric_ratio = 15.0f,
        .burn_rate = 0.4f,                     /* Oil burns quickly */
        .smoke_rate = 0.8f,
        .cohesion_strength = 0.02f,
        .adhesion_water = 0.0f,                /* Doesn't mix with water */
    };
    
    /* =========================================================================
     * MAT_WET_WOOD - Water-soaked wood
     * ========================================================================= */
    g_materials[MAT_WET_WOOD] = (MaterialProps){
        .id = MAT_WET_WOOD,
        .name = "Wet Wood",
        .state = STATE_SOLID,
        .base_color = {110, 75, 50, 255},
        .color_variation = 15,
        .density = 900.0f,                     /* kg/m³ (heavier with water) */
        .friction = 0.85f,
        .cohesion = 1.0f,
        .porosity = 0.5f,
        .gravity_scale = 0.0f,
        .conductivity = 0.35f,                 /* W/(m·K) - better with water */
        .heat_capacity = 2500.0f,              /* J/(kg·K) */
        .emissivity = 0.9f,
        .melting_temp = 9999.0f,
        .boiling_temp = 100.0f,                /* Dries to regular wood */
        .latent_heat_vaporization = 800000.0f, /* More water to evaporate */
        .flammability = 0.1f,                  /* Very low - won't burn when wet */
        .ignition_temp = 9999.0f,              /* Won't burn when wet */
        .corrosion_resistance = 0.3f,
    };
    
    /* =========================================================================
     * MAT_GLASS - Amorphous silica (melted sand)
     * ========================================================================= */
    g_materials[MAT_GLASS] = (MaterialProps){
        .id = MAT_GLASS,
        .name = "Glass",
        .state = STATE_SOLID,
        .base_color = {200, 220, 255, 100},    /* Transparent blue tint */
        .color_variation = 5,
        .density = 2500.0f,                    /* kg/m³ */
        .friction = 0.4f,
        .restitution = 0.1f,
        .cohesion = 1.0f,
        .gravity_scale = 0.0f,
        .conductivity = 1.0f,                  /* W/(m·K) */
        .heat_capacity = 840.0f,               /* J/(kg·K) */
        .emissivity = 0.92f,
        .melting_temp = 1700.0f,               /* °C */
        .boiling_temp = 9999.0f,
        .latent_heat_fusion = 50000.0f,
        .flammability = 0.0f,                  /* Glass doesn't burn */
        .ignition_temp = 9999.0f,
        .corrosion_resistance = 0.7f,
    };
    
    /* =========================================================================
     * MAT_OBSIDIAN - Volcanic glass (water + lava)
     * ========================================================================= */
    g_materials[MAT_OBSIDIAN] = (MaterialProps){
        .id = MAT_OBSIDIAN,
        .name = "Obsidian",
        .state = STATE_SOLID,
        .base_color = {20, 15, 30, 255},       /* Dark purple-black */
        .color_variation = 10,
        .density = 2600.0f,                    /* kg/m³ */
        .friction = 0.6f,
        .restitution = 0.05f,
        .cohesion = 1.0f,
        .gravity_scale = 0.0f,
        .conductivity = 1.5f,                  /* W/(m·K) */
        .heat_capacity = 900.0f,               /* J/(kg·K) */
        .emissivity = 0.95f,
        .melting_temp = 1200.0f,               /* °C - back to lava */
        .boiling_temp = 9999.0f,
        .latent_heat_fusion = 400000.0f,
        .flammability = 0.0f,                  /* Obsidian doesn't burn */
        .ignition_temp = 9999.0f,
        .corrosion_resistance = 0.9f,          /* Very resistant */
    };
    
    /* =========================================================================
     * MAT_COAL - Solid carbon fuel
     * ========================================================================= */
    g_materials[MAT_COAL] = (MaterialProps){
        .id = MAT_COAL,
        .name = "Coal",
        .state = STATE_SOLID,
        .base_color = {30, 30, 35, 255},
        .color_variation = 10,
        .density = 1400.0f,                    /* kg/m³ */
        .friction = 0.7f,
        .restitution = 0.05f,
        .cohesion = 1.0f,
        .porosity = 0.1f,
        .gravity_scale = 0.0f,
        .conductivity = 0.2f,                  /* W/(m·K) */
        .heat_capacity = 1260.0f,              /* J/(kg·K) */
        .emissivity = 0.9f,
        .melting_temp = 9999.0f,               /* Doesn't melt, burns */
        .boiling_temp = 9999.0f,
        .flammability = 0.6f,                  /* Medium flammability - burns hot but slower to ignite */
        .ignition_temp = 427.0f,               /* °C - higher than wood */
        .heat_of_combustion = 32000000.0f,     /* J/kg (32 MJ/kg) */
        .activation_energy = 150000.0f,
        .oxygen_consumption = 2.67f,           /* C + O₂ → CO₂ */
        .stoichiometric_ratio = 11.5f,
        .burn_rate = 0.08f,                    /* Coal burns slowly but steadily */
        .smoke_rate = 0.5f,
        .corrosion_resistance = 0.6f,
    };
    
    /* =========================================================================
     * MAT_SALT - Sodium chloride (NaCl)
     * ========================================================================= */
    g_materials[MAT_SALT] = (MaterialProps){
        .id = MAT_SALT,
        .name = "Salt",
        .state = STATE_POWDER,
        .base_color = {240, 240, 245, 255},
        .color_variation = 5,
        .density = 2170.0f,                    /* kg/m³ */
        .friction = 0.6f,
        .cohesion = 0.3f,
        .gravity_scale = 1.1f,
        .drag_coeff = 0.35f,
        .terminal_velocity = 2.5f,
        .settle_probability = 0.35f,
        .slide_bias = 0.5f,
        .conductivity = 0.5f,                  /* W/(m·K) */
        .heat_capacity = 880.0f,               /* J/(kg·K) */
        .emissivity = 0.9f,
        .melting_temp = 801.0f,                /* °C */
        .boiling_temp = 1465.0f,
        .latent_heat_fusion = 520000.0f,
        .flammability = 0.0f,                  /* Salt doesn't burn */
        .ignition_temp = 9999.0f,
        .solubility_water = 0.36f,             /* kg/L */
        .freezing_point_depression = 1.86f,    /* K·kg/mol */
        .corrosion_resistance = 0.3f,
    };
    
    /* =========================================================================
     * MAT_MOLTEN_METAL - Liquid steel/iron
     * ========================================================================= */
    g_materials[MAT_MOLTEN_METAL] = (MaterialProps){
        .id = MAT_MOLTEN_METAL,
        .name = "Molten Metal",
        .state = STATE_FLUID,
        .base_color = {255, 200, 100, 255},
        .color_variation = 20,
        .density = 7000.0f,                    /* kg/m³ */
        .viscosity = 0.006f,                   /* Pa·s */
        .surface_tension = 1.8f,               /* N/m - high */
        .gravity_scale = 1.0f,
        .drag_coeff = 0.2f,
        .terminal_velocity = 3.0f,
        .flow_rate = 0.3f,                     /* Slow flow */
        .conductivity = 40.0f,                 /* W/(m·K) */
        .heat_capacity = 600.0f,               /* J/(kg·K) */
        .emissivity = 0.4f,
        .melting_temp = 1500.0f,
        .boiling_temp = 2900.0f,
        .freezing_temp = 1500.0f,              /* Solidifies to metal */
        .latent_heat_fusion = 270000.0f,
        .flammability = 0.0f,                  /* Molten metal doesn't burn */
        .ignition_temp = 9999.0f,
        .cohesion_strength = 1.5f,
    };
    
    /* =========================================================================
     * Finalize all materials and build LUTs
     * ========================================================================= */
    for (int i = 0; i < MAT_COUNT; i++) {
        material_finalize(&g_materials[i]);
        
        /* Build lookup tables */
        g_material_state_lut[i] = g_materials[i].state;
        g_material_is_powder_lut[i] = (g_materials[i].state == STATE_POWDER);
        g_material_is_fluid_lut[i] = (g_materials[i].state == STATE_FLUID);
        g_material_is_solid_lut[i] = (g_materials[i].state == STATE_SOLID);
        g_material_is_gas_lut[i] = (g_materials[i].state == STATE_GAS);
    }
}

/* =============================================================================
 * Material Access Functions
 * ============================================================================= */

const MaterialProps* material_get(MaterialID id) {
    if (id >= MAT_COUNT) {
        return &g_materials[MAT_AIR];
    }
    return &g_materials[id];
}

MaterialState material_state(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_state_lut[id] : STATE_GAS;
}

bool material_is_powder(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_powder_lut[id] : false;
}

bool material_is_fluid(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_fluid_lut[id] : false;
}

bool material_is_solid(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_solid_lut[id] : false;
}

bool material_is_gas(MaterialID id) {
    return (id < MAT_COUNT) ? g_material_is_gas_lut[id] : false;
}

/* =============================================================================
 * Color Functions
 * ============================================================================= */

Color material_color(MaterialID id, uint32_t seed) {
    const MaterialProps* mat = material_get(id);
    Color c = mat->base_color;
    
    if (mat->color_variation > 0 && seed != 0) {
        uint32_t h = hash32(seed);
        int var = (int)(h % (mat->color_variation * 2 + 1)) - mat->color_variation;
        
        c.r = (uint8_t)CLAMP((int)c.r + var, 0, 255);
        c.g = (uint8_t)CLAMP((int)c.g + var, 0, 255);
        c.b = (uint8_t)CLAMP((int)c.b + var, 0, 255);
    }
    
    return c;
}

/* Improved blackbody radiation color approximation
 * Now shows heat glow starting from lower temperatures for better visual feedback
 * Uses smoother transitions between color stages */
Color material_temp_to_glow_color(float temp_celsius) {
    /* Start showing warmth at 100C (boiling water), visible glow at 400C */
    
    if (temp_celsius < 50.0f) {
        /* Cold - no glow */
        return (Color){0, 0, 0, 0};
    } else if (temp_celsius < 150.0f) {
        /* Warm - very subtle red tint (barely visible) */
        float t = (temp_celsius - 50.0f) / 100.0f;
        return (Color){
            (uint8_t)(30 * t),
            0,
            0,
            (uint8_t)(30 * t)
        };
    } else if (temp_celsius < 300.0f) {
        /* Hot - faint red glow beginning */
        float t = (temp_celsius - 150.0f) / 150.0f;
        return (Color){
            (uint8_t)(30 + 40 * t),
            (uint8_t)(5 * t),
            0,
            (uint8_t)(30 + 50 * t)
        };
    } else if (temp_celsius < 500.0f) {
        /* Very hot - dark red glow (Draper point ~525C in reality) */
        float t = (temp_celsius - 300.0f) / 200.0f;
        return (Color){
            (uint8_t)(70 + 80 * t),
            (uint8_t)(5 + 20 * t),
            0,
            (uint8_t)(80 + 70 * t)
        };
    } else if (temp_celsius < 700.0f) {
        /* Glowing red - clearly visible */
        float t = (temp_celsius - 500.0f) / 200.0f;
        return (Color){
            (uint8_t)(150 + 50 * t),
            (uint8_t)(25 + 35 * t),
            (uint8_t)(5 * t),
            (uint8_t)(150 + 30 * t)
        };
    } else if (temp_celsius < 900.0f) {
        /* Bright red to orange */
        float t = (temp_celsius - 700.0f) / 200.0f;
        return (Color){
            (uint8_t)(200 + 55 * t),
            (uint8_t)(60 + 60 * t),
            (uint8_t)(5 + 15 * t),
            (uint8_t)(180 + 30 * t)
        };
    } else if (temp_celsius < 1100.0f) {
        /* Orange to yellow-orange */
        float t = (temp_celsius - 900.0f) / 200.0f;
        return (Color){
            255,
            (uint8_t)(120 + 80 * t),
            (uint8_t)(20 + 40 * t),
            (uint8_t)(210 + 20 * t)
        };
    } else if (temp_celsius < 1400.0f) {
        /* Yellow-orange to yellow */
        float t = (temp_celsius - 1100.0f) / 300.0f;
        return (Color){
            255,
            (uint8_t)(200 + 55 * t),
            (uint8_t)(60 + 60 * t),
            (uint8_t)(230 + 15 * t)
        };
    } else if (temp_celsius < 1800.0f) {
        /* Yellow to white-yellow */
        float t = (temp_celsius - 1400.0f) / 400.0f;
        return (Color){
            255,
            255,
            (uint8_t)(120 + 100 * t),
            245
        };
    } else if (temp_celsius < 2500.0f) {
        /* White-yellow to white */
        float t = (temp_celsius - 1800.0f) / 700.0f;
        return (Color){
            255,
            255,
            (uint8_t)(220 + 35 * t),
            255
        };
    } else {
        /* White to blue-white (very hot) */
        float t = CLAMP((temp_celsius - 2500.0f) / 1000.0f, 0.0f, 1.0f);
        return (Color){
            (uint8_t)(255 - 25 * t),
            (uint8_t)(255 - 15 * t),
            255,
            255
        };
    }
}

Color material_color_with_temp(MaterialID id, uint32_t seed, float temp) {
    Color c = material_color(id, seed);
    const MaterialProps* mat = material_get(id);
    
    /* Fire always uses its own color (age-based in fire.c) */
    if (id == MAT_FIRE) {
        return c;
    }
    
    /* Skip air - it doesn't glow */
    if (id == MAT_AIR) {
        return c;
    }
    
    /* Get glow color - now starts at lower temps for better visual feedback */
    Color glow = material_temp_to_glow_color(temp);
    
    if (glow.a == 0) {
        return c; /* No glow */
    }
    
    /* Scale glow intensity by emissivity (but allow some glow for all materials) */
    float emissivity_factor = 0.2f + mat->emissivity * 0.5f;  /* Reduced intensity to distinguish from fire */
    
    /* Calculate glow strength with smoother blending
     * Higher temps and higher emissivity = stronger glow */
    float glow_strength = (float)glow.a / 255.0f * emissivity_factor;
    
    /* Clamp glow strength to avoid over-saturation */
    if (glow_strength > 0.95f) glow_strength = 0.95f;
    
    float base_strength = 1.0f - glow_strength;
    
    /* Blend colors - additive for glow effect on dark colors */
    uint8_t r = (uint8_t)CLAMP((int)(c.r * base_strength + glow.r * glow_strength), 0, 255);
    uint8_t g = (uint8_t)CLAMP((int)(c.g * base_strength + glow.g * glow_strength), 0, 255);
    uint8_t b = (uint8_t)CLAMP((int)(c.b * base_strength + glow.b * glow_strength), 0, 255);
    
    return (Color){r, g, b, c.a};
}
