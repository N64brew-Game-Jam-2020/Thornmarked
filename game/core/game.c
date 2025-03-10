#include "game/core/game.h"

#include "base/base.h"
#include "base/console.h"
#include "game/core/input.h"
#include "game/core/random.h"

#include <stdbool.h>

void game_init(struct game_state *restrict gs) {
    sfx_init(&gs->sfx);
    rand_init(&grand, 0x01234567, 0x243F6A88); // Pi fractional digits.
    entity_init(&gs->ent);
    physics_init(&gs->physics);
    walk_init(&gs->walk);
    camera_init(&gs->camera);
    model_init(&gs->model);
    monster_init(&gs->monster);
    player_init(&gs->player);
    particle_init(&gs->particle);
    menu_init(gs);
}

void game_update(struct game_state *restrict gs, float dt) {
    cprintf("dt = %.3f\n", (double)dt);
    if (dt > 0.1f || dt < 0.0f) {
        fatal_error("dt = %f", (double)dt);
    }
    sfx_update(&gs->sfx, dt); // Must be first.
    menu_update(gs, dt);
    time_update2(&gs->time);
    if (gs->menu.stack_size == 0) {
        particle_update(&gs->particle, dt);
        player_update(gs, dt);
        stage_update(gs, dt);
        monster_update(&gs->monster, &gs->physics, &gs->walk, dt);
        walk_update(&gs->walk, &gs->physics, dt);
        physics_update(&gs->physics, dt);
        camera_update(&gs->camera);
        model_update(&gs->model);
    }

    if (gs->input.count >= 1 &&
        (gs->input.input[0].button_press & BUTTON_L) != 0) {
        gs->show_console = !gs->show_console;
    }
}

void entity_destroy(struct game_state *restrict gs, ent_id ent) {
    entity_freeid(&gs->ent, ent);
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

void entity_destroyall(struct game_state *restrict gs) {
    entity_freeall(&gs->ent);
    physics_destroyall(&gs->physics);
    walk_destroyall(&gs->walk);
    model_destroyall(&gs->model);
    monster_destroyall(&gs->monster);
    player_destroyall(&gs->player);
}
