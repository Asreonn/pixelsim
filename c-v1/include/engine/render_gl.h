/*
 * render_gl.h - OpenGL-based rendering system for high-performance pixel rendering
 * 
 * Uses OpenGL 3.3+ for GPU-accelerated texture streaming
 */
#ifndef RENDER_GL_H
#define RENDER_GL_H

#include "core/types.h"
#include "world/world.h"
#include <SDL2/SDL.h>
#include <stdbool.h>

/* =============================================================================
 * OpenGL Renderer State
 * ============================================================================= */

typedef struct {
    SDL_Window* window;
    SDL_GLContext gl_context;
    
    /* OpenGL objects */
    unsigned int texture_id;      /* Main world texture */
    unsigned int vao;             /* Vertex array object */
    unsigned int vbo;             /* Vertex buffer object */
    unsigned int shader_program;  /* Shader program */
    
    /* Pixel buffer for CPU-side updates */
    uint32_t* pixels;
    int width;
    int height;
    
    /* Dirty region tracking */
    bool* chunk_dirty;            /* Which chunks need texture update */
    
    /* Performance */
    double frame_time_ms;
    double fps;
    
    /* Frame timing for 144 FPS limiter */
    uint64_t frame_start_counter;
    uint64_t perf_frequency;
    
    /* Debug overlay */
    bool show_fps;
    bool show_stats;
    
} RendererGL;

/* =============================================================================
 * Renderer Functions
 * ============================================================================= */

/* Create OpenGL renderer and window */
RendererGL* render_gl_create(int width, int height, const char* title);

/* Destroy renderer */
void render_gl_destroy(RendererGL* renderer);

/* Begin frame (clear, start timing) */
void render_gl_begin_frame(RendererGL* renderer);

/* Update pixel buffer from world state */
void render_gl_update_world(RendererGL* renderer, const World* world);

/* Upload changed regions to GPU texture */
void render_gl_upload_texture(RendererGL* renderer, const World* world);

/* Render the texture to screen */
void render_gl_draw(RendererGL* renderer);

/* End frame (swap buffers, apply frame limit) */
void render_gl_end_frame(RendererGL* renderer);

/* Cycle to next overlay mode */
void render_gl_cycle_overlay(RendererGL* renderer);

/* Toggle FPS display */
void render_gl_toggle_fps(RendererGL* renderer);

/* Toggle stats display */
void render_gl_toggle_stats(RendererGL* renderer);

/* Render debug overlay (no-op in GL path) */
void render_gl_render_overlay(RendererGL* renderer, const World* world);

/* Update FPS counter */
void render_gl_update_fps(RendererGL* renderer, double delta_time);

/* Frame timing control */
void render_gl_frame_limit_begin(RendererGL* renderer);
void render_gl_frame_limit_end(RendererGL* renderer);

/* Check if OpenGL is available */
bool render_gl_available(void);

#endif /* RENDER_GL_H */
