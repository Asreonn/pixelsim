/*
 * behavior.h - Material Behavior Classifiers Module
 *
 * Provides unified material behavior classification, reaction rules,
 * and state transition definitions.
 * 
 * Updated for new materials: AIR, EMBER, GLASS, OBSIDIAN, COAL, SALT, MOLTEN_METAL
 */
#ifndef BEHAVIOR_H
#define BEHAVIOR_H

#include "core/types.h"
#include "materials/material.h"

/* =============================================================================
 * Behavior Flags
 * ============================================================================= */

typedef uint32_t BehaviorFlags;

/* Movement behaviors */
#define BHV_NONE            0x00000000
#define BHV_FALLS           0x00000001  /* Affected by gravity (downward) */
#define BHV_RISES           0x00000002  /* Negative gravity (upward) */
#define BHV_FLOWS           0x00000004  /* Spreads horizontally */
#define BHV_SLIDES          0x00000008  /* Can slide diagonally */
#define BHV_STATIC          0x00000010  /* Never moves */
#define BHV_BUOYANT         0x00000020  /* Affected by buoyancy */

/* Interaction behaviors */
#define BHV_FLAMMABLE       0x00000100  /* Can catch fire */
#define BHV_CONDUCTS_HEAT   0x00000200  /* Transfers heat to neighbors */
#define BHV_CORRODIBLE      0x00000400  /* Can be dissolved by acid */
#define BHV_CORROSIVE       0x00000800  /* Dissolves other materials */
#define BHV_EXTINGUISHES    0x00001000  /* Puts out fire on contact */
#define BHV_ABSORBS_WATER   0x00002000  /* Can absorb water */

/* State change behaviors */
#define BHV_MELTS           0x00010000  /* Can melt to liquid */
#define BHV_FREEZES         0x00020000  /* Can freeze to solid */
#define BHV_BOILS           0x00040000  /* Can boil to gas */
#define BHV_CONDENSES       0x00080000  /* Can condense to liquid */
#define BHV_BURNS_OUT       0x00100000  /* Disappears when burning ends */
#define BHV_DISSOLVES       0x00200000  /* Dissolves in water */

/* Lifetime behaviors */
#define BHV_DISSIPATES      0x01000000  /* Fades over time */
#define BHV_SPREADS         0x02000000  /* Spreads to neighbors */
#define BHV_PRODUCES_SMOKE  0x04000000  /* Creates smoke */
#define BHV_PRODUCES_HEAT   0x08000000  /* Generates heat */
#define BHV_CONSUMES_OXYGEN 0x10000000  /* Requires oxygen */

/* =============================================================================
 * Behavior Lookup Table
 * ============================================================================= */

static inline BehaviorFlags behavior_get(MaterialID mat) {
    static const BehaviorFlags BEHAVIOR_TABLE[MAT_COUNT] = {
        /* MAT_AIR */
        BHV_BUOYANT,
        
        /* MAT_SAND */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT | BHV_ABSORBS_WATER | BHV_MELTS,
        
        /* MAT_STONE */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_CORRODIBLE | BHV_MELTS,
        
        /* MAT_WATER */
        BHV_FALLS | BHV_FLOWS | BHV_CONDUCTS_HEAT | BHV_FREEZES | BHV_BOILS | 
        BHV_EXTINGUISHES | BHV_BUOYANT,
        
        /* MAT_WOOD */
        BHV_STATIC | BHV_FLAMMABLE | BHV_CONDUCTS_HEAT | BHV_CORRODIBLE | 
        BHV_CONSUMES_OXYGEN,
        
        /* MAT_FIRE */
        BHV_RISES | BHV_SPREADS | BHV_PRODUCES_SMOKE | BHV_PRODUCES_HEAT | 
        BHV_BURNS_OUT | BHV_CONSUMES_OXYGEN | BHV_BUOYANT,
        
        /* MAT_SMOKE */
        BHV_RISES | BHV_FLOWS | BHV_DISSIPATES | BHV_BUOYANT,
        
        /* MAT_SOIL */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT | BHV_CORRODIBLE | BHV_ABSORBS_WATER,
        
        /* MAT_ICE */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_MELTS,
        
        /* MAT_STEAM */
        BHV_RISES | BHV_FLOWS | BHV_CONDENSES | BHV_DISSIPATES | BHV_BUOYANT,
        
        /* MAT_EMBER */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT | BHV_PRODUCES_HEAT | 
        BHV_FLAMMABLE | BHV_CONSUMES_OXYGEN | BHV_DISSIPATES,
        
        /* MAT_ACID */
        BHV_FALLS | BHV_FLOWS | BHV_CORROSIVE | BHV_CONDUCTS_HEAT | BHV_BUOYANT,
        
        /* MAT_METAL */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_MELTS | BHV_CORRODIBLE,
        
        /* MAT_MUD */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT | BHV_BOILS,
        
        /* MAT_LAVA */
        BHV_FALLS | BHV_FLOWS | BHV_CONDUCTS_HEAT | BHV_PRODUCES_HEAT | 
        BHV_FREEZES | BHV_BUOYANT,
        
        /* MAT_OIL */
        BHV_FALLS | BHV_FLOWS | BHV_FLAMMABLE | BHV_CONDUCTS_HEAT | 
        BHV_CONSUMES_OXYGEN | BHV_BUOYANT,
        
        /* MAT_WET_WOOD */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_BOILS,
        
        /* MAT_GLASS */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_MELTS,
        
        /* MAT_OBSIDIAN */
        BHV_STATIC | BHV_CONDUCTS_HEAT | BHV_MELTS,
        
        /* MAT_COAL */
        BHV_STATIC | BHV_FLAMMABLE | BHV_CONDUCTS_HEAT | BHV_CONSUMES_OXYGEN,
        
        /* MAT_SALT */
        BHV_FALLS | BHV_SLIDES | BHV_CONDUCTS_HEAT | BHV_DISSOLVES,
        
        /* MAT_MOLTEN_METAL */
        BHV_FALLS | BHV_FLOWS | BHV_CONDUCTS_HEAT | BHV_PRODUCES_HEAT | 
        BHV_FREEZES | BHV_BUOYANT,
    };
    
    if (mat >= MAT_COUNT) return BHV_NONE;
    return BEHAVIOR_TABLE[mat];
}

/* Check for specific behavior flag */
static inline bool behavior_has(MaterialID mat, BehaviorFlags flag) {
    return (behavior_get(mat) & flag) != 0;
}

/* =============================================================================
 * Behavior Queries
 * ============================================================================= */

/* Movement queries */
static inline bool bhv_falls(MaterialID mat)     { return behavior_has(mat, BHV_FALLS); }
static inline bool bhv_rises(MaterialID mat)     { return behavior_has(mat, BHV_RISES); }
static inline bool bhv_flows(MaterialID mat)     { return behavior_has(mat, BHV_FLOWS); }
static inline bool bhv_slides(MaterialID mat)    { return behavior_has(mat, BHV_SLIDES); }
static inline bool bhv_is_static(MaterialID mat) { return behavior_has(mat, BHV_STATIC); }
static inline bool bhv_is_buoyant(MaterialID mat){ return behavior_has(mat, BHV_BUOYANT); }

/* Interaction queries */
static inline bool bhv_is_flammable(MaterialID mat)     { return behavior_has(mat, BHV_FLAMMABLE); }
static inline bool bhv_conducts_heat(MaterialID mat)    { return behavior_has(mat, BHV_CONDUCTS_HEAT); }
static inline bool bhv_is_corrodible(MaterialID mat)    { return behavior_has(mat, BHV_CORRODIBLE); }
static inline bool bhv_is_corrosive(MaterialID mat)     { return behavior_has(mat, BHV_CORROSIVE); }
static inline bool bhv_extinguishes(MaterialID mat)     { return behavior_has(mat, BHV_EXTINGUISHES); }
static inline bool bhv_absorbs_water(MaterialID mat)    { return behavior_has(mat, BHV_ABSORBS_WATER); }
static inline bool bhv_consumes_oxygen(MaterialID mat)  { return behavior_has(mat, BHV_CONSUMES_OXYGEN); }

/* State change queries */
static inline bool bhv_can_melt(MaterialID mat)     { return behavior_has(mat, BHV_MELTS); }
static inline bool bhv_can_freeze(MaterialID mat)   { return behavior_has(mat, BHV_FREEZES); }
static inline bool bhv_can_boil(MaterialID mat)     { return behavior_has(mat, BHV_BOILS); }
static inline bool bhv_can_condense(MaterialID mat) { return behavior_has(mat, BHV_CONDENSES); }
static inline bool bhv_dissolves(MaterialID mat)    { return behavior_has(mat, BHV_DISSOLVES); }

/* Lifetime queries */
static inline bool bhv_dissipates(MaterialID mat)     { return behavior_has(mat, BHV_DISSIPATES); }
static inline bool bhv_spreads(MaterialID mat)        { return behavior_has(mat, BHV_SPREADS); }
static inline bool bhv_produces_smoke(MaterialID mat) { return behavior_has(mat, BHV_PRODUCES_SMOKE); }
static inline bool bhv_produces_heat(MaterialID mat)  { return behavior_has(mat, BHV_PRODUCES_HEAT); }

/* =============================================================================
 * State Transitions (for thermal.c compatibility)
 * ============================================================================= */

typedef struct {
    MaterialID result;
    float threshold;
    float probability;
} StateTransition;

/* Get melting transition for material */
static inline StateTransition bhv_get_melt_transition(MaterialID mat) {
    switch (mat) {
        case MAT_ICE:
            return (StateTransition){MAT_WATER, 0.0f, 0.02f};
        case MAT_STONE:
            return (StateTransition){MAT_LAVA, 1200.0f, 0.005f};
        case MAT_METAL:
            return (StateTransition){MAT_MOLTEN_METAL, 1500.0f, 0.003f};
        case MAT_SAND:
            return (StateTransition){MAT_GLASS, 1700.0f, 0.001f};
        case MAT_GLASS:
            return (StateTransition){MAT_LAVA, 1700.0f, 0.002f};
        case MAT_OBSIDIAN:
            return (StateTransition){MAT_LAVA, 1200.0f, 0.002f};
        default:
            return (StateTransition){MAT_AIR, 9999.0f, 0.0f};
    }
}

/* Get freezing transition for material */
static inline StateTransition bhv_get_freeze_transition(MaterialID mat) {
    switch (mat) {
        case MAT_WATER:
            return (StateTransition){MAT_ICE, 0.0f, 0.01f};
        case MAT_LAVA:
            return (StateTransition){MAT_STONE, 700.0f, 0.01f};
        case MAT_MOLTEN_METAL:
            return (StateTransition){MAT_METAL, 1500.0f, 0.01f};
        default:
            return (StateTransition){MAT_AIR, -9999.0f, 0.0f};
    }
}

/* Get boiling transition for material */
static inline StateTransition bhv_get_boil_transition(MaterialID mat) {
    switch (mat) {
        case MAT_WATER:
            return (StateTransition){MAT_STEAM, 100.0f, 0.03f};
        case MAT_MUD:
            return (StateTransition){MAT_SOIL, 80.0f, 0.01f};
        case MAT_WET_WOOD:
            return (StateTransition){MAT_WOOD, 100.0f, 0.01f};
        default:
            return (StateTransition){MAT_AIR, 9999.0f, 0.0f};
    }
}

/* Get condensation transition for material */
static inline StateTransition bhv_get_condense_transition(MaterialID mat) {
    switch (mat) {
        case MAT_STEAM:
            return (StateTransition){MAT_WATER, 80.0f, 0.015f};
        default:
            return (StateTransition){MAT_AIR, -9999.0f, 0.0f};
    }
}

/* Get combustion products for material */
static inline StateTransition bhv_get_burn_transition(MaterialID mat) {
    switch (mat) {
        case MAT_WOOD:
            return (StateTransition){MAT_EMBER, 300.0f, 0.03f};
        case MAT_COAL:
            return (StateTransition){MAT_EMBER, 427.0f, 0.02f};
        case MAT_OIL:
            return (StateTransition){MAT_FIRE, 200.0f, 0.1f};
        case MAT_EMBER:
            return (StateTransition){MAT_AIR, 250.0f, 0.01f};  /* Burns out */
        default:
            return (StateTransition){MAT_AIR, 9999.0f, 0.0f};
    }
}

/* =============================================================================
 * Reaction Rules
 * ============================================================================= */

typedef struct {
    MaterialID target;
    MaterialID result_self;
    MaterialID result_target;
    float probability;
    MaterialID byproduct;
    float byproduct_chance;
} ReactionRule;

/* Get corrosion reaction for acid + target */
static inline ReactionRule bhv_get_corrosion_reaction(MaterialID target) {
    if (!bhv_is_corrodible(target)) {
        return (ReactionRule){MAT_AIR, MAT_AIR, MAT_AIR, 0.0f, MAT_AIR, 0.0f};
    }
    
    return (ReactionRule){
        .target = target,
        .result_self = MAT_AIR,
        .result_target = MAT_AIR,
        .probability = 0.05f,
        .byproduct = MAT_SMOKE,
        .byproduct_chance = 0.3f
    };
}

/* Get fire spread reaction */
static inline ReactionRule bhv_get_fire_spread_reaction(MaterialID target) {
    if (!bhv_is_flammable(target)) {
        return (ReactionRule){MAT_AIR, MAT_AIR, MAT_AIR, 0.0f, MAT_AIR, 0.0f};
    }
    
    float prob = 0.03f;
    if (target == MAT_OIL) prob = 0.2f;      /* Oil burns fast */
    if (target == MAT_COAL) prob = 0.01f;    /* Coal hard to ignite */
    if (target == MAT_EMBER) prob = 0.15f;   /* Embers reignite easily */
    
    return (ReactionRule){
        .target = target,
        .result_self = MAT_FIRE,
        .result_target = MAT_FIRE,
        .probability = prob,
        .byproduct = MAT_AIR,
        .byproduct_chance = 0.0f
    };
}

/* Get extinguish reaction */
static inline ReactionRule bhv_get_extinguish_reaction(MaterialID source) {
    if (!bhv_extinguishes(source)) {
        return (ReactionRule){MAT_AIR, MAT_AIR, MAT_AIR, 0.0f, MAT_AIR, 0.0f};
    }
    
    return (ReactionRule){
        .target = MAT_FIRE,
        .result_self = MAT_STEAM,
        .result_target = MAT_SMOKE,
        .probability = 0.6f,
        .byproduct = MAT_STEAM,
        .byproduct_chance = 0.4f
    };
}

/* =============================================================================
 * Movement Priority Tables
 * ============================================================================= */

typedef struct {
    int dx, dy;
} MoveOffset;

/* Powder movement priorities */
static const MoveOffset POWDER_MOVE_PRIORITY[] = {
    { 0,  1},   /* Down */
    {-1,  1},   /* Down-left */
    { 1,  1},   /* Down-right */
};
#define POWDER_MOVE_COUNT 3

/* Fluid movement priorities */
static const MoveOffset FLUID_MOVE_PRIORITY[] = {
    { 0,  1},   /* Down */
    {-1,  0},   /* Left */
    { 1,  0},   /* Right */
    {-1,  1},   /* Down-left */
    { 1,  1},   /* Down-right */
};
#define FLUID_MOVE_COUNT 5

/* Gas movement priorities */
static const MoveOffset GAS_MOVE_PRIORITY[] = {
    { 0, -1},   /* Up */
    {-1, -1},   /* Up-left */
    { 1, -1},   /* Up-right */
    {-1,  0},   /* Left */
    { 1,  0},   /* Right */
};
#define GAS_MOVE_COUNT 5

#endif /* BEHAVIOR_H */
