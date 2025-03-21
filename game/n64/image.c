#include "game/n64/image.h"

#include "assets/image.h"
#include "assets/pak.h"
#include "base/base.h"
#include "base/fixup.h"
#include "base/memory.h"
#include "base/pak/pak.h"
#include "game/core/menu.h"
#include "game/n64/graphics.h"

enum {
    // Maximum number of images which can be loaded at once.
    IMAGE_SLOTS = 4,

    // Amount of memory which can be used for images.
    IMAGE_HEAPSIZE = 192 * 1024,
};

// A single rectangle of image data in a larger image.
struct image_rect {
    short x, y, xsz, ysz;
    void *pixels;
};

// Header for a strip-based image.
struct image_header {
    int rect_count;
    struct image_rect rect[];
};

// Image system state.
struct image_state {
    struct mem_zone heap;
    struct image_header *image[IMAGE_SLOTS];
    int image_from_slot[IMAGE_SLOTS];
    int image_to_slot[PAK_IMAGE_COUNT + 1];
};

// Fix the internal pointers in an image after loading.
static void image_fixup(struct image_header *restrict img, size_t size) {
    uintptr_t base = (uintptr_t)img;
    for (int i = 0; i < img->rect_count; i++) {
        img->rect[i].pixels = pointer_fixup(img->rect[i].pixels, base, size);
    }
}

// Load an image into the given slot.
static void image_load_slot(struct image_state *restrict ist, pak_image asset,
                            int slot) {
    int obj = pak_image_object(asset);
    size_t obj_size = pak_objects[obj].size;
    struct image_header *restrict img = mem_zone_alloc(&ist->heap, obj_size);
    pak_load_asset_sync(img, obj_size, obj);
    image_fixup(img, obj_size);
    ist->image[slot] = img;
    ist->image_to_slot[asset.id] = slot;
    ist->image_from_slot[slot] = asset.id;
}

// State of the image system.
static struct image_state image_state;

// Load an image into an available slot.
static void image_load(pak_image asset) {
    struct image_state *restrict ist = &image_state;
    if (asset.id < 1 || PAK_IMAGE_COUNT < asset.id) {
        fatal_error("image_load: invalid image\nImage: %d", asset.id);
    }
    int slot = ist->image_to_slot[asset.id];
    if (ist->image_from_slot[slot] == asset.id) {
        return;
    }
    for (slot = 0; slot < IMAGE_SLOTS; slot++) {
        if (ist->image_from_slot[slot] == 0) {
            image_load_slot(ist, asset, slot);
            return;
        }
    }
    fatal_error("image_load: no slots available");
}

void image_init(void) {
    mem_zone_init(&image_state.heap, IMAGE_HEAPSIZE, "image");
    image_load(IMG_LOGO);
    image_load(IMG_POINT);
}

static const Gfx image_dl[] = {
    gsDPPipeSync(),
    gsDPSetTexturePersp(G_TP_NONE),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF),
    gsSPGeometryMode(~(unsigned)0, 0),
    gsSPTexture(0x2000, 0x2000, 0, G_TX_RENDERTILE, G_ON),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsDPSetTexturePersp(G_TP_NONE),
    gsDPSetTextureFilter(G_TF_POINT),
    gsSPEndDisplayList(),
};

static Gfx *image_draw(Gfx *dl, Gfx *dl_end, pak_image asset, int x, int y) {
    (void)dl_end;
    struct image_state *restrict ist = &image_state;
    int slot = ist->image_to_slot[asset.id];
    if (ist->image_from_slot[slot] != asset.id) {
        fatal_error("image_draw: not loaded\nImage: %d", asset.id);
    }
    const struct image_header *restrict img = ist->image[slot];
    gSPDisplayList(dl++, image_dl);
    for (int i = 0; i < img->rect_count; i++) {
        struct image_rect r = img->rect[i];
        unsigned xsz = (r.xsz + 3) & ~3u;
        gDPSetTextureImage(dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
                           img->rect[i].pixels);
        gDPSetTile(dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
                   G_TX_NOMIRROR, 0, G_TX_NOLOD, G_TX_NOMIRROR, 0, G_TX_NOLOD);
        gDPLoadSync(dl++);
        gDPLoadBlock(dl++, G_TX_LOADTILE, 0, 0, xsz * r.ysz - 1, 0);
        gDPPipeSync(dl++);
        gDPSetTile(dl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, xsz >> 2, 0, 0, 0, 0, 0,
                   0, 0, 0, 0);
        gDPSetTileSize(dl++, 0, 0, 0, (xsz - 1) << G_TEXTURE_IMAGE_FRAC,
                       (r.ysz - 1) << G_TEXTURE_IMAGE_FRAC);
        gSPTextureRectangle(dl++, (x + r.x) << 2, (y + r.y) << 2,
                            (x + r.x + xsz) << 2, (y + r.y + r.ysz) << 2, 0, 0,
                            0, 1 << 10, 1 << 10);
    }
    return dl;
}

Gfx *image_render(Gfx *dl, struct graphics *restrict gr,
                  struct sys_menu *restrict msys) {
    // Coordinates of screen center.
    const int x0 = gr->width >> 1, y0 = gr->height >> 1;
    for (int i = 0; i < msys->image_count; i++) {
        const struct menu_image *restrict imp = &msys->image[i];
        dl = image_draw(dl, gr->dl_end, imp->image, x0 + imp->pos.x,
                        y0 - imp->pos.y);
    }
    return dl;
}
