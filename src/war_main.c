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

#include "h/war_main.h"
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
#include "h/war_new_vulkan.h"
#include "h/war_nsgt.h"
#include "h/war_pool.h"
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
#include <time.h>
#include <unistd.h>

//---------------------------------------------------------------------------
// WAYLAND LISTENERS
//---------------------------------------------------------------------------

#define WASSERT(x) do { if (!(x)) { call_king_terry("assert: %s", #x); exit(1); } } while(0)

static uint32_t find_mem_type(VkPhysicalDeviceMemoryProperties props,
                              uint32_t bits,
                              VkMemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((bits & (1 << i)) &&
            (props.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return UINT32_MAX;
}
static void war_wayland_registry_global(void* data,
                                        struct wl_registry* registry,
                                        uint32_t name,
                                        const char* interface,
                                        uint32_t version) {
    war_wayland_context* ctx_wayland = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0)
        ctx_wayland->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version < 6 ? version : 6);
    else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
        ctx_wayland->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, version < 6 ? version : 6);
    else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0)
        ctx_wayland->dmabuf = wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, version < 4 ? version : 4);
    else if (strcmp(interface, wl_seat_interface.name) == 0)
        ctx_wayland->seat = wl_registry_bind(registry, name, &wl_seat_interface, version < 8 ? version : 8);
    else if (strcmp(interface, wl_output_interface.name) == 0)
        ctx_wayland->output = wl_registry_bind(registry, name, &wl_output_interface, version < 4 ? version : 4);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        ctx_wayland->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
}
static void war_wayland_registry_global_remove(void* data,
                                                struct wl_registry* registry,
                                                uint32_t name) {
    (void)data; (void)registry; (void)name;
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
    if (width > 0) ctx_wayland->width = (uint32_t)width;
    if (height > 0) ctx_wayland->height = (uint32_t)height;
}
static void war_xdg_toplevel_configure_bounds(void* data,
                                                struct xdg_toplevel* toplevel,
                                                int32_t width,
                                                int32_t height) {
    (void)data; (void)toplevel; (void)width; (void)height;
}
static void war_xdg_toplevel_wm_capabilities(void* data,
                                               struct xdg_toplevel* toplevel,
                                               struct wl_array* capabilities) {
    (void)data; (void)toplevel; (void)capabilities;
}
static void war_xdg_toplevel_close(void* data,
                                    struct xdg_toplevel* toplevel) {
    war_wayland_context* ctx_wayland = data;
    ctx_wayland->running = 0;
}
static const struct xdg_toplevel_listener war_xdg_toplevel_listener = {
    .configure = war_xdg_toplevel_configure,
    .configure_bounds = war_xdg_toplevel_configure_bounds,
    .wm_capabilities = war_xdg_toplevel_wm_capabilities,
    .close = war_xdg_toplevel_close,
};
static const struct wl_callback_listener war_frame_listener;
static void war_frame_done(void* data,
                            struct wl_callback* callback,
                            uint32_t time) {
    war_wayland_context* ctx_wayland = data;
    wl_callback_destroy(callback);
    //-----------------------------------------------------------------
    // PER-FRAME RENDER
    //
    // This fires at vsync. Put your per-frame drawing code here.
    //
    //   Steps:
    //     1. vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    //     2. vkQueueWaitIdle(queue);
    //
    //   The VkCommandBuffer was allocated with SIMULTANEOUS_USE
    //   so you can re-submit it without re-recording.
    //-----------------------------------------------------------------
    wl_surface_attach(ctx_wayland->surface, ctx_wayland->buffer, 0, 0);
    wl_surface_damage_buffer(ctx_wayland->surface, 0, 0,
                             (int32_t)ctx_wayland->width,
                             (int32_t)ctx_wayland->height);
    wl_surface_commit(ctx_wayland->surface);
    ctx_wayland->frame_callback = wl_surface_frame(ctx_wayland->surface);
    wl_callback_add_listener(
        ctx_wayland->frame_callback, &war_frame_listener, ctx_wayland);
}
static const struct wl_callback_listener war_frame_listener = {
    .done = war_frame_done,
};

int main(int argc, char** argv) {
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
                        tmp_ctx_pool->total_size);
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
                        ctx_pool->total_size);
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
    //-------------------------------------------------------------------------
    // WAYLAND SETUP
    //-------------------------------------------------------------------------
    war_wayland_context* ctx_wayland =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_WAYLAND_CONTEXT);
    ctx_wayland->width = 1280;
    ctx_wayland->height = 720;
    ctx_wayland->display = wl_display_connect_to_fd(war_wayland_make_fd());
    WASSERT(ctx_wayland->display);
    ctx_wayland->registry = wl_display_get_registry(ctx_wayland->display);
    WASSERT(ctx_wayland->registry);
    wl_registry_add_listener(
        ctx_wayland->registry, &war_wayland_registry_listener, ctx_wayland);
    wl_display_roundtrip(ctx_wayland->display);
    WASSERT(ctx_wayland->compositor && ctx_wayland->xdg_wm_base
        && ctx_wayland->dmabuf && ctx_wayland->shm);
    ctx_wayland->surface =
        wl_compositor_create_surface(ctx_wayland->compositor);
    ctx_wayland->xdg_surface = xdg_wm_base_get_xdg_surface(
        ctx_wayland->xdg_wm_base, ctx_wayland->surface);
    ctx_wayland->toplevel = xdg_surface_get_toplevel(ctx_wayland->xdg_surface);
    WASSERT(ctx_wayland->surface && ctx_wayland->xdg_surface && ctx_wayland->toplevel);
    xdg_toplevel_add_listener(
        ctx_wayland->toplevel, &war_xdg_toplevel_listener, ctx_wayland);
    xdg_toplevel_set_title(ctx_wayland->toplevel, "war");
    xdg_surface_add_listener(
        ctx_wayland->xdg_surface, &war_xdg_surface_listener, ctx_wayland);
    wl_surface_commit(ctx_wayland->surface);
    wl_display_roundtrip(ctx_wayland->display);
    WASSERT(ctx_wayland->configured);
    ctx_wayland->running = 1;

    //-------------------------------------------------------------------------
    // VULKAN + DMABUF SETUP
    //-------------------------------------------------------------------------
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "war",
        .apiVersion = VK_API_VERSION_1_2,
    };
    const char* extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };
    VkInstance instance;
    VkResult result = vkCreateInstance(&instance_info, NULL, &instance);
    WASSERT(result == VK_SUCCESS);

    uint32_t gpu_count;
    vkEnumeratePhysicalDevices(instance, &gpu_count, NULL);
    WASSERT(gpu_count > 0);
    VkPhysicalDevice physical_device;
    vkEnumeratePhysicalDevices(instance, &gpu_count, &physical_device);

    uint32_t qfam_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfam_count, NULL);
    VkQueueFamilyProperties qprops[32];
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &qfam_count, qprops);
    uint32_t graphics_family = UINT32_MAX;
    for (uint32_t i = 0; i < qfam_count; i++) {
        if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i; break;
        }
    }
    WASSERT(graphics_family != UINT32_MAX);

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo dqci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    const char* dev_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    };
    VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &dqci,
        .enabledExtensionCount = 5,
        .ppEnabledExtensionNames = dev_extensions,
    };
    VkDevice device;
    result = vkCreateDevice(physical_device, &dci, NULL, &device);
    WASSERT(result == VK_SUCCESS);

    VkQueue queue;
    vkGetDeviceQueue(device, graphics_family, 0, &queue);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = {ctx_wayland->width, ctx_wayland->height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image;
    result = vkCreateImage(device, &ici, NULL, &image);
    WASSERT(result == VK_SUCCESS);

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device, image, &mem_req);
    uint32_t mem_type = find_mem_type(
        mem_props, mem_req.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    WASSERT(mem_type != UINT32_MAX);

    VkExportMemoryAllocateInfo export_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export_info,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    VkDeviceMemory memory;
    result = vkAllocateMemory(device, &mai, NULL, &memory);
    WASSERT(result == VK_SUCCESS);
    result = vkBindImageMemory(device, image, memory, 0);
    WASSERT(result == VK_SUCCESS);

    VkImageSubresource sub = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT};
    VkSubresourceLayout img_layout;
    vkGetImageSubresourceLayout(device, image, &sub, &img_layout);

    VkImageMemoryBarrier init_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    VkCommandPoolCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
    };
    VkCommandPool cmd_pool;
    vkCreateCommandPool(device, &cpci, NULL, &cmd_pool);
    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer init_cmd;
    vkAllocateCommandBuffers(device, &cbai, &init_cmd);
    VkCommandBufferBeginInfo init_cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(init_cmd, &init_cbbi);
    vkCmdPipelineBarrier(init_cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, NULL, 0, NULL, 1, &init_barrier);
    vkEndCommandBuffer(init_cmd);
    VkSubmitInfo init_si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &init_cmd,
    };
    vkQueueSubmit(queue, 1, &init_si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, cmd_pool, 1, &init_cmd);

    PFN_vkGetMemoryFdKHR pfn_vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR");
    WASSERT(pfn_vkGetMemoryFdKHR);
    int dmabuf_fd = -1;
    VkMemoryGetFdInfoKHR fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    result = pfn_vkGetMemoryFdKHR(device, &fd_info, &dmabuf_fd);
    WASSERT(result == VK_SUCCESS && dmabuf_fd >= 0);

    struct zwp_linux_buffer_params_v1* params =
        zwp_linux_dmabuf_v1_create_params(ctx_wayland->dmabuf);
    zwp_linux_buffer_params_v1_add(params, dmabuf_fd, 0,
        (uint32_t)img_layout.offset, (uint32_t)img_layout.rowPitch, 0, 0);
    ctx_wayland->buffer = zwp_linux_buffer_params_v1_create_immed(
        params, ctx_wayland->width, ctx_wayland->height,
        DRM_FORMAT_ARGB8888, 0);

    //-------------------------------------------------------------------------
    // FIRST FRAME RENDER
    //-------------------------------------------------------------------------
    VkRenderPass render_pass;
    VkAttachmentDescription attach = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkAttachmentReference color_ref = {
        .attachment = 0, .layout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1, .pColorAttachments = &color_ref,
    };
    VkRenderPassCreateInfo rpci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &attach,
        .subpassCount = 1, .pSubpasses = &subpass,
    };
    vkCreateRenderPass(device, &rpci, NULL, &render_pass);

    VkShaderModule vert_module;
    uint8_t vert_ok = war_new_vulkan_get_shader_module(
        device, &vert_module, "build/spv/war_red_quad_vertex.spv");
    WASSERT(vert_ok);
    VkShaderModule frag_module;
    uint8_t frag_ok = war_new_vulkan_get_shader_module(
        device, &frag_module, "build/spv/war_red_quad_fragment.spv");
    WASSERT(frag_ok);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag_module,
         .pName = "main"},
    };
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    VkViewport viewport = {0, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 1};
    VkRect2D scissor = {{0, 0}, {ctx_wayland->width, ctx_wayland->height}};
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport,
        .scissorCount = 1, .pScissors = &scissor,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState blend_attach = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo color_blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &blend_attach,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
    VkPipelineLayout pipeline_layout;
    vkCreatePipelineLayout(device, &plci, NULL, &pipeline_layout);
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisample,
        .pColorBlendState = &color_blend,
        .layout = pipeline_layout,
        .renderPass = render_pass,
    };
    VkPipeline pipeline;
    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpci, NULL, &pipeline);
    vkDestroyShaderModule(device, vert_module, NULL);
    vkDestroyShaderModule(device, frag_module, NULL);

    VkImageViewCreateInfo ivci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    VkImageView image_view;
    vkCreateImageView(device, &ivci, NULL, &image_view);

    VkFramebufferCreateInfo fbci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1, .pAttachments = &image_view,
        .width = ctx_wayland->width,
        .height = ctx_wayland->height,
        .layers = 1,
    };
    VkFramebuffer framebuffer;
    vkCreateFramebuffer(device, &fbci, NULL, &framebuffer);

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cbai, &cmd);
    VkCommandBufferBeginInfo cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    vkBeginCommandBuffer(cmd, &cbbi);
    VkClearValue clear = {.color = {{0, 0, 0, 0}}};
    VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass, .framebuffer = framebuffer,
        .renderArea = {{0, 0}, {ctx_wayland->width, ctx_wayland->height}},
        .clearValueCount = 1, .pClearValues = &clear,
    };
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmd, 6, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    VkMemoryBarrier mem_barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1, &mem_barrier, 0, NULL, 0, NULL);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    wl_surface_attach(ctx_wayland->surface, ctx_wayland->buffer, 0, 0);
    wl_surface_damage_buffer(ctx_wayland->surface, 0, 0, ctx_wayland->width, ctx_wayland->height);
    wl_surface_commit(ctx_wayland->surface);

    ctx_wayland->frame_callback = wl_surface_frame(ctx_wayland->surface);
    wl_callback_add_listener(
        ctx_wayland->frame_callback, &war_frame_listener, ctx_wayland);

    //-------------------------------------------------------------------------
    // INPUT (insert input handling code here)
    //-------------------------------------------------------------------------
    printf("test");

    //-------------------------------------------------------------------------
    // MAIN LOOP
    //-------------------------------------------------------------------------
    while (ctx_wayland->running) {
        wl_display_dispatch(ctx_wayland->display);
    }

    //-------------------------------------------------------------------------
    // CLEANUP
    //-------------------------------------------------------------------------
    vkDeviceWaitIdle(device);
    vkDestroyCommandPool(device, cmd_pool, NULL);
    vkDestroyFramebuffer(device, framebuffer, NULL);
    vkDestroyImageView(device, image_view, NULL);
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);
    vkDestroyRenderPass(device, render_pass, NULL);
    vkFreeMemory(device, memory, NULL);
    vkDestroyImage(device, image, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(instance, NULL);
    wl_buffer_destroy(ctx_wayland->buffer);
    wl_display_disconnect(ctx_wayland->display);
}
