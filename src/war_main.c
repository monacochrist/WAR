//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_main.c
//-----------------------------------------------------------------------------

#include "../vendor/wayland/generated/linux-dmabuf-v1-protocol.c"
#include "../vendor/wayland/generated/xdg-shell-protocol.c"

#include "../key/key.h"
#include "../vendor/libsodium-1.0.21/include/sodium.h"
#include "../vendor/wayland/generated/linux-dmabuf-v1-client-protocol.h"
#include "../vendor/wayland/generated/xdg-shell-client-protocol.h"
#include "h/war_build_keymap_functions.h"
#include "h/war_color.h"
#include "h/war_command.h"
#include "h/war_config.h"
#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_functions.h"
#include "h/war_keymap.h"
#include "h/war_keymap_functions.h"
#include "h/war_main.h"
#include "h/war_pool.h"
#include "h/war_vulkan.h"
#include "h/war_wayland.h"

#include <errno.h>
#include <fcntl.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <pipewire-0.3/pipewire/context.h>
#include <pipewire-0.3/pipewire/core.h>
#include <pipewire-0.3/pipewire/pipewire.h>
#include <pipewire-0.3/pipewire/stream.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <spa-0.2/spa/param/audio/format-utils.h>
#include <spa-0.2/spa/param/audio/format.h>
#include <spa-0.2/spa/param/audio/raw.h>
#include <spa-0.2/spa/param/latency-utils.h>
#include <spa-0.2/spa/pod/builder.h>
#include <spa-0.2/spa/pod/pod.h>
#include <spa-0.2/spa/utils/hook.h>
#include <spa-0.2/spa/utils/list.h>
#include <spa-0.2/spa/utils/result.h>
#include <spa-0.2/spa/utils/string.h>
#include <stdint.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

//---------------------------------------------------------------------------
// WAYLAND LISTENERS
//---------------------------------------------------------------------------

#define WASSERT(x)                                                             \
    do {                                                                       \
        if (!(x)) {                                                            \
            call_king_terry("assert: %s", #x);                                 \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

static void war_wayland_registry_global(void* data,
                                        struct wl_registry* registry,
                                        uint32_t name,
                                        const char* interface,
                                        uint32_t version) {
    war_wayland_context* ctx_wayland = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        ctx_wayland->compositor = wl_registry_bind(registry,
                                                   name,
                                                   &wl_compositor_interface,
                                                   version < 6 ? version : 6);
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
        ctx_wayland->xdg_wm_base = wl_registry_bind(
            registry, name, &xdg_wm_base_interface, version < 6 ? version : 6);
    else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0)
        ctx_wayland->dmabuf = wl_registry_bind(registry,
                                               name,
                                               &zwp_linux_dmabuf_v1_interface,
                                               version < 4 ? version : 4);
    else if (strcmp(interface, wl_seat_interface.name) == 0)
        ctx_wayland->seat = wl_registry_bind(
            registry, name, &wl_seat_interface, version < 8 ? version : 8);
    else if (strcmp(interface, wl_output_interface.name) == 0)
        ctx_wayland->output = wl_registry_bind(
            registry, name, &wl_output_interface, version < 4 ? version : 4);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        ctx_wayland->shm =
            wl_registry_bind(registry, name, &wl_shm_interface, 1);
}
static void war_wayland_registry_global_remove(void* data,
                                               struct wl_registry* registry,
                                               uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}
static const struct wl_registry_listener war_wayland_registry_listener = {
    .global = war_wayland_registry_global,
    .global_remove = war_wayland_registry_global_remove,
};
static void war_xdg_surface_configure(void* data,
                                      struct xdg_surface* xdg_surface,
                                      uint32_t serial) {
    war_wayland_context* ctx_wayland = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    ctx_wayland->configured = 1;
}
static const struct xdg_surface_listener war_xdg_surface_listener = {
    .configure = war_xdg_surface_configure,
};
static void war_xdg_toplevel_configure(void* data,
                                       struct xdg_toplevel* toplevel,
                                       int32_t width,
                                       int32_t height,
                                       struct wl_array* states) {
    war_wayland_context* ctx_wayland = data;
    (void)toplevel;
    (void)states;
    if (width > 0) ctx_wayland->width = (uint32_t)width;
    if (height > 0) ctx_wayland->height = (uint32_t)height;
    if (ctx_wayland->env && ctx_wayland->env->ctx_cursor) {
        double cw = ctx_wayland->env->ctx_cursor->cell_width;
        double ch = ctx_wayland->env->ctx_cursor->cell_height;
        if (cw > 0 && ch > 0) {
            ctx_wayland->num_cols =
                (uint32_t)((double)width / cw) - ctx_wayland->gutter_cols;
            ctx_wayland->num_rows =
                (uint32_t)((double)height / ch) - ctx_wayland->gutter_rows;
        }
    }
}
static void war_xdg_toplevel_configure_bounds(void* data,
                                              struct xdg_toplevel* toplevel,
                                              int32_t width,
                                              int32_t height) {
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
}
static void war_xdg_toplevel_wm_capabilities(void* data,
                                             struct xdg_toplevel* toplevel,
                                             struct wl_array* capabilities) {
    (void)data;
    (void)toplevel;
    (void)capabilities;
}
static void war_xdg_toplevel_close(void* data, struct xdg_toplevel* toplevel) {
    war_wayland_context* ctx_wayland = data;
    (void)toplevel;
    ctx_wayland->running = 0;
}
static const struct xdg_toplevel_listener war_xdg_toplevel_listener = {
    .configure = war_xdg_toplevel_configure,
    .configure_bounds = war_xdg_toplevel_configure_bounds,
    .wm_capabilities = war_xdg_toplevel_wm_capabilities,
    .close = war_xdg_toplevel_close,
};
static void
war_frame_done(void* data, struct wl_callback* callback, uint32_t time);
static const struct wl_callback_listener war_frame_listener = {
    .done = war_frame_done,
};
static void
war_frame_done(void* data, struct wl_callback* callback, uint32_t time) {
    war_wayland_context* ctx_wayland = data;
    (void)time;
    wl_callback_destroy(callback);
    if (ctx_wayland->rendering) {
        war_render_frame(ctx_wayland, ctx_wayland->vk);
    }
    ctx_wayland->frame_callback = wl_surface_frame(ctx_wayland->surface);
    wl_callback_add_listener(
        ctx_wayland->frame_callback, &war_frame_listener, ctx_wayland);
    wl_surface_attach(ctx_wayland->surface, ctx_wayland->buffer, 0, 0);
    wl_surface_damage_buffer(ctx_wayland->surface,
                             0,
                             0,
                             (int32_t)ctx_wayland->width,
                             (int32_t)ctx_wayland->height);
    wl_surface_commit(ctx_wayland->surface);
}

static void war_keyboard_keymap(void* data,
                                struct wl_keyboard* keyboard,
                                uint32_t format,
                                int fd,
                                uint32_t size);
static void war_keyboard_enter(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface,
                               struct wl_array* keys);
static void war_keyboard_leave(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface);
static void war_keyboard_key(void* data,
                             struct wl_keyboard* keyboard,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t key,
                             uint32_t state);
static void war_keyboard_modifiers(void* data,
                                   struct wl_keyboard* keyboard,
                                   uint32_t serial,
                                   uint32_t mods_depressed,
                                   uint32_t mods_latched,
                                   uint32_t mods_grp,
                                   uint32_t mods_locked);
static void war_keyboard_repeat_info(void* data,
                                     struct wl_keyboard* keyboard,
                                     int32_t rate,
                                     int32_t delay);
static const struct wl_keyboard_listener war_keyboard_listener = {
    .keymap = war_keyboard_keymap,
    .enter = war_keyboard_enter,
    .leave = war_keyboard_leave,
    .key = war_keyboard_key,
    .modifiers = war_keyboard_modifiers,
    .repeat_info = war_keyboard_repeat_info,
};
static void war_keyboard_keymap(void* data,
                                struct wl_keyboard* keyboard,
                                uint32_t format,
                                int fd,
                                uint32_t size) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)format;
    char* map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    WASSERT(map_str != MAP_FAILED);
    ctx_wayland->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    ctx_wayland->xkb_keymap =
        xkb_keymap_new_from_string(ctx_wayland->xkb_ctx,
                                   map_str,
                                   XKB_KEYMAP_FORMAT_TEXT_V1,
                                   XKB_KEYMAP_COMPILE_NO_FLAGS);
    ctx_wayland->xkb_state = xkb_state_new(ctx_wayland->xkb_keymap);
    munmap(map_str, size);
    close(fd);
}
static void war_keyboard_enter(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface,
                               struct wl_array* keys) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
    (void)keys;
}
static void war_keyboard_leave(void* data,
                               struct wl_keyboard* keyboard,
                               uint32_t serial,
                               struct wl_surface* surface) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
}
static void war_keyboard_key(void* data,
                             struct wl_keyboard* keyboard,
                             uint32_t serial,
                             uint32_t time,
                             uint32_t key,
                             uint32_t state) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)serial;
    (void)time;
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED) {
        if (key == ctx_wayland->repeat_key) {
            ctx_wayland->repeat_active = 0;
            struct itimerspec off = {0};
            timerfd_settime(ctx_wayland->repeat_timer_fd, 0, &off, NULL);
        }
        return;
    }
    xkb_keysym_t sym =
        xkb_state_key_get_one_sym(ctx_wayland->xkb_state, key + 8);
    uint32_t keysym = war_normalize_keysym(sym);
    if (keysym == XKB_KEY_NoSymbol) return;

    if (ctx_wayland->repeat_rate > 0) {
        ctx_wayland->repeat_key = key;
        ctx_wayland->repeat_sym = keysym;
        ctx_wayland->repeat_active = 1;
        // arm timerfd: initial delay then periodic at repeat_rate
        int32_t d = ctx_wayland->repeat_delay;
        int32_t r = ctx_wayland->repeat_rate;
        struct itimerspec its = {
            .it_value = {d / 1000, (long)(d % 1000) * 1000000L},
            .it_interval = {0, r > 0 ? 1000000000L / r : 0},
        };
        timerfd_settime(ctx_wayland->repeat_timer_fd, 0, &its, NULL);
    }

    war_env* env = ctx_wayland->env;
    war_keymap_context* keymap = env->ctx_keymap;
    war_config_context* config = env->ctx_config;

    uint32_t mode = WAR_MODE_ID_ROLL;
    uint64_t current_state = 0;
    uint32_t mod = 0;
    {   xkb_mod_index_t mi;
        mi = xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap, XKB_MOD_NAME_SHIFT);
        if (mi != XKB_MOD_INVALID && xkb_state_mod_index_is_active(ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED)) mod |= MOD_SHIFT;
        mi = xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap, XKB_MOD_NAME_CTRL);
        if (mi != XKB_MOD_INVALID && xkb_state_mod_index_is_active(ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED)) mod |= MOD_CTRL;
        mi = xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap, XKB_MOD_NAME_ALT);
        if (mi != XKB_MOD_INVALID && xkb_state_mod_index_is_active(ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED)) mod |= MOD_ALT;
        mi = xkb_keymap_mod_get_index(ctx_wayland->xkb_keymap, XKB_MOD_NAME_LOGO);
        if (mi != XKB_MOD_INVALID && xkb_state_mod_index_is_active(ctx_wayland->xkb_state, mi, XKB_STATE_MODS_DEPRESSED)) mod |= MOD_LOGO;
    }

    size_t trans_idx = mode * (size_t)config->KEYMAP_STATE_CAPACITY *
                           config->KEYMAP_KEYSYM_CAPACITY *
                           config->KEYMAP_MOD_CAPACITY +
                       current_state * (size_t)config->KEYMAP_KEYSYM_CAPACITY *
                           config->KEYMAP_MOD_CAPACITY +
                       (size_t)keysym * config->KEYMAP_MOD_CAPACITY + mod;

    uint64_t next = keymap->next_state[trans_idx];
    if (!next) return;

    size_t func_count_idx = mode * (size_t)config->KEYMAP_STATE_CAPACITY + next;
    uint8_t count = keymap->function_count[func_count_idx];
    if (!count) return;

    size_t func_base = mode * (size_t)config->KEYMAP_STATE_CAPACITY *
                           config->KEYMAP_FUNCTION_CAPACITY +
                       next * (size_t)config->KEYMAP_FUNCTION_CAPACITY;
    for (uint8_t i = 0; i < count; i++) {
        void (*fn)(war_env*) = keymap->function[func_base + i];
        if (fn) fn(env);
    }
}
static void war_keyboard_modifiers(void* data,
                                   struct wl_keyboard* keyboard,
                                   uint32_t serial,
                                   uint32_t mods_depressed,
                                   uint32_t mods_latched,
                                   uint32_t mods_grp,
                                   uint32_t mods_locked) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)serial;
    xkb_state_update_mask(ctx_wayland->xkb_state,
                          mods_depressed,
                          mods_latched,
                          mods_grp,
                          0,
                          0,
                          mods_locked);
}
static void war_keyboard_repeat_info(void* data,
                                     struct wl_keyboard* keyboard,
                                     int32_t rate,
                                     int32_t delay) {
    war_wayland_context* ctx_wayland = data;
    (void)keyboard;
    (void)rate;
    (void)delay;
    // client-side repeat uses hardcoded defaults in bootstrap
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    CALL_KING_TERRY("war");
    //--------------------------------------------------------------------
    // KEY CHECK
    //--------------------------------------------------------------------
    war_key();
    //-------------------------------------------------------------------------
    // BOOTSTRAP
    //-------------------------------------------------------------------------
    war_config_context* tmp_ctx_config = calloc(1, sizeof(war_config_context));
    war_config_default(tmp_ctx_config);
    war_pool_context* tmp_ctx_pool = calloc(1, sizeof(war_pool_context));
    tmp_ctx_pool->size =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    tmp_ctx_pool->offset =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    tmp_ctx_pool->alignment =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint32_t));
    tmp_ctx_pool->id =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(war_pool_id));
    war_pool_default(tmp_ctx_pool, tmp_ctx_config);
    for (uint32_t i = 0; i < tmp_ctx_pool->count; i++) {
        tmp_ctx_pool->total_size += tmp_ctx_pool->size[i];
    }
    tmp_ctx_pool->pool = mmap(NULL,
                              tmp_ctx_pool->total_size,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS,
                              -1,
                              0);
    if (tmp_ctx_pool->pool == MAP_FAILED) {
        call_king_terry("tmp pool map failed, total_size: %llu",
                        (unsigned long long)tmp_ctx_pool->total_size);
        exit(1);
    }
    memset(tmp_ctx_pool->pool, 0, tmp_ctx_pool->total_size);
    war_hot_context* tmp_ctx_hot =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT);
    tmp_ctx_hot->function =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FUNCTION);
    tmp_ctx_hot->handle =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_HANDLE);
    tmp_ctx_hot->fn_id =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FN_ID);
    tmp_ctx_hot->name =
        war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_HOT_CONTEXT_NAME);
    tmp_ctx_hot->name[WAR_HOT_ID_CONFIG] = "war_config_override";
    tmp_ctx_hot->name[WAR_HOT_ID_COMMAND] = "war_command_override";
    tmp_ctx_hot->name[WAR_HOT_ID_COLOR] = "war_color_override";
    tmp_ctx_hot->name[WAR_HOT_ID_PLUGIN] = "war_plugin_override";
    tmp_ctx_hot->name[WAR_HOT_ID_POOL] = "war_pool_override";
    tmp_ctx_hot->name[WAR_HOT_ID_KEYMAP] = "war_keymap_override";
    war_env* tmp_env = war_pool_alloc_new(tmp_ctx_pool, WAR_POOL_ID_ENV);
    tmp_env->ctx_config = tmp_ctx_config;
    tmp_env->ctx_hot = tmp_ctx_hot;
    war_mkdir(tmp_ctx_config->DIR_CONFIG, 0755);
    war_mkdir(tmp_ctx_config->DIR_CACHE, 0755);
    war_mkdir(tmp_ctx_config->DIR_OVERRIDE, 0755);
    war_mkdir(tmp_ctx_config->DIR_UNDO, 0755);
    war_mkdir(tmp_ctx_config->DIR_WARPOON, 0755);
    war_mkdir(tmp_ctx_config->DIR_JUMPLIST, 0755);
    war_mkdir(tmp_ctx_config->DIR_LOG, 0755);
    tmp_ctx_hot->fn_id[0] = WAR_HOT_ID_CONFIG;
    tmp_ctx_hot->fn_count = 1;
    war_override(tmp_ctx_hot->fn_count, tmp_ctx_hot->fn_id, tmp_env);
    war_pool_context* ctx_pool = calloc(1, sizeof(war_pool_context));
    ctx_pool->size =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    ctx_pool->offset =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint64_t));
    ctx_pool->alignment =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(uint32_t));
    ctx_pool->id =
        calloc(tmp_ctx_config->POOL_MAX_ALLOCATIONS, sizeof(war_pool_id));
    war_pool_default(ctx_pool, tmp_ctx_config);
    tmp_ctx_hot->fn_id[0] = WAR_HOT_ID_POOL;
    tmp_ctx_hot->fn_count = 1;
    tmp_env->ctx_pool = ctx_pool;
    war_override(tmp_ctx_hot->fn_count, tmp_ctx_hot->fn_id, tmp_env);
    for (uint32_t i = 0; i < ctx_pool->count; i++) {
        ctx_pool->total_size += ctx_pool->size[i];
    }
    ctx_pool->pool = mmap(NULL,
                          ctx_pool->total_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0);
    if (ctx_pool->pool == MAP_FAILED) {
        call_king_terry("pool map failed, total_size: %llu",
                        (unsigned long long)ctx_pool->total_size);
        exit(1);
    }
    memset(ctx_pool->pool, 0, ctx_pool->total_size);
    war_config_context* ctx_config =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_CONFIG_CONTEXT);
    memcpy((uint8_t*)ctx_config,
           (uint8_t*)tmp_ctx_config,
           war_pool_size(ctx_pool, WAR_POOL_ID_CONFIG_CONTEXT));
    war_hot_context* ctx_hot =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT);
    ctx_hot->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FUNCTION);
    ctx_hot->handle =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_HANDLE);
    ctx_hot->fn_id =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FN_ID);
    ctx_hot->name = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_NAME);
    memcpy((uint8_t*)ctx_hot->function,
           (uint8_t*)tmp_ctx_hot->function,
           war_pool_size(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_FUNCTION));
    memcpy((uint8_t*)ctx_hot->handle,
           (uint8_t*)tmp_ctx_hot->handle,
           war_pool_size(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_HANDLE));
    memcpy((uint8_t*)ctx_hot->name,
           (uint8_t*)tmp_ctx_hot->name,
           war_pool_size(ctx_pool, WAR_POOL_ID_HOT_CONTEXT_NAME));
    free(tmp_ctx_config);
    tmp_ctx_config = NULL;
    tmp_env = NULL;
    free(tmp_ctx_pool->size);
    free(tmp_ctx_pool->offset);
    free(tmp_ctx_pool->alignment);
    free(tmp_ctx_pool->id);
    if (munmap(tmp_ctx_pool->pool, tmp_ctx_pool->total_size) == -1) {
        call_king_terry("munmap failed");
    }
    free(tmp_ctx_pool);
    tmp_ctx_pool = NULL;
    //------------------------------------------------------------------------
    // INIT
    //------------------------------------------------------------------------
    war_color_context* ctx_color =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COLOR_CONTEXT);
    war_color_default(ctx_color);
    war_keymap_context* ctx_keymap =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT);
    ctx_keymap->function_id =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FUNCTION_ID);
    ctx_keymap->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FUNCTION);
    ctx_keymap->function_count =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FUNCTION_COUNT);
    ctx_keymap->flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_FLAGS);
    ctx_keymap->next_state =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_NEXT_STATE);
    ctx_keymap->state_count =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_KEYMAP_CONTEXT_STATE_COUNT);
    for (uint32_t i = 0; i < ctx_config->KEYMAP_MODE_CAPACITY; i++) {
        ctx_keymap->state_count[i] = 1;
    }
    war_keymap_default(ctx_keymap, ctx_config);
    war_command_context* ctx_command =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT);
    ctx_command->function_id =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FUNCTION_ID);
    ctx_command->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FUNCTION);
    ctx_command->function_count = war_pool_alloc_new(
        ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FUNCTION_COUNT);
    ctx_command->flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_FLAGS);
    ctx_command->next_state =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_COMMAND_CONTEXT_NEXT_STATE);
    ctx_command->state_count = 1;
    war_command_default(ctx_command, ctx_config);
    war_hook_context* ctx_hook =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT);
    ctx_hook->mode_flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT_MODE_FLAGS);
    ctx_hook->event_flags =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT_EVENT_FLAGS);
    ctx_hook->function =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_HOOK_CONTEXT_FUNCTION);
    war_env* env = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_ENV);
    env->ctx_color = ctx_color;
    env->ctx_keymap = ctx_keymap;
    env->ctx_command = ctx_command;
    env->ctx_hook = ctx_hook;
    env->ctx_pool = ctx_pool;
    env->ctx_config = ctx_config;
    env->ctx_hot = ctx_hot;
    ctx_hot->fn_id[0] = WAR_HOT_ID_COLOR;
    ctx_hot->fn_id[1] = WAR_HOT_ID_KEYMAP;
    ctx_hot->fn_id[2] = WAR_HOT_ID_COMMAND;
    ctx_hot->fn_id[3] = WAR_HOT_ID_PLUGIN;
    ctx_hot->fn_count = 4;
    war_override(ctx_hot->fn_count, ctx_hot->fn_id, env);
    war_wayland_context* ctx_wayland =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_WAYLAND_CONTEXT);
    ctx_wayland->env = env;
    env->ctx_wayland = ctx_wayland;
    war_vulkan_context* ctx_vk =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_VULKAN);
    ctx_wayland->vk = ctx_vk;
    //-------------------------------------------------------------------------
    // WAYLAND SETUP
    //-------------------------------------------------------------------------
    ctx_wayland->width = 1920;
    ctx_wayland->height = 1080;
    ctx_wayland->display = wl_display_connect_to_fd(war_wayland_make_fd());
    WASSERT(ctx_wayland->display);
    ctx_wayland->registry = wl_display_get_registry(ctx_wayland->display);
    WASSERT(ctx_wayland->registry);
    wl_registry_add_listener(
        ctx_wayland->registry, &war_wayland_registry_listener, ctx_wayland);
    wl_display_roundtrip(ctx_wayland->display);
    WASSERT(ctx_wayland->compositor && ctx_wayland->xdg_wm_base &&
            ctx_wayland->dmabuf && ctx_wayland->shm);
    ctx_wayland->surface =
        wl_compositor_create_surface(ctx_wayland->compositor);
    ctx_wayland->xdg_surface = xdg_wm_base_get_xdg_surface(
        ctx_wayland->xdg_wm_base, ctx_wayland->surface);
    ctx_wayland->toplevel = xdg_surface_get_toplevel(ctx_wayland->xdg_surface);
    WASSERT(ctx_wayland->surface && ctx_wayland->xdg_surface &&
            ctx_wayland->toplevel);
    xdg_toplevel_add_listener(
        ctx_wayland->toplevel, &war_xdg_toplevel_listener, ctx_wayland);
    xdg_toplevel_set_title(ctx_wayland->toplevel, "war");
    xdg_surface_add_listener(
        ctx_wayland->xdg_surface, &war_xdg_surface_listener, ctx_wayland);
    wl_surface_commit(ctx_wayland->surface);
    wl_display_roundtrip(ctx_wayland->display);
    WASSERT(ctx_wayland->configured);
    ctx_wayland->keyboard = wl_seat_get_keyboard(ctx_wayland->seat);
    wl_keyboard_add_listener(
        ctx_wayland->keyboard, &war_keyboard_listener, ctx_wayland);
    ctx_wayland->running = 1;
    ctx_wayland->rendering = 1;
    ctx_wayland->zoom = 2.0f;
    ctx_wayland->initial_zoom = ctx_wayland->zoom;
    ctx_wayland->panning[0] = 0;
    ctx_wayland->panning[1] = 0;
    ctx_wayland->right_bound = 1000000.0;
    ctx_wayland->top_bound = 127.0;
    ctx_wayland->repeat_active = 0;
    ctx_wayland->repeat_rate = 20;
    ctx_wayland->repeat_delay = 200;
    ctx_wayland->repeat_timer_fd =
        timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    WASSERT(ctx_wayland->repeat_timer_fd >= 0);

    //-------------------------------------------------------------------------
    // VULKAN + DMABUF SETUP
    //-------------------------------------------------------------------------
    war_vulkan_init(ctx_wayland, ctx_vk);

    struct zwp_linux_buffer_params_v1* params =
        zwp_linux_dmabuf_v1_create_params(ctx_wayland->dmabuf);
    zwp_linux_buffer_params_v1_add(params,
                                   ctx_vk->dmabuf_fd,
                                   0,
                                   (uint32_t)ctx_vk->img_layout.offset,
                                   (uint32_t)ctx_vk->img_layout.rowPitch,
                                   0,
                                   0);
    ctx_wayland->buffer =
        zwp_linux_buffer_params_v1_create_immed(params,
                                                ctx_wayland->width,
                                                ctx_wayland->height,
                                                DRM_FORMAT_ARGB8888,
                                                0);
    //-------------------------------------------------------------------------
    // FIRST FRAME RENDER SETUP (render pass, framebuffer)
    //-------------------------------------------------------------------------
    war_render_init(ctx_wayland, ctx_vk);

    //-------------------------------------------------------------------------
    // CURSOR INIT (needs render pass to exist)
    //-------------------------------------------------------------------------
    war_cursor_context* ctx_cursor =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR);
    env->ctx_cursor = ctx_cursor;
    ctx_cursor->cell_width = 10;
    ctx_cursor->cell_height = 20;
    ctx_wayland->gutter_rows = 3;
    ctx_wayland->gutter_cols = 4;
    ctx_wayland->num_cols = ctx_wayland->width / ctx_cursor->cell_width;
    ctx_wayland->num_rows = ctx_wayland->height / ctx_cursor->cell_height;
    ctx_cursor->x_width =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_X_WIDTH);
    ctx_cursor->x_width[0] = 1;
    war_cursor_init(ctx_cursor, ctx_pool, ctx_config, ctx_vk);
    ctx_cursor->instance_count = 1;
    ctx_cursor->instance[0].pos[0] = ctx_wayland->gutter_cols;
    ctx_cursor->instance[0].pos[1] = ctx_wayland->gutter_rows;
    ctx_cursor->instance[0].size[0] = 1;
    ctx_cursor->instance[0].size[1] = 1;
    ctx_cursor->instance[0].color[0] = 1;
    ctx_cursor->instance[0].color[1] = 0;
    ctx_cursor->instance[0].color[2] = 0;
    ctx_cursor->instance[0].color[3] = 1;
    //-------------------------------------------------------------------------
    // PIANO GUTTER INIT
    //-------------------------------------------------------------------------
    war_piano_gutter_context* ctx_pg =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER);
    war_piano_gutter_init(ctx_pg, ctx_pool, ctx_config, ctx_vk);
    war_piano_gutter_generate(ctx_pg);
    env->ctx_piano_gutter = ctx_pg;
    //-------------------------------------------------------------------------
    // FIRST FRAME RENDER (record + submit)
    //-------------------------------------------------------------------------
    war_render_frame(ctx_wayland, ctx_vk);
    ctx_wayland->frame_callback = wl_surface_frame(ctx_wayland->surface);
    wl_callback_add_listener(
        ctx_wayland->frame_callback, &war_frame_listener, ctx_wayland);

    wl_surface_attach(ctx_wayland->surface, ctx_wayland->buffer, 0, 0);
    wl_surface_damage_buffer(
        ctx_wayland->surface, 0, 0, ctx_wayland->width, ctx_wayland->height);
    wl_surface_commit(ctx_wayland->surface);

    //-------------------------------------------------------------------------
    // MAIN LOOP
    //-------------------------------------------------------------------------
    int wayland_fd = wl_display_get_fd(ctx_wayland->display);
    struct pollfd pfds[2] = {
        {.fd = wayland_fd, .events = POLLIN},
        {.fd = ctx_wayland->repeat_timer_fd, .events = POLLIN},
    };
    while (ctx_wayland->running) {
        wl_display_flush(ctx_wayland->display);
        if (wl_display_prepare_read(ctx_wayland->display) == 0) {
            poll(pfds, 2, -1);
            if (pfds[0].revents & POLLIN)
                wl_display_read_events(ctx_wayland->display);
            else
                wl_display_cancel_read(ctx_wayland->display);
        }
        wl_display_dispatch_pending(ctx_wayland->display);
        // drain timerfd (periodic, no re-arm needed)
        struct pollfd tfd = {.fd = ctx_wayland->repeat_timer_fd,
                             .events = POLLIN};
        if (poll(&tfd, 1, 0) > 0) {
            uint64_t exp;
            read(ctx_wayland->repeat_timer_fd, &exp, sizeof(exp));
            if (ctx_wayland->repeat_active &&
                ctx_wayland->repeat_sym != XKB_KEY_NoSymbol) {
                for (uint64_t i = 0; i < exp; i++) {
                    war_env* env = ctx_wayland->env;
                    war_keymap_context* keymap = env->ctx_keymap;
                    war_config_context* config = env->ctx_config;
                    size_t ti = (size_t)ctx_wayland->repeat_sym *
                                config->KEYMAP_MOD_CAPACITY;
                    uint64_t next = keymap->next_state[ti];
                    if (next) {
                        uint8_t cnt = keymap->function_count[next];
                        size_t fb =
                            next * (size_t)config->KEYMAP_FUNCTION_CAPACITY;
                        for (uint8_t j = 0; j < cnt; j++) {
                            void (*fn)(war_env*) = keymap->function[fb + j];
                            if (fn) fn(env);
                        }
                    }
                }
            }
        }
    }

    //-------------------------------------------------------------------------
    // CLEANUP
    //-------------------------------------------------------------------------
    vkDeviceWaitIdle(ctx_vk->device);
    vkDestroyCommandPool(ctx_vk->device, ctx_vk->cmd_pool, NULL);
    vkDestroyFramebuffer(ctx_vk->device, ctx_vk->framebuffer, NULL);
    vkDestroyImageView(ctx_vk->device, ctx_vk->image_view, NULL);
    vkDestroyRenderPass(ctx_vk->device, ctx_vk->render_pass, NULL);
    vkFreeMemory(ctx_vk->device, ctx_vk->memory, NULL);
    vkDestroyImage(ctx_vk->device, ctx_vk->image, NULL);
    vkDestroyDevice(ctx_vk->device, NULL);
    vkDestroyInstance(ctx_vk->instance, NULL);
    if (ctx_wayland->repeat_timer_fd >= 0) close(ctx_wayland->repeat_timer_fd);
    wl_buffer_destroy(ctx_wayland->buffer);
    xkb_state_unref(ctx_wayland->xkb_state);
    xkb_keymap_unref(ctx_wayland->xkb_keymap);
    xkb_context_unref(ctx_wayland->xkb_ctx);
    wl_keyboard_destroy(ctx_wayland->keyboard);
    wl_display_disconnect(ctx_wayland->display);
}
