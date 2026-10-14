#include "game_engine.h"

void game_engine_init(game_engine_t *engine,
                      int left, int top, int width, int height) {
    if (!engine) return;

    engine->left = left;
    engine->top = top;
    engine->right = left + width;
    engine->bottom = top + height;

    for (int i = 0; i < GAME_ENGINE_MAX_ENTITIES; i++) {
        engine->entities[i].x = 0;
        engine->entities[i].y = 0;
        engine->entities[i].w = 0;
        engine->entities[i].h = 0;
        engine->entities[i].vx = 0;
        engine->entities[i].vy = 0;
        engine->entities[i].color = COLOR_WHITE;
        engine->entities[i].active = 0;
        engine->entities[i].flags = GE_FLAG_NONE;
    }
}

int game_engine_spawn(game_engine_t *engine,
                      int x, int y, int w, int h,
                      int vx, int vy, uint8_t color, uint8_t flags) {
    if (!engine || w <= 0 || h <= 0) return -1;

    for (int i = 0; i < GAME_ENGINE_MAX_ENTITIES; i++) {
        game_entity_t *entity = &engine->entities[i];
        if (entity->active) continue;

        entity->x = x;
        entity->y = y;
        entity->w = w;
        entity->h = h;
        entity->vx = vx;
        entity->vy = vy;
        entity->color = color;
        entity->flags = flags;
        entity->active = 1;
        return i;
    }

    return -1;
}

void game_engine_remove(game_engine_t *engine, int entity_id) {
    if (!engine || entity_id < 0 || entity_id >= GAME_ENGINE_MAX_ENTITIES) return;
    engine->entities[entity_id].active = 0;
}

game_entity_t *game_engine_get(game_engine_t *engine, int entity_id) {
    if (!engine || entity_id < 0 || entity_id >= GAME_ENGINE_MAX_ENTITIES) {
        return (game_entity_t *)0;
    }
    if (!engine->entities[entity_id].active) return (game_entity_t *)0;
    return &engine->entities[entity_id];
}

static void clamp_entity(const game_engine_t *engine, game_entity_t *entity) {
    if (entity->x < engine->left) entity->x = engine->left;
    if (entity->y < engine->top) entity->y = engine->top;
    if (entity->x + entity->w > engine->right) entity->x = engine->right - entity->w;
    if (entity->y + entity->h > engine->bottom) entity->y = engine->bottom - entity->h;
}

void game_engine_move(game_engine_t *engine, int entity_id, int dx, int dy) {
    game_entity_t *entity = game_engine_get(engine, entity_id);
    if (!entity) return;

    entity->x += dx;
    entity->y += dy;
    if (entity->flags & GE_FLAG_CLAMP_TO_ARENA) {
        clamp_entity(engine, entity);
    }
}

void game_engine_update(game_engine_t *engine) {
    if (!engine) return;

    for (int i = 0; i < GAME_ENGINE_MAX_ENTITIES; i++) {
        game_entity_t *entity = &engine->entities[i];
        if (!entity->active) continue;

        entity->x += entity->vx;
        entity->y += entity->vy;

        if (entity->flags & GE_FLAG_CLAMP_TO_ARENA) {
            clamp_entity(engine, entity);
        }

        if (entity->flags & GE_FLAG_REMOVE_OUTSIDE) {
            if (entity->x + entity->w < engine->left ||
                entity->x > engine->right ||
                entity->y + entity->h < engine->top ||
                entity->y > engine->bottom) {
                entity->active = 0;
            }
        }
    }
}

int game_engine_overlaps(const game_entity_t *a, const game_entity_t *b) {
    if (!a || !b || !a->active || !b->active) return 0;

    return a->x < b->x + b->w &&
           a->x + a->w > b->x &&
           a->y < b->y + b->h &&
           a->y + a->h > b->y;
}

void game_engine_draw(const game_engine_t *engine,
                      const customos_api_t *api) {
    if (!engine || !api) return;

    for (int i = 0; i < GAME_ENGINE_MAX_ENTITIES; i++) {
        const game_entity_t *entity = &engine->entities[i];
        if (!entity->active) continue;
        api->gfx_fill_rect(entity->x, entity->y,
                           entity->w, entity->h, entity->color);
    }
}
