#pragma once

#include "game/core/model.h"

#include <ultra64.h>

struct graphics;
struct sys_model;
struct sys_phys;

// Initialize model rendering.
void model_render_init(void);

// Update model animation state.
void model_update(struct sys_model *restrict msys, float dt);

// Render all models.
Gfx *model_render(Gfx *dl, struct graphics *restrict gr,
                  struct sys_model *restrict msys,
                  struct sys_phys *restrict psys);
