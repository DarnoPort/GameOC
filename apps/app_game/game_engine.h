#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include "customos.h"

/*
 * A tiny reusable 2D engine for CustomOS applications.
 * Coordinates are pixels; the play area uses a left/top inclusive,
 * right/bottom exclusive boundary.
 */
#define GAME_ENGINE_MAX_ENTITIES 16

#define GE_FLAG_NONE            0
#define GE_FLAG_CLAMP_TO_ARENA  1
#define GE_FLAG_REMOVE_OUTSIDE  2

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int vx;
    int vy;
    uint8_t color;
    uint8_t active;
    uint8_t flags;
} game_entity_t;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
    game_entity_t entities[GAME_ENGINE_MAX_ENTITIES];
} game_engine_t;

void game_engine_init(game_engine_t *engine,
                      int left, int top, int width, int height);
int game_engine_spawn(game_engine_t *engine,
                      int x, int y, int w, int h,
                      int vx, int vy, uint8_t color, uint8_t flags);
void game_engine_remove(game_engine_t *engine, int entity_id);
game_entity_t *game_engine_get(game_engine_t *engine, int entity_id);
void game_engine_move(game_engine_t *engine, int entity_id, int dx, int dy);
void game_engine_update(game_engine_t *engine);
int game_engine_overlaps(const game_entity_t *a, const game_entity_t *b);
void game_engine_draw(const game_engine_t *engine,
                      const customos_api_t *api);

#endif
