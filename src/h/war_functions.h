//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_functions.h
//-----------------------------------------------------------------------------

#ifndef WAR_FUNCTIONS_H
#define WAR_FUNCTIONS_H

#include "war_data.h"
#include "war_debug_macros.h"

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <float.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

// COMMENT OPTIMIZE: Duff's Device + SIMD (intrinsics)

#define ALIGN32(p) (uint8_t*)(((uintptr_t)(p) + 31) & ~((uintptr_t)31))

#define obj_op_index(obj, op) ((obj) * max_opcodes + (op))

#define STR(x) #x

#define FSM_3D_INDEX(state, keysym, mod)                                       \
    ((state) * (ctx_fsm->keysym_count * ctx_fsm->mod_count) +                  \
     (keysym) * ctx_fsm->mod_count + (mod))
// For: next_state, key_down, key_last_event_us

#define FSM_2D_MODE(state, mode) ((state) * ctx_fsm->mode_count + (mode))
// For: is_terminal, is_prefix, handle_release, handle_repeat, handle_timeout,
// function, type

#define FSM_3D_NAME(state, mode)                                               \
    ((state) * (ctx_fsm->mode_count * ctx_fsm->name_limit) +                   \
     (mode) * ctx_fsm->name_limit)
// For: name (when accessing the start of a name string)

static inline int war_load_lua_config(war_lua_context* ctx_lua,
                                      const char* lua_file) {
    if (luaL_dofile(ctx_lua->L, lua_file) != LUA_OK) {
        call_king_terry("Lua error: %s", lua_tostring(ctx_lua->L, -1));
        return -1;
    }

    lua_getglobal(ctx_lua->L, "ctx_lua");
    if (!lua_istable(ctx_lua->L, -1)) {
        call_king_terry("ctx_lua not a table");
        return -1;
    }

#define LOAD_INT(field)                                                        \
    lua_getfield(ctx_lua->L, -1, #field);                                      \
    if (lua_type(ctx_lua->L, -1) == LUA_TNUMBER) {                             \
        ctx_lua->field = (int)lua_tointeger(ctx_lua->L, -1);                   \
        call_king_terry("ctx_lua: %s = %d", #field, ctx_lua->field);           \
    }                                                                          \
    lua_pop(ctx_lua->L, 1);

    // audio
    LOAD_INT(A_SAMPLE_RATE)
    LOAD_INT(A_CHANNEL_COUNT)
    LOAD_INT(A_NOTE_COUNT)
    LOAD_INT(A_LAYER_COUNT)
    LOAD_INT(A_LAYERS_IN_RAM)
    LOAD_INT(A_PLAY_DATA_SIZE)
    LOAD_INT(A_CAPTURE_DATA_SIZE)
    LOAD_INT(A_BASE_FREQUENCY)
    LOAD_INT(A_BASE_NOTE)
    LOAD_INT(A_BYTES_NEEDED)
    LOAD_INT(A_EDO)
    LOAD_INT(A_NOTES_MAX)
    LOAD_INT(CACHE_FILE_CAPACITY)
    LOAD_INT(CONFIG_PATH_MAX)
    LOAD_INT(A_WARMUP_FRAMES_FACTOR)
    LOAD_INT(ROLL_POSITION_X_Y)
    LOAD_INT(A_SCHED_FIFO_PRIORITY)
    // window render
    LOAD_INT(WR_VIEWS_SAVED)
    LOAD_INT(WR_WARPOON_TEXT_COLS)
    LOAD_INT(WR_STATES)
    LOAD_INT(WR_SEQUENCE_COUNT)
    LOAD_INT(WR_SEQUENCE_LENGTH_MAX)
    LOAD_INT(WR_FN_NAME_LIMIT)
    LOAD_INT(WR_MODE_COUNT)
    LOAD_INT(WR_KEYSYM_COUNT)
    LOAD_INT(WR_CALLBACK_SIZE)
    LOAD_INT(WR_MOD_COUNT)
    LOAD_INT(WR_NOTE_QUADS_MAX)
    LOAD_INT(WR_STATUS_BAR_COLS_MAX)
    LOAD_INT(WR_TEXT_QUADS_MAX)
    LOAD_INT(WR_QUADS_MAX)
    LOAD_INT(WR_WAYLAND_MSG_BUFFER_SIZE)
    LOAD_INT(WR_WAYLAND_MAX_OBJECTS)
    LOAD_INT(WR_WAYLAND_MAX_OP_CODES)
    LOAD_INT(WR_UNDO_NODES_MAX)
    LOAD_INT(WR_TIMESTAMP_LENGTH_MAX)
    LOAD_INT(WR_CURSOR_BLINK_DURATION_US)
    LOAD_INT(WR_REPEAT_DELAY_US)
    LOAD_INT(WR_REPEAT_RATE_US)
    LOAD_INT(WR_UNDO_NOTES_BATCH_MAX)
    LOAD_INT(WR_INPUT_SEQUENCE_LENGTH_MAX)
    // vk
    LOAD_INT(VK_ATLAS_HEIGHT)
    LOAD_INT(VK_ATLAS_WIDTH)
    LOAD_INT(VK_GLYPH_COUNT)
    LOAD_INT(VK_MAX_FRAMES)
    LOAD_INT(VK_ALIGNMENT)
    // nsgt
    LOAD_INT(NSGT_BIN_CAPACITY)
    LOAD_INT(NSGT_FRAME_CAPACITY)
    LOAD_INT(NSGT_FREQUENCY_MIN)
    LOAD_INT(NSGT_FREQUENCY_MAX)
    LOAD_INT(NSGT_WINDOW_LENGTH_MIN)
    LOAD_INT(NSGT_RESOURCE_COUNT)
    LOAD_INT(NSGT_DESCRIPTOR_SET_COUNT)
    LOAD_INT(NSGT_SHADER_COUNT)
    LOAD_INT(NSGT_PIPELINE_COUNT)
    LOAD_INT(NSGT_GRAPHICS_FPS)
    LOAD_INT(NSGT_GROUPS)
    LOAD_INT(CACHE_NSGT_CAPACITY)
    LOAD_INT(NEW_VULKAN_RESOURCE_COUNT)
    LOAD_INT(NEW_VULKAN_DESCRIPTOR_SET_COUNT)
    LOAD_INT(NEW_VULKAN_SHADER_COUNT)
    LOAD_INT(NEW_VULKAN_PIPELINE_COUNT)
    LOAD_INT(NEW_VULKAN_GROUPS)
    LOAD_INT(NEW_VULKAN_NOTE_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_TEXT_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_LINE_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_CURSOR_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_HUD_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_HUD_CURSOR_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_HUD_TEXT_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_HUD_LINE_INSTANCE_MAX)
    LOAD_INT(NEW_VULKAN_ATLAS_WIDTH)
    LOAD_INT(NEW_VULKAN_ATLAS_HEIGHT)
    LOAD_INT(NEW_VULKAN_FONT_PIXEL_HEIGHT)
    LOAD_INT(NEW_VULKAN_GLYPH_COUNT)
    LOAD_INT(NEW_VULKAN_SDF_SCALE)
    LOAD_INT(NEW_VULKAN_SDF_PADDING)
    LOAD_INT(NEW_VULKAN_BUFFER_MAX)
    // hud
    LOAD_INT(HUD_COUNT)
    LOAD_INT(HUD_STATUS_BOTTOM_INSTANCE_MAX)
    LOAD_INT(HUD_STATUS_TOP_INSTANCE_MAX)
    LOAD_INT(HUD_STATUS_MIDDLE_INSTANCE_MAX)
    LOAD_INT(HUD_LINE_NUMBERS_INSTANCE_MAX)
    LOAD_INT(HUD_PIANO_INSTANCE_MAX)
    LOAD_INT(HUD_EXPLORE_INSTANCE_MAX)
    // hud line
    LOAD_INT(HUD_LINE_COUNT)
    LOAD_INT(HUD_LINE_PIANO_INSTANCE_MAX)
    // hud text
    LOAD_INT(HUD_TEXT_COUNT)
    LOAD_INT(HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX)
    LOAD_INT(HUD_TEXT_STATUS_TOP_INSTANCE_MAX)
    LOAD_INT(HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX)
    LOAD_INT(HUD_TEXT_PIANO_INSTANCE_MAX)
    LOAD_INT(HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX)
    LOAD_INT(HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX)
    LOAD_INT(HUD_TEXT_EXPLORE_INSTANCE_MAX)
    LOAD_INT(HUD_TEXT_ERROR_INSTANCE_MAX)
    // hud cursor
    LOAD_INT(HUD_CURSOR_COUNT)
    LOAD_INT(HUD_CURSOR_DEFAULT_INSTANCE_MAX)
    // cursor
    LOAD_INT(CURSOR_COUNT)
    LOAD_INT(CURSOR_DEFAULT_INSTANCE_MAX)
    // line
    LOAD_INT(LINE_COUNT)
    LOAD_INT(LINE_CELL_INSTANCE_MAX)
    LOAD_INT(LINE_BPM_INSTANCE_MAX)
    // text
    LOAD_INT(TEXT_COUNT)
    LOAD_INT(TEXT_STATUS_BOTTOM_INSTANCE_MAX)
    LOAD_INT(TEXT_STATUS_TOP_INSTANCE_MAX)
    LOAD_INT(TEXT_STATUS_MIDDLE_INSTANCE_MAX)
    LOAD_INT(TEXT_PIANO_INSTANCE_MAX)
    LOAD_INT(TEXT_LINE_NUMBERS_INSTANCE_MAX)
    LOAD_INT(TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX)
    LOAD_INT(TEXT_EXPLORE_INSTANCE_MAX)
    LOAD_INT(TEXT_ERROR_INSTANCE_MAX)
    // sequence
    LOAD_INT(NOTE_COUNT)
    LOAD_INT(NOTE_GRID_INSTANCE_MAX)
    // nsgt visual
    LOAD_INT(VK_NSGT_VISUAL_QUAD_CAPACITY)
    LOAD_INT(VK_NSGT_VISUAL_RESOURCE_COUNT)
    // pool
    LOAD_INT(POOL_ALIGNMENT)
    // cmd
    LOAD_INT(CMD_COUNT)
    // pc
    LOAD_INT(PC_CONTROL_BUFFER_SIZE)
    LOAD_INT(PC_PLAY_BUFFER_SIZE)
    LOAD_INT(PC_CAPTURE_BUFFER_SIZE)
    LOAD_INT(A_BUILDER_DATA_SIZE)

#undef LOAD_INT

#define LOAD_FLOAT(field)                                                      \
    lua_getfield(ctx_lua->L, -1, #field);                                      \
    if (lua_type(ctx_lua->L, -1) == LUA_TNUMBER) {                             \
        ctx_lua->field = (float)lua_tonumber(ctx_lua->L, -1);                  \
        call_king_terry("ctx_lua: %s = %f", #field, ctx_lua->field);           \
    }                                                                          \
    lua_pop(ctx_lua->L, 1);

    LOAD_FLOAT(A_DEFAULT_ATTACK)
    LOAD_FLOAT(A_DEFAULT_SUSTAIN)
    LOAD_FLOAT(A_DEFAULT_RELEASE)
    LOAD_FLOAT(A_DEFAULT_GAIN)
    LOAD_FLOAT(VK_FONT_PIXEL_HEIGHT)
    LOAD_FLOAT(DEFAULT_BOLD_TEXT_THICKNESS)
    LOAD_FLOAT(DEFAULT_BOLD_TEXT_FEATHER)
    LOAD_FLOAT(DEFAULT_ALPHA_SCALE)
    LOAD_FLOAT(DEFAULT_CURSOR_ALPHA_SCALE)
    LOAD_FLOAT(DEFAULT_PLAYBACK_BAR_THICKNESS)
    LOAD_FLOAT(WR_CAPTURE_THRESHOLD)
    LOAD_FLOAT(DEFAULT_TEXT_FEATHER)
    LOAD_FLOAT(DEFAULT_TEXT_THICKNESS)
    LOAD_FLOAT(WINDOWED_TEXT_FEATHER)
    LOAD_FLOAT(WINDOWED_TEXT_THICKNESS)
    LOAD_FLOAT(DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE)
    LOAD_FLOAT(DEFAULT_WINDOWED_ALPHA_SCALE)
    LOAD_FLOAT(WR_COLOR_STEP)
    LOAD_FLOAT(NEW_VULKAN_SDF_RANGE)
    LOAD_FLOAT(NEW_VULKAN_SDF_LARGE)
    // vk nsgt
    LOAD_FLOAT(NSGT_ALPHA)
    LOAD_FLOAT(NSGT_SHAPE_FACTOR)

#undef LOAD_FLOAT

#define LOAD_DOUBLE(field)                                                     \
    lua_getfield(ctx_lua->L, -1, #field);                                      \
    if (lua_type(ctx_lua->L, -1) == LUA_TNUMBER) {                             \
        ctx_lua->field = (double)lua_tonumber(ctx_lua->L, -1);                 \
        call_king_terry("ctx_lua: %s = %f", #field, ctx_lua->field);           \
    }                                                                          \
    lua_pop(ctx_lua->L, 1);

    LOAD_DOUBLE(A_DEFAULT_COLUMNS_PER_BEAT)
    LOAD_DOUBLE(A_TARGET_SAMPLES_FACTOR)
    LOAD_DOUBLE(A_BPM)
    LOAD_DOUBLE(BPM_SECONDS_PER_CELL)
    LOAD_DOUBLE(SUBDIVISION_SECONDS_PER_CELL)
    LOAD_DOUBLE(A_SAMPLE_DURATION)
    LOAD_DOUBLE(WR_FPS)
    LOAD_DOUBLE(WR_PLAY_CALLBACK_FPS)
    LOAD_DOUBLE(WR_CAPTURE_CALLBACK_FPS)

#undef LOAD_DOUBLE

    // #define LOAD_STRING(field) \
    //     lua_getfield(ctx_lua->L, -1, #field); \
    //     if (lua_isstring(ctx_lua->L, -1)) { \
    //         char* str = strdup(lua_tostring(ctx_lua->L, -1)); \
    //         if (str) { \
    //             atomic_store(&ctx_lua->field, str); \
    //             call_king_terry( \
    //                 "ctx_lua: %s = %s", #field,
    //                 atomic_load(&ctx_lua->field));     \
    //         } \
    //     } \ lua_pop(ctx_lua->L, 1);
    //
    //     LOAD_STRING(CWD)

    // #undef LOAD_STRING
    return 0;
}

static inline size_t war_get_pool_a_size(war_pool* pool,
                                         war_lua_context* ctx_lua,
                                         const char* lua_file) {
    lua_getglobal(ctx_lua->L, "pool_a");
    if (!lua_istable(ctx_lua->L, -1)) {
        call_king_terry("pool_a not a table");
        return 0;
    }

    size_t total_size = 0;

    lua_pushnil(ctx_lua->L);
    while (lua_next(ctx_lua->L, -2) != 0) { // iterate pool_a entries
        if (lua_istable(ctx_lua->L, -1)) {
            lua_getfield(ctx_lua->L, -1, "type");
            const char* type = lua_tostring(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

            lua_getfield(ctx_lua->L, -1, "count");
            size_t count = (size_t)lua_tointeger(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

            size_t type_size = 0;

            if (strcmp(type, "uint8_t") == 0)
                type_size = sizeof(uint8_t);
            else if (strcmp(type, "uint64_t") == 0)
                type_size = sizeof(uint64_t);
            else if (strcmp(type, "int16_t") == 0)
                type_size = sizeof(int16_t);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "float") == 0)
                type_size = sizeof(float);
            else if (strcmp(type, "uint32_t") == 0)
                type_size = sizeof(uint32_t);
            else if (strcmp(type, "int32_t") == 0)
                type_size = sizeof(int32_t);
            else if (strcmp(type, "void*") == 0)
                type_size = sizeof(void*);
            else if (strcmp(type, "char*") == 0)
                type_size = sizeof(char*);
            else if (strcmp(type, "char") == 0)
                type_size = sizeof(char);
            else if (strcmp(type, "war_pipewire_context") == 0)
                type_size = sizeof(war_pipewire_context);
            else if (strcmp(type, "ssize_t") == 0)
                type_size = sizeof(ssize_t);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "int16_t*") == 0)
                type_size = sizeof(int16_t*);
            else if (strcmp(type, "int16_t**") == 0)
                type_size = sizeof(int16_t**);
            else if (strcmp(type, "uint32_t") == 0)
                type_size = sizeof(uint32_t);
            else if (strcmp(type, "int") == 0)
                type_size = sizeof(int);
            else if (strcmp(type, "size_t") == 0)
                type_size = sizeof(size_t);
            else if (strcmp(type, "war_riff_header") == 0)
                type_size = sizeof(war_riff_header);
            else if (strcmp(type, "war_fmt_chunk") == 0)
                type_size = sizeof(war_fmt_chunk);
            else if (strcmp(type, "war_data_chunk") == 0)
                type_size = sizeof(war_data_chunk);
            else if (strcmp(type, "bool") == 0) {
                type_size = sizeof(bool);
            } else {
                call_king_terry("Unknown pool_a type: %s", type);
                type_size = 0;
            }

            total_size += type_size * count;
        }
        lua_pop(ctx_lua->L, 1); // pop value
    }

    // align total_size to pool alignment
    size_t alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    total_size = (total_size + alignment - 1) & ~(alignment - 1);
    call_king_terry("pool_a size: %zu", total_size);
    return total_size;
}

static inline size_t war_get_pool_wr_size(war_pool* pool,
                                          war_lua_context* ctx_lua,
                                          const char* lua_file) {
    lua_getglobal(ctx_lua->L, "pool_wr");
    if (!lua_istable(ctx_lua->L, -1)) {
        call_king_terry("pool_wr not a table");
        return 0;
    }

    size_t total_size = 0;

    lua_pushnil(ctx_lua->L);
    while (lua_next(ctx_lua->L, -2) != 0) { // iterate pool_wr entries
        if (lua_istable(ctx_lua->L, -1)) {
            lua_getfield(ctx_lua->L, -1, "type");
            const char* type = lua_tostring(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

            lua_getfield(ctx_lua->L, -1, "count");
            size_t count = (size_t)lua_tointeger(ctx_lua->L, -1);
            lua_pop(ctx_lua->L, 1);

            size_t type_size = 0;

            if (strcmp(type, "uint8_t") == 0)
                type_size = sizeof(uint8_t);
            else if (strcmp(type, "uint16_t") == 0)
                type_size = sizeof(uint16_t);
            else if (strcmp(type, "uint32_t") == 0)
                type_size = sizeof(uint32_t);
            else if (strcmp(type, "timespec") == 0)
                type_size = sizeof(struct timespec);
            else if (strcmp(type, "struct timespec") == 0)
                type_size = sizeof(struct timespec);
            else if (strcmp(type, "uint64_t") == 0)
                type_size = sizeof(uint64_t);
            else if (strcmp(type, "int16_t") == 0)
                type_size = sizeof(int16_t);
            else if (strcmp(type, "int32_t") == 0)
                type_size = sizeof(int32_t);
            else if (strcmp(type, "int") == 0)
                type_size = sizeof(int);
            else if (strcmp(type, "float") == 0)
                type_size = sizeof(float);
            else if (strcmp(type, "float*") == 0)
                type_size = sizeof(float*);
            else if (strcmp(type, "war_hud_context") == 0)
                type_size = sizeof(war_hud_context);
            else if (strcmp(type, "war_hud_text_context") == 0)
                type_size = sizeof(war_hud_text_context);
            else if (strcmp(type, "war_new_vulkan_note_push_constant") == 0)
                type_size = sizeof(war_new_vulkan_note_push_constant);
            else if (strcmp(type, "war_new_vulkan_text_push_constant") == 0)
                type_size = sizeof(war_new_vulkan_text_push_constant);
            else if (strcmp(type, "war_new_vulkan_line_push_constant") == 0)
                type_size = sizeof(war_new_vulkan_line_push_constant);
            else if (strcmp(type, "war_new_vulkan_cursor_push_constant") == 0)
                type_size = sizeof(war_new_vulkan_cursor_push_constant);
            else if (strcmp(type, "war_new_vulkan_hud_push_constant") == 0)
                type_size = sizeof(war_new_vulkan_hud_push_constant);
            else if (strcmp(type, "war_new_vulkan_hud_cursor_push_constant") ==
                     0)
                type_size = sizeof(war_new_vulkan_hud_cursor_push_constant);
            else if (strcmp(type, "war_new_vulkan_hud_text_push_constant") == 0)
                type_size = sizeof(war_new_vulkan_hud_text_push_constant);
            else if (strcmp(type, "war_new_vulkan_hud_line_push_constant") == 0)
                type_size = sizeof(war_new_vulkan_hud_line_push_constant);
            else if (strcmp(type, "war_text_context") == 0)
                type_size = sizeof(war_text_context);
            else if (strcmp(type, "war_line_context") == 0)
                type_size = sizeof(war_line_context);
            else if (strcmp(type, "double") == 0)
                type_size = sizeof(double);
            else if (strcmp(type, "void*") == 0)
                type_size = sizeof(void*);
            else if (strcmp(type, "war_cursor_context") == 0)
                type_size = sizeof(war_cursor_context);
            else if (strcmp(type, "war_misc_context") == 0)
                type_size = sizeof(war_misc_context);
            else if (strcmp(type, "VkPipelineDepthStencilStateCreateInfo") == 0)
                type_size = sizeof(VkPipelineDepthStencilStateCreateInfo);
            else if (strcmp(type, "VkPipelineColorBlendStateCreateInfo") == 0)
                type_size = sizeof(VkPipelineColorBlendStateCreateInfo);
            else if (strcmp(type, "VkPipelineColorBlendAttachmentState") == 0)
                type_size = sizeof(VkPipelineColorBlendAttachmentState);
            else if (strcmp(type, "VkFence*") == 0)
                type_size = sizeof(VkFence*);
            else if (strcmp(type, "VkFence") == 0)
                type_size = sizeof(VkFence);
            else if (strcmp(type, "VkDescriptorSet") == 0)
                type_size = sizeof(VkDescriptorSet);
            else if (strcmp(type, "war_glyph_info*") == 0)
                type_size = sizeof(war_glyph_info*);
            else if (strcmp(type, "war_glyph_info") == 0)
                type_size = sizeof(war_glyph_info);
            else if (strcmp(type, "char") == 0)
                type_size = sizeof(char);
            else if (strcmp(type, "char*") == 0)
                type_size = sizeof(char*);
            else if (strcmp(type, "bool") == 0)
                type_size = sizeof(bool);
            else if (strcmp(type, "war_undo_node*") == 0)
                type_size = sizeof(war_undo_node*);
            else if (strcmp(type, "war_undo_node") == 0)
                type_size = sizeof(war_undo_node);
            else if (strcmp(type, "war_fsm_context") == 0)
                type_size = sizeof(war_fsm_context);
            else if (strcmp(type, "war_function_union") == 0)
                type_size = sizeof(war_function_union);
            else if (strcmp(type, "void (*)(war_env*)") == 0)
                type_size = sizeof(void (*)(war_env*));
            else if (strcmp(type, "war_capture_context") == 0)
                type_size = sizeof(war_capture_context);
            else if (strcmp(type, "VkMappedMemoryRange") == 0)
                type_size = sizeof(VkMappedMemoryRange);
            else if (strcmp(type, "VkBufferMemoryBarrier") == 0)
                type_size = sizeof(VkBufferMemoryBarrier);
            else if (strcmp(type, "VkImageMemoryBarrier") == 0)
                type_size = sizeof(VkImageMemoryBarrier);
            else if (strcmp(type, "VkSampler") == 0)
                type_size = sizeof(VkSampler);
            // else if (strcmp(type, "war_command_context") == 0)
            //     type_size = sizeof(war_command_context);
            else if (strcmp(type, "war_play_context") == 0)
                type_size = sizeof(war_play_context);
            else if (strcmp(type, "war_nsgt_context") == 0)
                type_size = sizeof(war_nsgt_context);
            else if (strcmp(type, "war_new_vulkan_context") == 0)
                type_size = sizeof(war_new_vulkan_context);
            else if (strcmp(type, "VkViewport") == 0)
                type_size = sizeof(VkViewport);
            else if (strcmp(type, "VkRect2D") == 0)
                type_size = sizeof(VkRect2D);
            else if (strcmp(type, "VkVertexInputBindingDescription") == 0)
                type_size = sizeof(VkVertexInputBindingDescription);
            else if (strcmp(type, "VkVertexInputAttributeDescription") == 0)
                type_size = sizeof(VkVertexInputAttributeDescription);
            else if (strcmp(type, "VkVertexInputAttributeDescription*") == 0)
                type_size = sizeof(VkVertexInputAttributeDescription*);
            else if (strcmp(type, "VkVertexInputRate") == 0)
                type_size = sizeof(VkVertexInputRate);
            else if (strcmp(type, "uint32_t*") == 0)
                type_size = sizeof(uint32_t*);
            else if (strcmp(type, "VkFormat*") == 0)
                type_size = sizeof(VkFormat*);
            else if (strcmp(type, "VkFormat") == 0)
                type_size = sizeof(VkFormat);
            else if (strcmp(type, "VkMemoryPropertyFlags") == 0)
                type_size = sizeof(VkMemoryPropertyFlags);
            else if (strcmp(type, "VkDescriptorBufferInfo") == 0)
                type_size = sizeof(VkDescriptorBufferInfo);
            else if (strcmp(type, "VkDescriptorImageInfo") == 0)
                type_size = sizeof(VkDescriptorImageInfo);
            else if (strcmp(type, "VkImage") == 0)
                type_size = sizeof(VkImage);
            else if (strcmp(type, "VkStructureType") == 0)
                type_size = sizeof(VkStructureType);
            else if (strcmp(type, "VkShaderStageFlags") == 0)
                type_size = sizeof(VkShaderStageFlags);
            else if (strcmp(type, "VkPipelineShaderStageCreateInfo") == 0)
                type_size = sizeof(VkPipelineShaderStageCreateInfo);
            else if (strcmp(type, "VkShaderStageFlagBits") == 0)
                type_size = sizeof(VkShaderStageFlagBits);
            else if (strcmp(type, "VkPipelineBindPoint") == 0)
                type_size = sizeof(VkPipelineBindPoint);
            else if (strcmp(type, "VkImageView") == 0)
                type_size = sizeof(VkImageView);
            else if (strcmp(type, "VkFormat") == 0)
                type_size = sizeof(VkFormat);
            else if (strcmp(type, "VkExtent3D") == 0)
                type_size = sizeof(VkExtent3D);
            else if (strcmp(type, "VkImageUsageFlags") == 0)
                type_size = sizeof(VkImageUsageFlags);
            else if (strcmp(type, "VkBufferUsageFlags") == 0)
                type_size = sizeof(VkBufferUsageFlags);
            else if (strcmp(type, "VkBuffer") == 0)
                type_size = sizeof(VkBuffer);
            else if (strcmp(type, "VkMemoryRequirements") == 0)
                type_size = sizeof(VkMemoryRequirements);
            else if (strcmp(type, "VkDeviceMemory") == 0)
                type_size = sizeof(VkDeviceMemory);
            else if (strcmp(type, "VkDeviceSize") == 0)
                type_size = sizeof(VkDeviceSize);
            else if (strcmp(type, "VkDescriptorSetLayoutBinding") == 0)
                type_size = sizeof(VkDescriptorSetLayoutBinding);
            else if (strcmp(type, "VkDescriptorSetLayout") == 0)
                type_size = sizeof(VkDescriptorSetLayout);
            else if (strcmp(type, "VkDescriptorType") == 0)
                type_size = sizeof(VkDescriptorType);
            else if (strcmp(type, "VkDescriptorPool") == 0)
                type_size = sizeof(VkDescriptorPool);
            else if (strcmp(type, "VkImageLayout") == 0)
                type_size = sizeof(VkImageLayout);
            else if (strcmp(type, "VkShaderModule") == 0)
                type_size = sizeof(VkShaderModule);
            else if (strcmp(type, "VkPipeline") == 0)
                type_size = sizeof(VkPipeline);
            else if (strcmp(type, "VkPipelineLayout") == 0)
                type_size = sizeof(VkPipelineLayout);
            else if (strcmp(type, "VkAccessFlags") == 0)
                type_size = sizeof(VkAccessFlags);
            else if (strcmp(type, "VkPipelineStageFlags") == 0)
                type_size = sizeof(VkPipelineStageFlags);
            else if (strcmp(type, "VkWriteDescriptorSet") == 0)
                type_size = sizeof(VkWriteDescriptorSet);
            else if (strcmp(type, "VkShaderStageFlags") == 0)
                type_size = sizeof(VkShaderStageFlags);
            else if (strcmp(type, "war_new_vulkan_hud_instance*") == 0)
                type_size = sizeof(war_new_vulkan_hud_instance*);
            else if (strcmp(type, "war_new_vulkan_cursor_instance*") == 0)
                type_size = sizeof(war_new_vulkan_cursor_instance*);
            else if (strcmp(type, "war_sequence_context") == 0)
                type_size = sizeof(war_sequence_context);
            else if (strcmp(type, "war_sequence_entry") == 0)
                type_size = sizeof(war_sequence_entry);
            else if (strcmp(type, "war_hud_line_context") == 0)
                type_size = sizeof(war_hud_line_context);
            else if (strcmp(type, "war_hud_text_context") == 0)
                type_size = sizeof(war_hud_text_context);
            else if (strcmp(type, "war_hud_cursor_context") == 0)
                type_size = sizeof(war_hud_cursor_context);
            else if (strcmp(type, "ino_t") == 0)
                type_size = sizeof(ino_t);
            else if (strcmp(type, "dev_t") == 0)
                type_size = sizeof(dev_t);
            else if (strcmp(type, "war_file") == 0)
                type_size = sizeof(war_file);
            else if (strcmp(type, "war_env") == 0)
                type_size = sizeof(war_env);
            else if (strcmp(type, "war_color_context") == 0)
                type_size = sizeof(war_color_context);
            else if (strcmp(type, "uint8_t*") == 0)
                type_size = sizeof(uint8_t*);
            else if (strcmp(type, "uint16_t*") == 0)
                type_size = sizeof(uint16_t*);
            else if (strcmp(type, "uint32_t*") == 0)
                type_size = sizeof(uint32_t*);
            else if (strcmp(type, "void**") == 0)
                type_size = sizeof(void**);

            else {
                call_king_terry("Unknown pool_wr type: %s", type);
                type_size = 0;
            }

            total_size += type_size * count;
        }
        lua_pop(ctx_lua->L, 1); // pop value
    }

    // align total_size to pool alignment
    size_t alignment = atomic_load(&ctx_lua->POOL_ALIGNMENT);
    total_size = (total_size + alignment - 1) & ~(alignment - 1);
    call_king_terry("pool_wr size: %zu", total_size);
    return total_size;
}

static inline void* war_pool_alloc(war_pool* pool, size_t size) {
    size =
        ((size) + ((pool->pool_alignment) - 1)) & ~((pool->pool_alignment) - 1);
    if (pool->pool_ptr + size > (uint8_t*)pool->pool + pool->pool_size) {
        call_king_terry("war_pool_alloc not big enough! %zu bytes", size);
        abort();
    }
    void* ptr = pool->pool_ptr;
    pool->pool_ptr += size;
    return ptr;
}

static inline void* war_pool_alloc_new(war_pool_context* ctx_pool,
                                       war_pool_id id) {
    for (uint32_t i = 0; i < ctx_pool->count; i++) {
        if (ctx_pool->id[i] == id) {
            assert(ctx_pool->pool + ctx_pool->offset[i]);
            return ctx_pool->pool + ctx_pool->offset[i];
        }
    }
    call_king_terry("no pool id found");
    return NULL;
}

// --------------------------
// Writer: WR -> Audio (to_a)
static inline uint8_t war_pc_to_a(war_producer_consumer* pc,
                                  uint32_t header,
                                  uint32_t payload_size,
                                  const void* payload) {
    uint32_t total_size = 8 + payload_size; // header(4) + size(4) + payload
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;
    // free bytes calculation (circular buffer)
    uint32_t free_bytes =
        (pc->size + read_index - write_index - 1) & (pc->size - 1);
    if (free_bytes < total_size) return 0;
    // --- write header (4) + size (4) allowing split ---
    uint32_t cont_bytes = pc->size - write_index;
    if (cont_bytes >= 8) {
        // contiguous place for header+size
        *(uint32_t*)(pc->to_a + write_index) = header;
        *(uint32_t*)(pc->to_a + write_index + 4) = payload_size;
    } else {
        // split header+size across end -> wrap
        if (cont_bytes >= 4) {
            *(uint32_t*)(pc->to_a + write_index) = header;
            *(uint32_t*)pc->to_a = payload_size;
        } else {
            *(uint16_t*)(pc->to_a + write_index) = (uint16_t)header;
            *(uint16_t*)(pc->to_a + write_index + 2) = (uint16_t)(header >> 16);
            *(uint32_t*)pc->to_a = payload_size;
        }
    }
    // --- write payload (may be zero length) allowing split ---
    if (payload_size) {
        uint32_t payload_write_pos = (write_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_write_pos;
        if (first_chunk >= payload_size) {
            memcpy(pc->to_a + payload_write_pos, payload, payload_size);
        } else {
            // wrap
            memcpy(pc->to_a + payload_write_pos, payload, first_chunk);
            memcpy(pc->to_a,
                   (const uint8_t*)payload + first_chunk,
                   payload_size - first_chunk);
        }
    }
    // commit write index
    pc->i_to_a = (write_index + total_size) & (pc->size - 1);
    return 1;
}

// --------------------------
// Reader: Audio <- WR (from_wr)
static inline uint8_t war_pc_from_wr(war_producer_consumer* pc,
                                     uint32_t* out_header,
                                     uint32_t* out_size,
                                     void* out_payload) {
    uint32_t write_index = pc->i_to_a;
    uint32_t read_index = pc->i_from_wr;
    uint32_t used_bytes =
        (pc->size + write_index - read_index) & (pc->size - 1);
    if (used_bytes < 8) return 0; // need at least header+size
    // read header+size (handle split)
    uint32_t cont_bytes = pc->size - read_index;
    if (cont_bytes >= 8) {
        *out_header = *(uint32_t*)(pc->to_a + read_index);
        *out_size = *(uint32_t*)(pc->to_a + read_index + 4);
    } else {
        if (cont_bytes >= 4) {
            *out_header = *(uint32_t*)(pc->to_a + read_index);
            *out_size = *(uint32_t*)pc->to_a;
        } else {
            uint16_t low = *(uint16_t*)(pc->to_a + read_index);
            uint16_t high = *(uint16_t*)(pc->to_a + read_index + 2);
            *out_header = (uint32_t)high << 16 | low;
            *out_size = *(uint32_t*)pc->to_a;
        }
    }
    uint32_t total_size = 8 + *out_size;
    if (used_bytes < total_size) return 0; // not all payload present yet
    // read payload (if any)
    if (*out_size) {
        uint32_t payload_start = (read_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_start;
        if (first_chunk >= *out_size) {
            memcpy(out_payload, pc->to_a + payload_start, *out_size);
        } else {
            memcpy(out_payload, pc->to_a + payload_start, first_chunk);
            memcpy((uint8_t*)out_payload + first_chunk,
                   pc->to_a,
                   *out_size - first_chunk);
        }
    }
    // commit read index
    pc->i_from_wr = (read_index + total_size) & (pc->size - 1);
    return 1;
}

// --------------------------
// Writer: Main -> WR (to_wr)
static inline uint8_t war_pc_to_wr(war_producer_consumer* pc,
                                   uint32_t header,
                                   uint32_t payload_size,
                                   const void* payload) {
    uint32_t total_size = 8 + payload_size;
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;
    uint32_t free_bytes =
        (pc->size + read_index - write_index - 1) & (pc->size - 1);
    if (free_bytes < total_size) return 0;
    // header+size
    uint32_t cont_bytes = pc->size - write_index;
    if (cont_bytes >= 8) {
        *(uint32_t*)(pc->to_wr + write_index) = header;
        *(uint32_t*)(pc->to_wr + write_index + 4) = payload_size;
    } else {
        if (cont_bytes >= 4) {
            *(uint32_t*)(pc->to_wr + write_index) = header;
            *(uint32_t*)pc->to_wr = payload_size;
        } else {
            *(uint16_t*)(pc->to_wr + write_index) = (uint16_t)header;
            *(uint16_t*)(pc->to_wr + write_index + 2) =
                (uint16_t)(header >> 16);
            *(uint32_t*)pc->to_wr = payload_size;
        }
    }
    // payload
    if (payload_size) {
        uint32_t payload_write_pos = (write_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_write_pos;
        if (first_chunk >= payload_size) {
            memcpy(pc->to_wr + payload_write_pos, payload, payload_size);
        } else {
            memcpy(pc->to_wr + payload_write_pos, payload, first_chunk);
            memcpy(pc->to_wr,
                   (const uint8_t*)payload + first_chunk,
                   payload_size - first_chunk);
        }
    }
    pc->i_to_wr = (write_index + total_size) & (pc->size - 1);
    return 1;
}

// --------------------------
// Reader: WR <- Main (from_a)
static inline uint8_t war_pc_from_a(war_producer_consumer* pc,
                                    uint32_t* out_header,
                                    uint32_t* out_size,
                                    void* out_payload) {
    uint32_t write_index = pc->i_to_wr;
    uint32_t read_index = pc->i_from_a;
    uint32_t used_bytes =
        (pc->size + write_index - read_index) & (pc->size - 1);
    if (used_bytes < 8) return 0;
    uint32_t cont_bytes = pc->size - read_index;
    if (cont_bytes >= 8) {
        *out_header = *(uint32_t*)(pc->to_wr + read_index);
        *out_size = *(uint32_t*)(pc->to_wr + read_index + 4);
    } else {
        if (cont_bytes >= 4) {
            *out_header = *(uint32_t*)(pc->to_wr + read_index);
            *out_size = *(uint32_t*)pc->to_wr;
        } else {
            uint16_t low = *(uint16_t*)(pc->to_wr + read_index);
            uint16_t high = *(uint16_t*)(pc->to_wr + read_index + 2);
            *out_header = (uint32_t)high << 16 | low;
            *out_size = *(uint32_t*)pc->to_wr;
        }
    }
    uint32_t total_size = 8 + *out_size;
    if (used_bytes < total_size) return 0;
    if (*out_size) {
        uint32_t payload_start = (read_index + 8) & (pc->size - 1);
        uint32_t first_chunk = pc->size - payload_start;
        if (first_chunk >= *out_size) {
            memcpy(out_payload, pc->to_wr + payload_start, *out_size);
        } else {
            memcpy(out_payload, pc->to_wr + payload_start, first_chunk);
            memcpy((uint8_t*)out_payload + first_chunk,
                   pc->to_wr,
                   *out_size - first_chunk);
        }
    }
    pc->i_from_a = (read_index + total_size) & (pc->size - 1);
    return 1;
}

static inline uint64_t war_get_monotonic_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static inline int32_t war_to_fixed(float f) { return (int32_t)(f * 256.0f); }

static inline uint32_t war_pad_to_scale(float value, uint32_t scale) {
    uint32_t rounded = (uint32_t)(value + 0.5f);
    return (rounded + scale - 1) / scale * scale;
}

static inline uint64_t war_read_le64(const uint8_t* p) {
    return ((uint64_t)p[0]) | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

static inline uint32_t war_read_le32(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static inline uint16_t war_read_le16(const uint8_t* p) {
    return ((uint16_t)p[0]) | ((uint16_t)p[1] << 8);
}

static inline void war_write_le64(uint8_t* p, uint64_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 32);
    p[5] = (uint8_t)(v >> 40);
    p[6] = (uint8_t)(v >> 48);
    p[7] = (uint8_t)(v >> 56);
}

static inline void war_write_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline void war_write_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
}

static inline uint32_t
war_clamp_add_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t sum = (uint64_t)a + b;
    uint64_t mask = -(sum > max_value);
    return (uint32_t)((sum & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t
war_clamp_subtract_uint32(uint32_t a, uint32_t b, uint32_t min_value) {
    uint32_t diff = a - b;
    uint32_t underflow_mask = -(a < b);
    uint32_t below_min_mask = -(diff < min_value);
    uint32_t clamped_diff =
        (diff & ~below_min_mask) | (min_value & below_min_mask);
    return (clamped_diff & ~underflow_mask) | (min_value & underflow_mask);
}

static inline uint32_t
war_clamp_multiply_uint32(uint32_t a, uint32_t b, uint32_t max_value) {
    uint64_t prod = (uint64_t)a * (uint64_t)b;
    uint64_t mask = -(prod > max_value);
    return (uint32_t)((prod & ~mask) | ((uint64_t)max_value & mask));
}

static inline uint32_t
war_clamp_uint32(uint32_t a, uint32_t min_value, uint32_t max_value) {
    a = a < min_value ? min_value : a;
    a = a > max_value ? max_value : a;
    return a;
}

static inline int war_num_digits(uint32_t n) {
    int digits = 0;
    do {
        digits++;
        n /= 10;
    } while (n != 0);
    return digits;
}

int war_compare_desc_uint32(const void* a, const void* b) {
    uint32_t f_a = *(const uint32_t*)a;
    uint32_t f_b = *(const uint32_t*)b;

    if (f_b > f_a) return 1;
    if (f_b < f_a) return -1;
    return 0;
}

static inline void war_wl_surface_set_opaque_region(int fd,
                                                    uint32_t wl_surface_id,
                                                    uint32_t wl_region_id) {
    uint8_t set_opaque_region[12];
    war_write_le32(set_opaque_region, wl_surface_id);
    war_write_le16(set_opaque_region + 4, 4);
    war_write_le16(set_opaque_region + 6, 12);
    war_write_le32(set_opaque_region + 8, wl_region_id);
    // dump_bytes("wl_surface::set_opaque_region request", set_opaque_region,
    // 12);
    ssize_t set_opaque_region_written = write(fd, set_opaque_region, 12);
    assert(set_opaque_region_written == 12);
}

static inline uint32_t war_gcd(uint32_t a, uint32_t b) {
    while (b != 0) {
        uint32_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static inline uint32_t war_lcm(uint32_t a, uint32_t b) {
    return a / war_gcd(a, b) * b;
}

static inline float war_midi_to_frequency(float midi_note) {
    return 440.0f * pow(2.0f, (midi_note - 69) / 12.0f);
}

static inline uint32_t war_to_ascii(uint32_t keysym, uint32_t mod) {
    uint8_t modless_letters = keysym >= XKB_KEY_a && keysym <= XKB_KEY_z;
    uint8_t modless_numbers = keysym >= XKB_KEY_0 && keysym <= XKB_KEY_9;
    uint8_t mod_shift = mod == MOD_SHIFT;
    if (mod_shift) {
        if (modless_letters) { return keysym - 32; }
        switch (keysym) {
        case XKB_KEY_comma:
            return '<';
        case XKB_KEY_period:
            return '>';
        case XKB_KEY_slash:
            return '?';
        case XKB_KEY_semicolon:
            return ':';
        case XKB_KEY_apostrophe:
            return '"';
        case XKB_KEY_bracketleft:
            return '{';
        case XKB_KEY_bracketright:
            return '}';
        case XKB_KEY_backslash:
            return '|';
        case XKB_KEY_grave:
            return '~';
        case XKB_KEY_minus:
            return '_';
        case XKB_KEY_equal:
            return '+';
        case XKB_KEY_1:
            return '!';
        case XKB_KEY_2:
            return '@';
        case XKB_KEY_3:
            return '#';
        case XKB_KEY_4:
            return '$';
        case XKB_KEY_5:
            return '%';
        case XKB_KEY_6:
            return '^';
        case XKB_KEY_7:
            return '&';
        case XKB_KEY_8:
            return '*';
        case XKB_KEY_9:
            return '(';
        case XKB_KEY_0:
            return ')';
        default:
            break;
        }
    }
    if (modless_letters) { return keysym; }
    if (modless_numbers) { return keysym; }
    switch (keysym) {
    case XKB_KEY_BackSpace:
        return '\b';
    case XKB_KEY_Return:
        return '\n';
    case XKB_KEY_Escape:
        return '\e';
    case XKB_KEY_Left:
        return 3;
    case XKB_KEY_Right:
        return 4;
    case XKB_KEY_space:
        return ' ';
    case XKB_KEY_Tab:
        return '\t';
    case XKB_KEY_comma:
        return ',';
    case XKB_KEY_period:
        return '.';
    case XKB_KEY_slash:
        return '/';
    case XKB_KEY_semicolon:
        return ';';
    case XKB_KEY_apostrophe:
        return '\'';
    case XKB_KEY_bracketleft:
        return '[';
    case XKB_KEY_bracketright:
        return ']';
    case XKB_KEY_backslash:
        return '\\';
    case XKB_KEY_grave:
        return '`';
    case XKB_KEY_minus:
        return '-';
    case XKB_KEY_equal:
        return '=';
    default:
        break;
    }
    return '\0';
}

static inline uint32_t war_normalize_keysym(uint32_t keysym) {
    uint8_t modless_letters = keysym >= XKB_KEY_a && keysym <= XKB_KEY_z;
    uint8_t modless_numbers = keysym >= XKB_KEY_0 && keysym <= XKB_KEY_9;
    uint8_t modless_arrows = keysym >= XKB_KEY_Left && keysym <= XKB_KEY_Down;
    uint8_t modless_special =
        keysym == XKB_KEY_Return || keysym == XKB_KEY_Tab ||
        keysym == XKB_KEY_Caps_Lock || keysym == XKB_KEY_Escape ||
        keysym == XKB_KEY_Print || keysym == XKB_KEY_Home ||
        keysym == XKB_KEY_BackSpace || keysym == XKB_KEY_Delete ||
        keysym == XKB_KEY_space || keysym == XKB_KEY_semicolon ||
        keysym == XKB_KEY_comma || keysym == XKB_KEY_period ||
        keysym == XKB_KEY_slash || keysym == XKB_KEY_apostrophe ||
        keysym == XKB_KEY_bracketleft || keysym == XKB_KEY_bracketright ||
        keysym == XKB_KEY_backslash || keysym == XKB_KEY_equal ||
        keysym == XKB_KEY_minus || keysym == XKB_KEY_grave;
    if (modless_letters || modless_numbers || modless_arrows ||
        modless_special) {
        return keysym;
    }
    uint8_t mod_letters = keysym >= XKB_KEY_A && keysym <= XKB_KEY_Z;
    if (mod_letters) { return keysym + 32; }
    switch (keysym) {
    case XKB_KEY_asciitilde: // ~
        return XKB_KEY_grave;
    case XKB_KEY_exclam: // !
        return XKB_KEY_1;
    case XKB_KEY_at: // @
        return XKB_KEY_2;
    case XKB_KEY_numbersign: // #
        return XKB_KEY_3;
    case XKB_KEY_dollar: // $
        return XKB_KEY_4;
    case XKB_KEY_percent: // %
        return XKB_KEY_5;
    case XKB_KEY_asciicircum: // ^
        return XKB_KEY_6;
    case XKB_KEY_ampersand: // &
        return XKB_KEY_7;
    case XKB_KEY_asterisk: // *
        return XKB_KEY_8;
    case XKB_KEY_parenleft: // (
        return XKB_KEY_9;
    case XKB_KEY_parenright: // )
        return XKB_KEY_0;
    case XKB_KEY_underscore: // _
        return XKB_KEY_minus;
    case XKB_KEY_plus: // +
        return XKB_KEY_equal;
    case XKB_KEY_less: // <
        return XKB_KEY_comma;
    case XKB_KEY_greater: // >
        return XKB_KEY_period;
    case XKB_KEY_bar: // |
        return XKB_KEY_backslash;
    case XKB_KEY_colon: // :
        return XKB_KEY_semicolon;
    case XKB_KEY_quotedbl: // "
        return XKB_KEY_apostrophe;
    case XKB_KEY_question: // ?
        return XKB_KEY_slash;
    case XKB_KEY_braceleft: // {
        return XKB_KEY_bracketleft;
    case XKB_KEY_braceright: // }
        return XKB_KEY_bracketright;
    case 65056: // <S-Tab>
        return XKB_KEY_Tab;
    default:
        return XKB_KEY_NoSymbol;
    }
}

static inline uint8_t war_parse_token_to_keysym_mod(const char* token,
                                                    uint32_t* keysym_out,
                                                    uint8_t* mod_out) {
    if (!token || !keysym_out || !mod_out) { return 0; }
    *mod_out = 0;
    const char* key_str = token;
    char token_buf[64] = {0};
    if (token[0] == '<') {
        size_t len = strlen(token);
        if (len > 1 && token[len - 1] == '>') {
            size_t inner_len = len - 2;
            if (inner_len >= sizeof(token_buf))
                inner_len = sizeof(token_buf) - 1;
            memcpy(token_buf, token + 1, inner_len);
            token_buf[inner_len] = '\0';
            char key_part[64] = {0};
            char* saveptr = NULL;
            char* part = strtok_r(token_buf, "-", &saveptr);
            while (part) {
                if (strcasecmp(part, "C") == 0 ||
                    strcasecmp(part, "Ctrl") == 0 ||
                    strcasecmp(part, "Control") == 0) {
                    *mod_out |= MOD_CTRL;
                } else if (strcmp(part, "S") == 0 ||
                           strcasecmp(part, "Shift") == 0) {
                    *mod_out |= MOD_SHIFT;
                } else if (strcmp(part, "A") == 0 ||
                           strcasecmp(part, "Alt") == 0 ||
                           strcasecmp(part, "M") == 0 ||
                           strcasecmp(part, "Meta") == 0) {
                    *mod_out |= MOD_ALT;
                } else if (strcmp(part, "D") == 0 ||
                           strcasecmp(part, "Cmd") == 0 ||
                           strcasecmp(part, "Super") == 0 ||
                           strcasecmp(part, "Logo") == 0) {
                    *mod_out |= MOD_LOGO;
                } else {
                    strncpy(key_part, part, sizeof(key_part) - 1);
                    key_part[sizeof(key_part) - 1] = '\0';
                }
                part = strtok_r(NULL, "-", &saveptr);
            }
            if (key_part[0] == '\0' && inner_len > 0) {
                key_part[0] = token_buf[inner_len - 1];
                key_part[1] = '\0';
            }
            if (key_part[0] != '\0') { key_str = key_part; }
        }
    }
    xkb_keysym_t ks = XKB_KEY_NoSymbol;
    if (strcasecmp(key_str, "CR") == 0 || strcasecmp(key_str, "Enter") == 0 ||
        strcasecmp(key_str, "Return") == 0) {
        ks = XKB_KEY_Return;
    } else if (strcasecmp(key_str, "Esc") == 0 ||
               strcasecmp(key_str, "Escape") == 0) {
        ks = XKB_KEY_Escape;
    } else if (strcasecmp(key_str, "Space") == 0) {
        ks = XKB_KEY_space;
    } else if (strcasecmp(key_str, "Tab") == 0) {
        ks = XKB_KEY_Tab;
    } else if (strcasecmp(key_str, "BS") == 0 ||
               strcasecmp(key_str, "Backspace") == 0) {
        ks = XKB_KEY_BackSpace;
    } else if (strcasecmp(key_str, "Del") == 0 ||
               strcasecmp(key_str, "Delete") == 0) {
        ks = XKB_KEY_Delete;
    } else if (strcasecmp(key_str, "Insert") == 0) {
        ks = XKB_KEY_Insert;
    } else if (strcasecmp(key_str, "Home") == 0) {
        ks = XKB_KEY_Home;
    } else if (strcasecmp(key_str, "End") == 0) {
        ks = XKB_KEY_End;
    } else if (strcasecmp(key_str, "PageUp") == 0) {
        ks = XKB_KEY_Page_Up;
    } else if (strcasecmp(key_str, "PageDown") == 0) {
        ks = XKB_KEY_Page_Down;
    } else if (strcasecmp(key_str, "Up") == 0) {
        ks = XKB_KEY_Up;
    } else if (strcasecmp(key_str, "Down") == 0) {
        ks = XKB_KEY_Down;
    } else if (strcasecmp(key_str, "Left") == 0) {
        ks = XKB_KEY_Left;
    } else if (strcasecmp(key_str, "Right") == 0) {
        ks = XKB_KEY_Right;
    } else if (strcasecmp(key_str, "lt") == 0) {
        ks = XKB_KEY_less;
    } else if (strlen(key_str) == 1) {
        char c = key_str[0];
        switch (c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ks = XKB_KEY_0 + (c - '0');
            break;
        case '!':
            ks = XKB_KEY_1;
            *mod_out |= MOD_SHIFT;
            break;
        case '@':
            ks = XKB_KEY_2;
            *mod_out |= MOD_SHIFT;
            break;
        case '#':
            ks = XKB_KEY_3;
            *mod_out |= MOD_SHIFT;
            break;
        case '$':
            ks = XKB_KEY_4;
            *mod_out |= MOD_SHIFT;
            break;
        case '%':
            ks = XKB_KEY_5;
            *mod_out |= MOD_SHIFT;
            break;
        case '^':
            ks = XKB_KEY_6;
            *mod_out |= MOD_SHIFT;
            break;
        case '&':
            ks = XKB_KEY_7;
            *mod_out |= MOD_SHIFT;
            break;
        case '*':
            ks = XKB_KEY_8;
            *mod_out |= MOD_SHIFT;
            break;
        case '(':
            ks = XKB_KEY_9;
            *mod_out |= MOD_SHIFT;
            break;
        case ')':
            ks = XKB_KEY_0;
            *mod_out |= MOD_SHIFT;
            break;
        case '[':
            ks = XKB_KEY_bracketleft;
            break;
        case ']':
            ks = XKB_KEY_bracketright;
            break;
        case '_':
            ks = XKB_KEY_minus;
            *mod_out |= MOD_SHIFT;
            break;
        case '-':
            ks = XKB_KEY_minus;
            break;
        case '+':
            ks = XKB_KEY_equal;
            *mod_out |= MOD_SHIFT;
            break;
        case '=':
            ks = XKB_KEY_equal;
            break;
        case ':':
            ks = XKB_KEY_semicolon;
            *mod_out |= MOD_SHIFT;
            break;
        case ';':
            ks = XKB_KEY_semicolon;
            break;
        case '?':
            ks = XKB_KEY_slash;
            *mod_out |= MOD_SHIFT;
            break;
        case '/':
            ks = XKB_KEY_slash;
            break;
        case '>':
            ks = XKB_KEY_period;
            *mod_out |= MOD_SHIFT;
            break;
        case '.':
            ks = XKB_KEY_period;
            break;
        case '<':
            ks = XKB_KEY_comma;
            *mod_out |= MOD_SHIFT;
            break;
        case ',':
            ks = XKB_KEY_comma;
            break;
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
            ks = XKB_KEY_a + (c - 'a');
            break;
        default:
            ks = xkb_keysym_from_name(key_str, XKB_KEYSYM_NO_FLAGS);
            break;
        }
    } else {
        ks = xkb_keysym_from_name(key_str, XKB_KEYSYM_NO_FLAGS);
    }
    if (ks == XKB_KEY_NoSymbol) { return 0; }
    if (strlen(key_str) == 1 && key_str[0] >= 'A' && key_str[0] <= 'Z' &&
        !(*mod_out & MOD_SHIFT)) {
        *mod_out |= MOD_SHIFT;
    }
    *keysym_out = war_normalize_keysym(ks);
    return 1;
}

static inline uint16_t war_find_prefix_state(char** prefixes,
                                             uint16_t prefix_count,
                                             const char* prefix) {
    for (uint16_t i = 0; i < prefix_count; i++) {
        if (strcmp(prefixes[i], prefix) == 0) { return i; }
    }
    return UINT16_MAX;
}

static inline void war_fsm_execute_command(war_env* env,
                                           war_fsm_context* ctx_fsm,
                                           uint16_t state_index) {
    if (!ctx_fsm || state_index >= ctx_fsm->state_count) { return; }
    size_t mode_idx =
        (size_t)state_index * ctx_fsm->mode_count + ctx_fsm->current_mode;
    if (ctx_fsm->function_type[mode_idx] != ctx_fsm->FUNCTION_C) { return; }
    void (*fn)(war_env*) = ctx_fsm->function[mode_idx].c;
    if (fn) { fn(env); }
}

static inline uint32_t war_trim_whitespace(char* text) {
    int i = 0;
    int j = 0;
    while (isspace((unsigned char)text[i])) i++;
    while ((text[j++] = text[i++]));
    uint32_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }
    return len;
}

static inline uint32_t
war_get_ext(const char* file_name, char* ext, uint32_t name_limit) {
    if (!file_name || !ext) return 0;
    memset(ext, 0, name_limit);

    char* last_dot = strrchr(file_name, '.');
    if (!last_dot) {
        ext[0] = '\0';
        return 0; // No extension found
    }

    // Only copy if there's actually an extension
    char* ext_start = last_dot + 1;
    uint32_t ext_len = strlen(ext_start);
    uint32_t copy_len = (ext_len < name_limit) ? ext_len : name_limit - 1;

    memcpy(ext, ext_start, copy_len);
    ext[copy_len] = '\0';
    return copy_len;
}

static inline void war_mkdir(char* dir, __mode_t mode) {
    if (!dir || !dir[0]) return;
    char path[4096] = {0};
    size_t j = 0;
    // Inline environment variable expansion ($HOME, $VAR, etc.)
    for (size_t i = 0; dir[i] && j < sizeof(path) - 1; i++) {
        if (dir[i] == '$') {
            size_t start = i + 1;
            size_t end = start;
            while ((dir[end] >= 'A' && dir[end] <= 'Z') ||
                   (dir[end] >= 'a' && dir[end] <= 'z') ||
                   (dir[end] >= '0' && dir[end] <= '9') || dir[end] == '_') {
                end++;
            }
            char varname[64] = {0};
            size_t len = end - start;
            if (len >= sizeof(varname)) len = sizeof(varname) - 1;
            memcpy(varname, dir + start, len);
            char* val = getenv(varname);
            if (val) {
                size_t val_len = strlen(val);
                if (j + val_len >= sizeof(path) - 1)
                    val_len = sizeof(path) - 1 - j;
                memcpy(path + j, val, val_len);
                j += val_len;
            }
            i = end - 1;
        } else {
            path[j++] = dir[i];
        }
    }
    path[j] = '\0';
    // Attempt to create the directory
    if (mkdir(path, mode) && errno != EEXIST) {
        call_king_terry("mkdir failed: %s (%s)", path, strerror(errno));
    }
}

static inline void war_override(uint32_t count, war_hot_id* id, war_env* env) {
    war_hot_context* hot = env->ctx_hot;
    war_config_context* config = env->ctx_config;
    for (uint32_t idx = 0; idx < count; idx++) {
        char tmp_path[4096] = {0};
        char output[4096] = {0};
        switch (id[idx]) {
        case WAR_HOT_ID_CONFIG:
            goto war_label_load_config;
        case WAR_HOT_ID_COMMAND:
            goto war_label_load_command;
        case WAR_HOT_ID_COLOR:
            goto war_label_load_color;
        case WAR_HOT_ID_PLUGIN:
            goto war_label_load_plugin;
        case WAR_HOT_ID_POOL:
            goto war_label_load_pool;
        case WAR_HOT_ID_KEYMAP:
            goto war_label_load_keymap;
        }
        continue;
    war_label_load_config:
        // close previous handle if exists
        if (hot->handle[id[idx]]) {
            dlclose(hot->handle[id[idx]]);
            hot->handle[id[idx]] = NULL;
            hot->function[id[idx]] = NULL;
        }
        // --------------------------------------------------
        // 1) Expand $HOME in DIR_CONFIG
        // --------------------------------------------------
        memcpy(tmp_path, config->DIR_CONFIG, strlen(config->DIR_CONFIG));
        size_t j = 0;
        for (size_t i = 0; tmp_path[i] && j < sizeof(output) - 1; i++) {
            if (tmp_path[i] == '$') {
                size_t start = i + 1;
                size_t end = start;
                while ((tmp_path[end] >= 'A' && tmp_path[end] <= 'Z') ||
                       (tmp_path[end] >= 'a' && tmp_path[end] <= 'z') ||
                       (tmp_path[end] >= '0' && tmp_path[end] <= '9') ||
                       tmp_path[end] == '_')
                    end++;
                char varname[64] = {0};
                size_t len = end - start;
                if (len >= sizeof(varname)) len = sizeof(varname) - 1;
                memcpy(varname, tmp_path + start, len);
                char* val = getenv(varname);
                if (val) {
                    size_t val_len = strlen(val);
                    if (j + val_len >= sizeof(output) - 1)
                        val_len = sizeof(output) - 1 - j;
                    memcpy(output + j, val, val_len);
                    j += val_len;
                }
                i = end - 1;
            } else {
                output[j++] = tmp_path[i];
            }
        }
        output[j] = '\0';
        // --------------------------------------------------
        // 2) Open config dir
        // --------------------------------------------------
        DIR* directory = opendir(output);
        if (!directory) {
            call_king_terry("war_override: invalid directory");
            continue;
        }
        struct dirent* entry;
        while ((entry = readdir(directory)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;
            size_t len = strlen(entry->d_name);
            if (len > 2 && strcmp(entry->d_name + len - 2, ".c") == 0) {
                // --------------------------------------------------
                // 3) Full path to .c file
                // --------------------------------------------------
                char cpath[4096];
                snprintf(cpath, sizeof(cpath), "%s/%s", output, entry->d_name);
                // --------------------------------------------------
                // 4) Corresponding .so path in override
                // --------------------------------------------------
                char sopath[4096];
                snprintf(sopath,
                         sizeof(sopath),
                         "%s/%s.so",
                         config->DIR_OVERRIDE,
                         entry->d_name);

                struct stat st_c, st_so;
                int so_exists = (stat(sopath, &st_so) == 0);
                int recompile = 1;
                if (stat(cpath, &st_c) == 0) {
                    if (so_exists && st_c.st_mtime <= st_so.st_mtime) {
                        // up-to-date, load inline
                        void* handle = dlopen(sopath, RTLD_NOW);
                        if (handle) {
                            hot->handle[id[idx]] = handle;
                            hot->function[id[idx]] =
                                dlsym(handle, "war_config_override");
                            char* err = dlerror();
                            if (err) {
                                call_king_terry("dlsym error: %s", err);
                            }
                        }
                        recompile = 0;
                    }
                }
                if (recompile) {
                    // --------------------------------------------------
                    // 5) Recompile using template
                    // --------------------------------------------------
                    char cmd[8192];
                    snprintf(cmd,
                             sizeof(cmd),
                             config->HOT_CONTEXT_TEMPLATE,
                             cpath,
                             sopath);
                    system(cmd);
                    // dlopen after compilation
                    void* handle = dlopen(sopath, RTLD_NOW);
                    if (handle) {
                        hot->handle[id[idx]] = handle;
                        hot->function[id[idx]] =
                            dlsym(handle, "war_config_override");
                        char* err = dlerror();
                        if (err) { call_king_terry("dlsym error: %s", err); }
                    }
                }
            }
        }
        closedir(directory);
        if (hot->function[id[idx]]) {
            ((void (*)(war_config_context*))hot->function[id[idx]])(config);
        }
        continue;
    war_label_load_command:
        continue;
    war_label_load_color:
        continue;
    war_label_load_plugin:
        continue;
    war_label_load_pool:
        continue;
    war_label_load_keymap:
        continue;
    }
}

#endif // WAR_FUNCTIONS_H
