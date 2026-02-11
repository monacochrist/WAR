//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_config.h
//-----------------------------------------------------------------------------

#ifndef WAR_CONFIG_H
#define WAR_CONFIG_H

#ifndef WAR_CONFIG_H_VERSION
#define WAR_CONFIG_H_VERSION 0
#endif // WAR_CONFIG_H_VERSION

#include "stdint.h"
#include "war_data.h"

// sets defaults, no need to call during override since it's called at init
static inline void war_config_default(war_config_context* config) {
    config->version = WAR_CONFIG_H_VERSION;
    //
    config->A_BASE_FREQUENCY = 440;
    config->A_BASE_NOTE = 69;
    config->A_EDO = 12;
    config->A_SAMPLE_RATE = 44100;
    config->A_BPM = 100.0;
    config->BPM_SECONDS_PER_CELL = 60.0;
    config->SUBDIVISION_SECONDS_PER_CELL = 4.0;
    config->A_SAMPLE_DURATION = 15.0;
    config->A_CHANNEL_COUNT = 2;
    config->A_NOTE_COUNT = 128;
    config->A_LAYERS_IN_RAM = 13;
    config->A_LAYER_COUNT = 9;
    config->A_PLAY_DATA_SIZE = 6;
    config->A_CAPTURE_DATA_SIZE = 6;
    config->A_WARMUP_FRAMES_FACTOR = 1000;
    config->A_NOTES_MAX = 20000;
    config->A_DEFAULT_ATTACK = 0.0;
    config->A_DEFAULT_SUSTAIN = 1.0;
    config->A_DEFAULT_RELEASE = 0.0;
    config->A_DEFAULT_GAIN = 1.0;
    config->A_DEFAULT_COLUMNS_PER_BEAT = 4.0;
    config->CACHE_FILE_CAPACITY = 100;
    config->CONFIG_PATH_MAX = 4096;
    config->A_SCHED_FIFO_PRIORITY = 10;
    config->A_BUILDER_DATA_SIZE = 1024;
    // window render
    config->WR_VIEWS_SAVED = 13;
    config->WR_COLOR_STEP = 43.2;
    config->WR_WARPOON_TEXT_COLS = 25;
    config->WR_STATES = 256;
    config->WR_SEQUENCE_COUNT = 0;
    config->WR_SEQUENCE_LENGTH_MAX = 0;
    config->WR_INPUT_SEQUENCE_LENGTH_MAX = 256;
    config->WR_MODE_COUNT = 8;
    config->WR_KEYSYM_COUNT = 512;
    config->WR_MOD_COUNT = 16;
    config->WR_NOTE_QUADS_MAX = 20000;
    config->WR_STATUS_BAR_COLS_MAX = 400;
    config->WR_TEXT_QUADS_MAX = 20000;
    config->WR_QUADS_MAX = 20000;
    config->WR_LEADER = "<Space>";
    config->WR_WAYLAND_MSG_BUFFER_SIZE = 4096;
    config->WR_WAYLAND_MAX_OBJECTS = 1000;
    config->WR_WAYLAND_MAX_OP_CODES = 20;
    config->WR_UNDO_NODES_MAX = 10000;
    config->WR_TIMESTAMP_LENGTH_MAX = 33;
    config->WR_CAPTURE_THRESHOLD = 0.0001;
    config->WR_REPEAT_DELAY_US = 150000;
    config->WR_REPEAT_RATE_US = 40000;
    config->WR_CURSOR_BLINK_DURATION_US = 700000;
    config->WR_UNDO_NOTES_BATCH_MAX = 100;
    config->WR_FPS = 60.0;
    config->WR_PLAY_CALLBACK_FPS = 173.0;
    config->WR_CAPTURE_CALLBACK_FPS = 47.0;
    config->WR_CALLBACK_SIZE = 4192;
    config->A_BYTES_NEEDED = 2048;
    config->A_TARGET_SAMPLES_FACTOR = 2.0;
    config->ROLL_POSITION_X_Y = 0;
    // pool
    config->POOL_ALIGNMENT = 256;
    config->VK_ALIGNMENT = 256;
    // cmd
    config->CMD_COUNT = 1;
    // pc
    config->PC_CONTROL_BUFFER_SIZE = 65536;
    config->PC_PLAY_BUFFER_SIZE = 65536;
    config->PC_CAPTURE_BUFFER_SIZE = 65536;
    // vk
    config->VK_ATLAS_WIDTH = 8192;
    config->VK_ATLAS_HEIGHT = 8192;
    config->VK_FONT_PIXEL_HEIGHT = 69.0;
    config->VK_GLYPH_COUNT = 128;
    config->VK_MAX_FRAMES = 1;
    // nsgt
    config->NSGT_FRAME_CAPACITY = 2048;
    config->NSGT_BIN_CAPACITY = 512;
    config->NSGT_FREQUENCY_MIN = 20;
    config->NSGT_FREQUENCY_MAX = 20000;
    config->NSGT_ALPHA = 1.0;
    config->NSGT_WINDOW_LENGTH_MIN = 32;
    config->NSGT_SHAPE_FACTOR = 6.0;
    config->NSGT_RESOURCE_COUNT = 25;
    config->NSGT_DESCRIPTOR_SET_COUNT = 2;
    config->NSGT_PIPELINE_COUNT = 6;
    config->NSGT_SHADER_COUNT = 7;
    config->NSGT_GRAPHICS_FPS = 10;
    config->NSGT_GROUPS = 3;
    config->CACHE_NSGT_CAPACITY = 100;
    // new_vulkat
    config->NEW_VULKAN_RESOURCE_COUNT = 20;
    config->NEW_VULKAN_DESCRIPTOR_SET_COUNT = 1;
    config->NEW_VULKAN_PIPELINE_COUNT = 8;
    config->NEW_VULKAN_SHADER_COUNT = 16;
    config->NEW_VULKAN_GROUPS = 3;
    config->NEW_VULKAN_NOTE_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_TEXT_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_LINE_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_CURSOR_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_HUD_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_HUD_CURSOR_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_HUD_TEXT_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_HUD_LINE_INSTANCE_MAX = 30000;
    config->NEW_VULKAN_ATLAS_WIDTH = 1024;
    config->NEW_VULKAN_ATLAS_HEIGHT = 1024;
    config->NEW_VULKAN_FONT_PIXEL_HEIGHT = 69.0;
    config->NEW_VULKAN_GLYPH_COUNT = 128;
    config->NEW_VULKAN_SDF_SCALE = 4;
    config->NEW_VULKAN_SDF_PADDING = 8;
    config->NEW_VULKAN_SDF_RANGE = 8.0;
    config->NEW_VULKAN_SDF_LARGE = 1e20;
    config->NEW_VULKAN_BUFFER_MAX = 40;
    // hud context
    config->HUD_COUNT = 6;
    config->HUD_STATUS_BOTTOM_INSTANCE_MAX = 1;
    config->HUD_STATUS_TOP_INSTANCE_MAX = 1;
    config->HUD_STATUS_MIDDLE_INSTANCE_MAX = 1;
    config->HUD_LINE_NUMBERS_INSTANCE_MAX = 128;
    config->HUD_PIANO_INSTANCE_MAX = 128;
    config->HUD_EXPLORE_INSTANCE_MAX = 1;
    // hud line context
    config->HUD_LINE_COUNT = 1;
    config->HUD_LINE_PIANO_INSTANCE_MAX = 128;
    // hud text context
    config->HUD_TEXT_COUNT = 8;
    config->HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX = 6000;
    config->HUD_TEXT_STATUS_TOP_INSTANCE_MAX = 6000;
    config->HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX = 6000;
    config->HUD_TEXT_PIANO_INSTANCE_MAX = 127 * 6;
    config->HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX = 127 * 6;
    config->HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX = 127 * 6;
    config->HUD_TEXT_EXPLORE_INSTANCE_MAX = 1;
    config->HUD_TEXT_ERROR_INSTANCE_MAX = 1;
    // cursor context
    config->CURSOR_COUNT = 1;
    config->CURSOR_DEFAULT_INSTANCE_MAX = 1;
    // hud cursor context
    config->HUD_CURSOR_COUNT = 1;
    config->HUD_CURSOR_DEFAULT_INSTANCE_MAX = 1;
    // line context
    config->LINE_COUNT = 2;
    config->LINE_CELL_INSTANCE_MAX = 1000;
    config->LINE_BPM_INSTANCE_MAX = 1000;
    // text context
    config->TEXT_COUNT = 8;
    config->TEXT_STATUS_BOTTOM_INSTANCE_MAX = 6000;
    config->TEXT_STATUS_TOP_INSTANCE_MAX = 6000;
    config->TEXT_STATUS_MIDDLE_INSTANCE_MAX = 6000;
    config->TEXT_PIANO_INSTANCE_MAX = 127 * 6;
    config->TEXT_LINE_NUMBERS_INSTANCE_MAX = 127 * 6;
    config->TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX = 127 * 6;
    config->TEXT_EXPLORE_INSTANCE_MAX = 1;
    config->TEXT_ERROR_INSTANCE_MAX = 1;
    // sequence context
    config->NOTE_COUNT = 1;
    config->NOTE_GRID_INSTANCE_MAX = 10000;
    // visual
    config->VK_NSGT_VISUAL_RESOURCE_COUNT = 0;
    config->VK_NSGT_VISUAL_QUAD_CAPACITY = 0;
    // misc
    config->DEFAULT_ALPHA_SCALE = 0.2;
    config->DEFAULT_CURSOR_ALPHA_SCALE = 0.6;
    config->DEFAULT_PLAYBACK_BAR_THICKNESS = 0.05;
    config->DEFAULT_TEXT_FEATHER = 0.0;
    config->DEFAULT_TEXT_THICKNESS = 0.2;
    config->WINDOWED_TEXT_FEATHER = 0.0;
    config->WINDOWED_TEXT_THICKNESS = 0.2;
    config->DEFAULT_BOLD_TEXT_FEATHER = 0.0;
    config->DEFAULT_BOLD_TEXT_THICKNESS = 0.2;
    config->DEFAULT_WINDOWED_ALPHA_SCALE = 0.02;
    config->DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE = 0.02;
    config->WR_FN_NAME_LIMIT = 4096;
    // pool context
    config->POOL_MAX_ALLOCATIONS = WAR_POOL_ID_COUNT;
    // keymap context
    config->KEYMAP_STATE_CAPACITY = 500;
    config->KEYMAP_KEYSYM_CAPACITY = 256;
    config->KEYMAP_MOD_CAPACITY = 32;
    config->KEYMAP_FUNCTION_CAPACITY = 4;
}

void war_config_override(war_config_context* config);

#endif // WAR_CONFIG_H
