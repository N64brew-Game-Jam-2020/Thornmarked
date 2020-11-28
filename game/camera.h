#pragma once

#include <ultra64.h>

struct graphics;

// Camera system.
struct sys_camera {
    int unused_;
};

// Initialize the camera system.
void camera_init(struct sys_camera *restrict csys);

// Set the projection matrix for rendering from the camera's perspective.
Gfx *camera_render(struct sys_camera *restrict csys,
                   struct graphics *restrict gr, Gfx *dl);
