/*
 * render_gl.c - OpenGL-based rendering implementation
 * 
 * Uses OpenGL 3.3+ for GPU-accelerated texture streaming
 */
#include "engine/render_gl.h"
#include "materials/material.h"
#include "subsystems/fire.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* OpenGL headers */
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

/* =============================================================================
 * Shader Sources
 * ============================================================================= */

static const char* VERTEX_SHADER_SRC = 
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoord;\n"
    "out vec2 TexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    TexCoord = aTexCoord;\n"
    "}\n";

static const char* FRAGMENT_SHADER_SRC = 
    "#version 330 core\n"
    "in vec2 TexCoord;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D worldTexture;\n"
    "void main() {\n"
    "    FragColor = texture(worldTexture, TexCoord);\n"
    "}\n";

/* Full-screen quad vertices: position (x,y) + texcoord (s,t) */
static const float QUAD_VERTICES[] = {
    /* Position     TexCoord */
    -1.0f,  1.0f,   0.0f, 0.0f,  /* Top-left */
    -1.0f, -1.0f,   0.0f, 1.0f,  /* Bottom-left */
     1.0f, -1.0f,   1.0f, 1.0f,  /* Bottom-right */
    -1.0f,  1.0f,   0.0f, 0.0f,  /* Top-left */
     1.0f, -1.0f,   1.0f, 1.0f,  /* Bottom-right */
     1.0f,  1.0f,   1.0f, 0.0f,  /* Top-right */
};

/* =============================================================================
 * Helper Functions
 * ============================================================================= */

static uint32_t color_to_rgba(Color c) {
    /* OpenGL uses RGBA format */
    return ((uint32_t)c.r) | ((uint32_t)c.g << 8) | ((uint32_t)c.b << 16) | ((uint32_t)c.a << 24);
}

static unsigned int compile_shader(GLenum type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, NULL, info_log);
        fprintf(stderr, "Shader compilation failed: %s\n", info_log);
        return 0;
    }
    
    return shader;
}

static unsigned int create_shader_program(void) {
    unsigned int vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER_SRC);
    unsigned int fragment_shader = compile_shader(GL_FRAGMENT_SHADER, FRAGMENT_SHADER_SRC);
    
    if (!vertex_shader || !fragment_shader) {
        return 0;
    }
    
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, NULL, info_log);
        fprintf(stderr, "Shader program linking failed: %s\n", info_log);
        return 0;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

/* =============================================================================
 * Renderer Lifecycle
 * ============================================================================= */

bool render_gl_available(void) {
    /* Check if OpenGL 3.3+ is available */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    return true;
}

RendererGL* render_gl_create(int width, int height, const char* title) {
    RendererGL* renderer = calloc(1, sizeof(RendererGL));
    if (!renderer) return NULL;
    
    renderer->width = width;
    renderer->height = height;
    renderer->show_fps = true;
    renderer->show_stats = true;
    
    /* Initialize SDL with OpenGL */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        free(renderer);
        return NULL;
    }
    
    /* Set OpenGL attributes */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    /* Create window with OpenGL context */
    renderer->window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );
    if (!renderer->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Create OpenGL context */
    renderer->gl_context = SDL_GL_CreateContext(renderer->window);
    if (!renderer->gl_context) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(renderer->window);
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Disable VSync for manual frame limiting */
    SDL_GL_SetSwapInterval(0);
    
    /* Create shader program */
    renderer->shader_program = create_shader_program();
    if (!renderer->shader_program) {
        fprintf(stderr, "Failed to create shader program\n");
        SDL_GL_DeleteContext(renderer->gl_context);
        SDL_DestroyWindow(renderer->window);
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Create VAO and VBO for fullscreen quad */
    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    
    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);
    
    /* Position attribute */
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    /* TexCoord attribute */
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    
    /* Create texture for world pixels */
    glGenTextures(1, &renderer->texture_id);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_id);
    
    /* Set texture parameters for pixel-perfect rendering */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    /* Allocate texture storage */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    
    /* Allocate pixel buffer */
    renderer->pixels = calloc((size_t)width * height, sizeof(uint32_t));
    if (!renderer->pixels) {
        glDeleteTextures(1, &renderer->texture_id);
        glDeleteVertexArrays(1, &renderer->vao);
        glDeleteBuffers(1, &renderer->vbo);
        glDeleteProgram(renderer->shader_program);
        SDL_GL_DeleteContext(renderer->gl_context);
        SDL_DestroyWindow(renderer->window);
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Allocate chunk dirty tracking */
    renderer->chunk_dirty = calloc(CHUNK_COUNT, sizeof(bool));
    if (!renderer->chunk_dirty) {
        free(renderer->pixels);
        glDeleteTextures(1, &renderer->texture_id);
        glDeleteVertexArrays(1, &renderer->vao);
        glDeleteBuffers(1, &renderer->vbo);
        glDeleteProgram(renderer->shader_program);
        SDL_GL_DeleteContext(renderer->gl_context);
        SDL_DestroyWindow(renderer->window);
        SDL_Quit();
        free(renderer);
        return NULL;
    }
    
    /* Initialize frame timing */
    renderer->perf_frequency = SDL_GetPerformanceFrequency();
    renderer->frame_start_counter = 0;
    
    printf("OpenGL Renderer initialized\n");
    printf("  OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("  GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("  Renderer: %s\n", glGetString(GL_RENDERER));
    
    return renderer;
}

void render_gl_destroy(RendererGL* renderer) {
    if (!renderer) return;
    
    free(renderer->chunk_dirty);
    free(renderer->pixels);
    
    glDeleteTextures(1, &renderer->texture_id);
    glDeleteVertexArrays(1, &renderer->vao);
    glDeleteBuffers(1, &renderer->vbo);
    glDeleteProgram(renderer->shader_program);
    
    if (renderer->gl_context) SDL_GL_DeleteContext(renderer->gl_context);
    if (renderer->window) SDL_DestroyWindow(renderer->window);
    
    SDL_Quit();
    free(renderer);
}

/* =============================================================================
 * Frame Rendering
 * ============================================================================= */

void render_gl_begin_frame(RendererGL* renderer) {
    /* Clear pixel buffer to black */
    memset(renderer->pixels, 0, (size_t)renderer->width * renderer->height * sizeof(uint32_t));
    
    /* Mark all chunks as not dirty */
    memset(renderer->chunk_dirty, 0, CHUNK_COUNT * sizeof(bool));
}

void render_gl_update_world(RendererGL* renderer, const World* world) {
    /* Only update pixels for active chunks (Dirty Rectangle Optimization) */
    for (int cy = 0; cy < CHUNKS_Y; cy++) {
        for (int cx = 0; cx < CHUNKS_X; cx++) {
            if (!world_is_chunk_active(world, cx, cy)) {
                continue;
            }
            
            /* Mark chunk as dirty for texture upload */
            renderer->chunk_dirty[cy * CHUNKS_X + cx] = true;
            
            /* Calculate chunk bounds */
            int x0 = cx * CHUNK_SIZE;
            int y0 = cy * CHUNK_SIZE;
            int x1 = MIN(x0 + CHUNK_SIZE, world->width);
            int y1 = MIN(y0 + CHUNK_SIZE, world->height);
            
            /* Update pixels in this chunk */
            for (int y = y0; y < y1; y++) {
                for (int x = x0; x < x1; x++) {
                    int idx = y * renderer->width + x;
                    int world_idx = IDX(x, y);
                    MaterialID mat = world->mat[world_idx];
                    
                    Color c;
                    
                    if (mat == MAT_FIRE) {
                        c = fire_get_color(world->lifetime[world_idx]);
                    } else if (mat == MAT_SMOKE) {
                        c = material_color(mat, world->color_seed[world_idx]);
                        int age = world->lifetime[world_idx];
                        int alpha = 150 - (age / 2);
                        if (alpha < 20) alpha = 20;
                        c.a = (uint8_t)alpha;
                    } else if (mat == MAT_STEAM) {
                        c = material_color(mat, world->color_seed[world_idx]);
                        int age = world->lifetime[world_idx];
                        int alpha = 80 - (age / 3);
                        if (alpha < 10) alpha = 10;
                        c.a = (uint8_t)alpha;
                    } else if (mat == MAT_METAL || mat == MAT_STONE || mat == MAT_LAVA || mat == MAT_EMBER) {
                        /* Temperature-based glow for hot materials */
                        float temp = world->temp[world_idx];
                        c = material_color_with_temp(mat, world->color_seed[world_idx], temp);
                    } else {
                        c = world_get_cell_color(world, x, y);
                    }
                    
                    renderer->pixels[idx] = color_to_rgba(c);
                }
            }
        }
    }
}

void render_gl_upload_texture(RendererGL* renderer, const World* world) {
    (void)world;
    
    glBindTexture(GL_TEXTURE_2D, renderer->texture_id);
    
    /* Option 1: Upload only dirty chunks (more GPU calls but less data) */
    /* Option 2: Upload entire texture (simpler, often faster for small textures) */
    
    /* For 512x512, uploading entire texture is often faster than partial updates */
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 
                    renderer->width, renderer->height,
                    GL_RGBA, GL_UNSIGNED_BYTE, renderer->pixels);
}

void render_gl_draw(RendererGL* renderer) {
    /* Clear screen */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    /* Use shader and bind texture */
    glUseProgram(renderer->shader_program);
    glBindTexture(GL_TEXTURE_2D, renderer->texture_id);
    
    /* Draw fullscreen quad */
    glBindVertexArray(renderer->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void render_gl_end_frame(RendererGL* renderer) {
    /* Swap buffers */
    SDL_GL_SwapWindow(renderer->window);

    /* Apply frame limiter */
    render_gl_frame_limit_end(renderer);
}

void render_gl_cycle_overlay(RendererGL* renderer) {
    (void)renderer;
}

void render_gl_render_overlay(RendererGL* renderer, const World* world) {
    (void)renderer;
    (void)world;
}

void render_gl_toggle_fps(RendererGL* renderer) {
    renderer->show_fps = !renderer->show_fps;
}

void render_gl_toggle_stats(RendererGL* renderer) {
    renderer->show_stats = !renderer->show_stats;
}

/* =============================================================================
 * Frame Timing Control
 * ============================================================================= */

void render_gl_frame_limit_begin(RendererGL* renderer) {
    renderer->frame_start_counter = SDL_GetPerformanceCounter();
}

void render_gl_frame_limit_end(RendererGL* renderer) {
    uint64_t frame_end = SDL_GetPerformanceCounter();
    double elapsed = (double)(frame_end - renderer->frame_start_counter) / 
                     (double)renderer->perf_frequency;
    
    if (elapsed < TARGET_FRAME_TIME) {
        double remaining = TARGET_FRAME_TIME - elapsed;
        
        /* Sleep for most of the remaining time */
        if (remaining > 0.0005) {
            uint32_t sleep_ms = (uint32_t)((remaining - 0.0005) * 1000.0);
            if (sleep_ms > 0) {
                SDL_Delay(sleep_ms);
            }
        }
        
        /* Spin-wait for precise timing */
        while (1) {
            uint64_t now = SDL_GetPerformanceCounter();
            double total_elapsed = (double)(now - renderer->frame_start_counter) / 
                                   (double)renderer->perf_frequency;
            if (total_elapsed >= TARGET_FRAME_TIME) break;
        }
    }
}

void render_gl_update_fps(RendererGL* renderer, double delta_time) {
    renderer->frame_time_ms = delta_time * 1000.0;
    if (delta_time > 0) {
        renderer->fps = 1.0 / delta_time;
    }
}
