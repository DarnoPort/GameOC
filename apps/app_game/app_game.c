#include "customos.h"
#include "game_engine.h"

#define ARENA_LEFT   10
#define ARENA_TOP    26
#define ARENA_WIDTH  300
#define ARENA_HEIGHT 142
#define PLAYER_SIZE  8
#define PLAYER_ID    0

enum game_state {
    GAME_TITLE,
    GAME_PLAYING,
    GAME_PAUSED,
    GAME_OVER
};

static uint32_t rng_state = 0xC0570A5u;

static uint32_t random_next(void) {
    /* Small deterministic PRNG: no libc or hardware random source required. */
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    if (rng_state == 0) rng_state = 0xC0570A5u;
    return rng_state;
}

static void int_to_text(uint32_t value, char *out) {
    char reversed[10];
    int count = 0;
    int index = 0;

    do {
        reversed[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && count < (int)sizeof(reversed));

    while (count > 0) out[index++] = reversed[--count];
    out[index] = '\0';
}

static void start_game(game_engine_t *engine, const customos_api_t *api,
                       uint32_t *score, uint32_t *frames,
                       uint32_t *spawn_clock) {
    (void)api;
    game_engine_init(engine, ARENA_LEFT, ARENA_TOP, ARENA_WIDTH, ARENA_HEIGHT);

    game_engine_spawn(engine,
                      ARENA_LEFT + ARENA_WIDTH / 2 - PLAYER_SIZE / 2,
                      ARENA_TOP + ARENA_HEIGHT - PLAYER_SIZE - 5,
                      PLAYER_SIZE, PLAYER_SIZE, 0, 0,
                      COLOR_WHITE, GE_FLAG_CLAMP_TO_ARENA);

    *score = 0;
    *frames = 0;
    *spawn_clock = 0;
    rng_state ^= 0x9E3779B9u + *score + (*frames << 6);
    if (rng_state == 0) rng_state = 1;
}

static void spawn_hazard(game_engine_t *engine, uint32_t score) {
    int width = 6 + (int)(random_next() % 7u);
    int height = 6 + (int)(random_next() % 7u);
    int max_x = ARENA_LEFT + ARENA_WIDTH - width;
    int x = ARENA_LEFT + (int)(random_next() % (uint32_t)(max_x - ARENA_LEFT + 1));
    int drift = (int)(random_next() % 3u) - 1;
    int speed = 1 + (int)(score / 35u);

    if (speed > 4) speed = 4;

    game_engine_spawn(engine, x, ARENA_TOP, width, height,
                      drift, speed, COLOR_LIGHT_GRAY,
                      GE_FLAG_REMOVE_OUTSIDE);
}

static int player_hit_hazard(const game_engine_t *engine) {
    const game_entity_t *player = &engine->entities[PLAYER_ID];
    if (!player->active) return 1;

    for (int i = 1; i < GAME_ENGINE_MAX_ENTITIES; i++) {
        if (game_engine_overlaps(player, &engine->entities[i])) return 1;
    }
    return 0;
}

static void draw_arena(const customos_api_t *api,
                       const game_engine_t *engine,
                       uint32_t score, uint32_t best) {
    char score_text[11];
    char best_text[11];

    int_to_text(score, score_text);
    int_to_text(best, best_text);

    api->gfx_clear(COLOR_BLACK);

    api->gfx_draw_text(8, 7, "DODGE / STAGE 7", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(185, 7, "S:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(201, 7, score_text, COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(246, 7, "BEST:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(286, 7, best_text, COLOR_WHITE, COLOR_TRANSPARENT);

    api->gfx_draw_rect(6, 22, 308, 150, COLOR_DARK_GRAY);
    /* Subtle reference grid; each cell is 20 pixels. */
    for (int x = ARENA_LEFT + 20; x < ARENA_LEFT + ARENA_WIDTH; x += 20) {
        api->gfx_draw_line(x, ARENA_TOP, x, ARENA_TOP + ARENA_HEIGHT - 1,
                           COLOR_DARK_GRAY);
    }
    for (int y = ARENA_TOP + 20; y < ARENA_TOP + ARENA_HEIGHT; y += 20) {
        api->gfx_draw_line(ARENA_LEFT, y, ARENA_LEFT + ARENA_WIDTH - 1, y,
                           COLOR_DARK_GRAY);
    }

    game_engine_draw(engine, api);

    api->gfx_fill_rect(0, 176, 320, 24, COLOR_DARK_GRAY);
    api->gfx_draw_text(8, 180, "MOVE: arrows/WASD   P: pause   ESC: exit",
                       COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_flip();
}

static void draw_title(const customos_api_t *api, uint32_t best) {
    char best_text[11];
    int_to_text(best, best_text);

    api->gfx_clear(COLOR_BLACK);
    api->gfx_draw_rect(12, 10, 296, 180, COLOR_LIGHT_GRAY);
    api->gfx_fill_rect(14, 12, 292, 20, COLOR_DARK_GRAY);
    api->gfx_draw_text(24, 18, "CUSTOMOS GAME ENGINE", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(30, 48, "DODGE", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(30, 64, "Avoid the falling blocks.", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(30, 78, "Stay inside the arena and survive.", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(30, 100, "MOVE: arrows or W/A/S/D", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(30, 114, "P: pause   R: restart after a hit", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(30, 136, "Best score:", COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(126, 136, best_text, COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(30, 160, "ENTER: start   SPACE/ESC: launcher", COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_flip();
}

static void draw_overlay(const customos_api_t *api, const char *title,
                         const char *line1, const char *line2) {
    api->gfx_fill_rect(42, 68, 236, 60, COLOR_DARK_GRAY);
    api->gfx_draw_rect(42, 68, 236, 60, COLOR_WHITE);
    api->gfx_draw_text(54, 78, title, COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_draw_text(54, 94, line1, COLOR_LIGHT_GRAY, COLOR_TRANSPARENT);
    api->gfx_draw_text(54, 108, line2, COLOR_WHITE, COLOR_TRANSPARENT);
    api->gfx_flip();
}

int app_main(const customos_api_t *api) {
    game_engine_t engine;
    enum game_state state = GAME_TITLE;
    uint32_t score = 0;
    uint32_t best = 0;
    uint32_t frames = 0;
    uint32_t spawn_clock = 0;
    int redraw_title = 1;

    rng_state ^= api->timer_get_ticks() + 0xA341316Cu;
    if (rng_state == 0) rng_state = 1;

    while (1) {
        while (api->kbd_has_key()) {
            uint8_t key = api->kbd_get_key();

            if (key == 27 || key == ' ') {
                return 0;
            }

            if (state == GAME_TITLE) {
                if (key == '\n' || key == 'r' || key == 'R') {
                    start_game(&engine, api, &score, &frames, &spawn_clock);
                    state = GAME_PLAYING;
                    redraw_title = 0;
                }
                continue;
            }

            if (state == GAME_OVER) {
                if (key == 'r' || key == 'R' || key == '\n') {
                    start_game(&engine, api, &score, &frames, &spawn_clock);
                    state = GAME_PLAYING;
                }
                continue;
            }

            if (key == 'p' || key == 'P') {
                state = (state == GAME_PAUSED) ? GAME_PLAYING : GAME_PAUSED;
                continue;
            }

            if (state == GAME_PLAYING) {
                int dx = 0;
                int dy = 0;
                if (key == KEY_LEFT || key == 'a' || key == 'A') dx = -5;
                else if (key == KEY_RIGHT || key == 'd' || key == 'D') dx = 5;
                else if (key == KEY_UP || key == 'w' || key == 'W') dy = -5;
                else if (key == KEY_DOWN || key == 's' || key == 'S') dy = 5;
                if (dx || dy) game_engine_move(&engine, PLAYER_ID, dx, dy);
            }
        }

        if (state == GAME_TITLE) {
            if (redraw_title) {
                draw_title(api, best);
                redraw_title = 0;
            }
            api->timer_sleep(20);
            continue;
        }

        if (state == GAME_PLAYING) {
            game_engine_update(&engine);
            frames++;
            spawn_clock++;

            /* Score advances at 5 points per second on the nominal 50 FPS loop. */
            if ((frames % 10u) == 0) score++;
            if (score > best) best = score;

            uint32_t spawn_interval = 22u;
            if (score / 10u < 14u) spawn_interval -= score / 10u;
            else spawn_interval = 8u;

            if (spawn_clock >= spawn_interval) {
                spawn_hazard(&engine, score);
                spawn_clock = 0;
            }

            if (player_hit_hazard(&engine)) {
                state = GAME_OVER;
            }
        }

        draw_arena(api, &engine, score, best);

        if (state == GAME_PAUSED) {
            draw_overlay(api, "PAUSED", "Press P to resume.", "SPACE/ESC exits.");
        } else if (state == GAME_OVER) {
            draw_overlay(api, "YOU WERE HIT!", "R or ENTER: try again.", "SPACE/ESC: launcher.");
        }

        api->timer_sleep(20);
    }
}
