#include "game/core/game.h"

#include "assets/model.h"
#include "assets/texture.h"
#include "base/base.h"
#include "game/core/input.h"
#include "game/core/random.h"

#include <stdbool.h>

static void spawn_player(struct game_state *restrict gs, int player_index,
                         ent_id ent) {
    struct cp_phys *pp = physics_new(&gs->physics, ent);
    pp->radius = 0.5f;
    pp->team = TEAM_PLAYER;
    walk_new(&gs->walk, ent);
    struct cp_model *mp = model_new(&gs->model, ent);
    pak_texture texture = player_index == 0 ? IMG_FAIRY1 : IMG_FAIRY2;
    mp->model_id = MODEL_FAIRY;
    mp->material[0] = (struct material){
        .flags = MAT_ENABLED | MAT_CULL_BACK | MAT_VERTEX_COLOR,
        .texture_id = texture,
    };
    mp->material[1] = (struct material){
        .flags = MAT_ENABLED | MAT_VERTEX_COLOR,
        .texture_id = texture,
    };
    mp->animation_id = 4;
    player_new(&gs->player, player_index, ent);
}

static void spawn_monster(struct game_state *restrict gs, ent_id ent,
                          pak_model model, pak_texture texture) {
    struct cp_phys *pp = physics_new(&gs->physics, ent);
    pp->pos = (vec2){{
        rand_frange(&grand, -2.0f, 2.0f),
        rand_frange(&grand, -2.0f, 2.0f),
    }};
    pp->radius = 0.5f;
    pp->team = TEAM_MONSTER;
    walk_new(&gs->walk, ent);
    struct cp_model *mp = model_new(&gs->model, ent);
    mp->model_id = model;
    mp->material[0] = (struct material){
        .flags = MAT_ENABLED | MAT_CULL_BACK | MAT_VERTEX_COLOR,
        .texture_id = texture,
    };
    monster_new(&gs->monster, ent);
}

void game_init(struct game_state *restrict gs) {
    rand_init(&grand, 0x01234567, 0x243F6A88); // Pi fractional digits.
    physics_init(&gs->physics);
    walk_init(&gs->walk);
    camera_init(&gs->camera);
    model_init(&gs->model);
    monster_init(&gs->monster);
    player_init(&gs->player);
    particle_init(&gs->particle);

    int id = 1;
    for (int i = 0; i < gs->input.count; i++) {
        spawn_player(gs, i, (ent_id){id++});
    }
    for (int i = 0; i < 5; i++) {
        spawn_monster(gs, (ent_id){id++},
                      i & 1 ? MODEL_BLUEENEMY : MODEL_GREENENEMY,
                      i & 1 ? IMG_BLUEENEMY : IMG_GREENENEMY);
    }

    static const vec3 particle_pos[4] = {
        {{-1.0f, -1.0f, 1.0f}},
        {{+1.0f, -1.0f, 1.0f}},
        {{-1.0f, +1.0f, 1.0f}},
        {{+1.0f, +1.0f, 1.0f}},
    };
    static const color particle_color[4] = {
        {{255, 0, 0, 255}},
        {{255, 255, 0, 255}},
        {{0, 255, 0, 255}},
        {{255, 0, 255, 255}},
    };
    for (int i = 0; i < 4; i++) {
        particle_create(&gs->particle, particle_pos[i], (float)(i + 1),
                        particle_color[i]);
    }
}

void game_update(struct game_state *restrict gs, float dt) {
    particle_update(&gs->particle, dt);
    player_update(gs, dt);
    monster_update(&gs->monster, &gs->physics, &gs->walk, dt);
    walk_update(&gs->walk, &gs->physics, dt);
    physics_update(&gs->physics, dt);
    camera_update(&gs->camera);
    model_update(&gs->model);
    if (gs->input.count >= 1 &&
        (gs->input.input[0].button_press & BUTTON_A) != 0) {
        gs->show_console = !gs->show_console;
    }
}

void entity_destroy(struct game_state *restrict gs, ent_id ent) {
    {
        struct cp_phys *pp = physics_get(&gs->physics, ent);
        if (pp != NULL) {
            pp->ent = ENTITY_DESTROY;
        }
    }
    {
        struct cp_walk *wp = walk_get(&gs->walk, ent);
        if (wp != NULL) {
            wp->ent = ENTITY_DESTROY;
        }
    }
    {
        struct cp_model *mp = model_get(&gs->model, ent);
        if (mp != NULL) {
            mp->ent = ENTITY_DESTROY;
        }
    }
    {
        struct cp_monster *mp = monster_get(&gs->monster, ent);
        if (mp != NULL) {
            mp->ent = ENTITY_DESTROY;
        }
    }
    {
        struct cp_player *pl = player_get(&gs->player, ent);
        if (pl != NULL) {
            pl->ent = ENTITY_DESTROY;
        }
    }
}
