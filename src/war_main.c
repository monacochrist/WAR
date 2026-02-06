//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/war_main.c
//-----------------------------------------------------------------------------

#include "h/war_main.h"
#include "../build/h/war_build_keymap_functions.h"
#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_functions.h"
#include "h/war_keymap_functions.h"
#include "h/war_new_vulkan.h"
#include "h/war_nsgt.h"
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

int main() {
    CALL_KING_TERRY("war");
    //-------------------------------------------------------------------------
    // LUA
    //-------------------------------------------------------------------------
    war_lua_context ctx_lua;
    ctx_lua.L = luaL_newstate();
    if (!ctx_lua.L) {
        // call_king_terry("failed to create Lua state");
        return -1;
    }
    luaL_openlibs(ctx_lua.L);
    war_load_lua_config(&ctx_lua, "src/lua/war_main.lua");
    //-------------------------------------------------------------------------
    // PC CONTROL
    //-------------------------------------------------------------------------
    war_producer_consumer pc_control;
    pc_control.size = atomic_load(&ctx_lua.PC_CONTROL_BUFFER_SIZE);
    pc_control.to_a = mmap(NULL,
                           pc_control.size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1,
                           0);
    assert(pc_control.to_a);
    memset(pc_control.to_a, 0, pc_control.size);
    mlock(pc_control.to_a, pc_control.size);
    pc_control.to_wr = mmap(NULL,
                            pc_control.size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
    assert(pc_control.to_wr);
    memset(pc_control.to_wr, 0, pc_control.size);
    mlock(pc_control.to_wr, pc_control.size);
    pc_control.i_to_a = 0;
    pc_control.i_to_wr = 0;
    pc_control.i_from_a = 0;
    pc_control.i_from_wr = 0;
    //-------------------------------------------------------------------------
    // PC PLAY
    //-------------------------------------------------------------------------
    war_producer_consumer pc_play;
    pc_play.size = atomic_load(&ctx_lua.PC_PLAY_BUFFER_SIZE);
    pc_play.to_a = mmap(NULL,
                        pc_play.size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    assert(pc_play.to_a);
    memset(pc_play.to_a, 0, pc_play.size);
    mlock(pc_play.to_a, pc_play.size);
    pc_play.to_wr = mmap(NULL,
                         pc_play.size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS,
                         -1,
                         0);
    assert(pc_play.to_wr);
    memset(pc_play.to_wr, 0, pc_play.size);
    mlock(pc_play.to_wr, pc_play.size);
    pc_play.i_to_a = 0;
    pc_play.i_to_wr = 0;
    pc_play.i_from_a = 0;
    pc_play.i_from_wr = 0;
    //-------------------------------------------------------------------------
    // PC CAPTURE
    //-------------------------------------------------------------------------
    war_producer_consumer pc_capture;
    pc_capture.size = atomic_load(&ctx_lua.PC_CAPTURE_BUFFER_SIZE);
    pc_capture.to_a = mmap(NULL,
                           pc_capture.size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1,
                           0);
    assert(pc_capture.to_a);
    memset(pc_capture.to_a, 0, pc_capture.size);
    mlock(pc_capture.to_a, pc_capture.size);
    pc_capture.to_wr = mmap(NULL,
                            pc_capture.size,
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS,
                            -1,
                            0);
    assert(pc_capture.to_wr);
    memset(pc_capture.to_wr, 0, pc_capture.size);
    mlock(pc_capture.to_wr, pc_capture.size);
    pc_capture.i_to_a = 0;
    pc_capture.i_to_wr = 0;
    pc_capture.i_from_a = 0;
    pc_capture.i_from_wr = 0;
    //-------------------------------------------------------------------------
    // ATOMICS
    //-------------------------------------------------------------------------
    war_atomics atomics = {
        .play_clock = 0,
        .play_frames = 0,
        .capture_frames = 0,
        .capture_monitor = 0,
        .capture_threshold = 0.01f,
        .play_writer_rate = 0,
        .play_reader_rate = 0,
        .capture_writer_rate = 0,
        .capture_reader_rate = 0,
        .play_gain = 1.0f,
        .capture_gain = 1.0f,
        .capture = 0,
        .play = 0,
        .map = 0,
        .map_note = -1,
        .loop = 0,
        .start_war = 0,
        .resample = 0,
        .repeat_section = 0,
        .repeat_start_frames = 0,
        .repeat_end_frames = 0,
        .note_next_id = 1,
        .cache_next_id = 1,
        .cache_next_timestamp = 1,
        .layer = 0,
    };
    //-------------------------------------------------------------------------
    // THREADS
    //-------------------------------------------------------------------------
    struct rlimit r_limit;
    if (getrlimit(RLIMIT_MEMLOCK, &r_limit) == -1) {
        call_king_terry("failed to get r_limit");
    }
    if (r_limit.rlim_max != RLIM_INFINITY) {
        call_king_terry("r_limit max: %ul", r_limit.rlim_max);
    } else {
        call_king_terry("r_limit max: %s", "unlimited");
    }
    war_pool pool_wr;
    war_pool pool_a;
    pthread_t war_window_render_thread;
    pthread_create(
        &war_window_render_thread,
        NULL,
        war_window_render,
        (void* [6]){
            &pc_control, &atomics, &pool_wr, &ctx_lua, &pc_play, &pc_capture});
    pthread_t war_audio_thread;
    pthread_create(
        &war_audio_thread,
        NULL,
        war_audio,
        (void* [6]){
            &pc_control, &atomics, &pool_a, &ctx_lua, &pc_play, &pc_capture});
    pthread_join(war_window_render_thread, NULL);
    pthread_join(war_audio_thread, NULL);
    END("war");
    return 0;
}
//-----------------------------------------------------------------------------
// THREAD WINDOW RENDER
//-----------------------------------------------------------------------------
void* war_window_render(void* args) {
    header("war_window_render");
    void** args_ptrs = (void**)args;
    war_producer_consumer* pc_control = args_ptrs[0];
    war_atomics* atomics = args_ptrs[1];
    while (!atomic_load(&atomics->start_war)) { usleep(1000); }
    war_pool* pool_wr = args_ptrs[2];
    war_lua_context* ctx_lua = args_ptrs[3];
    war_producer_consumer* pc_play = args_ptrs[4];
    war_producer_consumer* pc_capture = args_ptrs[5];
    // call_king_terry("ctx_lua WR_STATES: %i",
    // atomic_load(&ctx_lua->WR_STATES));
    pool_wr->pool_alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    pool_wr->pool_size =
        war_get_pool_wr_size(pool_wr, ctx_lua, "src/lua/war_main.lua");
    pool_wr->pool_size =
        ((pool_wr->pool_size * 1) + ((pool_wr->pool_alignment) - 1)) &
        ~((pool_wr->pool_alignment) - 1);
    int pool_result = posix_memalign(
        &pool_wr->pool, pool_wr->pool_alignment, pool_wr->pool_size);
    assert(pool_result == 0 && pool_wr->pool);
    memset(pool_wr->pool, 0, pool_wr->pool_size);
    pool_wr->pool_ptr = (uint8_t*)pool_wr->pool;
    //-------------------------------------------------------------------------
    // NEW VULKAN CONTEXT
    //-------------------------------------------------------------------------
    war_new_vulkan_context* ctx_new_vulkan =
        war_pool_alloc(pool_wr, sizeof(war_new_vulkan_context));
    war_new_vulkan_init(ctx_new_vulkan, pool_wr, ctx_lua);
    assert(ctx_new_vulkan->dmabuf_fd >= 0);
    //-------------------------------------------------------------------------
    // MISC CONTEXT
    //-------------------------------------------------------------------------
    war_misc_context* ctx_misc =
        war_pool_alloc(pool_wr, sizeof(war_misc_context));
    ctx_misc->stride = ctx_new_vulkan->physical_width * 4;
    ctx_misc->bpm = atomic_load(&ctx_lua->A_BPM);
    ctx_misc->fps = atomic_load(&ctx_lua->WR_FPS);
    ctx_misc->bpm_seconds_per_cell =
        atomic_load(&ctx_lua->BPM_SECONDS_PER_CELL);
    ctx_misc->subdivision_seconds_per_cell =
        atomic_load(&ctx_lua->SUBDIVISION_SECONDS_PER_CELL);
    ctx_misc->seconds_per_beat = 60.0 / ctx_misc->bpm_seconds_per_cell;
    ctx_misc->seconds_per_cell =
        ctx_misc->seconds_per_beat / ctx_misc->subdivision_seconds_per_cell;
    ctx_misc->epsilon = 1e-6;
    ctx_misc->scale_factor = 1.483333; // 1.483333
    ctx_misc->logical_width = (uint32_t)floor(ctx_new_vulkan->physical_width /
                                              ctx_misc->scale_factor);
    ctx_misc->logical_height = (uint32_t)floor(ctx_new_vulkan->physical_height /
                                               ctx_misc->scale_factor);
    ctx_misc->frame_duration_us = 1e6 / ctx_misc->fps;
    //-------------------------------------------------------------------------
    // COMMAND CONTEXT
    //-------------------------------------------------------------------------
    war_command_context* ctx_command =
        war_pool_alloc(pool_wr, sizeof(war_command_context));
    ctx_command->capacity = atomic_load(&ctx_lua->CONFIG_PATH_MAX);
    ctx_command->input =
        war_pool_alloc(pool_wr, sizeof(int) * ctx_command->capacity);
    ctx_command->text =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_command->capacity);
    ctx_command->prompt_text =
        war_pool_alloc(pool_wr, sizeof(char) * ctx_command->capacity);
    ctx_command->prompt_text_size = 0;
    ctx_command->input_write_index = 0;
    ctx_command->text_write_index = 0;
    ctx_command->input_read_index = 0;
    ctx_command->text_size = 0;
    //-------------------------------------------------------------------------
    // FSM CONTEXT + REPEATS + TIMEOUTS
    //-------------------------------------------------------------------------
    // Watch for overflow
    war_fsm_context* ctx_fsm = war_pool_alloc(pool_wr, sizeof(war_fsm_context));
    // Set counts
    ctx_fsm->keysym_count = atomic_load(&ctx_lua->WR_KEYSYM_COUNT);
    ctx_fsm->mod_count = atomic_load(&ctx_lua->WR_MOD_COUNT);
    ctx_fsm->state_count = atomic_load(&ctx_lua->WR_STATES);
    ctx_fsm->mode_count = atomic_load(&ctx_lua->WR_MODE_COUNT);
    ctx_fsm->name_limit = atomic_load(&ctx_lua->WR_FN_NAME_LIMIT);
    // paths
    ctx_fsm->current_file_path = war_pool_alloc(pool_wr, ctx_fsm->name_limit);
    ctx_fsm->current_file_path_size = 0;
    ctx_fsm->cwd = war_pool_alloc(pool_wr, ctx_fsm->name_limit);
    getcwd(ctx_fsm->cwd, ctx_fsm->name_limit);
    ctx_fsm->cwd_size = strlen(ctx_fsm->cwd);
    ctx_fsm->ext = war_pool_alloc(pool_wr, ctx_fsm->name_limit);
    ctx_fsm->ext_size = 0;
    // FSM data allocations (per state per mode)
    ctx_fsm->next_state =
        war_pool_alloc(pool_wr,
                       sizeof(uint64_t) * ctx_fsm->state_count *
                           ctx_fsm->keysym_count * ctx_fsm->mod_count);
    ctx_fsm->is_terminal = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->is_prefix = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->handle_release = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->handle_repeat = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->handle_timeout = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->function =
        war_pool_alloc(pool_wr,
                       sizeof(war_function_union) * ctx_fsm->state_count *
                           ctx_fsm->mode_count);
    ctx_fsm->function_type = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->state_count * ctx_fsm->mode_count);
    ctx_fsm->name =
        war_pool_alloc(pool_wr,
                       sizeof(char) * ctx_fsm->state_count *
                           ctx_fsm->mode_count * ctx_fsm->name_limit);
    // Runtime state allocations
    ctx_fsm->key_down = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_fsm->keysym_count * ctx_fsm->mod_count);
    ctx_fsm->key_last_event_us = war_pool_alloc(
        pool_wr, sizeof(uint64_t) * ctx_fsm->keysym_count * ctx_fsm->mod_count);
    // repeats
    ctx_fsm->repeat_delay_us = 150000;
    ctx_fsm->repeat_rate_us = 40000;
    ctx_fsm->repeat_keysym = 0;
    ctx_fsm->repeat_mod = 0;
    ctx_fsm->repeating = 0;
    ctx_fsm->goto_cmd_repeat_done = 0;
    // timeouts
    ctx_fsm->timeout_duration_us = 500000;
    ctx_fsm->timeout_state_index = 0;
    ctx_fsm->timeout_start_us = 0;
    ctx_fsm->timeout = 0;
    ctx_fsm->goto_cmd_timeout_done = 0;
    // Initialize runtime values
    ctx_fsm->state_last_event_us = 0;
    ctx_fsm->current_state = 0;
    // modes
    ctx_fsm->current_mode = ctx_fsm->MODE_ROLL;
    ctx_fsm->previous_mode = 0;
    // Initialize modifiers
    ctx_fsm->mod_shift = 0;
    ctx_fsm->mod_ctrl = 0;
    ctx_fsm->mod_alt = 0;
    ctx_fsm->mod_logo = 0;
    ctx_fsm->mod_caps = 0;
    ctx_fsm->mod_num = 0;
    ctx_fsm->mod_fn = 0;
    // XKB initialization (separate from pool allocation)
    ctx_fsm->xkb_context = NULL;
    ctx_fsm->xkb_state = NULL;
    // default modes
    ctx_fsm->MODE_ROLL = 0;
    ctx_fsm->MODE_VIEWS = 1;
    ctx_fsm->MODE_CAPTURE = 2;
    ctx_fsm->MODE_MIDI = 3;
    ctx_fsm->MODE_COMMAND = 4;
    ctx_fsm->MODE_WAV = 5;
    //-----------------------------------------------------------------------------
    // WAYLAND
    //-----------------------------------------------------------------------------
    uint32_t zwp_linux_dmabuf_v1_id = 0;
    uint32_t zwp_linux_buffer_params_v1_id = 0;
    uint32_t zwp_linux_dmabuf_feedback_v1_id = 0;
    uint32_t wl_display_id = 1;
    uint32_t wl_registry_id = 2;
    uint32_t wl_buffer_id = 0;
    uint32_t wl_callback_id = 0;
    uint32_t wl_compositor_id = 0;
    uint32_t wl_region_id = 0;
    uint32_t wp_viewporter_id = 0;
    uint32_t wl_surface_id = 0;
    uint32_t wp_viewport_id = 0;
    uint32_t xdg_wm_base_id = 0;
    uint32_t xdg_surface_id = 0;
    uint32_t xdg_toplevel_id = 0;
    uint32_t wl_output_id = 0;
    uint32_t wl_seat_id = 0;
    uint32_t wl_keyboard_id = 0;
    uint32_t wl_pointer_id = 0;
    uint32_t wl_touch_id = 0;
    uint32_t wp_linux_drm_syncobj_manager_v1_id = 0;
    uint32_t wp_linux_drm_syncobj_timeline_v1_id = 0;
    uint32_t wp_linux_drm_syncobj_surface_v1_id = 0;
    uint32_t zwp_idle_inhibit_manager_v1_id = 0;
    uint32_t zxdg_decoration_manager_v1_id = 0;
    uint32_t zwp_relative_pointer_manager_v1_id = 0;
    uint32_t zwp_pointer_constraints_v1_id = 0;
    uint32_t zwlr_output_manager_v1_id = 0;
    uint32_t zwlr_data_control_manager_v1_id = 0;
    uint32_t zwp_virtual_keyboard_manager_v1_id = 0;
    uint32_t wp_fractional_scale_manager_v1_id = 0;
    uint32_t zwp_pointer_gestures_v1_id = 0;
    uint32_t xdg_activation_v1_id = 0;
    uint32_t wp_presentation_id = 0;
    uint32_t zwlr_layer_shell_v1_id = 0;
    uint32_t ext_foreign_toplevel_list_v1_id = 0;
    uint32_t wp_content_type_manager_v1_id = 0;
    uint32_t zxdg_toplevel_decoration_v1_id = 0;
    // uint32_t wl_keyboard_id = 0;
    // uint32_t wl_pointer_id = 0;
    // uint32_t zwp_linux_explicit_synchronization_v1_id = 0;
    int fd = war_wayland_make_fd();
    assert(fd >= 0);
    uint32_t new_id;
    uint8_t get_registry[12];
    war_write_le32(get_registry, wl_display_id);
    war_write_le16(get_registry + 4, 1);
    war_write_le16(get_registry + 6, 12);
    war_write_le32(get_registry + 8, wl_registry_id);
    ssize_t written = write(fd, get_registry, 12);
    // call_king_terry("written size: %lu", written);
    // dump_bytes("written", get_registry, 12);
    assert(written == 12);
    new_id = wl_registry_id + 1;
    uint32_t msg_buffer_alloc_size =
        atomic_load(&ctx_lua->WR_WAYLAND_MSG_BUFFER_SIZE);
    uint8_t* msg_buffer =
        war_pool_alloc(pool_wr, sizeof(uint8_t) * msg_buffer_alloc_size);
    size_t msg_buffer_size = 0;
    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };
    uint32_t max_wayland_objects =
        atomic_load(&ctx_lua->WR_WAYLAND_MAX_OBJECTS);
    uint32_t max_wayland_opcodes =
        atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES);
    void** obj_op = war_pool_alloc(
        pool_wr, sizeof(void*) * max_wayland_objects * max_wayland_opcodes);
    obj_op[wl_display_id * max_wayland_opcodes + 0] = &&wl_display_error;
    obj_op[wl_display_id * max_wayland_opcodes + 1] = &&wl_display_delete_id;
    obj_op[wl_registry_id * max_wayland_opcodes + 0] =
        &&war_label_wl_registry_global;
    obj_op[wl_registry_id * max_wayland_opcodes + 1] =
        &&war_label_wl_registry_global_remove;
    //-------------------------------------------------------------------------
    // PC CONTROL
    //-------------------------------------------------------------------------
    uint32_t header;
    uint32_t size;
    uint32_t control_cmd_count = atomic_load(&ctx_lua->CMD_COUNT);
    uint8_t* control_payload =
        war_pool_alloc(pool_wr, sizeof(uint8_t) * pc_control->size);
    uint8_t* tmp_control_payload =
        war_pool_alloc(pool_wr, sizeof(uint8_t) * pc_control->size);
    void** pc_control_cmd =
        war_pool_alloc(pool_wr, sizeof(void*) * control_cmd_count);
    pc_control_cmd[0] = &&war_label_end_wr;
    //-------------------------------------------------------------------------
    // CAPTURE CONTEXT
    //-------------------------------------------------------------------------
    war_capture_context* ctx_capture =
        war_pool_alloc(pool_wr, sizeof(war_capture_context));
    ctx_capture->name_limit = atomic_load(&ctx_lua->CONFIG_PATH_MAX);
    ctx_capture->fps = atomic_load(&ctx_lua->WR_CAPTURE_CALLBACK_FPS);
    // rate
    ctx_capture->last_frame_time = war_get_monotonic_time_us();
    ctx_capture->last_read_time = 0;
    ctx_capture->read_count = 0;
    // misc
    ctx_capture->capture_wait = 1;
    ctx_capture->capture_delay = 0;
    ctx_capture->state = CAPTURE_WAITING;
    ctx_capture->threshold = atomic_load(&ctx_lua->WR_CAPTURE_THRESHOLD);
    ctx_capture->monitor = 0;
    ctx_capture->prompt = 1;
    ctx_capture->prompt_fname_text =
        war_pool_alloc(pool_wr, ctx_capture->name_limit);
    ctx_capture->prompt_fname_text = "filename";
    ctx_capture->prompt_fname_text_size =
        strlen(ctx_capture->prompt_fname_text);
    ctx_capture->prompt_note_text =
        war_pool_alloc(pool_wr, ctx_capture->name_limit);
    ctx_capture->prompt_note_text = "note";
    ctx_capture->prompt_note_text_size = strlen(ctx_capture->prompt_note_text);
    ctx_capture->prompt_layer_text =
        war_pool_alloc(pool_wr, ctx_capture->name_limit);
    ctx_capture->prompt_layer_text = "layer";
    ctx_capture->prompt_layer_text_size =
        strlen(ctx_capture->prompt_layer_text);
    ctx_capture->fname = war_pool_alloc(pool_wr, ctx_capture->name_limit);
    ctx_capture->fname_size = 0;
    ctx_capture->note = 0;
    ctx_capture->layer = 0;
    //-------------------------------------------------------------------------
    // PLAY CONTEXT
    //-------------------------------------------------------------------------
    war_play_context* ctx_play =
        war_pool_alloc(pool_wr, sizeof(war_capture_context));
    ctx_play->fps = atomic_load(&ctx_lua->WR_PLAY_CALLBACK_FPS);
    // rate
    ctx_play->last_frame_time = war_get_monotonic_time_us();
    ctx_play->last_write_time = 0;
    ctx_play->write_count = 0;
    // misc
    ctx_play->play = 0;
    ctx_play->octave = 4;
    //-------------------------------------------------------------------------
    // HUD CONTEXT
    //-------------------------------------------------------------------------
    war_hud_context* ctx_hud = war_pool_alloc(pool_wr, sizeof(war_hud_context));
    ctx_hud->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_hud *
                                      ctx_new_vulkan->buffer_max];
    ctx_hud->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_hud *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud->idx_status_bottom = 0;
    ctx_hud->idx_status_middle = 1;
    ctx_hud->idx_status_top = 2;
    ctx_hud->idx_line_numbers = 3;
    ctx_hud->idx_piano = 4;
    ctx_hud->idx_explore = 5;
    ctx_hud->buffer_count = atomic_load(&ctx_lua->HUD_COUNT);
    ctx_hud->capacity =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_hud->buffer_count);
    ctx_hud->stage = war_pool_alloc(
        pool_wr, sizeof(war_new_vulkan_hud_instance*) * ctx_hud->buffer_count);
    ctx_hud->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_hud *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud->count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_hud *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud->capacity[ctx_hud->idx_status_bottom] =
        atomic_load(&ctx_lua->HUD_STATUS_BOTTOM_INSTANCE_MAX);
    ctx_hud->capacity[ctx_hud->idx_status_middle] =
        atomic_load(&ctx_lua->HUD_STATUS_MIDDLE_INSTANCE_MAX);
    ctx_hud->capacity[ctx_hud->idx_status_top] =
        atomic_load(&ctx_lua->HUD_STATUS_TOP_INSTANCE_MAX);
    ctx_hud->capacity[ctx_hud->idx_line_numbers] =
        atomic_load(&ctx_lua->HUD_LINE_NUMBERS_INSTANCE_MAX);
    ctx_hud->capacity[ctx_hud->idx_piano] =
        atomic_load(&ctx_lua->HUD_PIANO_INSTANCE_MAX);
    ctx_hud->capacity[ctx_hud->idx_explore] =
        atomic_load(&ctx_lua->HUD_EXPLORE_INSTANCE_MAX);
    ctx_hud->x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud->buffer_count);
    ctx_hud->y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud->buffer_count);
    ctx_hud->width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud->buffer_count);
    ctx_hud->height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud->buffer_count);
    for (uint32_t i = 0; i < ctx_hud->buffer_count; i++) {
        ctx_hud->x_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud->capacity[i]);
        ctx_hud->y_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud->capacity[i]);
        ctx_hud->width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud->capacity[i]);
        ctx_hud->height_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud->capacity[i]);
    }
    uint32_t tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_hud->buffer_count; i++) {
        ctx_hud->stage[i] =
            &((war_new_vulkan_hud_instance*)ctx_new_vulkan
                  ->map[ctx_new_vulkan->idx_hud_stage])[tmp_first_instance];
        tmp_first_instance += ctx_hud->capacity[i];
        if (i + 1 < ctx_hud->buffer_count) {
            ctx_hud->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_hud->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud];
    ctx_hud->viewport =
        ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud];
    ctx_hud->push_constant =
        (war_new_vulkan_hud_push_constant*)ctx_new_vulkan
            ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud];
    for (uint32_t i = 0; i < ctx_hud->buffer_count; i++) {
        ctx_hud->push_constant[i] = (war_new_vulkan_hud_push_constant){
            .screen_size = {ctx_new_vulkan->physical_width,
                            ctx_new_vulkan->physical_height},
            .cell_size = {ctx_new_vulkan->cell_width,
                          ctx_new_vulkan->cell_height},
            .panning = {0.0f, 0.0f},
            .zoom = 1.0f,
            .cell_offset = {0.0f, 0.0f},
        };
        ctx_hud->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_hud->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // hud status bottom
    ctx_hud->x_seconds[ctx_hud->idx_status_bottom][0] = 0;
    ctx_hud->y_cells[ctx_hud->idx_status_bottom][0] = 0;
    ctx_hud->width_seconds[ctx_hud->idx_status_bottom][0] =
        (ctx_new_vulkan->physical_width / ctx_new_vulkan->cell_width) *
        ctx_misc->seconds_per_cell;
    ctx_hud->height_cells[ctx_hud->idx_status_bottom][0] = 1.0;
    ctx_hud->stage[ctx_hud->idx_status_bottom][0] =
        (war_new_vulkan_hud_instance){
            .pos = {0.0f,
                    0.0f,
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_hud]},
            .size = {ctx_hud->width_seconds[ctx_hud->idx_status_bottom][0] /
                         ctx_misc->seconds_per_cell,
                     1.0f},
            .color = {0.87f, 0.0f, 0.0f, 1.0f},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_hud->count[ctx_hud->idx_status_bottom] = 1;
    ctx_hud->dirty[ctx_hud->idx_status_bottom] = 1;
    // hud status middle
    ctx_hud->x_seconds[ctx_hud->idx_status_middle][0] = 0;
    ctx_hud->y_cells[ctx_hud->idx_status_middle][0] = 1;
    ctx_hud->width_seconds[ctx_hud->idx_status_middle][0] =
        (ctx_new_vulkan->physical_width / ctx_new_vulkan->cell_width) *
        ctx_misc->seconds_per_cell;
    ctx_hud->height_cells[ctx_hud->idx_status_middle][0] = 1.0;
    ctx_hud->stage[ctx_hud->idx_status_middle][0] =
        (war_new_vulkan_hud_instance){
            .pos = {0.0f,
                    1.0f,
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_hud]},
            .size = {ctx_hud->width_seconds[ctx_hud->idx_status_middle][0] /
                         ctx_misc->seconds_per_cell,
                     1.0f},
            .color = {0.1569, 0.1569, 0.1569, 1.0f},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_hud->count[ctx_hud->idx_status_middle] = 1;
    ctx_hud->dirty[ctx_hud->idx_status_middle] = 1;
    // hud status top
    ctx_hud->x_seconds[ctx_hud->idx_status_top][0] = 0;
    ctx_hud->y_cells[ctx_hud->idx_status_top][0] = 2;
    ctx_hud->width_seconds[ctx_hud->idx_status_top][0] =
        (ctx_new_vulkan->physical_width / ctx_new_vulkan->cell_width) *
        ctx_misc->seconds_per_cell;
    ctx_hud->height_cells[ctx_hud->idx_status_top][0] = 1.0;
    ctx_hud->stage[ctx_hud->idx_status_top][0] = (war_new_vulkan_hud_instance){
        .pos = {0.0f, 2.0f, ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_hud]},
        .size = {ctx_hud->width_seconds[ctx_hud->idx_status_top][0] /
                     ctx_misc->seconds_per_cell,
                 1.0f},
        .color = {0.3137, 0.2863, 0.2706, 1.0},
        .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .flags = 0,
    };
    ctx_hud->count[ctx_hud->idx_status_top] = 1;
    ctx_hud->dirty[ctx_hud->idx_status_top] = 1;
    //-------------------------------------------------------------------------
    // HUD LINE CONTEXT
    //-------------------------------------------------------------------------
    war_hud_line_context* ctx_hud_line =
        war_pool_alloc(pool_wr, sizeof(war_hud_line_context));
    ctx_hud_line->buffer_count = atomic_load(&ctx_lua->HUD_LINE_COUNT);
    ctx_hud_line->capacity =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_hud_line->buffer_count);
    ctx_hud_line->stage =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_line->buffer_count);
    ctx_hud_line->idx_piano = 0;
    ctx_hud_line->capacity[ctx_hud_line->idx_piano] =
        atomic_load(&ctx_lua->HUD_LINE_PIANO_INSTANCE_MAX);
    ctx_hud_line->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_hud_line *
                                      ctx_new_vulkan->buffer_max];
    ctx_hud_line->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_hud_line *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_line->stage = war_pool_alloc(pool_wr,
                                         sizeof(war_new_vulkan_line_instance*) *
                                             ctx_hud_line->buffer_count);
    ctx_hud_line->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_hud_line *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_line->count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_hud_line *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_line->x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_line->buffer_count);
    ctx_hud_line->y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_line->buffer_count);
    ctx_hud_line->width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_line->buffer_count);
    ctx_hud_line->height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_line->buffer_count);
    ctx_hud_line->line_width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_line->buffer_count);
    for (uint32_t i = 0; i < ctx_hud_line->buffer_count; i++) {
        ctx_hud_line->x_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_line->capacity[i]);
        ctx_hud_line->y_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_line->capacity[i]);
        ctx_hud_line->width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_line->capacity[i]);
        ctx_hud_line->height_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_line->capacity[i]);
        ctx_hud_line->line_width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_line->capacity[i]);
    }
    tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_hud_line->buffer_count; i++) {
        ctx_hud_line->stage[i] = &(
            (war_new_vulkan_hud_line_instance*)ctx_new_vulkan
                ->map[ctx_new_vulkan->idx_hud_line_stage])[tmp_first_instance];
        tmp_first_instance += ctx_hud_line->capacity[i];
        if (i + 1 < ctx_hud_line->buffer_count) {
            ctx_hud_line->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_hud_line->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud_line];
    ctx_hud_line->viewport =
        ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud_line];
    ctx_hud_line->push_constant =
        ctx_new_vulkan
            ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud_line];
    for (uint32_t i = 0; i < ctx_hud_line->buffer_count; i++) {
        ctx_hud_line->push_constant[i] =
            (war_new_vulkan_hud_line_push_constant){
                .screen_size = {ctx_new_vulkan->physical_width,
                                ctx_new_vulkan->physical_height},
                .cell_size = {ctx_new_vulkan->cell_width,
                              ctx_new_vulkan->cell_height},
                .panning = {0.0f, 0.0f},
                .zoom = 1.0f,
                .cell_offset = {3.0f, 3.0f},
            };
        ctx_hud_line->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_hud_line->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // cell grid
    ctx_hud_line->x_seconds[ctx_hud_line->idx_piano][0] = 0;
    ctx_hud_line->y_cells[ctx_hud_line->idx_piano][0] = 5;
    ctx_hud_line->width_seconds[ctx_hud_line->idx_piano][0] =
        (ctx_new_vulkan->physical_width / ctx_new_vulkan->cell_width) *
        ctx_misc->seconds_per_cell;
    ctx_hud_line->height_cells[ctx_hud_line->idx_piano][0] = 0;
    ctx_hud_line->line_width_seconds[ctx_hud_line->idx_piano][0] =
        0.025 * ctx_misc->seconds_per_cell;
    ctx_hud_line->stage[ctx_hud_line->idx_piano][0] =
        (war_new_vulkan_hud_line_instance){
            .pos = {ctx_hud_line->x_seconds[ctx_hud_line->idx_piano][0] /
                        ctx_misc->seconds_per_cell,
                    ctx_hud_line->y_cells[ctx_hud_line->idx_piano][0],
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_line]},
            .size = {ctx_hud_line->width_seconds[ctx_hud_line->idx_piano][0] /
                         ctx_misc->seconds_per_cell,
                     ctx_hud_line->height_cells[ctx_hud_line->idx_piano][0]},
            .width =
                ctx_hud_line->line_width_seconds[ctx_hud_line->idx_piano][0] /
                ctx_misc->seconds_per_cell,
            .color = {0.3137, 0.2863, 0.2706, 1.0},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_hud_line->count[ctx_hud_line->idx_piano] = 1;
    ctx_hud_line->dirty[ctx_hud_line->idx_piano] = 1;
    ctx_hud_line->draw[ctx_hud_line->idx_piano] = 0;
    //-------------------------------------------------------------------------
    // HUD TEXT
    //-------------------------------------------------------------------------
    war_hud_text_context* ctx_hud_text =
        war_pool_alloc(pool_wr, sizeof(war_hud_text_context));
    ctx_hud_text->buffer_count = atomic_load(&ctx_lua->HUD_TEXT_COUNT);
    ctx_hud_text->capacity =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_hud_text->buffer_count);
    ctx_hud_text->stage =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_text->buffer_count);
    // indices / capacities
    ctx_hud_text->idx_status_bottom = 0;
    ctx_hud_text->capacity[ctx_hud_text->idx_status_bottom] =
        atomic_load(&ctx_lua->HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX);
    ctx_hud_text->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_hud_text *
                                      ctx_new_vulkan->buffer_max];
    ctx_hud_text->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_hud_text *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_text->stage = war_pool_alloc(
        pool_wr,
        sizeof(war_new_vulkan_hud_text_instance*) * ctx_hud_text->buffer_count);
    ctx_hud_text->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_hud_text *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_text->count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_hud_text *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_text->x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_text->buffer_count);
    ctx_hud_text->y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_text->buffer_count);
    ctx_hud_text->width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_text->buffer_count);
    ctx_hud_text->height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_text->buffer_count);
    for (uint32_t i = 0; i < ctx_hud_text->buffer_count; i++) {
        ctx_hud_text->x_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_text->capacity[i]);
        ctx_hud_text->y_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_text->capacity[i]);
        ctx_hud_text->width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_text->capacity[i]);
        ctx_hud_text->height_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_hud_text->capacity[i]);
    }
    tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_hud_text->buffer_count; i++) {
        ctx_hud_text->stage[i] = &(
            (war_new_vulkan_hud_text_instance*)ctx_new_vulkan
                ->map[ctx_new_vulkan->idx_hud_text_stage])[tmp_first_instance];
        tmp_first_instance += ctx_hud_text->capacity[i];
        if (i + 1 < ctx_hud_text->buffer_count) {
            ctx_hud_text->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_hud_text->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud_text];
    ctx_hud_text->viewport =
        ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud_text];
    ctx_hud_text->push_constant =
        ctx_new_vulkan
            ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud_text];
    for (uint32_t i = 0; i < ctx_hud_text->buffer_count; i++) {
        ctx_hud_text->push_constant[i] =
            (war_new_vulkan_hud_text_push_constant){
                .screen_size = {ctx_new_vulkan->physical_width,
                                ctx_new_vulkan->physical_height},
                .cell_size = {ctx_new_vulkan->cell_width,
                              ctx_new_vulkan->cell_height},
                .panning = {0.0f, 0.0f},
                .zoom = 1.0f,
                .cell_offset = {3.0f, 3.0f},
            };
        ctx_hud_text->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_hud_text->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // status bottom
    ctx_hud_text->x_seconds[ctx_hud_text->idx_status_bottom][0] =
        3 * ctx_misc->seconds_per_cell;
    ctx_hud_text->y_cells[ctx_hud_text->idx_status_bottom][0] = 6;
    ctx_hud_text->width_seconds[ctx_hud_text->idx_status_bottom][0] =
        1 * ctx_misc->seconds_per_cell;
    ctx_hud_text->height_cells[ctx_hud_text->idx_status_bottom][0] = 1;
    ctx_hud_text->stage[ctx_hud_text->idx_status_bottom]
                       [0] = (war_new_vulkan_hud_text_instance){
        .pos = {ctx_hud_text->x_seconds[ctx_hud_text->idx_status_bottom][0] /
                    ctx_misc->seconds_per_cell,
                ctx_hud_text->y_cells[ctx_hud_text->idx_status_bottom][0],
                ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_line]},
        .size =
            {ctx_hud_text->width_seconds[ctx_hud_text->idx_status_bottom][0] /
                 ctx_misc->seconds_per_cell,
             ctx_hud_text->height_cells[ctx_hud_text->idx_status_bottom][0]},
        .uv = {ctx_new_vulkan->glyph_info['M'].uv_x0,
               ctx_new_vulkan->glyph_info['M'].uv_y0,
               ctx_new_vulkan->glyph_info['M'].uv_x1,
               ctx_new_vulkan->glyph_info['M'].uv_y1},
        .glyph_scale = {ctx_new_vulkan->glyph_info['M'].norm_width,
                        ctx_new_vulkan->glyph_info['M'].norm_height},
        .baseline = ctx_new_vulkan->glyph_info['M'].norm_baseline,
        .ascent = ctx_new_vulkan->glyph_info['M'].norm_ascent,
        .descent = ctx_new_vulkan->glyph_info['M'].norm_descent,
        .color = {0.9216, 0.8549, 0.6902, 1.0},
        .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
        .flags = 0,
    };
    ctx_hud_text->count[ctx_hud_text->idx_status_bottom] = 1;
    ctx_hud_text->dirty[ctx_hud_text->idx_status_bottom] = 1;
    ctx_hud_text->draw[ctx_hud_text->idx_status_bottom] = 0;
    //-------------------------------------------------------------------------
    // HUD CURSOR
    //-------------------------------------------------------------------------
    war_hud_cursor_context* ctx_hud_cursor =
        war_pool_alloc(pool_wr, sizeof(war_hud_cursor_context));
    ctx_hud_cursor->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_hud_cursor *
                                      ctx_new_vulkan->buffer_max];
    ctx_hud_cursor->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_hud_cursor *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_cursor->idx_cursor_default = 0;
    ctx_hud_cursor->buffer_count = atomic_load(&ctx_lua->HUD_CURSOR_COUNT);
    ctx_hud_cursor->capacity = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->stage =
        war_pool_alloc(pool_wr,
                       sizeof(war_new_vulkan_hud_cursor_instance*) *
                           ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_hud_cursor *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_cursor->count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_hud_cursor *
                                     ctx_new_vulkan->buffer_max];
    ctx_hud_cursor->capacity[ctx_hud_cursor->idx_cursor_default] =
        atomic_load(&ctx_lua->HUD_CURSOR_DEFAULT_INSTANCE_MAX);
    ctx_hud_cursor->x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->visual_x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->visual_y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->visual_width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    ctx_hud_cursor->visual_height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_hud_cursor->buffer_count);
    for (uint32_t i = 0; i < ctx_hud_cursor->buffer_count; i++) {
        ctx_hud_cursor->x_seconds[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
        ctx_hud_cursor->y_cells[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
        ctx_hud_cursor->width_seconds[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
        ctx_hud_cursor->height_cells[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
        ctx_hud_cursor->visual_x_seconds[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
        ctx_hud_cursor->visual_y_cells[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
        ctx_hud_cursor->visual_width_seconds[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
        ctx_hud_cursor->visual_height_cells[i] = war_pool_alloc(
            pool_wr, sizeof(double) * ctx_hud_cursor->capacity[i]);
    }
    tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_hud_cursor->buffer_count; i++) {
        ctx_hud_cursor->stage[i] =
            &((war_new_vulkan_hud_cursor_instance*)ctx_new_vulkan->map
                  [ctx_new_vulkan->idx_hud_cursor_stage])[tmp_first_instance];
        tmp_first_instance += ctx_hud_cursor->capacity[i];
        if (i + 1 < ctx_hud_cursor->buffer_count) {
            ctx_hud_cursor->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_hud_cursor->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud_cursor];
    ctx_hud_cursor->viewport =
        ctx_new_vulkan
            ->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud_cursor];
    ctx_hud_cursor->push_constant =
        ctx_new_vulkan
            ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud_cursor];
    for (uint32_t i = 0; i < ctx_hud_cursor->buffer_count; i++) {
        ctx_hud_cursor->push_constant[i] =
            (war_new_vulkan_hud_cursor_push_constant){
                .screen_size = {ctx_new_vulkan->physical_width,
                                ctx_new_vulkan->physical_height},
                .cell_size = {ctx_new_vulkan->cell_width,
                              ctx_new_vulkan->cell_height},
                .panning = {0.0f, 0.0f},
                .zoom = 1.0f,
                .cell_offset = {0.0f, 0.0f},
            };
        ctx_hud_cursor->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_hud_cursor->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // cursor default
    ctx_hud_cursor->x_seconds[ctx_hud_cursor->idx_cursor_default][0] = 0.0;
    ctx_hud_cursor->y_cells[ctx_hud_cursor->idx_cursor_default][0] = 0.0;
    ctx_hud_cursor->width_seconds[ctx_hud_cursor->idx_cursor_default][0] =
        1.0 * ctx_misc->seconds_per_cell;
    ctx_hud_cursor->height_cells[ctx_hud_cursor->idx_cursor_default][0] = 1.0;
    ctx_hud_cursor->top_bound_cells =
        ctx_new_vulkan->physical_height / ctx_new_vulkan->cell_height;
    ctx_hud_cursor->bottom_bound_cells = 0.0;
    ctx_hud_cursor->left_bound_seconds = 0.0;
    ctx_hud_cursor->right_bound_seconds =
        (ctx_new_vulkan->physical_width / ctx_new_vulkan->cell_width) *
        ctx_misc->seconds_per_cell;
    ctx_hud_cursor->max_cells_y = 30; // atomic_load(&ctx_lua->A_NOTE_COUNT);
    ctx_hud_cursor->max_seconds_x = 40;
    ctx_hud_cursor->move_factor = 1;
    ctx_hud_cursor->leap_cells = 13;
    ctx_hud_cursor->stage[ctx_hud_cursor->idx_cursor_default][0] =
        (war_new_vulkan_hud_cursor_instance){
            .pos = {0.0f,
                    0.0f,
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_cursor]},
            .size = {1.0f, 1.0f},
            .color = {0.9216, 0.8549, 0.6902, 1.0},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_hud_cursor->push_constant[ctx_hud_cursor->idx_cursor_default]
        .cell_offset[0] = 3.0f;
    ctx_hud_cursor->push_constant[ctx_hud_cursor->idx_cursor_default]
        .cell_offset[1] = 3.0f;
    ctx_hud_cursor->count[ctx_hud_cursor->idx_cursor_default] = 1;
    ctx_hud_cursor->dirty[ctx_hud_cursor->idx_cursor_default] = 1;
    ctx_hud_cursor->draw[ctx_hud_cursor->idx_cursor_default] = 0;
    //-------------------------------------------------------------------------
    // CURSOR CONTEXT
    //-------------------------------------------------------------------------
    war_cursor_context* ctx_cursor =
        war_pool_alloc(pool_wr, sizeof(war_cursor_context));
    ctx_cursor->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_cursor *
                                      ctx_new_vulkan->buffer_max];
    ctx_cursor->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_cursor *
                                     ctx_new_vulkan->buffer_max];
    ctx_cursor->idx_cursor_default = 0;
    ctx_cursor->buffer_count = atomic_load(&ctx_lua->CURSOR_COUNT);
    ctx_cursor->capacity =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_cursor->buffer_count);
    ctx_cursor->stage = war_pool_alloc(pool_wr,
                                       sizeof(war_new_vulkan_cursor_instance*) *
                                           ctx_cursor->buffer_count);
    ctx_cursor->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_cursor *
                                     ctx_new_vulkan->buffer_max];
    ctx_cursor->count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_cursor *
                                     ctx_new_vulkan->buffer_max];
    ctx_cursor->capacity[ctx_cursor->idx_cursor_default] =
        atomic_load(&ctx_lua->CURSOR_DEFAULT_INSTANCE_MAX);
    ctx_cursor->x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    ctx_cursor->y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    ctx_cursor->width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    ctx_cursor->height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    ctx_cursor->visual_x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    ctx_cursor->visual_y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    ctx_cursor->visual_width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    ctx_cursor->visual_height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_cursor->buffer_count);
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        ctx_cursor->x_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
        ctx_cursor->y_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
        ctx_cursor->width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
        ctx_cursor->height_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
        ctx_cursor->visual_x_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
        ctx_cursor->visual_y_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
        ctx_cursor->visual_width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
        ctx_cursor->visual_height_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_cursor->capacity[i]);
    }
    tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        ctx_cursor->stage[i] =
            &((war_new_vulkan_cursor_instance*)ctx_new_vulkan
                  ->map[ctx_new_vulkan->idx_cursor_stage])[tmp_first_instance];
        tmp_first_instance += ctx_cursor->capacity[i];
        if (i + 1 < ctx_cursor->buffer_count) {
            ctx_cursor->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_cursor->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_cursor];
    ctx_cursor->viewport =
        ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_cursor];
    ctx_cursor->push_constant =
        ctx_new_vulkan
            ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_cursor];
    for (uint32_t i = 0; i < ctx_cursor->buffer_count; i++) {
        ctx_cursor->push_constant[i] = (war_new_vulkan_cursor_push_constant){
            .screen_size = {ctx_new_vulkan->physical_width,
                            ctx_new_vulkan->physical_height},
            .cell_size = {ctx_new_vulkan->cell_width,
                          ctx_new_vulkan->cell_height},
            .panning = {0.0f, 0.0f},
            .zoom = 1.0f,
            .cell_offset = {0.0f, 0.0f},
        };
        ctx_cursor->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_cursor->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // cursor default
    ctx_cursor->x_seconds[ctx_cursor->idx_cursor_default][0] = 0.0;
    ctx_cursor->y_cells[ctx_cursor->idx_cursor_default][0] = 0.0;
    ctx_cursor->width_seconds[ctx_cursor->idx_cursor_default][0] =
        1.0 * ctx_misc->seconds_per_cell;
    ctx_cursor->height_cells[ctx_cursor->idx_cursor_default][0] = 1.0;
    ctx_cursor->top_bound_cells =
        ctx_new_vulkan->physical_height / ctx_new_vulkan->cell_height;
    ctx_cursor->bottom_bound_cells = 0.0;
    ctx_cursor->left_bound_seconds = 0.0;
    ctx_cursor->right_bound_seconds =
        (ctx_new_vulkan->physical_width / ctx_new_vulkan->cell_width) *
        ctx_misc->seconds_per_cell;
    ctx_cursor->max_cells_y = 30; // atomic_load(&ctx_lua->A_NOTE_COUNT);
    ctx_cursor->max_seconds_x = 40;
    ctx_cursor->move_factor = 1;
    ctx_cursor->leap_cells = 13;
    ((war_new_vulkan_cursor_instance*)
         ctx_new_vulkan->map[ctx_new_vulkan->idx_cursor_stage])[0] =
        (war_new_vulkan_cursor_instance){
            .pos = {0.0f,
                    0.0f,
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_cursor]},
            .size = {1.0f, 1.0f},
            .color = {0.9216, 0.8549, 0.6902, 1.0},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_cursor->push_constant[ctx_cursor->idx_cursor_default].cell_offset[0] =
        3.0f;
    ctx_cursor->push_constant[ctx_cursor->idx_cursor_default].cell_offset[1] =
        3.0f;
    ctx_cursor->count[ctx_cursor->idx_cursor_default] = 1;
    ctx_cursor->dirty[ctx_cursor->idx_cursor_default] = 1;
    //-------------------------------------------------------------------------
    // LINE CONTEXT
    //-------------------------------------------------------------------------
    war_line_context* ctx_line =
        war_pool_alloc(pool_wr, sizeof(war_line_context));
    ctx_line->buffer_count = atomic_load(&ctx_lua->LINE_COUNT);
    ctx_line->capacity =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_line->buffer_count);
    ctx_line->stage =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_line->buffer_count);
    ctx_line->idx_cell_grid = 0;
    ctx_line->idx_bpm_grid = 1;
    ctx_line->capacity[ctx_line->idx_cell_grid] =
        atomic_load(&ctx_lua->LINE_CELL_INSTANCE_MAX);
    ctx_line->capacity[ctx_line->idx_bpm_grid] =
        atomic_load(&ctx_lua->LINE_BPM_INSTANCE_MAX);
    ctx_line->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_line *
                                      ctx_new_vulkan->buffer_max];
    ctx_line->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_line *
                                     ctx_new_vulkan->buffer_max];
    ctx_line->stage = war_pool_alloc(pool_wr,
                                     sizeof(war_new_vulkan_line_instance*) *
                                         ctx_line->buffer_count);
    ctx_line->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_line *
                                     ctx_new_vulkan->buffer_max];
    ctx_line->count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_line *
                                     ctx_new_vulkan->buffer_max];
    ctx_line->x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_line->buffer_count);
    ctx_line->y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_line->buffer_count);
    ctx_line->width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_line->buffer_count);
    ctx_line->height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_line->buffer_count);
    ctx_line->line_width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_line->buffer_count);
    for (uint32_t i = 0; i < ctx_line->buffer_count; i++) {
        ctx_line->x_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_line->capacity[i]);
        ctx_line->y_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_line->capacity[i]);
        ctx_line->width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_line->capacity[i]);
        ctx_line->height_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_line->capacity[i]);
        ctx_line->line_width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_line->capacity[i]);
    }
    tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_line->buffer_count; i++) {
        ctx_line->stage[i] =
            &((war_new_vulkan_line_instance*)ctx_new_vulkan
                  ->map[ctx_new_vulkan->idx_line_stage])[tmp_first_instance];
        tmp_first_instance += ctx_line->capacity[i];
        if (i + 1 < ctx_line->buffer_count) {
            ctx_line->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_line->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_line];
    ctx_line->viewport =
        ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_line];
    ctx_line->push_constant =
        ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_line];
    for (uint32_t i = 0; i < ctx_line->buffer_count; i++) {
        ctx_line->push_constant[i] = (war_new_vulkan_line_push_constant){
            .screen_size = {ctx_new_vulkan->physical_width,
                            ctx_new_vulkan->physical_height},
            .cell_size = {ctx_new_vulkan->cell_width,
                          ctx_new_vulkan->cell_height},
            .panning = {0.0f, 0.0f},
            .zoom = 1.0f,
            .cell_offset = {3.0f, 3.0f},
        };
        ctx_line->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_line->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // cell grid
    ctx_line->x_seconds[ctx_line->idx_cell_grid][0] = 0;
    ctx_line->y_cells[ctx_line->idx_cell_grid][0] = 10;
    ctx_line->width_seconds[ctx_line->idx_cell_grid][0] =
        (ctx_new_vulkan->physical_width / ctx_new_vulkan->cell_width) *
        ctx_misc->seconds_per_cell;
    ctx_line->height_cells[ctx_line->idx_cell_grid][0] = 0;
    ctx_line->line_width_seconds[ctx_line->idx_cell_grid][0] =
        0.025 * ctx_misc->seconds_per_cell;
    ctx_line->stage[ctx_line->idx_cell_grid][0] =
        (war_new_vulkan_line_instance){
            .pos = {ctx_line->x_seconds[ctx_line->idx_cell_grid][0] /
                        ctx_misc->seconds_per_cell,
                    ctx_line->y_cells[ctx_line->idx_cell_grid][0],
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_line]},
            .size = {ctx_line->width_seconds[ctx_line->idx_cell_grid][0] /
                         ctx_misc->seconds_per_cell,
                     ctx_line->height_cells[ctx_line->idx_cell_grid][0]},
            .width = ctx_line->line_width_seconds[ctx_line->idx_cell_grid][0] /
                     ctx_misc->seconds_per_cell,
            .color = {0.3137, 0.2863, 0.2706, 1.0},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_line->count[ctx_line->idx_cell_grid] = 1;
    ctx_line->dirty[ctx_line->idx_cell_grid] = 1;
    //-------------------------------------------------------------------------
    // TEXT CONTEXT
    //-------------------------------------------------------------------------
    war_text_context* ctx_text =
        war_pool_alloc(pool_wr, sizeof(war_text_context));
    ctx_text->buffer_count = atomic_load(&ctx_lua->TEXT_COUNT);
    ctx_text->capacity =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_text->buffer_count);
    ctx_text->stage =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_text->buffer_count);
    // indices / capacities
    ctx_text->idx_status_bottom = 0;
    ctx_text->capacity[ctx_text->idx_status_bottom] =
        atomic_load(&ctx_lua->TEXT_STATUS_BOTTOM_INSTANCE_MAX);
    ctx_text->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_text *
                                      ctx_new_vulkan->buffer_max];
    ctx_text->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_text *
                                     ctx_new_vulkan->buffer_max];
    ctx_text->stage = war_pool_alloc(pool_wr,
                                     sizeof(war_new_vulkan_text_instance*) *
                                         ctx_cursor->buffer_count);
    ctx_text->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_text *
                                     ctx_new_vulkan->buffer_max];
    ctx_text->count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_text *
                                     ctx_new_vulkan->buffer_max];
    ctx_text->x_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_text->buffer_count);
    ctx_text->y_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_text->buffer_count);
    ctx_text->width_seconds =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_text->buffer_count);
    ctx_text->height_cells =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_text->buffer_count);
    for (uint32_t i = 0; i < ctx_text->buffer_count; i++) {
        ctx_text->x_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_text->capacity[i]);
        ctx_text->y_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_text->capacity[i]);
        ctx_text->width_seconds[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_text->capacity[i]);
        ctx_text->height_cells[i] =
            war_pool_alloc(pool_wr, sizeof(double) * ctx_text->capacity[i]);
    }
    tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_text->buffer_count; i++) {
        ctx_text->stage[i] =
            &((war_new_vulkan_text_instance*)ctx_new_vulkan
                  ->map[ctx_new_vulkan->idx_text_stage])[tmp_first_instance];
        tmp_first_instance += ctx_text->capacity[i];
        if (i + 1 < ctx_text->buffer_count) {
            ctx_text->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_text->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_text];
    ctx_text->viewport =
        ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_text];
    ctx_text->push_constant =
        ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_text];
    for (uint32_t i = 0; i < ctx_text->buffer_count; i++) {
        ctx_text->push_constant[i] = (war_new_vulkan_text_push_constant){
            .screen_size = {ctx_new_vulkan->physical_width,
                            ctx_new_vulkan->physical_height},
            .cell_size = {ctx_new_vulkan->cell_width,
                          ctx_new_vulkan->cell_height},
            .panning = {0.0f, 0.0f},
            .zoom = 1.0f,
            .cell_offset = {3.0f, 3.0f},
        };
        ctx_text->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_text->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // status bottom
    ctx_text->x_seconds[ctx_text->idx_status_bottom][0] =
        3 * ctx_misc->seconds_per_cell;
    ctx_text->y_cells[ctx_text->idx_status_bottom][0] = 3;
    ctx_text->width_seconds[ctx_text->idx_status_bottom][0] =
        1 * ctx_misc->seconds_per_cell;
    ctx_text->height_cells[ctx_text->idx_status_bottom][0] = 1;
    ctx_text->stage[ctx_text->idx_status_bottom][0] =
        (war_new_vulkan_text_instance){
            .pos = {ctx_text->x_seconds[ctx_text->idx_status_bottom][0] /
                        ctx_misc->seconds_per_cell,
                    ctx_text->y_cells[ctx_text->idx_status_bottom][0],
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_line]},
            .size = {ctx_text->width_seconds[ctx_text->idx_status_bottom][0] /
                         ctx_misc->seconds_per_cell,
                     ctx_text->height_cells[ctx_text->idx_status_bottom][0]},
            .uv = {ctx_new_vulkan->glyph_info['M'].uv_x0,
                   ctx_new_vulkan->glyph_info['M'].uv_y0,
                   ctx_new_vulkan->glyph_info['M'].uv_x1,
                   ctx_new_vulkan->glyph_info['M'].uv_y1},
            .glyph_scale = {ctx_new_vulkan->glyph_info['M'].norm_width,
                            ctx_new_vulkan->glyph_info['M'].norm_height},
            .baseline = ctx_new_vulkan->glyph_info['M'].norm_baseline,
            .ascent = ctx_new_vulkan->glyph_info['M'].norm_ascent,
            .descent = ctx_new_vulkan->glyph_info['M'].norm_descent,
            .color = {0.9216, 0.8549, 0.6902, 1.0},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_text->count[ctx_text->idx_status_bottom] = 1;
    ctx_text->dirty[ctx_text->idx_status_bottom] = 1;
    //-------------------------------------------------------------------------
    // SEQUENCE CONTEXT
    //-------------------------------------------------------------------------
    war_sequence_context* ctx_sequence =
        war_pool_alloc(pool_wr, sizeof(war_sequence_context));
    ctx_sequence->buffer_count = atomic_load(&ctx_lua->NOTE_COUNT);
    ctx_sequence->dirty =
        &ctx_new_vulkan->dirty_buffer[ctx_new_vulkan->pipeline_idx_note *
                                      ctx_new_vulkan->buffer_max];
    ctx_sequence->draw =
        &ctx_new_vulkan->draw_buffer[ctx_new_vulkan->pipeline_idx_note *
                                     ctx_new_vulkan->buffer_max];
    ctx_sequence->idx_grid = 0;
    ctx_sequence->buffer_capacity =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_sequence->buffer_count);
    ctx_sequence->stage = war_pool_alloc(
        pool_wr,
        sizeof(war_new_vulkan_cursor_instance*) * ctx_sequence->buffer_count);
    ctx_sequence->first =
        &ctx_new_vulkan
             ->buffer_first_instance[ctx_new_vulkan->pipeline_idx_note *
                                     ctx_new_vulkan->buffer_max];
    ctx_sequence->instance_count =
        &ctx_new_vulkan
             ->buffer_instance_count[ctx_new_vulkan->pipeline_idx_note *
                                     ctx_new_vulkan->buffer_max];
    ctx_sequence->buffer_capacity[ctx_sequence->idx_grid] =
        atomic_load(&ctx_lua->NOTE_GRID_INSTANCE_MAX);
    ctx_sequence->sequence =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_sequence->buffer_count);
    for (uint32_t i = 0; i < ctx_sequence->buffer_count; i++) {
        ctx_sequence->sequence[i] = war_pool_alloc(
            pool_wr,
            sizeof(war_sequence_entry) * ctx_sequence->buffer_capacity[i]);
    }
    tmp_first_instance = 0;
    for (uint32_t i = 0; i < ctx_sequence->buffer_count; i++) {
        ctx_sequence->stage[i] =
            &((war_new_vulkan_note_instance*)ctx_new_vulkan
                  ->map[ctx_new_vulkan->idx_cursor_stage])[tmp_first_instance];
        tmp_first_instance += ctx_sequence->buffer_capacity[i];
        if (i + 1 < ctx_sequence->buffer_count) {
            ctx_sequence->first[i + 1] = tmp_first_instance;
        }
    }
    ctx_sequence->rect_2d =
        ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_note];
    ctx_sequence->viewport =
        ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_note];
    ctx_sequence->push_constant =
        ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_note];
    for (uint32_t i = 0; i < ctx_sequence->buffer_count; i++) {
        ctx_sequence->push_constant[i] = (war_new_vulkan_note_push_constant){
            .screen_size = {ctx_new_vulkan->physical_width,
                            ctx_new_vulkan->physical_height},
            .cell_size = {ctx_new_vulkan->cell_width,
                          ctx_new_vulkan->cell_height},
            .panning = {0.0f, 0.0f},
            .zoom = 1.0f,
            .cell_offset = {0.0f, 0.0f},
        };
        ctx_sequence->viewport[i] = (VkViewport){
            .width = ctx_new_vulkan->physical_width,
            .height = ctx_new_vulkan->physical_height,
            .maxDepth = 1.0f,
            .minDepth = 0.0f,
            .x = 0.0f,
            .y = 0.0f,
        };
        ctx_sequence->rect_2d[i] = (VkRect2D){
            .extent = {(uint32_t)ctx_new_vulkan->physical_width,
                       (uint32_t)ctx_new_vulkan->physical_height},
            .offset = {0, 0},
        };
    }
    // sequence grid
    ctx_sequence->sequence[ctx_sequence->idx_grid][0].duration_seconds = 13;
    ((war_new_vulkan_note_instance*)
         ctx_new_vulkan->map[ctx_new_vulkan->idx_note_stage])[0] =
        (war_new_vulkan_note_instance){
            .pos = {1.0f,
                    0.0f,
                    ctx_new_vulkan->z_layer[ctx_new_vulkan->idx_cursor]},
            .size = {1.0f, 1.0f},
            .color = {0.9216, 0.8549, 0.6902, 1.0},
            .outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .foreground_outline_color = {1.0f, 1.0f, 1.0f, 1.0f},
            .flags = 0,
        };
    ctx_sequence->push_constant[ctx_sequence->idx_grid].cell_offset[0] = 3.0f;
    ctx_sequence->push_constant[ctx_sequence->idx_grid].cell_offset[1] = 3.0f;
    ctx_sequence->instance_count[ctx_sequence->idx_grid] = 1;
    ctx_sequence->dirty[ctx_sequence->idx_grid] = 1;
    //

    //-------------------------------------------------------------------------
    // CAPTURE WAV
    //-------------------------------------------------------------------------
    war_file* capture_wav = war_pool_alloc(pool_wr, sizeof(war_file));
    capture_wav->type = WAR_FILE_TYPE_WAV;
    capture_wav->fd = -1;
    capture_wav->fd_size = 0;
    capture_wav->memfd_size = 44;
    capture_wav->path_capacity = atomic_load(&ctx_lua->CONFIG_PATH_MAX);
    uint64_t init_capacity = 44 + sizeof(float) *
                                      atomic_load(&ctx_lua->A_SAMPLE_RATE) *
                                      atomic_load(&ctx_lua->A_SAMPLE_DURATION) *
                                      atomic_load(&ctx_lua->A_CHANNEL_COUNT);
    capture_wav->memfd_capacity = init_capacity;
    capture_wav->path =
        war_pool_alloc(pool_wr, sizeof(char) * capture_wav->path_capacity);
    capture_wav->path = "capture.wav";
    capture_wav->path_size = strlen(capture_wav->path);
    capture_wav->memfd = memfd_create(capture_wav->path, MFD_CLOEXEC);
    if (capture_wav->memfd < 0) {
        // call_king_terry("memfd failed to open: %s", capture_wav->fname);
    }
    if (ftruncate(capture_wav->memfd, capture_wav->memfd_capacity) == -1) {
        // call_king_terry("memfd ftruncate failed: %s", capture_wav->fname);
    }
    capture_wav->file = mmap(NULL,
                             capture_wav->memfd_capacity,
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED,
                             capture_wav->memfd,
                             0);
    memset(capture_wav->file, 0, capture_wav->memfd_capacity);
    if (capture_wav->file == MAP_FAILED) {
        // call_king_terry("mmap failed: %s", capture_wav->fname);
    }
    war_riff_header init_riff_header = (war_riff_header){
        .chunk_id = "RIFF",
        .chunk_size = init_capacity - 8,
        .format = "WAVE",
    };
    ctx_misc->init_riff_header = init_riff_header;
    war_fmt_chunk init_fmt_chunk = (war_fmt_chunk){
        .subchunk1_id = "fmt ",
        .subchunk1_size = 16,
        .audio_format = 3,
        .num_channels = atomic_load(&ctx_lua->A_CHANNEL_COUNT),
        .sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE),
        .byte_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE) *
                     atomic_load(&ctx_lua->A_CHANNEL_COUNT) * sizeof(float),
        .block_align = atomic_load(&ctx_lua->A_CHANNEL_COUNT) * sizeof(float),
        .bits_per_sample = 32,
    };
    ctx_misc->init_fmt_chunk = init_fmt_chunk;
    war_data_chunk init_data_chunk = (war_data_chunk){
        .subchunk2_id = "data",
        .subchunk2_size = init_capacity - 44,
    };
    ctx_misc->init_data_chunk = init_data_chunk;
    *(war_riff_header*)capture_wav->file = init_riff_header;
    *(war_fmt_chunk*)(capture_wav->file + sizeof(war_riff_header)) =
        init_fmt_chunk;
    *(war_data_chunk*)(capture_wav->file + sizeof(war_riff_header) +
                       sizeof(war_fmt_chunk)) = init_data_chunk;
    //-------------------------------------------------------------------------
    //  ENV
    //-------------------------------------------------------------------------
    war_env* env = war_pool_alloc(pool_wr, sizeof(war_env));
    env->atomics = atomics;
    env->ctx_lua = ctx_lua;
    env->ctx_play = ctx_play;
    env->ctx_capture = ctx_capture;
    env->ctx_command = ctx_command;
    env->pool_wr = pool_wr;
    env->ctx_fsm = ctx_fsm;
    env->pc_capture = pc_capture;
    env->ctx_new_vulkan = ctx_new_vulkan;
    env->ctx_cursor = ctx_cursor;
    env->ctx_misc = ctx_misc;
war_label_wr: {
    if (war_pc_from_a(pc_control, &header, &size, control_payload)) {
        goto* pc_control_cmd[header];
    }
    ctx_misc->now = war_get_monotonic_time_us();
    //-------------------------------------------------------------------------
    // PLAY WRITER
    //-------------------------------------------------------------------------
    if (ctx_misc->now - ctx_play->last_frame_time >= ctx_play->rate_us) {
        ctx_play->last_frame_time += ctx_play->rate_us;
        if (!ctx_play->play) { goto war_label_skip_play; }
        if (ctx_misc->now - ctx_play->last_write_time >= 1000000) {
            atomic_store(&atomics->play_writer_rate,
                         (double)ctx_play->write_count);
            ctx_play->write_count = 0;
            ctx_play->last_write_time = ctx_misc->now;
        }
        ctx_play->write_count++;
        uint64_t write_pos = pc_play->i_to_a;
        uint64_t read_pos = pc_play->i_from_wr;
        int64_t used_bytes = write_pos - read_pos;
        if (used_bytes < 0) used_bytes += pc_play->size;
        float buffer_percent = ((float)used_bytes / pc_play->size) * 100.0f;
        // uint64_t target_samples =
        //     (uint64_t)(((double)atomic_load(&ctx_lua->A_BYTES_NEEDED) *
        //                 atomic_load(&atomics->writer_rate)) /
        //                ((double)atomic_load(&atomics->reader_rate) * 8.0));
        uint64_t target_samples =
            (uint64_t)((double)atomic_load(&ctx_lua->A_BYTES_NEEDED) / 8.0);
        //---------------------------------------------------------------------
        // PLAYBACK
        //---------------------------------------------------------------------
        for (uint32_t i = 0; i < ctx_play->note_count; i++) {}
    }
war_label_skip_play:
    //-------------------------------------------------------------------------
    // CAPTURE READER
    //-------------------------------------------------------------------------
    if (ctx_misc->now - ctx_capture->last_frame_time >= ctx_capture->rate_us) {
        ctx_capture->last_frame_time += ctx_capture->rate_us;
        if (ctx_fsm->current_mode != ctx_fsm->MODE_CAPTURE) {
            pc_capture->i_from_a = pc_capture->i_to_a;
            ctx_capture->state = CAPTURE_WAITING;
            goto war_label_skip_capture;
        }
        if (ctx_misc->now - ctx_capture->last_read_time >= 1000000) {
            atomic_store(&atomics->capture_reader_rate,
                         (double)ctx_capture->read_count);
            // call_king_terry("capture_reader_rate: %.2f Hz",
            //              atomic_load(&atomics->capture_reader_rate));
            ctx_capture->read_count = 0;
            ctx_capture->last_read_time = ctx_misc->now;
        }
        ctx_capture->read_count++;
        uint64_t write_pos = pc_capture->i_to_a;
        uint64_t read_pos = pc_capture->i_from_a;
        int64_t available_bytes;
        if (write_pos >= read_pos) {
            available_bytes = write_pos - read_pos;
        } else {
            available_bytes = pc_capture->size + write_pos - read_pos;
        }
        if (ctx_capture->capture_wait &&
            ctx_capture->state == CAPTURE_WAITING) {
            float max_amplitude = 0.0f;
            uint64_t read_idx = read_pos;
            uint64_t samples_to_check = available_bytes / sizeof(float);
            for (uint64_t i = 0; i < samples_to_check; i++) {
                float sample = ((float*)pc_capture->to_a)[read_idx / 4];
                float amplitude = fabsf(sample);
                if (amplitude > max_amplitude) { max_amplitude = amplitude; }
                read_idx = (read_idx + 4) & (pc_capture->size - 1);
            }
            if (max_amplitude > ctx_capture->threshold) {
                ctx_capture->state = CAPTURE_CAPTURING;
                memset(capture_wav->file, 0, capture_wav->memfd_capacity);
                capture_wav->memfd_size = 44;
                *(war_riff_header*)capture_wav->file = init_riff_header;
                *(war_fmt_chunk*)(capture_wav->file + sizeof(war_riff_header)) =
                    init_fmt_chunk;
                *(war_data_chunk*)(capture_wav->file + sizeof(war_riff_header) +
                                   sizeof(war_fmt_chunk)) = init_data_chunk;
            }
        } else if (!ctx_capture->capture_wait &&
                   ctx_capture->state == CAPTURE_WAITING) {
        }
        if (available_bytes > 0) {
            uint64_t space_left =
                capture_wav->memfd_capacity - capture_wav->memfd_size;
            uint64_t bytes_to_copy =
                available_bytes < space_left ? available_bytes : space_left;
            if (bytes_to_copy > 0) {
                uint64_t read_idx = read_pos;
                uint64_t samples_to_copy = bytes_to_copy / sizeof(float);
                float* wav_samples =
                    (float*)(capture_wav->file + capture_wav->memfd_size);
                for (uint64_t i = 0; i < samples_to_copy; i++) {
                    wav_samples[i] = ((float*)pc_capture->to_a)[read_idx / 4];
                    read_idx = (read_idx + 4) & (pc_capture->size - 1);
                }
                pc_capture->i_from_a = read_idx;
                if (ctx_capture->state == CAPTURE_CAPTURING) {
                    capture_wav->memfd_size += bytes_to_copy;
                    war_riff_header* riff_header =
                        (war_riff_header*)capture_wav->file;
                    riff_header->chunk_size = capture_wav->memfd_size - 8;
                    war_data_chunk* data_chunk =
                        (war_data_chunk*)(capture_wav->file +
                                          sizeof(war_riff_header) +
                                          sizeof(war_fmt_chunk));
                    data_chunk->subchunk2_size = capture_wav->memfd_size - 44;
                }
            }
        }
    }
war_label_skip_capture:
    //-------------------------------------------------------------------------
    // FPS
    //-------------------------------------------------------------------------
    if (ctx_misc->now - ctx_misc->last_frame_time >=
        ctx_misc->frame_duration_us) {
        ctx_misc->last_frame_time += ctx_misc->frame_duration_us;
        if (ctx_misc->trinity) {
            war_holy_trinity(fd,
                             wl_surface_id,
                             wl_buffer_id,
                             0,
                             0,
                             0,
                             0,
                             ctx_new_vulkan->physical_width,
                             ctx_new_vulkan->physical_height);
        }
    }
    //-------------------------------------------------------------------------
    // COMMAND MODE HANDLING
    //-------------------------------------------------------------------------
    if (ctx_fsm->current_mode == ctx_fsm->MODE_COMMAND) {
        if (ctx_command->input_write_index == 0 ||
            ctx_command->input_read_index >= ctx_command->input_write_index) {
            goto war_label_skip_command;
        }
        int input = ctx_command->input[ctx_command->input_read_index];
        if (input == '\b') { // ASCII backspace
            if (ctx_command->text_write_index > 0) {
                for (int i = ctx_command->text_write_index - 1;
                     i < ctx_command->text_size;
                     i++) {
                    ctx_command->text[i] = ctx_command->text[i + 1];
                }
                ctx_command->text_write_index--;
                ctx_command->text_size--;
                ctx_command->text[ctx_command->text_size] = '\0';
            } else if (ctx_command->text_write_index == 0 &&
                       ctx_command->text_size == 0) {
                if (!ctx_command->prompt_type) {}
            }
        } else if (input == '\n') { // ASCII newline
            uint32_t len = war_trim_whitespace(ctx_command->text);
            if (ctx_command->prompt_type == WAR_COMMAND_PROMPT_NONE) {
                goto war_label_command_mode_no_prompt;
            }
            switch (ctx_command->prompt_type) {
            case WAR_COMMAND_PROMPT_CAPTURE_FNAME: {
                if (len == 0) { goto war_label_command_processed; }
                for (uint32_t i = 0; i < len; i++) {
                    if (ctx_command->text[i] == ' ') {
                        goto war_label_command_processed;
                    }
                }
                memset(ctx_capture->fname, 0, ctx_capture->name_limit);
                memcpy(ctx_capture->fname, ctx_fsm->cwd, ctx_fsm->cwd_size);
                memcpy(ctx_capture->fname + ctx_fsm->cwd_size, "/", 1);
                memcpy(ctx_capture->fname + ctx_fsm->cwd_size + 1,
                       ctx_command->text,
                       len);
                ctx_capture->fname_size = len + ctx_fsm->cwd_size + 1;
                // done
                ctx_command->prompt_type = WAR_COMMAND_PROMPT_CAPTURE_NOTE;
                memset(ctx_command->prompt_text, 0, ctx_command->capacity);
                memcpy(ctx_command->prompt_text,
                       ctx_capture->prompt_note_text,
                       ctx_capture->prompt_note_text_size);
                ctx_command->prompt_text_size =
                    ctx_capture->prompt_note_text_size;
                ctx_command->input_write_index = 1;
                goto war_label_skip_command_processed;
            }
            case WAR_COMMAND_PROMPT_CAPTURE_NOTE: {
                if (len == 0) { goto war_label_command_processed; }
                int64_t note = 0;
                for (uint32_t i = 0; i < len; i++) {
                    if (ctx_command->text[i] == ' ' ||
                        !isdigit(ctx_command->text[i])) {
                        goto war_label_command_processed;
                    }
                    note = note * 10 + (ctx_command->text[i] - '0');
                }
                if (note < 0 || note > 127) {
                    goto war_label_command_processed;
                }
                ctx_capture->note = note;
                // done
                ctx_command->prompt_type = WAR_COMMAND_PROMPT_CAPTURE_LAYER;
                memset(ctx_command->prompt_text, 0, ctx_command->capacity);
                memcpy(ctx_command->prompt_text,
                       ctx_capture->prompt_layer_text,
                       ctx_capture->prompt_layer_text_size);
                ctx_command->prompt_text_size =
                    ctx_capture->prompt_layer_text_size;
                ctx_command->input_write_index = 1;
                goto war_label_skip_command_processed;
            }
            case WAR_COMMAND_PROMPT_CAPTURE_LAYER: {
                if (len == 0) { goto war_label_command_processed; }
                uint64_t layer = 0;
                for (uint32_t i = 0; i < len; i++) {
                    char entry = ctx_command->text[i];
                    if (!isdigit(entry) || entry == '0') {
                        goto war_label_command_processed;
                    }
                    uint64_t bit_pos = entry - '1';
                    layer |= (1ULL << bit_pos);
                }
                if (__builtin_popcountll(layer) != 1) {
                    goto war_label_command_processed;
                }
                ctx_capture->layer = __builtin_ctzll(layer);
                // uint64_t idx = ctx_capture->note * map_wav->layer_count +
                //                ctx_capture->layer;
                // uint64_t id = cache_file->next_id++;
                // uint64_t old_id = map_wav->id[idx];
                // map_wav->id[idx] = id;
                // memset(map_wav->fname + idx * map_wav->name_limit,
                //        0,
                //        map_wav->name_limit);
                // memcpy(map_wav->fname + idx * map_wav->name_limit,
                //        ctx_capture->fname,
                //        ctx_capture->fname_size);
                // map_wav->fname_size[idx] = ctx_capture->fname_size;
                // uint32_t cache_idx = 0;
                // if (old_id > 0) {
                //     for (uint32_t i = 0; i < cache_file->count; i++) {
                //         if (cache_file->id[i] != old_id) { continue; }
                //         cache_idx = i;
                //         break;
                //     }
                // } else if (old_id == 0 && cache_file->free_count > 0) {
                //     cache_idx = cache_file->free[--cache_file->free_count];
                // } else if (old_id == 0 &&
                //            cache_file->count < cache_file->capacity) {
                //     cache_idx = cache_file->count++;
                // } else {
                //     uint32_t oldest_idx = 0;
                //     for (uint32_t i = 1; i < cache_file->count; i++) {
                //         if (cache_file->timestamp[i] <=
                //             cache_file->timestamp[oldest_idx]) {
                //             oldest_idx = i;
                //         }
                //     }
                //     cache_idx = oldest_idx;
                // }
                // if (cache_file->id[cache_idx] != 0) {
                //     if (cache_file->fd[cache_idx] >= 0) {
                //         close(cache_file->fd[cache_idx]);
                //         cache_file->fd[cache_idx] = -1;
                //     }
                //     if (cache_file->file[cache_idx] != MAP_FAILED) {
                //         munmap(cache_file->file[cache_idx],
                //                cache_file->memfd_capacity[cache_idx]);
                //         cache_file->file[cache_idx] = MAP_FAILED;
                //     }
                //     if (cache_file->memfd[cache_idx] >= 0) {
                //         close(cache_file->memfd[cache_idx]);
                //         cache_file->memfd[cache_idx] = -1;
                //     }
                // }
                // cache_file->id[cache_idx] = id;
                // cache_file->type[cache_idx] = WAR_FILE_TYPE_WAV;
                // cache_file->timestamp[cache_idx] =
                // cache_file->next_timestamp++;
                // cache_file->memfd_capacity[cache_idx] =
                //     capture_wav->memfd_capacity;
                // cache_file->memfd_size[cache_idx] = capture_wav->memfd_size;
                // cache_file->memfd[cache_idx] = memfd_create(
                //     map_wav->fname + idx * map_wav->name_limit, MFD_CLOEXEC);
                // if (cache_file->memfd[cache_idx] < 0) {
                //     // call_king_terry("memfd_create failed: %s",
                //     //                 map_wav->fname + idx *
                //     //                 map_wav->name_limit);
                //     cache_file->id[cache_idx] = 0;
                //     map_wav->id[idx] = 0;
                //     goto war_label_command_processed;
                // }
                // cache_file->file[cache_idx] =
                //     mmap(NULL,
                //          cache_file->memfd_capacity[cache_idx],
                //          PROT_READ | PROT_WRITE,
                //          MAP_SHARED,
                //          cache_file->memfd[cache_idx],
                //          0);
                // if (cache_file->file[cache_idx] == MAP_FAILED) {
                //     // call_king_terry("mmap failed: %s",
                //     //                 map_wav->fname + idx *
                //     //                 map_wav->name_limit);
                //     cache_file->id[cache_idx] = 0;
                //     map_wav->id[idx] = 0;
                //     close(cache_file->memfd[cache_idx]);
                //     cache_file->memfd[cache_idx] = -1;
                // }
                // cache_file->fd[cache_idx] =
                //     open(map_wav->fname + idx * map_wav->name_limit,
                //          O_RDWR | O_CREAT | O_TRUNC,
                //          0644);
                // if (cache_file->fd[cache_idx] == -1) {
                //     // call_king_terry("fd failed to open: %s",
                //     //                 map_wav->fname + idx *
                //     //                 map_wav->name_limit);
                //     cache_file->id[cache_idx] = 0;
                //     map_wav->id[idx] = 0;
                //     close(cache_file->memfd[cache_idx]);
                //     cache_file->memfd[cache_idx] = -1;
                //     munmap(cache_file->file[cache_idx],
                //            cache_file->memfd_capacity[cache_idx]);
                //     cache_file->file[cache_idx] = MAP_FAILED;
                //     goto war_label_command_processed;
                // }
                // cache_file->fd_size[cache_idx] = capture_wav->memfd_size;
                // if (ftruncate(cache_file->fd[cache_idx],
                //               cache_file->fd_size[cache_idx]) == -1) {
                //     // call_king_terry("ftruncate failed: %s",
                //     //                 map_wav->fname + idx *
                //     //                 map_wav->name_limit);
                //     cache_file->id[cache_idx] = 0;
                //     map_wav->id[idx] = 0;
                //     close(cache_file->memfd[cache_idx]);
                //     cache_file->memfd[cache_idx] = -1;
                //     munmap(cache_file->file[cache_idx],
                //            cache_file->memfd_capacity[cache_idx]);
                //     cache_file->file[cache_idx] = MAP_FAILED;
                //     close(cache_file->fd[cache_idx]);
                //     cache_file->fd[cache_idx] = -1;
                //     goto war_label_command_processed;
                // }
                // off_t offset_1 = 0;
                // ssize_t bytes_copied =
                //     sendfile(cache_file->memfd[cache_idx],
                //              capture_wav->memfd,
                //              &offset_1,
                //              cache_file->memfd_size[cache_idx]);
                // if (bytes_copied !=
                //     (ssize_t)cache_file->memfd_size[cache_idx]) {
                //     // call_king_terry("sendfile failed: %s",
                //     //                 map_wav->fname + idx *
                //     //                 map_wav->name_limit);
                //     cache_file->id[cache_idx] = 0;
                //     map_wav->id[idx] = 0;
                //     close(cache_file->memfd[cache_idx]);
                //     cache_file->memfd[cache_idx] = -1;
                //     munmap(cache_file->file[cache_idx],
                //            cache_file->memfd_capacity[cache_idx]);
                //     cache_file->file[cache_idx] = MAP_FAILED;
                //     close(cache_file->fd[cache_idx]);
                //     cache_file->fd[cache_idx] = -1;
                //     goto war_label_command_processed;
                // }
                // off_t offset_2 = 0;
                // bytes_copied = sendfile(cache_file->fd[cache_idx],
                //                         cache_file->memfd[cache_idx],
                //                         &offset_2,
                //                         cache_file->memfd_size[cache_idx]);
                // if (bytes_copied !=
                //     (ssize_t)cache_file->memfd_size[cache_idx]) {
                //     // call_king_terry("sendfile failed: %s",
                //     //                 map_wav->fname + idx *
                //     //                 map_wav->name_limit);
                //     cache_file->id[cache_idx] = 0;
                //     map_wav->id[idx] = 0;
                //     close(cache_file->memfd[cache_idx]);
                //     cache_file->memfd[cache_idx] = -1;
                //     munmap(cache_file->file[cache_idx],
                //            cache_file->memfd_capacity[cache_idx]);
                //     cache_file->file[cache_idx] = MAP_FAILED;
                //     close(cache_file->fd[cache_idx]);
                //     cache_file->fd[cache_idx] = -1;
                //     goto war_label_command_processed;
                // }
                ctx_command->prompt_type = WAR_COMMAND_PROMPT_NONE;
                memset(ctx_command->prompt_text, 0, ctx_command->capacity);
                ctx_command->prompt_text_size = 0;
                goto war_label_skip_command_processed;
            }
            }
        war_label_command_mode_no_prompt:
            if (strcmp(ctx_command->text, "roll") == 0) {
                memset(ctx_fsm->current_file_path, 0, ctx_fsm->name_limit);
                ctx_fsm->current_file_path_size = 0;
            } else if (strcmp(ctx_command->text, "wav") == 0) {
                memset(ctx_fsm->current_file_path, 0, ctx_fsm->name_limit);
                ctx_fsm->current_file_path_size = 0;
            } else if (strncmp(ctx_command->text, "cd", 2) == 0) {
                if (ctx_command->text[2] != ' ' &&
                    ctx_command->text[2] != '\0') {
                    goto war_label_command_processed;
                }
                if (ctx_command->text[2] == '\0') {
                    // don't have $HOME
                    goto war_label_command_processed;
                }
                ctx_command->text[0] = ' ';
                ctx_command->text[1] = ' ';
                len = war_trim_whitespace(ctx_command->text);
                if (access(ctx_command->text, F_OK) == 0) {
                    if (chdir(ctx_command->text) == 0) {
                    } else {
                        goto war_label_command_processed;
                    }
                } else {
                    goto war_label_command_processed;
                }
                memset(ctx_fsm->cwd, 0, ctx_fsm->name_limit);
                getcwd(ctx_fsm->cwd, ctx_fsm->name_limit);
                ctx_fsm->cwd_size = strlen(ctx_fsm->cwd);
                // call_king_terry("changed working directory to %s",
                //                 ctx_fsm->cwd);
                goto war_label_command_processed;
            } else if (strncmp(ctx_command->text, "e", 1) == 0) {
                if (ctx_command->text[1] != ' ' &&
                    ctx_command->text[1] != '\0') {
                    goto war_label_command_processed;
                }
                if (ctx_command->text[1] == '\0') {
                    // reload project?
                    goto war_label_command_processed;
                }
                // ctx_command->text[0] = ' ';
                // len = war_trim_whitespace(ctx_command->text);
                // memset(ctx_fsm->current_file_path, 0, ctx_fsm->name_limit);
                // ctx_fsm->current_file_path_size =
                //     snprintf(ctx_fsm->current_file_path,
                //              len + ctx_fsm->cwd_size + 2,
                //              "%s/%s",
                //              ctx_fsm->cwd,
                //              ctx_command->text);
                // ctx_fsm->ext_size = war_get_ext(
                //     ctx_command->text, ctx_fsm->ext, ctx_fsm->name_limit);
                // if (strcmp(ctx_fsm->ext, "wav") == 0) {
                //     ctx_fsm->current_file_type = WAR_FILE_TYPE_WAV;
                //     war_wav_mode(env);
                // } else if (strcmp(ctx_fsm->ext, "war") == 0) {
                //     ctx_fsm->current_file_type = WAR_FILE_TYPE_WAR;
                //     war_roll_mode(env);
                // } else {
                //     switch (ctx_fsm->current_file_type) {
                //     case WAR_FILE_TYPE_WAR:
                //         war_roll_mode(env);
                //         break;
                //     case WAR_FILE_TYPE_WAV:
                //         war_wav_mode(env);
                //         break;
                //     }
                // }
                //  call_king_terry("file_path: %s",
                //  ctx_fsm->current_file_path);
            } else if (strcmp(ctx_command->text, "pwd") == 0) {
                // call_king_terry("pwd");
                //  reset without resettign status bar
                memset(ctx_command->text, 0, ctx_command->capacity);
                ctx_command->text_size = 0;
                ctx_command->text_write_index = 0;
                memset(ctx_command->input, 0, ctx_command->capacity);
                ctx_command->input_write_index = 0;
                ctx_command->input_read_index = 0;
                memset(ctx_command->prompt_text, 0, ctx_command->capacity);
                ctx_command->prompt_text_size = 0;
                ctx_command->prompt_type = WAR_COMMAND_PROMPT_NONE;
                ctx_command->text_write_index = 0;
                ctx_command->text_size = 0;
                ctx_command->text[0] = '\0';
                goto war_label_skip_command_processed;
            } else if (strcmp(ctx_command->text, "q!") == 0) {
                // call_king_terry("q!");
                goto war_label_xdg_toplevel_close;
            } else if (strcmp(ctx_command->text, "q") == 0) {
                // call_king_terry("q");
                goto war_label_xdg_toplevel_close;
            }
        war_label_command_processed:
            ctx_command->text_write_index = 0;
            ctx_command->text_size = 0;
            ctx_command->text[0] = '\0';
        war_label_skip_command_processed:
        } else if (input == '\e') { // ASCII escape
            ctx_command->text_write_index = 0;
        } else if (input == 3) { // Arrow left
            if (ctx_command->text_write_index > 0) {
                ctx_command->text_write_index--;
            }
        } else if (input == 4) { // Arrow right
            if (ctx_command->text_write_index < ctx_command->text_size) {
                ctx_command->text_write_index++;
            }
        } else if (input == '\t') {
        } else if (input == '\0') {
        } else {
            if (ctx_command->text_size < ctx_command->capacity - 1) {
                if (ctx_command->text_write_index < ctx_command->text_size) {
                    for (int i = ctx_command->text_size;
                         i > ctx_command->text_write_index;
                         i--) {
                        ctx_command->text[i] = ctx_command->text[i - 1];
                    }
                }

                ctx_command->text[ctx_command->text_write_index] = (char)input;
                ctx_command->text_write_index++;
                ctx_command->text_size++;
                ctx_command->text[ctx_command->text_size] = '\0';
            }
        }
        ctx_command->input_read_index++;
    }
war_label_skip_command:
    //---------------------------------------------------------------------
    // KEY REPEATS
    //---------------------------------------------------------------------
    if (ctx_fsm->repeat_keysym) {
        uint32_t k = ctx_fsm->repeat_keysym;
        uint8_t m = ctx_fsm->repeat_mod;
        if (ctx_fsm->key_down[k * atomic_load(&ctx_lua->WR_MOD_COUNT) + m]) {
            uint64_t elapsed =
                ctx_misc->now -
                ctx_fsm->key_last_event_us[k * atomic_load(
                                                   &ctx_lua->WR_MOD_COUNT) +
                                           m];
            if (!ctx_fsm->repeating) {
                // still waiting for initial delay
                if (elapsed >= ctx_fsm->repeat_delay_us) {
                    ctx_fsm->repeating = 1;
                    ctx_fsm->key_last_event_us[k * atomic_load(
                                                       &ctx_lua->WR_MOD_COUNT) +
                                               m] =
                        ctx_misc->now; // reset timer
                }
            } else {
                if (elapsed >= ctx_fsm->repeat_rate_us) {
                    ctx_fsm->key_last_event_us[k * atomic_load(
                                                       &ctx_lua->WR_MOD_COUNT) +
                                               m] = ctx_misc->now;
                    //---------------------------------------------------------
                    // COMMAND INPUT REPEATS
                    //---------------------------------------------------------
                    if (ctx_fsm->current_mode == ctx_fsm->MODE_COMMAND) {
                        // Write repeated key to command input buffer
                        if (ctx_command->input_write_index <
                            ctx_command->capacity) {
                            uint32_t merged = war_to_ascii(k, m);
                            ctx_command->input[ctx_command->input_write_index] =
                                merged;
                            ctx_command->input_write_index++;
                        }
                        goto war_label_cmd_repeat_done; // Skip FSM processing
                    }
                    uint32_t next_state_index =
                        ctx_fsm->next_state[FSM_3D_INDEX(
                            ctx_fsm->current_state, k, m)];
                    if (next_state_index != 0) {
                        ctx_fsm->current_state = next_state_index;
                        ctx_fsm->state_last_event_us = ctx_misc->now;
                        if (ctx_fsm->is_terminal[FSM_2D_MODE(
                                ctx_fsm->current_state,
                                ctx_fsm->current_mode)] &&
                            !ctx_fsm->is_prefix[FSM_2D_MODE(
                                ctx_fsm->current_state,
                                ctx_fsm->current_mode)] &&
                            ctx_fsm->handle_repeat[FSM_2D_MODE(
                                ctx_fsm->current_state,
                                ctx_fsm->current_mode)]) {
                            uint32_t temp = ctx_fsm->current_state;
                            ctx_fsm->current_state = 0;
                            ctx_fsm->goto_cmd_repeat_done = 1;
                            if (ctx_fsm->function_type[FSM_2D_MODE(
                                    temp, ctx_fsm->current_mode)] ==
                                ctx_fsm->FUNCTION_NONE) {
                                goto war_label_cmd_repeat_done;
                            }
                            if (ctx_fsm->function_type[FSM_2D_MODE(
                                    temp, ctx_fsm->current_mode)] ==
                                ctx_fsm->FUNCTION_C) {
                                ctx_fsm
                                    ->function[FSM_2D_MODE(
                                        temp, ctx_fsm->current_mode)]
                                    .c(env);
                                goto war_label_cmd_done;
                            }
                            // handle lua logic
                            goto war_label_cmd_done;
                        }
                    }
                }
            }
        }
    } else {
        ctx_fsm->repeat_keysym = 0;
        ctx_fsm->repeat_mod = 0;
        ctx_fsm->repeating = 0;
    }
war_label_cmd_repeat_done:
    //--------------------------------------------------------------------
    // KEY TIMEOUTS
    //--------------------------------------------------------------------
    if (ctx_fsm->timeout && ctx_misc->now >= ctx_fsm->timeout_start_us +
                                                 ctx_fsm->timeout_duration_us) {
        uint32_t temp = ctx_fsm->timeout_state_index;
        ctx_fsm->timeout = 0;
        ctx_fsm->timeout_state_index = 0;
        ctx_fsm->timeout_start_us = 0;
        ctx_fsm->goto_cmd_timeout_done = 1;
        // clear current
        ctx_fsm->current_state = 0;
        ctx_fsm->state_last_event_us = ctx_misc->now;
        if (ctx_fsm->function_type[FSM_2D_MODE(temp, ctx_fsm->current_mode)] ==
            ctx_fsm->FUNCTION_NONE) {
            goto war_label_cmd_timeout_done;
        }
        if (ctx_fsm->function_type[FSM_2D_MODE(temp, ctx_fsm->current_mode)] ==
            ctx_fsm->FUNCTION_C) {
            ctx_fsm->function[FSM_2D_MODE(temp, ctx_fsm->current_mode)].c(env);
            goto war_label_cmd_done;
        }
        // handle lua logic
        goto war_label_cmd_done;
    }
war_label_cmd_timeout_done:
    //---------------------------------------------------------------------
    // WAYLAND MESSAGE PARSING
    //---------------------------------------------------------------------
    int ret = poll(&pfd, 1, 0);
    assert(ret >= 0);
    // if (ret == 0) { call_king_terry("timeout"); }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        // call_king_terry("wayland socket error or hangup: %s",
        // strerror(errno));
        goto war_label_end_wr;
    }
    if (pfd.revents & POLLIN) {
        struct msghdr poll_msg_hdr = {0};
        struct iovec poll_iov;
        poll_iov.iov_base = msg_buffer + msg_buffer_size;
        poll_iov.iov_len =
            atomic_load(&ctx_lua->WR_WAYLAND_MSG_BUFFER_SIZE) - msg_buffer_size;
        poll_msg_hdr.msg_iov = &poll_iov;
        poll_msg_hdr.msg_iovlen = 1;
        char poll_ctrl_buf[CMSG_SPACE(sizeof(int) * 4)];
        poll_msg_hdr.msg_control = poll_ctrl_buf;
        poll_msg_hdr.msg_controllen = sizeof(poll_ctrl_buf);
        ssize_t size_read = recvmsg(fd, &poll_msg_hdr, 0);
        assert(size_read > 0);
        msg_buffer_size += size_read;
        size_t msg_buffer_offset = 0;
        while (msg_buffer_size - msg_buffer_offset >= 8) {
            uint32_t size = war_read_le16(msg_buffer + msg_buffer_offset + 6);
            if ((size < 8) || (size > (msg_buffer_size - msg_buffer_offset))) {
                break;
            };
            uint32_t object_id = war_read_le32(msg_buffer + msg_buffer_offset);
            uint32_t opcode = war_read_le16(msg_buffer + msg_buffer_offset + 4);
            if (object_id >=
                    (uint32_t)atomic_load(&ctx_lua->WR_WAYLAND_MAX_OBJECTS) ||
                opcode >=
                    (uint32_t)atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES)) {
                // COMMENT CONCERN: INVALID OBJECT/OP 27 TIMES!
                // call_king_terry(
                //    "invalid object/op: id=%u, op=%u", object_id,
                //    opcode);
                goto war_label_wayland_done;
            }
            size_t idx =
                object_id * atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                opcode;
            if (obj_op[idx]) { goto* obj_op[idx]; }
            goto war_label_wayland_default;
        war_label_wl_registry_global:
            // dump_bytes("global event", msg_buffer + msg_buffer_offset, size);
            // call_king_terry("iname: %s",
            //                 (const char*)msg_buffer + msg_buffer_offset +
            //                 16);

            const char* iname = (const char*)msg_buffer + msg_buffer_offset +
                                16; // COMMENT OPTIMIZE: perfect hash
            if (strcmp(iname, "wl_compositor") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wl_compositor_id = new_id;
                obj_op[wl_compositor_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_compositor_jump;
                new_id++;
            } else if (strcmp(iname, "wl_output") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wl_output_id = new_id;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_output_geometry;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_wl_output_mode;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&war_label_wl_output_done;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&war_label_wl_output_scale;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&war_label_wl_output_name;
                obj_op[wl_output_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&war_label_wl_output_description;
                new_id++;
            } else if (strcmp(iname, "wl_seat") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wl_seat_id = new_id;
                obj_op[wl_seat_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_seat_capabilities;
                obj_op[wl_seat_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_wl_seat_name;
                new_id++;
            } else if (strcmp(iname, "zwp_linux_dmabuf_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_linux_dmabuf_v1_id = new_id;
                obj_op[zwp_linux_dmabuf_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwp_linux_dmabuf_v1_format;
                obj_op[zwp_linux_dmabuf_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_zwp_linux_dmabuf_v1_modifier;
                new_id++;
            } else if (strcmp(iname, "xdg_wm_base") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                xdg_wm_base_id = new_id;
                obj_op[xdg_wm_base_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_xdg_wm_base_ping;
                new_id++;
            } else if (strcmp(iname, "wp_linux_drm_syncobj_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_linux_drm_syncobj_manager_v1_id = new_id;
                obj_op[wp_linux_drm_syncobj_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wp_linux_drm_syncobj_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_idle_inhibit_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_idle_inhibit_manager_v1_id = new_id;
                obj_op[zwp_idle_inhibit_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwp_idle_inhibit_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zxdg_decoration_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zxdg_decoration_manager_v1_id = new_id;
                obj_op[zxdg_decoration_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zxdg_decoration_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_relative_pointer_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_relative_pointer_manager_v1_id = new_id;
                obj_op[zwp_relative_pointer_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwp_relative_pointer_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_pointer_constraints_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_pointer_constraints_v1_id = new_id;
                obj_op[zwp_pointer_constraints_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwp_pointer_constraints_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwlr_output_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwlr_output_manager_v1_id = new_id;
                obj_op[zwlr_output_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwlr_output_manager_v1_head;
                obj_op[zwlr_output_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_zwlr_output_manager_v1_done;
                new_id++;
            } else if (strcmp(iname, "zwlr_data_control_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwlr_data_control_manager_v1_id = new_id;
                obj_op[zwlr_data_control_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwlr_data_control_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_virtual_keyboard_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_virtual_keyboard_manager_v1_id = new_id;
                obj_op[zwp_virtual_keyboard_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwp_virtual_keyboard_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "wp_viewporter") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_viewporter_id = new_id;
                obj_op[wp_viewporter_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wp_viewporter_jump;
                new_id++;
            } else if (strcmp(iname, "wp_fractional_scale_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_fractional_scale_manager_v1_id = new_id;
                obj_op[wp_fractional_scale_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wp_fractional_scale_manager_v1_jump;
                new_id++;
            } else if (strcmp(iname, "zwp_pointer_gestures_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwp_pointer_gestures_v1_id = new_id;
                obj_op[zwp_pointer_gestures_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwp_pointer_gestures_v1_jump;
                new_id++;
            } else if (strcmp(iname, "xdg_activation_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                xdg_activation_v1_id = new_id;
                obj_op[xdg_activation_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_xdg_activation_v1_jump;
                new_id++;
            } else if (strcmp(iname, "wp_presentation") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_presentation_id = new_id;
                obj_op[wp_presentation_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wp_presentation_clock_id;
                new_id++;
            } else if (strcmp(iname, "zwlr_layer_shell_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                zwlr_layer_shell_v1_id = new_id;
                obj_op[zwlr_layer_shell_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwlr_layer_shell_v1_jump;
                new_id++;
            } else if (strcmp(iname, "ext_foreign_toplevel_list_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                ext_foreign_toplevel_list_v1_id = new_id;
                obj_op[ext_foreign_toplevel_list_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_ext_foreign_toplevel_list_v1_toplevel;
                new_id++;
            } else if (strcmp(iname, "wp_content_type_manager_v1") == 0) {
                war_wayland_registry_bind(
                    fd, msg_buffer, msg_buffer_offset, size, new_id);
                wp_content_type_manager_v1_id = new_id;
                obj_op[wp_content_type_manager_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wp_content_type_manager_v1_jump;
                new_id++;
            }
            if (!wl_surface_id && wl_compositor_id) {
                uint8_t create_surface[12];
                war_write_le32(create_surface, wl_compositor_id);
                war_write_le16(create_surface + 4, 0);
                war_write_le16(create_surface + 6, 12);
                war_write_le32(create_surface + 8, new_id);
                // dump_bytes("create_surface request", create_surface, 12);
                // call_king_terry("bound: wl_surface");
                ssize_t create_surface_written = write(fd, create_surface, 12);
                assert(create_surface_written == 12);
                wl_surface_id = new_id;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_surface_enter;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_wl_surface_leave;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&war_label_wl_surface_preferred_buffer_scale;
                obj_op[wl_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&war_label_wl_surface_preferred_buffer_transform;
                new_id++;
            }
            if (!wl_region_id && wl_surface_id && wl_compositor_id) {
                uint8_t create_region[12];
                war_write_le32(create_region, wl_compositor_id);
                war_write_le16(create_region + 4, 1);
                war_write_le16(create_region + 6, 12);
                war_write_le32(create_region + 8, new_id);
                // dump_bytes("create_region request", create_region, 12);
                // call_king_terry("bound: wl_region");
                ssize_t create_region_written = write(fd, create_region, 12);
                assert(create_region_written == 12);
                wl_region_id = new_id;
                new_id++;

                uint8_t region_add[24];
                war_write_le32(region_add, wl_region_id);
                war_write_le16(region_add + 4, 1);
                war_write_le16(region_add + 6, 24);
                war_write_le32(region_add + 8, 0);
                war_write_le32(region_add + 12, 0);
                war_write_le32(region_add + 16, ctx_new_vulkan->physical_width);
                war_write_le32(region_add + 20,
                               ctx_new_vulkan->physical_height);
                // dump_bytes("wl_region::add request", region_add, 24);
                ssize_t region_add_written = write(fd, region_add, 24);
                assert(region_add_written == 24);

                war_wl_surface_set_opaque_region(
                    fd, wl_surface_id, wl_region_id);
            }
            if (!zwp_linux_dmabuf_feedback_v1_id && zwp_linux_dmabuf_v1_id &&
                wl_surface_id) {
                uint8_t get_surface_feedback[16];
                war_write_le32(get_surface_feedback, zwp_linux_dmabuf_v1_id);
                war_write_le16(get_surface_feedback + 4, 3);
                war_write_le16(get_surface_feedback + 6, 16);
                war_write_le32(get_surface_feedback + 8, new_id);
                war_write_le32(get_surface_feedback + 12, wl_surface_id);
                // dump_bytes("zwp_linux_dmabuf_v1::get_surface_feedback
                // request",
                //            get_surface_feedback,
                //            16);
                // call_king_terry("bound: xdg_surface");
                ssize_t get_surface_feedback_written =
                    write(fd, get_surface_feedback, 16);
                assert(get_surface_feedback_written == 16);
                zwp_linux_dmabuf_feedback_v1_id = new_id;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zwp_linux_dmabuf_feedback_v1_done;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] =
                    &&war_label_zwp_linux_dmabuf_feedback_v1_format_table;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] =
                    &&war_label_zwp_linux_dmabuf_feedback_v1_main_device;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] =
                    &&war_label_zwp_linux_dmabuf_feedback_v1_tranche_done;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] =
                    &&war_label_zwp_linux_dmabuf_feedback_v1_tranche_target_device;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] =
                    &&war_label_zwp_linux_dmabuf_feedback_v1_tranche_formats;
                obj_op[zwp_linux_dmabuf_feedback_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       6] =
                    &&war_label_zwp_linux_dmabuf_feedback_v1_tranche_flags;
                new_id++;
            }
            if (!xdg_surface_id && xdg_wm_base_id && wl_surface_id) {
                uint8_t get_xdg_surface[16];
                war_write_le32(get_xdg_surface, xdg_wm_base_id);
                war_write_le16(get_xdg_surface + 4, 2);
                war_write_le16(get_xdg_surface + 6, 16);
                war_write_le32(get_xdg_surface + 8, new_id);
                war_write_le32(get_xdg_surface + 12, wl_surface_id);
                // dump_bytes("get_xdg_surface request", get_xdg_surface, 16);
                // call_king_terry("bound: xdg_surface");
                ssize_t get_xdg_surface_written =
                    write(fd, get_xdg_surface, 16);
                assert(get_xdg_surface_written == 16);
                xdg_surface_id = new_id;
                obj_op[xdg_surface_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_xdg_surface_configure;
                new_id++;
                uint8_t get_toplevel[12];
                war_write_le32(get_toplevel, xdg_surface_id);
                war_write_le16(get_toplevel + 4, 1);
                war_write_le16(get_toplevel + 6, 12);
                war_write_le32(get_toplevel + 8, new_id);
                // dump_bytes("get_xdg_toplevel request", get_toplevel, 12);
                // call_king_terry("bound: xdg_toplevel");
                ssize_t get_toplevel_written = write(fd, get_toplevel, 12);
                assert(get_toplevel_written == 12);
                xdg_toplevel_id = new_id;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_xdg_toplevel_configure;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_xdg_toplevel_close;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&war_label_xdg_toplevel_configure_bounds;
                obj_op[xdg_toplevel_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&war_label_xdg_toplevel_wm_capabilities;
                new_id++;
            }
            if (!zxdg_toplevel_decoration_v1_id && xdg_toplevel_id &&
                zxdg_decoration_manager_v1_id) {
                uint8_t get_toplevel_decoration[16];
                war_write_le32(get_toplevel_decoration,
                               zxdg_decoration_manager_v1_id);
                war_write_le16(get_toplevel_decoration + 4, 1);
                war_write_le16(get_toplevel_decoration + 6, 16);
                war_write_le32(get_toplevel_decoration + 8, new_id);
                war_write_le32(get_toplevel_decoration + 12, xdg_toplevel_id);
                // dump_bytes("get_toplevel_decoration request",
                //            get_toplevel_decoration,
                //            16);
                // call_king_terry("bound: zxdg_toplevel_decoration_v1");
                ssize_t get_toplevel_decoration_written =
                    write(fd, get_toplevel_decoration, 16);
                assert(get_toplevel_decoration_written == 16);
                zxdg_toplevel_decoration_v1_id = new_id;
                obj_op[zxdg_toplevel_decoration_v1_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_zxdg_toplevel_decoration_v1_configure;
                new_id++;
                //---------------------------------------------------------
                // initial commit
                //---------------------------------------------------------
                war_wayland_wl_surface_commit(fd, wl_surface_id);
            }
            goto war_label_wayland_done;
        war_label_wl_registry_global_remove:
            // dump_bytes("global_rm event", msg_buffer + msg_buffer_offset,
            // size);
            goto war_label_wayland_done;
        war_label_wl_callback_done: {
            VkResult result = vkWaitForFences(ctx_new_vulkan->device,
                                              1,
                                              &ctx_new_vulkan->fence,
                                              VK_TRUE,
                                              UINT64_MAX);
            assert(result == VK_SUCCESS);
            result = vkResetFences(
                ctx_new_vulkan->device, 1, &ctx_new_vulkan->fence);
            assert(result == VK_SUCCESS);
            result = vkResetCommandBuffer(ctx_new_vulkan->cmd_buffer, 0);
            assert(result == VK_SUCCESS);
            VkCommandBufferBeginInfo cmd_buffer_begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            };
            result = vkBeginCommandBuffer(ctx_new_vulkan->cmd_buffer,
                                          &cmd_buffer_begin_info);

            assert(result == VK_SUCCESS);
            //----------------------------------------------------------------
            // NEW VULKAN BUFFER FLUSH COPY
            //----------------------------------------------------------------
            ctx_new_vulkan->fn_buffer_idx_count = 0;
            for (uint32_t i = 0; i < ctx_new_vulkan->pipeline_count; i++) {
                for (uint32_t k = 0; k < ctx_new_vulkan->buffer_max; k++) {
                    if (!ctx_new_vulkan
                             ->dirty_buffer[i * ctx_new_vulkan->buffer_max +
                                            k] ||
                        !ctx_new_vulkan->buffer_instance_count
                             [i * ctx_new_vulkan->buffer_max + k]) {
                        continue;
                    }
                    ctx_new_vulkan->fn_buffer_pipeline_idx
                        [ctx_new_vulkan->fn_buffer_idx_count] = i;
                    ctx_new_vulkan
                        ->fn_buffer_idx[ctx_new_vulkan->fn_buffer_idx_count] =
                        k;
                    ctx_new_vulkan->fn_buffer_idx_count++;
                }
            }
            war_new_vulkan_buffer_flush_copy(
                ctx_new_vulkan->fn_buffer_idx_count,
                ctx_new_vulkan->fn_buffer_pipeline_idx,
                ctx_new_vulkan->fn_buffer_idx,
                ctx_new_vulkan,
                ctx_new_vulkan->device,
                ctx_new_vulkan->cmd_buffer);
            memset(ctx_new_vulkan->dirty_buffer,
                   0,
                   sizeof(uint8_t) * ctx_new_vulkan->pipeline_count *
                       ctx_new_vulkan->buffer_max);
            //-------------------------------------------------------------
            //  RENDER PASS
            //-------------------------------------------------------------
            VkClearValue clear_values[2];
            clear_values[0].color =
                (VkClearColorValue){{0.1569f, 0.1569f, 0.1569f, 1.0f}};
            clear_values[1].depthStencil =
                (VkClearDepthStencilValue){1.0f, 0.0f};
            VkRenderPassBeginInfo render_pass_info = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = ctx_new_vulkan->render_pass,
                .framebuffer = ctx_new_vulkan->frame_buffer,
                .renderArea =
                    {
                        .offset = {0, 0},
                        .extent = {ctx_new_vulkan->physical_width,
                                   ctx_new_vulkan->physical_height},
                    },
                .clearValueCount = 2,
                .pClearValues = clear_values,
            };
            vkCmdBeginRenderPass(ctx_new_vulkan->cmd_buffer,
                                 &render_pass_info,
                                 VK_SUBPASS_CONTENTS_INLINE);
            //-----------------------------------------------------------------
            // NEW VULKAN PIPELINE BUFFER DRAW
            //-----------------------------------------------------------------
            ctx_new_vulkan->fn_buffer_idx_count = 0;
            for (uint32_t i = 0; i < ctx_new_vulkan->pipeline_count; i++) {
                for (uint32_t k = 0; k < ctx_new_vulkan->buffer_max; k++) {
                    ctx_new_vulkan->fn_buffer_pipeline_idx
                        [ctx_new_vulkan->fn_buffer_idx_count] = i;
                    ctx_new_vulkan
                        ->fn_buffer_idx[ctx_new_vulkan->fn_buffer_idx_count] =
                        k;
                    ctx_new_vulkan->fn_buffer_idx_count++;
                }
            }
            war_new_vulkan_buffer_draw(ctx_new_vulkan->fn_buffer_idx_count,
                                       ctx_new_vulkan->fn_buffer_pipeline_idx,
                                       ctx_new_vulkan->fn_buffer_idx,
                                       ctx_new_vulkan->cmd_buffer,
                                       ctx_new_vulkan);
            //---------------------------------------------------------
            //    END RENDER PASS
            //---------------------------------------------------------
            vkCmdEndRenderPass(ctx_new_vulkan->cmd_buffer);
            result = vkEndCommandBuffer(ctx_new_vulkan->cmd_buffer);
            assert(result == VK_SUCCESS);
            VkSubmitInfo submit_info = {0};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &ctx_new_vulkan->cmd_buffer;
            result = vkResetFences(
                ctx_new_vulkan->device, 1, &ctx_new_vulkan->fence);
            assert(result == VK_SUCCESS);
            result = vkQueueSubmit(
                ctx_new_vulkan->queue, 1, &submit_info, ctx_new_vulkan->fence);
            assert(result == VK_SUCCESS);
            ctx_misc->trinity = 1;
            goto war_label_wayland_done;
        }
        wl_display_error:
            // dump_bytes("wl_display::error event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        wl_display_delete_id:
            // dump_bytes("wl_display::delete_id event",
            //            msg_buffer + msg_buffer_offset,
            //            size);

            if (war_read_le32(msg_buffer + msg_buffer_offset + 8) ==
                wl_callback_id) {
                war_wayland_wl_surface_frame(fd, wl_surface_id, wl_callback_id);
            }
            goto war_label_wayland_done;
        wl_buffer_release:
            // dump_bytes("wl_buffer_release event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_xdg_wm_base_ping:
            // dump_bytes(
            //     "war_label_xdg_wm_base_ping event", msg_buffer +
            //     msg_buffer_offset, size);
            assert(size == 12);
            uint8_t pong[12];
            war_write_le32(pong, xdg_wm_base_id);
            war_write_le16(pong + 4, 3);
            war_write_le16(pong + 6, 12);
            war_write_le32(pong + 8,
                           war_read_le32(msg_buffer + msg_buffer_offset + 8));
            // dump_bytes("xdg_wm_base_pong request", pong, 12);
            ssize_t pong_written = write(fd, pong, 12);
            assert(pong_written == 12);
            goto war_label_wayland_done;
        war_label_xdg_surface_configure:
            // dump_bytes("war_label_xdg_surface_configure event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            assert(size == 12);

            uint8_t ack_configure[12];
            war_write_le32(ack_configure, xdg_surface_id);
            war_write_le16(ack_configure + 4, 4);
            war_write_le16(ack_configure + 6, 12);
            war_write_le32(ack_configure + 8,
                           war_read_le32(msg_buffer + msg_buffer_offset + 8));
            // dump_bytes(
            //     "xdg_surface_ack_configure request", ack_configure, 12);
            ssize_t ack_configure_written = write(fd, ack_configure, 12);
            assert(ack_configure_written == 12);

            if (!wp_viewport_id) {
                uint8_t get_viewport[16];
                war_write_le32(get_viewport, wp_viewporter_id);
                war_write_le16(get_viewport + 4, 1);
                war_write_le16(get_viewport + 6, 16);
                war_write_le32(get_viewport + 8, new_id);
                war_write_le32(get_viewport + 12, wl_surface_id);
                // dump_bytes("wp_viewporter::get_viewport request",
                //            get_viewport,
                //            16);
                // call_king_terry("bound: wp_viewport");
                ssize_t get_viewport_written = write(fd, get_viewport, 16);
                assert(get_viewport_written == 16);
                wp_viewport_id = new_id;
                new_id++;

                // COMMENT: unecessary
                // uint8_t set_source[24];
                // war_write_le32(set_source, wp_viewport_id);
                // war_write_le16(set_source + 4, 1);
                // war_write_le16(set_source + 6, 24);
                // war_write_le32(set_source + 8, 0);
                // war_write_le32(set_source + 12, 0);
                // war_write_le32(set_source + 16,
                // ctx_new_vulkan->physical_width); war_write_le32(set_source +
                // 20, ctx_new_vulkan->physical_height); dump_bytes(
                //    "wp_viewport::set_source request", set_source, 24);
                // ssize_t set_source_written = write(fd, set_source, 24);
                // assert(set_source_written == 24);

                uint8_t set_destination[16];
                war_write_le32(set_destination, wp_viewport_id);
                war_write_le16(set_destination + 4, 2);
                war_write_le16(set_destination + 6, 16);
                war_write_le32(set_destination + 8, ctx_misc->logical_width);
                war_write_le32(set_destination + 12, ctx_misc->logical_height);
                // dump_bytes("wp_viewport::set_destination request",
                //            set_destination,
                //            16);
                ssize_t set_destination_written =
                    write(fd, set_destination, 16);
                assert(set_destination_written == 16);
            }
            //-------------------------------------------------------------
            // second attach, first frame, commit
            //-------------------------------------------------------------
            war_wayland_wl_surface_attach(
                fd, wl_surface_id, wl_buffer_id, 0, 0);
            if (!wl_callback_id) {
                war_wayland_wl_surface_frame(fd, wl_surface_id, new_id);
                wl_callback_id = new_id;
                obj_op[wl_callback_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_callback_done;
                new_id++;
            }
            war_wayland_wl_surface_commit(fd, wl_surface_id);
            ctx_misc->trinity = 1;
            goto war_label_wayland_done;
        war_label_xdg_toplevel_configure:
            // dump_bytes("war_label_xdg_toplevel_configure event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            uint32_t width = *(uint32_t*)(msg_buffer + msg_buffer_offset + 0);
            uint32_t height = *(uint32_t*)(msg_buffer + msg_buffer_offset + 4);
            // States array starts at offset 8
            uint8_t* states_ptr = msg_buffer + msg_buffer_offset + 8;
            size_t states_bytes =
                size - 12; // subtract object_id/opcode/length + width/height
            size_t num_states = states_bytes / 4;
            war_wl_surface_set_opaque_region(fd, wl_surface_id, 0);
            goto war_label_wayland_done;
        war_label_xdg_toplevel_close:
            // dump_bytes("war_label_xdg_toplevel_close event", msg_buffer +
            // msg_buffer_offset, size);

            uint8_t xdg_toplevel_destroy[8];
            war_write_le32(xdg_toplevel_destroy, xdg_toplevel_id);
            war_write_le16(xdg_toplevel_destroy + 4, 0);
            war_write_le16(xdg_toplevel_destroy + 6, 8);
            ssize_t xdg_toplevel_destroy_written =
                write(fd, xdg_toplevel_destroy, 8);
            // dump_bytes("xdg_toplevel::destroy request",
            // xdg_toplevel_destroy, 8);
            assert(xdg_toplevel_destroy_written == 8);

            uint8_t xdg_surface_destroy[8];
            war_write_le32(xdg_surface_destroy, xdg_surface_id);
            war_write_le16(xdg_surface_destroy + 4, 0);
            war_write_le16(xdg_surface_destroy + 6, 8);
            ssize_t xdg_surface_destroy_written =
                write(fd, xdg_surface_destroy, 8);
            // dump_bytes("xdg_surface::destroy request",
            // xdg_surface_destroy, 8);
            assert(xdg_surface_destroy_written == 8);

            uint8_t wl_buffer_destroy[8];
            war_write_le32(wl_buffer_destroy, wl_buffer_id);
            war_write_le16(wl_buffer_destroy + 4, 0);
            war_write_le16(wl_buffer_destroy + 6, 8);
            ssize_t wl_buffer_destroy_written = write(fd, wl_buffer_destroy, 8);
            // dump_bytes("wl_buffer::destroy request", wl_buffer_destroy,
            // 8);
            assert(wl_buffer_destroy_written == 8);

            uint8_t wl_region_destroy[8];
            war_write_le32(wl_region_destroy, wl_region_id);
            war_write_le16(wl_region_destroy + 4, 0);
            war_write_le16(wl_region_destroy + 6, 8);
            ssize_t wl_region_destroy_written = write(fd, wl_region_destroy, 8);
            // dump_bytes("wl_region::destroy request", wl_region_destroy,
            // 8);
            assert(wl_region_destroy_written == 8);

            uint8_t wl_surface_destroy[8];
            war_write_le32(wl_surface_destroy, wl_surface_id);
            war_write_le16(wl_surface_destroy + 4, 0);
            war_write_le16(wl_surface_destroy + 6, 8);
            ssize_t wl_surface_destroy_written =
                write(fd, wl_surface_destroy, 8);
            // dump_bytes("wl_surface::destroy request",
            // wl_surface_destroy, 8);
            assert(wl_surface_destroy_written == 8);

            war_pc_to_a(pc_control, CONTROL_END_WAR, 0, NULL);
            goto war_label_end_wr;
        war_label_xdg_toplevel_configure_bounds:
            // dump_bytes("war_label_xdg_toplevel_configure_bounds event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_xdg_toplevel_wm_capabilities:
            // dump_bytes("war_label_xdg_toplevel_wm_capabilities event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_v1_format:
            // dump_bytes("war_label_zwp_linux_dmabuf_v1_format event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_v1_modifier:
            // dump_bytes("war_label_zwp_linux_dmabuf_v1_modifier event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_buffer_params_v1_created:
            // dump_bytes("zwp_linux_buffer_params_v1_created", // COMMENT
            //                                                  // REFACTOR: to
            //                                                  ::
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_buffer_params_v1_failed:
            // dump_bytes("zwp_linux_buffer_params_v1_failed event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_feedback_v1_done:
            // dump_bytes("war_label_zwp_linux_dmabuf_feedback_v1_done event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            uint8_t create_params[12]; // REFACTOR: zero initialize
            war_write_le32(create_params, zwp_linux_dmabuf_v1_id);
            war_write_le16(create_params + 4, 1);
            war_write_le16(create_params + 6, 12);
            war_write_le32(create_params + 8, new_id);
            // dump_bytes(
            //     "zwp_linux_dmabuf_v1_create_params request", create_params,
            //     12);
            // call_king_terry("bound: zwp_linux_buffer_params_v1");
            ssize_t create_params_written = write(fd, create_params, 12);
            assert(create_params_written == 12);
            zwp_linux_buffer_params_v1_id = new_id;
            obj_op[zwp_linux_buffer_params_v1_id *
                       atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                   0] = &&war_label_zwp_linux_buffer_params_v1_created;
            obj_op[zwp_linux_buffer_params_v1_id *
                       atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                   1] = &&war_label_zwp_linux_buffer_params_v1_failed;
            new_id++; // COMMENT REFACTOR: move increment to declaration
                      // (one line it)

            uint8_t header[8];
            war_write_le32(header, zwp_linux_buffer_params_v1_id);
            war_write_le16(header + 4, 1);
            war_write_le16(header + 6, 28);
            uint8_t tail[20];
            war_write_le32(tail, 0);
            war_write_le32(tail + 4, 0);
            war_write_le32(tail + 8, ctx_misc->stride);
            war_write_le32(tail + 12, 0);
            war_write_le32(tail + 16, 0);
            struct iovec iov[2] = {
                {.iov_base = header, .iov_len = 8},
                {.iov_base = tail, .iov_len = 20},
            };
            char cmsgbuf[CMSG_SPACE(sizeof(int))] = {0};
            struct msghdr msg = {0};
            msg.msg_iov = iov;
            msg.msg_iovlen = 2;
            msg.msg_control = cmsgbuf;
            msg.msg_controllen = sizeof(cmsgbuf);
            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            *((int*)CMSG_DATA(cmsg)) = ctx_new_vulkan->dmabuf_fd;
            ssize_t dmabuf_sent = sendmsg(fd, &msg, 0);
            if (dmabuf_sent < 0) perror("sendmsg");
            assert(dmabuf_sent == 28);

            uint8_t create_immed[28]; // REFACTOR: maybe 0 initialize
            war_write_le32(
                create_immed,
                zwp_linux_buffer_params_v1_id); // COMMENT REFACTOR: is
                                                // it faster to copy the
                                                // incoming message
                                                // header and increment
                                                // accordingly?
            war_write_le16(create_immed + 4,
                           3); // COMMENT REFACTOR CONCERN: check for
                               // duplicate variables names
            war_write_le16(create_immed + 6, 28);
            war_write_le32(create_immed + 8, new_id);
            war_write_le32(create_immed + 12, ctx_new_vulkan->physical_width);
            war_write_le32(create_immed + 16, ctx_new_vulkan->physical_height);
            war_write_le32(create_immed + 20, DRM_FORMAT_ARGB8888);
            war_write_le32(create_immed + 24, 0);
            // dump_bytes("zwp_linux_buffer_params_v1::create_immed request",
            //            create_immed,
            //            28);
            // call_king_terry("bound: wl_buffer");
            ssize_t create_immed_written = write(fd, create_immed, 28);
            assert(create_immed_written == 28);
            wl_buffer_id = new_id;
            obj_op[wl_buffer_id *
                       atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                   0] = &&wl_buffer_release;
            new_id++;

            uint8_t destroy[8];
            war_write_le32(destroy, zwp_linux_buffer_params_v1_id);
            war_write_le16(destroy + 4, 0);
            war_write_le16(destroy + 6, 8);
            ssize_t destroy_written = write(fd, destroy, 8);
            assert(destroy_written == 8);
            // dump_bytes(
            //     "zwp_linux_buffer_params_v1_id::destroy request", destroy,
            //     8);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_feedback_v1_format_table:
            // dump_bytes("war_label_zwp_linux_dmabuf_feedback_v1_format_table
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size); // REFACTOR: event
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_feedback_v1_main_device:
            // dump_bytes("war_label_zwp_linux_dmabuf_feedback_v1_main_device
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_feedback_v1_tranche_done:
            // dump_bytes("war_label_zwp_linux_dmabuf_feedback_v1_tranche_done
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_feedback_v1_tranche_target_device:
            // dump_bytes("zwp_linux_dmabuf_feedback_v1_tranche_target_"
            //            "device event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_feedback_v1_tranche_formats:
            // dump_bytes("war_label_zwp_linux_dmabuf_feedback_v1_tranche_formats
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_linux_dmabuf_feedback_v1_tranche_flags:
            // dump_bytes("war_label_zwp_linux_dmabuf_feedback_v1_tranche_flags
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wp_linux_drm_syncobj_manager_v1_jump:
            // dump_bytes("war_label_wp_linux_drm_syncobj_manager_v1_jump
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        wp_linux_drm_syncobj_timeline_v1_jump:
            // dump_bytes("wp_linux_drm_syncobj_timeline_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        wp_linux_drm_syncobj_surface_v1_jump:
            // dump_bytes("wp_linux_drm_syncobj_timeline_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_compositor_jump:
            // dump_bytes("war_label_wl_compositor_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_surface_enter:
            // dump_bytes(
            //     "war_label_wl_surface_enter event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_surface_leave:
            // dump_bytes(
            //     "war_label_wl_surface_leave event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_surface_preferred_buffer_scale:
            // dump_bytes("war_label_wl_surface_preferred_buffer_scale event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            assert(size == 12);

            uint8_t set_buffer_scale[12];
            war_write_le32(set_buffer_scale, wl_surface_id);
            war_write_le16(set_buffer_scale + 4, 8);
            war_write_le16(set_buffer_scale + 6, 12);
            war_write_le32(set_buffer_scale + 8,
                           war_read_le32(msg_buffer + msg_buffer_offset + 8));
            // dump_bytes(
            //     "wl_surface::set_buffer_scale request", set_buffer_scale,
            //     12);
            ssize_t set_buffer_scale_written = write(fd, set_buffer_scale, 12);
            assert(set_buffer_scale_written == 12);
            goto war_label_wayland_done;
        war_label_wl_surface_preferred_buffer_transform:
            // dump_bytes("war_label_wl_surface_preferred_buffer_transform
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            assert(size == 12);

            uint8_t set_buffer_transform[12];
            war_write_le32(set_buffer_transform, wl_surface_id);
            war_write_le16(set_buffer_transform + 4, 7);
            war_write_le16(set_buffer_transform + 6, 12);
            war_write_le32(set_buffer_transform + 8,
                           war_read_le32(msg_buffer + msg_buffer_offset + 8));
            // dump_bytes("wl_surface::set_buffer_transform request",
            //            set_buffer_transform,
            //            12);
            ssize_t set_buffer_transform_written =
                write(fd, set_buffer_transform, 12);
            assert(set_buffer_transform_written == 12);
            goto war_label_wayland_done;
        war_label_zwp_idle_inhibit_manager_v1_jump:
            // dump_bytes("war_label_zwp_idle_inhibit_manager_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwlr_layer_shell_v1_jump:
            // dump_bytes("war_label_zwlr_layer_shell_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zxdg_decoration_manager_v1_jump:
            // dump_bytes("war_label_zxdg_decoration_manager_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zxdg_toplevel_decoration_v1_configure: {
            // dump_bytes("war_label_zxdg_toplevel_decoration_v1_configure
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            uint8_t set_mode[12];
            war_write_le32(set_mode, zxdg_toplevel_decoration_v1_id);
            war_write_le16(set_mode + 4, 1);
            war_write_le16(set_mode + 6, 12);
            war_write_le32(set_mode + 8, 1);
            // war_write_le32(
            //     set_mode + 8,
            //     war_read_le32(msg_buffer + msg_buffer_offset + 8));
            // dump_bytes(
            //    "zxdg_toplevel_decoration_v1::set_mode request", set_mode,
            //    12);
            ssize_t set_mode_written = write(fd, set_mode, 12);
            assert(set_mode_written == 12);
            goto war_label_wayland_done;
        }
        war_label_zwp_relative_pointer_manager_v1_jump:
            // dump_bytes("war_label_zwp_relative_pointer_manager_v1_jump
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_pointer_constraints_v1_jump:
            // dump_bytes("war_label_zwp_pointer_constraints_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wp_presentation_clock_id:
            // dump_bytes("war_label_wp_presentation_clock_id event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwlr_output_manager_v1_head:
            // dump_bytes("war_label_zwlr_output_manager_v1_head event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwlr_output_manager_v1_done:
            // dump_bytes("war_label_zwlr_output_manager_v1_done event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_ext_foreign_toplevel_list_v1_toplevel:
            // dump_bytes("war_label_ext_foreign_toplevel_list_v1_toplevel
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwlr_data_control_manager_v1_jump:
            // dump_bytes("war_label_zwlr_data_control_manager_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wp_viewporter_jump:
            // dump_bytes("war_label_wp_viewporter_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wp_content_type_manager_v1_jump:
            // dump_bytes("war_label_wp_content_type_manager_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wp_fractional_scale_manager_v1_jump:
            // dump_bytes("war_label_wp_fractional_scale_manager_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_xdg_activation_v1_jump:
            // dump_bytes("war_label_xdg_activation_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_virtual_keyboard_manager_v1_jump:
            // dump_bytes("war_label_zwp_virtual_keyboard_manager_v1_jump
            // event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_zwp_pointer_gestures_v1_jump:
            // dump_bytes("war_label_zwp_pointer_gestures_v1_jump event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_seat_capabilities:
            // dump_bytes("war_label_wl_seat_capabilities event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            enum {
                wl_seat_pointer = 0x01,
                wl_seat_keyboard = 0x02,
                wl_seat_touch = 0x04,
            };
            uint32_t capabilities =
                war_read_le32(msg_buffer + msg_buffer_offset + 8);
            if (capabilities & wl_seat_keyboard) {
                // call_king_terry("keyboard detected");
                assert(size == 12);
                uint8_t get_keyboard[12];
                war_write_le32(get_keyboard, wl_seat_id);
                war_write_le16(get_keyboard + 4, 1);
                war_write_le16(get_keyboard + 6, 12);
                war_write_le32(get_keyboard + 8, new_id);
                // dump_bytes("get_keyboard request", get_keyboard, 12);
                // call_king_terry("bound: wl_keyboard",
                //                 (const char*)msg_buffer + msg_buffer_offset +
                //                     12);
                ssize_t get_keyboard_written = write(fd, get_keyboard, 12);
                assert(get_keyboard_written == 12);
                wl_keyboard_id = new_id;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_keyboard_keymap;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_wl_keyboard_enter;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&war_label_wl_keyboard_leave;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&war_label_wl_keyboard_key;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&war_label_wl_keyboard_modifiers;
                obj_op[wl_keyboard_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&war_label_wl_keyboard_repeat_info;
                new_id++;
            }
            if (capabilities & wl_seat_pointer) {
                // call_king_terry("pointer detected");
                assert(size == 12);
                uint8_t get_pointer[12];
                war_write_le32(get_pointer, wl_seat_id);
                war_write_le16(get_pointer + 4, 0);
                war_write_le16(get_pointer + 6, 12);
                war_write_le32(get_pointer + 8, new_id);
                // dump_bytes("get_pointer request", get_pointer, 12);
                // call_king_terry("bound: wl_pointer");
                ssize_t get_pointer_written = write(fd, get_pointer, 12);
                assert(get_pointer_written == 12);
                wl_pointer_id = new_id;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_pointer_enter;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_wl_pointer_leave;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&war_label_wl_pointer_motion;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&war_label_wl_pointer_button;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&war_label_wl_pointer_axis;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&war_label_wl_pointer_frame;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       6] = &&war_label_wl_pointer_axis_source;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       7] = &&war_label_wl_pointer_axis_stop;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       8] = &&war_label_wl_pointer_axis_discrete;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       9] = &&war_label_wl_pointer_axis_value120;
                obj_op[wl_pointer_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       10] = &&war_label_wl_pointer_axis_relative_direction;
                new_id++;
            }
            if (capabilities & wl_seat_touch) {
                // call_king_terry("touch detected");
                assert(size == 12);
                uint8_t get_touch[12];
                war_write_le32(get_touch, wl_seat_id);
                war_write_le16(get_touch + 4, 2);
                war_write_le16(get_touch + 6, 12);
                war_write_le32(get_touch + 8, new_id);
                // dump_bytes("get_touch request", get_touch, 12);
                // call_king_terry("bound: wl_touch");
                ssize_t get_touch_written = write(fd, get_touch, 12);
                assert(get_touch_written == 12);
                wl_touch_id = new_id;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       0] = &&war_label_wl_touch_down;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       1] = &&war_label_wl_touch_up;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       2] = &&war_label_wl_touch_motion;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       3] = &&war_label_wl_touch_frame;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       4] = &&war_label_wl_touch_cancel;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       5] = &&war_label_wl_touch_shape;
                obj_op[wl_touch_id *
                           atomic_load(&ctx_lua->WR_WAYLAND_MAX_OP_CODES) +
                       6] = &&war_label_wl_touch_orientation;
                new_id++;
            }
            goto war_label_wayland_done;
        war_label_wl_seat_name:
            // dump_bytes(
            //     "war_label_wl_seat_name event", msg_buffer +
            //     msg_buffer_offset, size);
            // call_king_terry("seat: %s",
            //                 (const char*)msg_buffer + msg_buffer_offset +
            //                 12);
            goto war_label_wayland_done;
        war_label_wl_keyboard_keymap: {
            // dump_bytes("war_label_wl_keyboard_keymap event", msg_buffer,
            // size);
            assert(size == 16);
            ctx_fsm->keymap_fd = -1;
            for (struct cmsghdr* poll_cmsg = CMSG_FIRSTHDR(&poll_msg_hdr);
                 poll_cmsg != NULL;
                 poll_cmsg = CMSG_NXTHDR(&poll_msg_hdr, poll_cmsg)) {
                if (poll_cmsg->cmsg_level == SOL_SOCKET &&
                    poll_cmsg->cmsg_type == SCM_RIGHTS) {
                    ctx_fsm->keymap_fd = *(int*)CMSG_DATA(poll_cmsg);
                    break;
                }
            }
            assert(ctx_fsm->keymap_fd >= 0);
            ctx_fsm->keymap_format =
                war_read_le32(msg_buffer + msg_buffer_offset + 8);
            assert(ctx_fsm->keymap_format == XKB_KEYMAP_FORMAT_TEXT_V1);
            ctx_fsm->keymap_size =
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4);
            assert(ctx_fsm->keymap_size > 0);
            //-----------------------------------------------------------------
            // FSM INIT
            //-----------------------------------------------------------------
            ctx_fsm->xkb_context = xkb_context_new(0);
            assert(ctx_fsm->xkb_context);
            ctx_fsm->keymap_map = mmap(NULL,
                                       ctx_fsm->keymap_size,
                                       PROT_READ,
                                       MAP_PRIVATE,
                                       ctx_fsm->keymap_fd,
                                       0);
            assert(ctx_fsm->keymap_map != MAP_FAILED);
            ctx_fsm->xkb_keymap =
                xkb_keymap_new_from_string(ctx_fsm->xkb_context,
                                           ctx_fsm->keymap_map,
                                           XKB_KEYMAP_FORMAT_TEXT_V1,
                                           0);
            assert(ctx_fsm->xkb_keymap);
            ctx_fsm->xkb_state = xkb_state_new(ctx_fsm->xkb_keymap);
            assert(ctx_fsm->xkb_state);
            ctx_fsm->mod_shift = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                          XKB_MOD_NAME_SHIFT);
            ctx_fsm->mod_ctrl = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                         XKB_MOD_NAME_CTRL);
            ctx_fsm->mod_alt =
                xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap, XKB_MOD_NAME_ALT);
            ctx_fsm->mod_logo = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                         XKB_MOD_NAME_LOGO);
            ctx_fsm->mod_caps = xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap,
                                                         XKB_MOD_NAME_CAPS);
            ctx_fsm->mod_num =
                xkb_keymap_mod_get_index(ctx_fsm->xkb_keymap, XKB_MOD_NAME_NUM);
            //-----------------------------------------------------------------
            // LUA DEFAULT MODES
            //-----------------------------------------------------------------
            lua_getglobal(ctx_lua->L, "war");
            if (!lua_istable(ctx_lua->L, -1)) {
                // call_king_terry("war not a table");
                lua_pop(ctx_lua->L, 1);
            }
            lua_getfield(ctx_lua->L, -1, "modes");
            if (!lua_istable(ctx_lua->L, -1)) {
                // call_king_terry("modes not a field");
                lua_pop(ctx_lua->L, 2);
            }
            lua_pushnil(ctx_lua->L);
            while (lua_next(ctx_lua->L, -2) != 0) {
                const char* mode_name = lua_tostring(ctx_lua->L, -2);
                int mode_value = (int)lua_tointeger(ctx_lua->L, -1);
                if (strcmp(mode_name, "roll") == 0) {
                    ctx_fsm->MODE_ROLL = mode_value;
                } else if (strcmp(mode_name, "views") == 0) {
                    ctx_fsm->MODE_VIEWS = mode_value;
                } else if (strcmp(mode_name, "capture") == 0) {
                    ctx_fsm->MODE_CAPTURE = mode_value;
                } else if (strcmp(mode_name, "midi") == 0) {
                    ctx_fsm->MODE_MIDI = mode_value;
                } else if (strcmp(mode_name, "command") == 0) {
                    ctx_fsm->MODE_COMMAND = mode_value;
                } else if (strcmp(mode_name, "wav") == 0) {
                    ctx_fsm->MODE_WAV = mode_value;
                } else if (strcmp(mode_name, "visual") == 0) {
                    ctx_fsm->MODE_VISUAL = mode_value;
                } else if (strcmp(mode_name, "chord") == 0) {
                    ctx_fsm->MODE_CHORD = mode_value;
                } else {
                    call_king_terry("unknown mode");
                }
                // call_king_terry("mode: %s, value: %i", mode_name,
                // mode_value);
                lua_pop(ctx_lua->L, 1);
            }
            lua_pop(ctx_lua->L, 2);
            //-----------------------------------------------------------------
            // LUA DEFAULT FUNCTION TYPES
            //-----------------------------------------------------------------
            lua_getglobal(ctx_lua->L, "war");
            if (!lua_istable(ctx_lua->L, -1)) {
                // call_king_terry("war not a table");
                lua_pop(ctx_lua->L, 1);
            }
            lua_getfield(ctx_lua->L, -1, "function_types");
            if (!lua_istable(ctx_lua->L, -1)) {
                // call_king_terry("function_types not a field");
                lua_pop(ctx_lua->L, 2);
            }
            lua_pushnil(ctx_lua->L);
            while (lua_next(ctx_lua->L, -2) != 0) {
                const char* type_name = lua_tostring(ctx_lua->L, -2);
                int type_value = (int)lua_tointeger(ctx_lua->L, -1);
                if (strcmp(type_name, "none") == 0) {
                    ctx_fsm->FUNCTION_NONE = type_value;
                } else if (strcmp(type_name, "c") == 0) {
                    ctx_fsm->FUNCTION_C = type_value;
                } else if (strcmp(type_name, "lua") == 0) {
                    ctx_fsm->FUNCTION_LUA = type_value;
                }
                // call_king_terry("type: %s, value: %i", type_name,
                // type_value);
                lua_pop(ctx_lua->L, 1);
            }
            lua_pop(ctx_lua->L, 2);
            //-----------------------------------------------------------------
            // LUA DEFAULT KEYMAP
            //-----------------------------------------------------------------
            int default_handle_release = 0;
            int default_handle_repeat = 1;
            int default_handle_timeout = 1;
            lua_getglobal(ctx_lua->L, "keymap_flags");
            if (lua_istable(ctx_lua->L, -1)) {
                lua_getfield(ctx_lua->L, -1, "handle_release");
                if (lua_isnumber(ctx_lua->L, -1)) {
                    default_handle_release = (int)lua_tointeger(ctx_lua->L, -1);
                }
                lua_pop(ctx_lua->L, 1);

                lua_getfield(ctx_lua->L, -1, "handle_repeat");
                if (lua_isnumber(ctx_lua->L, -1)) {
                    default_handle_repeat = (int)lua_tointeger(ctx_lua->L, -1);
                }
                lua_pop(ctx_lua->L, 1);

                lua_getfield(ctx_lua->L, -1, "handle_timeout");
                if (lua_isnumber(ctx_lua->L, -1)) {
                    default_handle_timeout = (int)lua_tointeger(ctx_lua->L, -1);
                }
                lua_pop(ctx_lua->L, 1);
            }
            lua_pop(ctx_lua->L, 1);

            lua_getglobal(ctx_lua->L, "war_flattened");
            if (!lua_istable(ctx_lua->L, -1)) {
                // call_king_terry("war_flattened not a table");
                lua_pop(ctx_lua->L, 1);
                goto war_label_wayland_done;
            }

            uint32_t max_states = ctx_fsm->state_count;
            uint8_t state_has_children[max_states];
            memset(state_has_children, 0, sizeof(state_has_children));
            uint32_t next_state_counter = 1;
            uint32_t sequence_count = 0;

            lua_pushnil(ctx_lua->L);
            while (lua_next(ctx_lua->L, -2) != 0) {
                if (!lua_istable(ctx_lua->L, -1)) {
                    lua_pop(ctx_lua->L, 1);
                    continue;
                }

                lua_getfield(ctx_lua->L, -1, "keys");
                int idx_keys = lua_gettop(ctx_lua->L);
                lua_getfield(ctx_lua->L, -2, "commands");
                int idx_commands = lua_gettop(ctx_lua->L);
                if (!lua_istable(ctx_lua->L, idx_keys) ||
                    !lua_istable(ctx_lua->L, idx_commands)) {
                    lua_pop(ctx_lua->L, 2);
                    lua_pop(ctx_lua->L, 1);
                    continue;
                }

                uint32_t current_state = 0;
                size_t keys_len = lua_objlen(ctx_lua->L, idx_keys);
                for (size_t i = 1; i <= keys_len; i++) {
                    lua_rawgeti(ctx_lua->L, idx_keys, (int)i);
                    const char* token = lua_tostring(ctx_lua->L, -1);
                    uint32_t keysym = 0;
                    uint8_t mod = 0;
                    if (token &&
                        war_parse_token_to_keysym_mod(token, &keysym, &mod)) {
                        size_t idx3d = FSM_3D_INDEX(current_state, keysym, mod);
                        uint32_t next_state = ctx_fsm->next_state[idx3d];
                        if (next_state == 0) {
                            if (next_state_counter < max_states) {
                                next_state = next_state_counter++;
                            } else {
                                next_state = max_states - 1;
                                // call_king_terry("fsm state overflow");
                            }
                            ctx_fsm->next_state[idx3d] = next_state;
                        }
                        if (next_state != current_state) {
                            state_has_children[current_state] = 1;
                        }
                        current_state = next_state;
                    } else {
                        // call_king_terry("invalid key token: %s",
                        //                token ? token : "(nil)");
                    }
                    lua_pop(ctx_lua->L, 1);
                }

                sequence_count++;
                size_t commands_len = lua_objlen(ctx_lua->L, idx_commands);
                for (size_t j = 1; j <= commands_len; j++) {
                    lua_rawgeti(ctx_lua->L, idx_commands, (int)j);
                    if (!lua_istable(ctx_lua->L, -1)) {
                        lua_pop(ctx_lua->L, 1);
                        continue;
                    }

                    lua_getfield(ctx_lua->L, -1, "cmd");
                    const char* cmd_name = lua_tostring(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    lua_getfield(ctx_lua->L, -1, "mode");
                    int mode = (int)lua_tointeger(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    lua_getfield(ctx_lua->L, -1, "type");
                    int type = (int)lua_tointeger(ctx_lua->L, -1);
                    lua_pop(ctx_lua->L, 1);

                    int handle_release = default_handle_release;
                    lua_getfield(ctx_lua->L, -1, "handle_release");
                    if (lua_isnumber(ctx_lua->L, -1)) {
                        handle_release = (int)lua_tointeger(ctx_lua->L, -1);
                    }
                    lua_pop(ctx_lua->L, 1);

                    int handle_repeat = default_handle_repeat;
                    lua_getfield(ctx_lua->L, -1, "handle_repeat");
                    if (lua_isnumber(ctx_lua->L, -1)) {
                        handle_repeat = (int)lua_tointeger(ctx_lua->L, -1);
                    }
                    lua_pop(ctx_lua->L, 1);

                    int handle_timeout = default_handle_timeout;
                    lua_getfield(ctx_lua->L, -1, "handle_timeout");
                    if (lua_isnumber(ctx_lua->L, -1)) {
                        handle_timeout = (int)lua_tointeger(ctx_lua->L, -1);
                    }
                    lua_pop(ctx_lua->L, 1);

                    if (cmd_name && mode < (int)ctx_fsm->mode_count) {
                        size_t mode_idx = FSM_2D_MODE(current_state, mode);
                        ctx_fsm->is_terminal[mode_idx] = 1;
                        ctx_fsm->handle_release[mode_idx] = handle_release;
                        ctx_fsm->handle_repeat[mode_idx] = handle_repeat;
                        ctx_fsm->handle_timeout[mode_idx] = handle_timeout;
                        ctx_fsm->function_type[mode_idx] = type;
                        size_t name_offset = FSM_3D_NAME(current_state, mode);
                        strncpy(ctx_fsm->name + name_offset,
                                cmd_name,
                                ctx_fsm->name_limit - 1);
                        ctx_fsm->name[name_offset + ctx_fsm->name_limit - 1] =
                            '\0';
                        if (type == ctx_fsm->FUNCTION_C) {
                            ctx_fsm->function[mode_idx].c =
                                war_build_keymap_functions(cmd_name);
                            if (!ctx_fsm->function[mode_idx].c) {
                                ctx_fsm->function_type[mode_idx] =
                                    ctx_fsm->FUNCTION_NONE;
                            }
                        }
                    }

                    lua_pop(ctx_lua->L, 1);
                }

                lua_pop(ctx_lua->L, 2); // commands, keys
                lua_pop(ctx_lua->L, 1); // entry table
            }
            lua_pop(ctx_lua->L, 1); // war_flattened table
            for (uint32_t s = 0; s < next_state_counter; s++) {
                if (!state_has_children[s]) { continue; }
                for (uint32_t m = 0; m < ctx_fsm->mode_count; m++) {
                    ctx_fsm->is_prefix[FSM_2D_MODE(s, m)] = 1;
                }
            }
            goto war_label_wayland_done;
        }
        war_label_wl_keyboard_enter:
            // dump_bytes("war_label_wl_keyboard_enter event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_keyboard_leave:
            // dump_bytes("war_label_wl_keyboard_leave event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_keyboard_key: {
            // dump_bytes("war_label_wl_keyboard_key event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            uint32_t wl_key_state =
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 + 4);
            uint32_t keycode =
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4) +
                8; // + 8 cuz wayland
            xkb_keysym_t keysym =
                xkb_state_key_get_one_sym(ctx_fsm->xkb_state, keycode);
            xkb_mod_mask_t mods = xkb_state_serialize_mods(
                ctx_fsm->xkb_state, XKB_STATE_MODS_DEPRESSED);
            uint32_t mod = 0;
            if (mods & (1 << ctx_fsm->mod_shift)) { mod |= MOD_SHIFT; }
            if (mods & (1 << ctx_fsm->mod_ctrl)) { mod |= MOD_CTRL; }
            if (mods & (1 << ctx_fsm->mod_alt)) { mod |= MOD_ALT; }
            if (mods & (1 << ctx_fsm->mod_logo)) { mod |= MOD_LOGO; }
            uint8_t pressed = (wl_key_state == 1);
            keysym = war_normalize_keysym(keysym);
            if (keysym == XKB_KEY_NoSymbol) {
                ctx_fsm->repeat_keysym = 0;
                ctx_fsm->repeat_mod = 0;
                ctx_fsm->repeating = 0;
                // timeouts
                ctx_fsm->timeout = 0;
                ctx_fsm->timeout_state_index = 0;
                ctx_fsm->timeout_start_us = 0;
                memset(ctx_fsm->key_down,
                       0,
                       ctx_fsm->keysym_count * ctx_fsm->mod_count);
                goto war_label_cmd_done;
            }
            //-----------------------------------------------------------------
            // COMMAND INPUT
            //-----------------------------------------------------------------
            if (ctx_fsm->current_mode == ctx_fsm->MODE_COMMAND) {
                if (pressed) {
                    // Write to input buffer for command mode
                    if (ctx_command->input_write_index <
                        ctx_command->capacity) {
                        uint32_t merged = war_to_ascii(keysym, mod);
                        ctx_command->input[ctx_command->input_write_index] =
                            merged;
                        ctx_command->input_write_index++;
                    }
                    // SET UP REPEATS FOR COMMAND MODE
                    ctx_fsm->repeat_keysym = keysym;
                    ctx_fsm->repeat_mod = mod;
                    ctx_fsm->repeating = 0;
                    ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                        ctx_fsm->current_state, keysym, mod)] = ctx_misc->now;
                    if (!ctx_fsm->key_down[FSM_3D_INDEX(
                            ctx_fsm->current_state, keysym, mod)]) {
                        ctx_fsm->key_down[FSM_3D_INDEX(
                            ctx_fsm->current_state, keysym, mod)] = 1;
                        ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                            ctx_fsm->current_state, keysym, mod)] =
                            ctx_misc->now;
                    }
                    goto war_label_cmd_done;
                }
                if (!pressed) {
                    ctx_fsm->key_down[FSM_3D_INDEX(
                        ctx_fsm->current_state, keysym, mod)] = 0;
                    ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                        ctx_fsm->current_state, keysym, mod)] = 0;
                    if (ctx_fsm->repeat_keysym == keysym &&
                        ctx_fsm->handle_repeat[FSM_2D_MODE(
                            ctx_fsm->current_state, ctx_fsm->current_mode)] &&
                        ctx_fsm->current_mode != ctx_fsm->MODE_COMMAND) {
                        ctx_fsm->repeat_keysym = 0;
                        ctx_fsm->repeat_mod = 0;
                        ctx_fsm->repeating = 0;
                        // timeouts
                        ctx_fsm->timeout = 0;
                        ctx_fsm->timeout_state_index = 0;
                        ctx_fsm->timeout_start_us = 0;
                    }
                    goto war_label_cmd_done;
                }
                goto war_label_cmd_done;
            }
            if (!pressed) {
                ctx_fsm->key_down[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = 0;
                ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = 0;
                // repeats
                if (ctx_fsm->repeat_keysym == keysym &&
                    ctx_fsm->handle_repeat[FSM_2D_MODE(
                        ctx_fsm->current_state, ctx_fsm->current_mode)] &&
                    ctx_fsm->current_mode != ctx_fsm->MODE_COMMAND) {
                    ctx_fsm->repeat_keysym = 0;
                    ctx_fsm->repeat_mod = 0;
                    ctx_fsm->repeating = 0;
                    // timeouts
                    ctx_fsm->timeout = 0;
                    ctx_fsm->timeout_state_index = 0;
                    ctx_fsm->timeout_start_us = 0;
                }
                uint64_t idx = ctx_fsm->next_state[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)];
                if (idx && ctx_fsm->handle_release[FSM_2D_MODE(
                               idx, ctx_fsm->current_mode)]) {
                    war_fsm_execute_command(env, ctx_fsm, idx);
                }
                goto war_label_cmd_done;
            }
            if (!ctx_fsm->key_down[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)]) {
                ctx_fsm->key_down[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = 1;
                ctx_fsm->key_last_event_us[FSM_3D_INDEX(
                    ctx_fsm->current_state, keysym, mod)] = ctx_misc->now;
            }
            uint64_t next_state_index = ctx_fsm->next_state[FSM_3D_INDEX(
                ctx_fsm->current_state, keysym, mod)];
            if (ctx_fsm->timeout &&
                ctx_fsm->next_state[FSM_3D_INDEX(
                    ctx_fsm->timeout_state_index, keysym, mod)]) {
                next_state_index = ctx_fsm->next_state[FSM_3D_INDEX(
                    ctx_fsm->timeout_state_index, keysym, mod)];
            }
            if (next_state_index == 0) {
                ctx_fsm->current_state = 0;
                // timeouts
                ctx_fsm->timeout = 0;
                ctx_fsm->timeout_state_index = 0;
                ctx_fsm->timeout_start_us = 0;
                goto war_label_cmd_done;
            }
            ctx_fsm->current_state = next_state_index;
            ctx_fsm->state_last_event_us = ctx_misc->now;
            size_t mode_idx =
                FSM_2D_MODE(ctx_fsm->current_state, ctx_fsm->current_mode);
            if (ctx_fsm->is_terminal[mode_idx] &&
                !ctx_fsm->is_prefix[mode_idx]) {
                uint64_t temp = ctx_fsm->current_state;
                ctx_fsm->current_state = 0;
                // repeats
                if ((ctx_fsm->current_mode != ctx_fsm->MODE_MIDI ||
                     (ctx_fsm->current_mode == ctx_fsm->MODE_MIDI)) &&
                    ctx_fsm->handle_repeat[mode_idx]) {
                    ctx_fsm->repeat_keysym = keysym;
                    ctx_fsm->repeat_mod = mod;
                    ctx_fsm->repeating = 0;
                }
                // timeouts
                if (keysym != XKB_KEY_Escape && mod != 0) {
                    ctx_fsm->timeout_state_index = 0;
                }
                ctx_fsm->timeout_start_us = 0;
                ctx_fsm->timeout = 0;
                war_fsm_execute_command(env, ctx_fsm, temp);
            } else if (ctx_fsm->is_terminal[mode_idx] &&
                       ctx_fsm->is_prefix[mode_idx]) {
                if (ctx_fsm->handle_timeout[mode_idx]) {
                    // repeats
                    ctx_fsm->repeat_keysym = 0;
                    ctx_fsm->repeat_mod = 0;
                    ctx_fsm->repeating = 0;
                    // timeouts
                    ctx_fsm->timeout_state_index = ctx_fsm->current_state;
                    ctx_fsm->timeout_start_us = ctx_misc->now;
                    ctx_fsm->timeout = 1;
                    ctx_fsm->current_state = 0;
                    goto war_label_cmd_done;
                }
                uint64_t temp = ctx_fsm->current_state;
                ctx_fsm->current_state = 0;
                // repeats
                if ((ctx_fsm->current_mode != ctx_fsm->MODE_MIDI ||
                     (ctx_fsm->current_mode == ctx_fsm->MODE_MIDI)) &&
                    ctx_fsm->handle_repeat[mode_idx]) {
                    ctx_fsm->repeat_keysym = keysym;
                    ctx_fsm->repeat_mod = mod;
                    ctx_fsm->repeating = 0;
                }
                // timeouts
                if (keysym != XKB_KEY_Escape && mod != 0) {
                    ctx_fsm->timeout_state_index = 0;
                }
                ctx_fsm->timeout_start_us = 0;
                ctx_fsm->timeout = 0;
                war_fsm_execute_command(env, ctx_fsm, temp);
            }
            goto war_label_cmd_done;
        }
        war_label_cmd_done: {
            ctx_misc->trinity = 1;
            if (ctx_fsm->goto_cmd_repeat_done) {
                ctx_fsm->goto_cmd_repeat_done = 0;
                goto war_label_cmd_repeat_done;
            }
            if (ctx_fsm->goto_cmd_timeout_done) {
                ctx_fsm->goto_cmd_timeout_done = 0;
                goto war_label_cmd_timeout_done;
            }
            goto war_label_wayland_done;
        }
        war_label_wl_keyboard_modifiers:
            // dump_bytes("war_label_wl_keyboard_modifiers event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            xkb_state_update_mask(
                ctx_fsm->xkb_state,
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4),
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4),
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 + 4),
                war_read_le32(msg_buffer + msg_buffer_offset + 8 + 4 + 4 + 4 +
                              4),
                0,
                0);
            goto war_label_wayland_done;
        war_label_wl_keyboard_repeat_info:
            // dump_bytes("war_label_wl_keyboard_repeat_info event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_enter:
            // dump_bytes("war_label_wl_pointer_enter event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_leave:
            // dump_bytes("war_label_wl_pointer_leave event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_motion:
            // dump_bytes("war_label_wl_pointer_motion event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            // ctx_misc->cursor_x = (float)(int32_t)war_read_le32(
            //                         msg_buffer + msg_buffer_offset + 12) /
            //                     256.0f * scale_factor;
            // ctx_misc->cursor_y = (float)(int32_t)war_read_le32(
            //                         msg_buffer + msg_buffer_offset + 16) /
            //                     256.0f * scale_factor;
            goto war_label_wayland_done;
        war_label_wl_pointer_button:
            // dump_bytes("war_label_wl_pointer_button event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            // switch (war_read_le32(msg_buffer + msg_buffer_offset + 8 + 12)) {
            // case 1:
            //    if (war_read_le32(msg_buffer + msg_buffer_offset + 8 + 8) ==
            //        BTN_LEFT) {
            //        if (((int)(ctx_misc->cursor_x /
            //                   ctx_new_vulkan->cell_width) -
            //             (int)ctx_misc->num_cols_for_line_numbers) < 0) {
            //            ctx_misc->cursor_pos_x = ctx_misc->left_col;
            //            break;
            //        }
            //        if ((((ctx_new_vulkan->physical_height -
            //               ctx_misc->cursor_y) /
            //              ctx_new_vulkan->cell_height) -
            //             ctx_misc->num_rows_for_status_bars) < 0) {
            //            ctx_misc->cursor_pos_y = ctx_misc->bottom_row;
            //            break;
            //        }
            //        ctx_misc->cursor_pos_x =
            //            (uint32_t)(ctx_misc->cursor_x /
            //                       ctx_new_vulkan->cell_width) -
            //            ctx_misc->num_cols_for_line_numbers +
            //            ctx_misc->left_col;
            //        ctx_misc->cursor_pos_y =
            //            (uint32_t)((ctx_new_vulkan->physical_height -
            //                        ctx_misc->cursor_y) /
            //                       ctx_new_vulkan->cell_height) -
            //            ctx_misc->num_rows_for_status_bars +
            //            ctx_misc->bottom_row;
            //        ctx_misc->cursor_blink_previous_us = ctx_misc->now;
            //        ctx_misc->cursor_blinking = 0;
            //        if (ctx_misc->cursor_pos_y > ctx_misc->max_row) {
            //            ctx_misc->cursor_pos_y = ctx_misc->max_row;
            //        }
            //        if (ctx_misc->cursor_pos_y > ctx_misc->top_row) {
            //            ctx_misc->cursor_pos_y = ctx_misc->top_row;
            //        }
            //        if (ctx_misc->cursor_pos_y < ctx_misc->bottom_row) {
            //            ctx_misc->cursor_pos_y = ctx_misc->bottom_row;
            //        }
            //        if (ctx_misc->cursor_pos_x > ctx_misc->max_col) {
            //            ctx_misc->cursor_pos_x = ctx_misc->max_col;
            //        }
            //        if (ctx_misc->cursor_pos_x > ctx_misc->right_col) {
            //            ctx_misc->cursor_pos_x = ctx_misc->right_col;
            //        }
            //        if (ctx_misc->cursor_pos_x < ctx_misc->left_col) {
            //            ctx_misc->cursor_pos_x = ctx_misc->left_col;
            //        }
            //    }
            //}
            goto war_label_wayland_done;
        war_label_wl_pointer_axis:
            // dump_bytes(
            //     "war_label_wl_pointer_axis event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_pointer_frame:
            // dump_bytes("war_label_wl_pointer_frame event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_axis_source:
            // dump_bytes("war_label_wl_pointer_axis_source event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_axis_stop:
            // dump_bytes("war_label_wl_pointer_axis_stop event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_axis_discrete:
            // dump_bytes("war_label_wl_pointer_axis_discrete event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_axis_value120:
            // dump_bytes("war_label_wl_pointer_axis_value120 event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_pointer_axis_relative_direction:
            // dump_bytes("war_label_wl_pointer_axis_relative_direction event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_touch_down:
            // dump_bytes(
            //     "war_label_wl_touch_down event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_touch_up:
            // dump_bytes(
            //     "war_label_wl_touch_up event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_touch_motion:
            // dump_bytes(
            //     "war_label_wl_touch_motion event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_touch_frame:
            // dump_bytes(
            //     "war_label_wl_touch_frame event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_touch_cancel:
            // dump_bytes(
            //     "war_label_wl_touch_cancel event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_touch_shape:
            // dump_bytes(
            //     "war_label_wl_touch_shape event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_touch_orientation:
            // dump_bytes("war_label_wl_touch_orientation event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_output_geometry:
            // dump_bytes("war_label_wl_output_geometry event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wl_output_mode:
            // dump_bytes(
            //     "war_label_wl_output_mode event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_output_done:
            // dump_bytes(
            //     "war_label_wl_output_done event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_output_scale:
            // dump_bytes(
            //     "war_label_wl_output_scale event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_output_name:
            // dump_bytes(
            //     "war_label_wl_output_name event", msg_buffer +
            //     msg_buffer_offset, size);
            goto war_label_wayland_done;
        war_label_wl_output_description:
            // dump_bytes("war_label_wl_output_description event",
            //            msg_buffer + msg_buffer_offset,
            //            size);
            goto war_label_wayland_done;
        war_label_wayland_default:
            // dump_bytes("default event", msg_buffer + msg_buffer_offset,
            // size);
            goto war_label_wayland_done;
        war_label_wayland_done:
            msg_buffer_offset += size;
            continue;
        }
        if (msg_buffer_offset > 0) {
            memmove(msg_buffer,
                    msg_buffer + msg_buffer_offset,
                    msg_buffer_size - msg_buffer_offset);
            msg_buffer_size -= msg_buffer_offset;
        }
    }
    goto war_label_wr;
}
war_label_end_wr:
    //-----------------------------------------------------------------------
    // CLEANUP END WAR
    //-----------------------------------------------------------------------
    close(ctx_new_vulkan->dmabuf_fd);
    ctx_new_vulkan->dmabuf_fd = -1;
    xkb_state_unref(ctx_fsm->xkb_state);
    xkb_context_unref(ctx_fsm->xkb_context);
    end("war_window_render");
    return 0;
}
static void war_play(void* userdata) {
    void** data = (void**)userdata;
    war_pipewire_context* ctx_pw = data[0];
    war_producer_consumer* pc_play = data[1];
    war_lua_context* ctx_lua = data[2];
    uint64_t* play_last_read_time = data[3];
    uint64_t* play_read_count = data[4];
    war_atomics* atomics = data[5];
    uint64_t now = war_get_monotonic_time_us();
    if (now - *play_last_read_time >= 1000000) {
        atomic_store(&atomics->play_reader_rate, (double)*play_read_count);
        *play_read_count = 0;
        *play_last_read_time = now;
    }
    (*play_read_count)++;
    struct pw_buffer* b = pw_stream_dequeue_buffer(ctx_pw->play_stream);
    if (!b) { return; }
    float* dst = (float*)b->buffer->datas[0].data;
    uint64_t bytes_needed = atomic_load(&ctx_lua->A_BYTES_NEEDED);
    uint64_t write_pos = pc_play->i_to_a;
    uint64_t read_pos = pc_play->i_from_a;
    int64_t available_bytes;
    if (write_pos >= read_pos) {
        available_bytes = write_pos - read_pos;
    } else {
        available_bytes = pc_play->size + write_pos - read_pos;
    }
    if (available_bytes >= bytes_needed) {
        uint64_t read_idx = read_pos;
        uint64_t samples_needed = bytes_needed / sizeof(float);
        for (uint64_t i = 0; i < samples_needed; i++) {
            dst[i] = ((float*)pc_play->to_a)[read_idx / 4];
            read_idx = (read_idx + 4) & (pc_play->size - 1);
        }
        pc_play->i_from_a = read_idx;
    } else {
        uint64_t samples_available = available_bytes / sizeof(float);
        uint64_t read_idx = read_pos;
        for (uint64_t i = 0; i < samples_available; i++) {
            dst[i] = ((float*)pc_play->to_a)[read_idx / 4];
            read_idx = (read_idx + 4) & (pc_play->size - 1);
        }
        pc_play->i_from_a = read_idx;
        memset(dst + samples_available, 0, bytes_needed - available_bytes);
    }
    b->buffer->datas[0].chunk->size = bytes_needed;
    pw_stream_queue_buffer(ctx_pw->play_stream, b);
}
static void war_capture(void* userdata) {
    void** data = (void**)userdata;
    war_pipewire_context* ctx_pw = data[0];
    war_producer_consumer* pc_capture = data[1];
    war_lua_context* ctx_lua = data[2];
    uint64_t* capture_last_write_time = data[3];
    uint64_t* capture_write_count = data[4];
    war_atomics* atomics = data[5];
    uint64_t now = war_get_monotonic_time_us();
    if (now - *capture_last_write_time >= 1000000) {
        atomic_store(&atomics->capture_writer_rate,
                     (double)*capture_write_count);
        // call_king_terry("capture_writer_rate: %.2f Hz",
        //              atomic_load(&atomics->capture_writer_rate));
        *capture_write_count = 0;
        *capture_last_write_time = now;
    }
    (*capture_write_count)++;
    struct pw_buffer* b = pw_stream_dequeue_buffer(ctx_pw->capture_stream);
    if (!b) { return; }
    float* src = (float*)b->buffer->datas[0].data;
    uint64_t available_bytes = b->buffer->datas[0].chunk->size;
    uint64_t write_pos = pc_capture->i_to_a;
    uint64_t read_pos = pc_capture->i_from_a;
    int64_t space_available;
    if (read_pos > write_pos) {
        space_available = read_pos - write_pos;
    } else {
        space_available = pc_capture->size + read_pos - write_pos;
    }
    uint64_t bytes_to_write = available_bytes;
    if (bytes_to_write > space_available - 4) {
        bytes_to_write = space_available - 4;
    }
    if (bytes_to_write > 0) {
        uint64_t write_idx = write_pos;
        uint64_t samples_to_write = bytes_to_write / sizeof(float);
        for (uint64_t i = 0; i < samples_to_write; i++) {
            ((float*)pc_capture->to_a)[write_idx / 4] = src[i];
            write_idx = (write_idx + 4) & (pc_capture->size - 1);
        }
        pc_capture->i_to_a = write_idx;
    }
    pw_stream_queue_buffer(ctx_pw->capture_stream, b);
}
//-----------------------------------------------------------------------------
// THREAD AUDIO
//-----------------------------------------------------------------------------
void* war_audio(void* args) {
    header("war_audio");
    void** args_ptrs = (void**)args;
    war_producer_consumer* pc_control = args_ptrs[0];
    war_atomics* atomics = args_ptrs[1];
    war_pool* pool_a = args_ptrs[2];
    war_lua_context* ctx_lua = args_ptrs[3];
    war_producer_consumer* pc_play = args_ptrs[4];
    war_producer_consumer* pc_capture = args_ptrs[5];
    pool_a->pool_alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    pool_a->pool_size =
        war_get_pool_a_size(pool_a, ctx_lua, "src/lua/war_main.lua");
    pool_a->pool_size =
        ((pool_a->pool_size * 3) + ((pool_a->pool_alignment) - 1)) &
        ~((pool_a->pool_alignment) - 1);
    int pool_result = posix_memalign(
        &pool_a->pool, pool_a->pool_alignment, pool_a->pool_size);
    memset(pool_a->pool, 0, pool_a->pool_size);
    assert(pool_result == 0 && pool_a->pool);
    pool_a->pool_ptr = (uint8_t*)pool_a->pool;
    struct sched_param param = {
        .sched_priority = atomic_load(&ctx_lua->A_SCHED_FIFO_PRIORITY)};
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        // call_king_terry("AUDIO THREAD ERROR WITH SCHEDULING FIFO");
        // perror("pthread_setschedparam");
    }
    //-------------------------------------------------------------------------
    //  PIPEWIRE
    //-------------------------------------------------------------------------
    war_pipewire_context* ctx_pw =
        war_pool_alloc(pool_a, sizeof(war_pipewire_context));
    ctx_pw->play_data = war_pool_alloc(
        pool_a, sizeof(void*) * atomic_load(&ctx_lua->A_PLAY_DATA_SIZE));
    // play_data
    ctx_pw->play_data[0] = ctx_pw;
    ctx_pw->play_data[1] = pc_play;
    ctx_pw->play_data[2] = ctx_lua;
    uint64_t* play_last_read_time = war_pool_alloc(pool_a, sizeof(uint64_t));
    uint64_t* play_read_count = war_pool_alloc(pool_a, sizeof(uint64_t));
    *play_last_read_time = 0;
    *play_read_count = 0;
    ctx_pw->play_data[3] = play_last_read_time;
    ctx_pw->play_data[4] = play_read_count;
    ctx_pw->play_data[5] = atomics;
    ctx_pw->capture_data = war_pool_alloc(
        pool_a, sizeof(void*) * atomic_load(&ctx_lua->A_CAPTURE_DATA_SIZE));
    ctx_pw->capture_data[0] = ctx_pw;
    ctx_pw->capture_data[1] = pc_capture;
    ctx_pw->capture_data[2] = ctx_lua;
    uint64_t* capture_last_write_time =
        war_pool_alloc(pool_a, sizeof(uint64_t));
    uint64_t* capture_write_count = war_pool_alloc(pool_a, sizeof(uint64_t));
    *capture_last_write_time = 0;
    *capture_write_count = 0;
    ctx_pw->capture_data[3] = capture_last_write_time;
    ctx_pw->capture_data[4] = capture_write_count;
    ctx_pw->capture_data[5] = atomics;
    pw_init(NULL, NULL);
    ctx_pw->loop = pw_loop_new(NULL);
    ctx_pw->play_info = (struct spa_audio_info_raw){
        .format = SPA_AUDIO_FORMAT_F32_LE,
        .rate = atomic_load(&ctx_lua->A_SAMPLE_RATE),
        .channels = atomic_load(&ctx_lua->A_CHANNEL_COUNT),
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
    };
    ctx_pw->play_builder_data = war_pool_alloc(
        pool_a, sizeof(uint8_t) * atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    spa_pod_builder_init(&ctx_pw->play_builder,
                         ctx_pw->play_builder_data,
                         sizeof(uint8_t) *
                             atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    ctx_pw->play_params = spa_format_audio_raw_build(
        &ctx_pw->play_builder, SPA_PARAM_EnumFormat, &ctx_pw->play_info);
    ctx_pw->play_events = (struct pw_stream_events){
        .version = PW_VERSION_STREAM_EVENTS,
        .process = war_play,
    };
    // ctx_pw->play_properties = pw_properties_new(...
    ctx_pw->play_stream = pw_stream_new_simple(ctx_pw->loop,
                                               "WAR_play",
                                               NULL,
                                               &ctx_pw->play_events,
                                               ctx_pw->play_data);
    if (!ctx_pw->play_stream) {
        // call_king_terry("play_stream init issue");
    }
    int pw_stream_result = pw_stream_connect(ctx_pw->play_stream,
                                             PW_DIRECTION_OUTPUT,
                                             PW_ID_ANY,
                                             PW_STREAM_FLAG_AUTOCONNECT |
                                                 PW_STREAM_FLAG_MAP_BUFFERS |
                                                 PW_STREAM_FLAG_RT_PROCESS,
                                             &ctx_pw->play_params,
                                             1);
    if (pw_stream_result < 0) {
        // call_king_terry("play stream connection error");
    }
    ctx_pw->capture_info = (struct spa_audio_info_raw){
        .format = SPA_AUDIO_FORMAT_F32_LE,
        .rate = atomic_load(&ctx_lua->A_SAMPLE_RATE),
        .channels = atomic_load(&ctx_lua->A_CHANNEL_COUNT),
        .position = {SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR},
    };
    ctx_pw->capture_builder_data = war_pool_alloc(
        pool_a, sizeof(uint8_t) * atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    spa_pod_builder_init(&ctx_pw->capture_builder,
                         ctx_pw->capture_builder_data,
                         sizeof(uint8_t) *
                             atomic_load(&ctx_lua->A_BUILDER_DATA_SIZE));
    ctx_pw->capture_params = spa_format_audio_raw_build(
        &ctx_pw->capture_builder, SPA_PARAM_EnumFormat, &ctx_pw->capture_info);
    ctx_pw->capture_events = (struct pw_stream_events){
        .version = PW_VERSION_STREAM_EVENTS,
        .process = war_capture,
    };
    ctx_pw->capture_stream = pw_stream_new_simple(ctx_pw->loop,
                                                  "WAR_capture",
                                                  NULL,
                                                  &ctx_pw->capture_events,
                                                  ctx_pw->capture_data);
    if (!ctx_pw->capture_stream) {
        // call_king_terry("capture_stream init issue");
    }
    pw_stream_result = pw_stream_connect(ctx_pw->capture_stream,
                                         PW_DIRECTION_INPUT,
                                         PW_ID_ANY,
                                         PW_STREAM_FLAG_AUTOCONNECT |
                                             PW_STREAM_FLAG_MAP_BUFFERS |
                                             PW_STREAM_FLAG_RT_PROCESS,
                                         &ctx_pw->capture_params,
                                         1);
    if (pw_stream_result < 0) {
        // call_king_terry("capture stream connection error");
    }
    while (pw_stream_get_state(ctx_pw->capture_stream, NULL) !=
           PW_STREAM_STATE_PAUSED) {
        pw_loop_iterate(ctx_pw->loop, 1);
        struct timespec ts = {0, 500000};
        nanosleep(&ts, NULL);
    }
    //-------------------------------------------------------------------------
    // PC CONTROL
    //-------------------------------------------------------------------------
    uint32_t header;
    uint32_t size;
    uint8_t* control_payload =
        war_pool_alloc(pool_a, sizeof(uint8_t) * pc_control->size);
    uint8_t* tmp_control_payload =
        war_pool_alloc(pool_a, sizeof(uint8_t) * pc_control->size);
    void** pc_control_cmd = war_pool_alloc(
        pool_a, sizeof(void*) * atomic_load(&ctx_lua->CMD_COUNT));
    pc_control_cmd[CONTROL_END_WAR] = &&war_label_end_a;
    atomic_store(&atomics->start_war, 1);
    struct timespec ts = {0, 500000}; // 0.5 ms
    //-------------------------------------------------------------------------
    // AUDIO LOOP
    //-------------------------------------------------------------------------
war_label_a: {
    if (war_pc_from_wr(pc_control, &header, &size, control_payload)) {
        goto* pc_control_cmd[header];
    }
    goto war_label_pc_a_done;
}
war_label_pc_a_done: {
    pw_loop_iterate(ctx_pw->loop, 0);
    goto war_label_a;
}
war_label_end_a: {
    war_pc_to_wr(pc_control, CONTROL_END_WAR, 0, NULL);
    pw_stream_destroy(ctx_pw->play_stream);
    pw_stream_destroy(ctx_pw->capture_stream);
    pw_loop_destroy(ctx_pw->loop);
    pw_deinit();
    end("war_audio");
    return 0;
}
}
