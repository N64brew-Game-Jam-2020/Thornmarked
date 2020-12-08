#pragma once

#include "game/core/game.h"

struct graphics;

// The entire game state, including platform-specific details.
struct game_system {
    // Platform-agnostic game state.
    struct game_state state;
};

void game_system_init(struct game_system *restrict sys);

void game_system_update(struct game_system *restrict sys, float dt);

void game_system_render(struct game_system *restrict sys,
                        struct graphics *restrict gr);
