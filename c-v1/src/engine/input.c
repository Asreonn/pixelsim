/*
 * input.c - Input handling implementation
 */
#include "engine/input.h"
#include "engine/render.h"
#include "engine/render_gl.h"
#include "materials/material.h"
#include <stdlib.h>
#include <stdio.h>

/* =============================================================================
 * Input Lifecycle
 * ============================================================================= */

Input* input_create(void) {
    Input* input = calloc(1, sizeof(Input));
    if (!input) return NULL;
    
    /* Default settings */
    input->current_material = MAT_SAND;
    input->brush_size = 5;
    input->min_brush_size = 1;
    input->max_brush_size = 50;
    
    input->mouse_x = 0;
    input->mouse_y = 0;
    input->prev_mouse_x = 0;
    input->prev_mouse_y = 0;
    
    input->quit_requested = false;
    
    return input;
}

void input_destroy(Input* input) {
    free(input);
}

/* =============================================================================
 * Event Processing
 * ============================================================================= */

void input_update(Input* input) {
    /* Reset single-press keys */
    input->key_space = false;
    input->key_escape = false;
    input->key_c = false;
    input->key_tab = false;
    input->key_f = false;
    input->key_s = false;
    input->key_period = false;
    input->key_1 = input->key_2 = input->key_3 = input->key_4 = input->key_5 = false;
    input->key_6 = input->key_7 = input->key_8 = input->key_9 = input->key_0 = false;
    input->key_minus = input->key_equals = false;
    input->key_q = input->key_w = input->key_e = input->key_r = false;
    input->key_t = input->key_y = input->key_u = input->key_i = false;
    input->key_o = input->key_p = input->key_a = false;
    
    /* Store previous mouse position */
    input->prev_mouse_x = input->mouse_x;
    input->prev_mouse_y = input->mouse_y;
    
    /* Process all pending events */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                input->quit_requested = true;
                break;
                
            case SDL_MOUSEMOTION:
                input->mouse_x = event.motion.x;
                input->mouse_y = event.motion.y;
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    input->mouse_left = true;
                    /* Initialize prev position on click */
                    input->prev_mouse_x = event.button.x;
                    input->prev_mouse_y = event.button.y;
                }
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    input->mouse_right = true;
                    input->prev_mouse_x = event.button.x;
                    input->prev_mouse_y = event.button.y;
                }
                if (event.button.button == SDL_BUTTON_MIDDLE) {
                    input->mouse_middle = true;
                }
                break;
                
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    input->mouse_left = false;
                }
                if (event.button.button == SDL_BUTTON_RIGHT) {
                    input->mouse_right = false;
                }
                if (event.button.button == SDL_BUTTON_MIDDLE) {
                    input->mouse_middle = false;
                }
                break;
                
            case SDL_MOUSEWHEEL:
                /* Mouse wheel changes brush size */
                if (event.wheel.y > 0) {
                    input_increase_brush(input);
                } else if (event.wheel.y < 0) {
                    input_decrease_brush(input);
                }
                break;
                
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: input->key_escape = true; break;
                    case SDLK_SPACE:  input->key_space = true; break;
                    case SDLK_c:      input->key_c = true; break;
                    case SDLK_TAB:    input->key_tab = true; break;
                    case SDLK_f:      input->key_f = true; break;
                    case SDLK_s:      input->key_s = true; break;
                    case SDLK_PERIOD: input->key_period = true; break;
                    
                    /* Number keys for material selection */
                    case SDLK_1: input->key_1 = true; break;
                    case SDLK_2: input->key_2 = true; break;
                    case SDLK_3: input->key_3 = true; break;
                    case SDLK_4: input->key_4 = true; break;
                    case SDLK_5: input->key_5 = true; break;
                    case SDLK_6: input->key_6 = true; break;
                    case SDLK_7: input->key_7 = true; break;
                    case SDLK_8: input->key_8 = true; break;
                    case SDLK_9: input->key_9 = true; break;
                    case SDLK_0: input->key_0 = true; break;
                    case SDLK_MINUS: input->key_minus = true; break;
                    case SDLK_EQUALS: input->key_equals = true; break;
                    case SDLK_q: input->key_q = true; break;
                    case SDLK_w: input->key_w = true; break;
                    case SDLK_e: input->key_e = true; break;
                    case SDLK_r: input->key_r = true; break;
                    case SDLK_t: input->key_t = true; break;
                    case SDLK_y: input->key_y = true; break;
                    case SDLK_u: input->key_u = true; break;
                    case SDLK_i: input->key_i = true; break;
                    case SDLK_o: input->key_o = true; break;
                    case SDLK_p: input->key_p = true; break;
                    case SDLK_a: input->key_a = true; break;
                    
                    /* Bracket keys for brush size */
                    case SDLK_LEFTBRACKET:  input_decrease_brush(input); break;
                    case SDLK_RIGHTBRACKET: input_increase_brush(input); break;
                    
                    default: break;
                }
                break;
                
            default:
                break;
        }
    }
}

/* =============================================================================
 * Input Application
 * ============================================================================= */

void input_apply(Input* input, World* world, Simulation* sim, RendererGL* renderer) {
    /* Handle quit request */
    if (input->key_escape) {
        input->quit_requested = true;
    }
    
    /* Handle pause toggle */
    if (input->key_space) {
        simulation_toggle_pause(sim);
    }
    
    /* Handle single step */
    if (input->key_period) {
        simulation_step_once(sim);
    }
    
    /* Handle clear */
    if (input->key_c) {
        world_clear(world);
    }
    
    /* Handle overlay toggle */
    if (input->key_tab) {
        render_gl_cycle_overlay(renderer);
    }

    /* Handle FPS toggle */
    if (input->key_f) {
        render_gl_toggle_fps(renderer);
    }

    /* Handle stats toggle */
    if (input->key_s) {
        render_gl_toggle_stats(renderer);
    }
    
    /* Handle material selection
     * Row 1 (1-0): Basic materials
     * Row 2 (Q-P): Advanced materials
     * A: Ash
     */
    if (input->key_1) input->current_material = MAT_SAND;
    if (input->key_2) input->current_material = MAT_STONE;
    if (input->key_3) input->current_material = MAT_WATER;
    if (input->key_4) input->current_material = MAT_WOOD;
    if (input->key_5) input->current_material = MAT_SOIL;
    if (input->key_6) input->current_material = MAT_FIRE;
    if (input->key_7) input->current_material = MAT_SMOKE;
    if (input->key_8) input->current_material = MAT_AIR;      /* Eraser */
    if (input->key_9) input->current_material = MAT_ICE;
    if (input->key_0) input->current_material = MAT_STEAM;
    if (input->key_minus) input->current_material = MAT_AIR;  /* Eraser */
    if (input->key_equals) input->current_material = MAT_ACID;
    if (input->key_q) input->current_material = MAT_METAL;
    if (input->key_w) input->current_material = MAT_MUD;
    if (input->key_e) input->current_material = MAT_LAVA;
    if (input->key_r) input->current_material = MAT_OIL;
    if (input->key_t) input->current_material = MAT_GLASS;
    if (input->key_y) input->current_material = MAT_OBSIDIAN;
    if (input->key_u) input->current_material = MAT_COAL;
    if (input->key_i) input->current_material = MAT_SALT;
    if (input->key_o) input->current_material = MAT_MOLTEN_METAL;
    if (input->key_p) input->current_material = MAT_EMBER;
    if (input->key_a) input->current_material = MAT_ASH;
    
    /* Handle painting */
    if (input->mouse_left) {
        /* Paint current material */
        world_paint_line(
            world,
            input->prev_mouse_x, input->prev_mouse_y,
            input->mouse_x, input->mouse_y,
            input->brush_size,
            input->current_material
        );
    }
    
    if (input->mouse_right) {
        /* Erase (paint air) */
        world_paint_line(
            world,
            input->prev_mouse_x, input->prev_mouse_y,
            input->mouse_x, input->mouse_y,
            input->brush_size,
            MAT_AIR
        );
    }
}

/* =============================================================================
 * Material and Brush Controls
 * ============================================================================= */

const char* input_get_material_name(const Input* input) {
    return material_get(input->current_material)->name;
}

void input_next_material(Input* input) {
    input->current_material++;
    if (input->current_material >= MAT_COUNT) {
        input->current_material = MAT_AIR;
    }
}

void input_prev_material(Input* input) {
    if (input->current_material == 0) {
        input->current_material = MAT_COUNT - 1;
    } else {
        input->current_material--;
    }
}

void input_increase_brush(Input* input) {
    input->brush_size++;
    if (input->brush_size > input->max_brush_size) {
        input->brush_size = input->max_brush_size;
    }
}

void input_decrease_brush(Input* input) {
    input->brush_size--;
    if (input->brush_size < input->min_brush_size) {
        input->brush_size = input->min_brush_size;
    }
}
