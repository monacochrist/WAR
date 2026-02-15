//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_pool.h
//-----------------------------------------------------------------------------

#ifndef WAR_POOL_H
#define WAR_POOL_H

#ifndef WAR_POOL_H_VERSION
#define WAR_POOL_H_VERSION 0
#endif // WAR_POOL_H_VERSION

#include "war_data.h"
#include "war_debug_macros.h"

static inline void war_pool_set(war_pool_context* pool,
                                war_config_context* config,
                                war_pool_id id,
                                uint64_t size,
                                uint64_t alignment) {
    if (pool->count >= (uint32_t)config->POOL_MAX_ALLOCATIONS) {
        call_king_terry("MAX ALLOCATIONS REACHED");
        return;
    }
    uint64_t offset = 0;
    for (uint32_t i = 0; i < pool->count; i++) {
        offset = (offset + pool->alignment[i] - 1) & ~(pool->alignment[i] - 1);
        if (pool->id[i] == id) {
            // round up size to alignment for real memory usage
            pool->size[i] = (size + alignment - 1) & ~(alignment - 1);
            pool->alignment[i] = alignment;
            return;
        }
        offset += pool->size[i];
    }
    offset = (offset + alignment - 1) & ~(alignment - 1);
    pool->id[pool->count] = id;
    pool->size[pool->count] =
        (size + alignment - 1) & ~(alignment - 1); // rounded up
    pool->alignment[pool->count] = alignment;
    pool->offset[pool->count] = offset;
    pool->count++;
}

static inline void war_pool_default(war_pool_context* pool,
                                    war_config_context* config) {
    pool->version = WAR_POOL_H_VERSION;
    //-------------------------------------------------------------------------
    // AUDIO
    //-------------------------------------------------------------------------
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CTX_PW,
                 sizeof(war_pipewire_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CTX_PW_PLAY_BUILDER_DATA,
                 sizeof(uint8_t) * (config->A_BUILDER_DATA_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CTX_PW_CAPTURE_BUILDER_DATA,
                 sizeof(uint8_t) * (config->A_BUILDER_DATA_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CTX_PW_PLAY_DATA,
                 sizeof(void*) * (config->A_PLAY_DATA_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CTX_PW_CAPTURE_DATA,
                 sizeof(void*) * (config->A_CAPTURE_DATA_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CONTROL_PAYLOAD,
                 sizeof(uint8_t) * (config->PC_CONTROL_BUFFER_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_TMP_CONTROL_PAYLOAD,
                 sizeof(uint8_t) * (config->PC_CONTROL_BUFFER_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_PC_CONTROL_CMD,
                 sizeof(void*) * (config->CMD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_PLAY_READ_COUNT,
                 sizeof(uint64_t) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_PLAY_LAST_READ_TIME,
                 sizeof(uint64_t) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CAPTURE_READ_COUNT,
                 sizeof(uint64_t) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_AUDIO_CAPTURE_LAST_READ_TIME,
                 sizeof(uint64_t) * (1),
                 32);
    //-------------------------------------------------------------------------
    // MAIN
    //-------------------------------------------------------------------------
    // ctx sequence
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_SEQUENCE,
                 sizeof(war_sequence_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_SEQUENCE_PTRS,
                 sizeof(void*) * (config->NOTE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_SEQUENCE_SEQUENCE,
                 sizeof(war_sequence_entry) * (config->NOTE_GRID_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_SEQUENCE_BUFFER_CAPACITY,
                 sizeof(uint32_t) * (config->NOTE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_SEQUENCE_STAGE,
                 sizeof(void*) * (config->NOTE_COUNT),
                 32);
    // ctx hud
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD,
                 sizeof(war_hud_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_PTRS,
                 sizeof(void*) * (config->HUD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_X_SECONDS,
                 sizeof(double) * (config->HUD_STATUS_BOTTOM_INSTANCE_MAX +
                                   config->HUD_STATUS_TOP_INSTANCE_MAX +
                                   config->HUD_STATUS_MIDDLE_INSTANCE_MAX +
                                   config->HUD_LINE_NUMBERS_INSTANCE_MAX +
                                   config->HUD_PIANO_INSTANCE_MAX +
                                   config->HUD_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_Y_CELLS,
                 sizeof(double) * (config->HUD_STATUS_BOTTOM_INSTANCE_MAX +
                                   config->HUD_STATUS_TOP_INSTANCE_MAX +
                                   config->HUD_STATUS_MIDDLE_INSTANCE_MAX +
                                   config->HUD_LINE_NUMBERS_INSTANCE_MAX +
                                   config->HUD_PIANO_INSTANCE_MAX +
                                   config->HUD_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_WIDTH_SECONDS,
                 sizeof(double) * (config->HUD_STATUS_BOTTOM_INSTANCE_MAX +
                                   config->HUD_STATUS_TOP_INSTANCE_MAX +
                                   config->HUD_STATUS_MIDDLE_INSTANCE_MAX +
                                   config->HUD_LINE_NUMBERS_INSTANCE_MAX +
                                   config->HUD_PIANO_INSTANCE_MAX +
                                   config->HUD_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_HEIGHT_CELLS,
                 sizeof(double) * (config->HUD_STATUS_BOTTOM_INSTANCE_MAX +
                                   config->HUD_STATUS_TOP_INSTANCE_MAX +
                                   config->HUD_STATUS_MIDDLE_INSTANCE_MAX +
                                   config->HUD_LINE_NUMBERS_INSTANCE_MAX +
                                   config->HUD_PIANO_INSTANCE_MAX +
                                   config->HUD_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CAPACITY,
                 sizeof(uint32_t) * (config->HUD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_STAGE,
                 sizeof(void*) * (config->HUD_COUNT),
                 32);
    // ctx hud line
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE,
                 sizeof(war_hud_line_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_PTRS,
                 sizeof(void*) * (config->HUD_LINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_X_SECONDS,
                 sizeof(double) * (config->HUD_LINE_NUMBERS_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_Y_CELLS,
                 sizeof(double) * (config->HUD_LINE_NUMBERS_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_WIDTH_SECONDS,
                 sizeof(double) * (config->HUD_LINE_NUMBERS_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_LINE_WIDTH_SECONDS,
                 sizeof(double) * (config->HUD_LINE_NUMBERS_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_HEIGHT_CELLS,
                 sizeof(double) * (config->HUD_LINE_NUMBERS_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_CAPACITY,
                 sizeof(uint32_t) * (config->HUD_LINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_LINE_STAGE,
                 sizeof(void*) * (config->HUD_LINE_COUNT),
                 32);
    // ctx cursor
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR,
                 sizeof(war_cursor_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_PTRS,
                 sizeof(void*) * (config->CURSOR_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_X_SECONDS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_Y_CELLS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_WIDTH_SECONDS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_HEIGHT_CELLS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_X_SECONDS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_Y_CELLS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_WIDTH_SECONDS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_HEIGHT_CELLS,
                 sizeof(double) * (config->CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_CAPACITY,
                 sizeof(uint32_t) * (config->CURSOR_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CURSOR_STAGE,
                 sizeof(void*) * (config->CURSOR_COUNT),
                 32);
    // ctx hud cursor
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR,
                 sizeof(war_hud_cursor_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_PTRS,
                 sizeof(void*) * (config->HUD_CURSOR_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_X_SECONDS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_Y_CELLS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_WIDTH_SECONDS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_HEIGHT_CELLS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_X_SECONDS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_Y_CELLS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_WIDTH_SECONDS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_HEIGHT_CELLS,
                 sizeof(double) * (config->HUD_CURSOR_DEFAULT_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_CAPACITY,
                 sizeof(uint32_t) * (config->HUD_CURSOR_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_STAGE,
                 sizeof(void*) * (config->HUD_CURSOR_COUNT),
                 32);
    // ctx line
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE,
                 sizeof(war_line_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_PTRS,
                 sizeof(void*) * (config->LINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_X_SECONDS,
                 sizeof(double) * (config->LINE_CELL_INSTANCE_MAX +
                                   config->LINE_BPM_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_Y_CELLS,
                 sizeof(double) * (config->LINE_CELL_INSTANCE_MAX +
                                   config->LINE_BPM_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_WIDTH_SECONDS,
                 sizeof(double) * (config->LINE_CELL_INSTANCE_MAX +
                                   config->LINE_BPM_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_LINE_WIDTH_SECONDS,
                 sizeof(double) * (config->LINE_CELL_INSTANCE_MAX +
                                   config->LINE_BPM_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_HEIGHT_CELLS,
                 sizeof(double) * (config->LINE_CELL_INSTANCE_MAX +
                                   config->LINE_BPM_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_CAPACITY,
                 sizeof(uint32_t) * (config->LINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_LINE_STAGE,
                 sizeof(void*) * (config->LINE_COUNT),
                 32);
    // ctx text
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT,
                 sizeof(war_text_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT_PTRS,
                 sizeof(void*) * (config->TEXT_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT_X_SECONDS,
                 sizeof(double) *
                     (config->TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->TEXT_PIANO_INSTANCE_MAX +
                      config->TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_ERROR_INSTANCE_MAX +
                      config->TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT_Y_CELLS,
                 sizeof(double) *
                     (config->TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->TEXT_PIANO_INSTANCE_MAX +
                      config->TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_ERROR_INSTANCE_MAX +
                      config->TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT_WIDTH_SECONDS,
                 sizeof(double) *
                     (config->TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->TEXT_PIANO_INSTANCE_MAX +
                      config->TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_ERROR_INSTANCE_MAX +
                      config->TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT_HEIGHT_CELLS,
                 sizeof(double) *
                     (config->TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->TEXT_PIANO_INSTANCE_MAX +
                      config->TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->TEXT_ERROR_INSTANCE_MAX +
                      config->TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT_CAPACITY,
                 sizeof(uint32_t) * (config->TEXT_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_TEXT_STAGE,
                 sizeof(void*) * (config->TEXT_COUNT),
                 32);
    // ctx hud text
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT,
                 sizeof(war_hud_text_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT_PTRS,
                 sizeof(void*) * (config->HUD_TEXT_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT_X_SECONDS,
                 sizeof(double) *
                     (config->HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->HUD_TEXT_PIANO_INSTANCE_MAX +
                      config->HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_ERROR_INSTANCE_MAX +
                      config->HUD_TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT_Y_CELLS,
                 sizeof(double) *
                     (config->HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->HUD_TEXT_PIANO_INSTANCE_MAX +
                      config->HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_ERROR_INSTANCE_MAX +
                      config->HUD_TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT_WIDTH_SECONDS,
                 sizeof(double) *
                     (config->HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->HUD_TEXT_PIANO_INSTANCE_MAX +
                      config->HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_ERROR_INSTANCE_MAX +
                      config->HUD_TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT_HEIGHT_CELLS,
                 sizeof(double) *
                     (config->HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_TOP_INSTANCE_MAX +
                      config->HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX +
                      config->HUD_TEXT_PIANO_INSTANCE_MAX +
                      config->HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX +
                      config->HUD_TEXT_ERROR_INSTANCE_MAX +
                      config->HUD_TEXT_EXPLORE_INSTANCE_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT_CAPACITY,
                 sizeof(uint32_t) * (config->HUD_TEXT_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_HUD_TEXT_STAGE,
                 sizeof(void*) * (config->HUD_TEXT_COUNT),
                 32);
    // misc context
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_MISC,
                 sizeof(war_misc_context) * (1),
                 32);
    // nsgt context
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT,
                 sizeof(war_nsgt_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_MEMORY_PROPERTY_FLAGS,
                 sizeof(VkMemoryPropertyFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_USAGE_FLAGS,
                 sizeof(VkBufferUsageFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_BUFFER,
                 sizeof(VkBuffer) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_MEMORY_REQUIREMENTS,
                 sizeof(VkMemoryRequirements) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_DEVICE_MEMORY,
                 sizeof(VkDeviceMemory) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_MAP,
                 sizeof(void*) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_CAPACITY,
                 sizeof(VkDeviceSize) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_BUFFER_INFO,
                 sizeof(VkDescriptorBufferInfo) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_COMPUTE_DESCRIPTOR_IMAGE_INFO,
                 sizeof(VkDescriptorImageInfo) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_GRAPHICS_DESCRIPTOR_IMAGE_INFO,
                 sizeof(VkDescriptorImageInfo) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE,
                 sizeof(VkImage) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_VIEW,
                 sizeof(VkImageView) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FORMAT,
                 sizeof(VkFormat) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_EXTENT_3D,
                 sizeof(VkExtent3D) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_USAGE_FLAGS,
                 sizeof(VkImageUsageFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_STAGE_FLAGS,
                 sizeof(VkShaderStageFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_SET_LAYOUT_BINDING,
                 sizeof(VkDescriptorSetLayoutBinding) *
                     (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_WRITE_DESCRIPTOR_SET,
                 sizeof(VkWriteDescriptorSet) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_MAPPED_MEMORY_RANGE,
                 sizeof(VkMappedMemoryRange) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_BUFFER_MEMORY_BARRIER,
                 sizeof(VkBufferMemoryBarrier) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_MEMORY_BARRIER,
                 sizeof(VkImageMemoryBarrier) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_LAYOUT,
                 sizeof(VkImageLayout) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_IMAGE_LAYOUT,
                 sizeof(VkImageLayout) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_ACCESS_FLAGS,
                 sizeof(VkAccessFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_ACCESS_FLAGS,
                 sizeof(VkAccessFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_STAGE_FLAGS,
                 sizeof(VkPipelineStageFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_PIPELINE_STAGE_FLAGS,
                 sizeof(VkPipelineStageFlags) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_SRC_IDX,
                 sizeof(uint32_t) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_DST_IDX,
                 sizeof(uint32_t) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_SIZE,
                 sizeof(VkDeviceSize) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_SRC_OFFSET,
                 sizeof(VkDeviceSize) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_DST_OFFSET,
                 sizeof(VkDeviceSize) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_DATA,
                 sizeof(uint32_t) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_FN_DATA_2,
                 sizeof(uint32_t) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_SET,
                 sizeof(VkDescriptorSet) * (config->NSGT_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_SET_LAYOUT,
                 sizeof(VkDescriptorSetLayout) *
                     (config->NSGT_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_DESCRIPTOR_TYPE,
                 sizeof(VkDescriptorType) * (config->NSGT_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_POOL,
                 sizeof(VkDescriptorPool) * (config->NSGT_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_IMAGE_LAYOUT,
                 sizeof(VkImageLayout) * (config->NSGT_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_MODULE,
                 sizeof(VkShaderModule) * (config->NSGT_SHADER_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_SHADER_STAGE_CREATE_INFO,
                 sizeof(VkPipelineShaderStageCreateInfo) *
                     (config->NSGT_SHADER_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_STAGE_FLAG_BITS,
                 sizeof(VkShaderStageFlagBits) * (config->NSGT_SHADER_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_PATH,
                 sizeof(char) *
                     (config->NSGT_SHADER_COUNT * config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE,
                 sizeof(VkPipeline) * (config->NSGT_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_LAYOUT,
                 sizeof(VkPipelineLayout) * (config->NSGT_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_SET_IDX,
                 sizeof(uint32_t) * (config->NSGT_PIPELINE_COUNT *
                                     config->NSGT_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_SHADER_IDX,
                 sizeof(uint32_t) *
                     (config->NSGT_PIPELINE_COUNT * config->NSGT_SHADER_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_STRUCTURE_TYPE,
                 sizeof(VkStructureType) * (config->NSGT_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PUSH_CONSTANT_SHADER_STAGE_FLAGS,
                 sizeof(VkShaderStageFlags) * (config->NSGT_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PUSH_CONSTANT_SIZE,
                 sizeof(uint32_t) * (config->NSGT_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_BIND_POINT,
                 sizeof(VkPipelineBindPoint) * (config->NSGT_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_SIZE,
                 sizeof(uint32_t) * (config->NSGT_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_DISPATCH_GROUP,
                 sizeof(uint32_t) *
                     (config->NSGT_PIPELINE_COUNT * config->NSGT_GROUPS),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_LOCAL_SIZE,
                 sizeof(uint32_t) *
                     (config->NSGT_PIPELINE_COUNT * config->NSGT_GROUPS),
                 32);
    // new_vulkan context
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN,
                 sizeof(war_new_vulkan_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MEMORY_PROPERTY_FLAGS,
                 sizeof(VkMemoryPropertyFlags) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_USAGE_FLAGS,
                 sizeof(VkBufferUsageFlags) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER,
                 sizeof(VkBuffer) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MEMORY_REQUIREMENTS,
                 sizeof(VkMemoryRequirements) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DEVICE_MEMORY,
                 sizeof(VkDeviceMemory) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MAP,
                 sizeof(void*) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_CAPACITY,
                 sizeof(VkDeviceSize) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_BUFFER_INFO,
                 sizeof(VkDescriptorBufferInfo) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_COMPUTE_DESCRIPTOR_IMAGE_INFO,
                 sizeof(VkDescriptorImageInfo) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_GRAPHICS_DESCRIPTOR_IMAGE_INFO,
                 sizeof(VkDescriptorImageInfo) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE,
                 sizeof(VkImage) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_VIEW,
                 sizeof(VkImageView) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FORMAT,
                 sizeof(VkFormat) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_EXTENT_3D,
                 sizeof(VkExtent3D) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_USAGE_FLAGS,
                 sizeof(VkImageUsageFlags) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_STAGE_FLAGS,
                 sizeof(VkShaderStageFlags) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDING,
                 sizeof(VkDescriptorSetLayoutBinding) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_WRITE_DESCRIPTOR_SET,
                 sizeof(VkWriteDescriptorSet) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MAPPED_MEMORY_RANGE,
                 sizeof(VkMappedMemoryRange) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_MEMORY_BARRIER,
                 sizeof(VkBufferMemoryBarrier) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_MEMORY_BARRIER,
                 sizeof(VkImageMemoryBarrier) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_LAYOUT,
                 sizeof(VkImageLayout) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_IMAGE_LAYOUT,
                 sizeof(VkImageLayout) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_ACCESS_FLAGS,
                 sizeof(VkAccessFlags) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_ACCESS_FLAGS,
                 sizeof(VkAccessFlags) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_STAGE_FLAGS,
                 sizeof(VkPipelineStageFlags) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PIPELINE_STAGE_FLAGS,
                 sizeof(VkPipelineStageFlags) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SRC_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DST_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SIZE,
                 sizeof(VkDeviceSize) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SIZE_2,
                 sizeof(VkDeviceSize) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SRC_OFFSET,
                 sizeof(VkDeviceSize) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DST_OFFSET,
                 sizeof(VkDeviceSize) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DATA,
                 sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DATA_2,
                 sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_SET,
                 sizeof(VkDescriptorSet) *
                     (config->NEW_VULKAN_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_SET_LAYOUT,
                 sizeof(VkDescriptorSetLayout) *
                     (config->NEW_VULKAN_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_DESCRIPTOR_TYPE,
                 sizeof(VkDescriptorType) *
                     (config->NEW_VULKAN_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_POOL,
                 sizeof(VkDescriptorPool) *
                     (config->NEW_VULKAN_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_IMAGE_LAYOUT,
                 sizeof(VkImageLayout) *
                     (config->NEW_VULKAN_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_MODULE,
                 sizeof(VkShaderModule) * (config->NEW_VULKAN_SHADER_COUNT),
                 32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_SHADER_STAGE_CREATE_INFO,
        sizeof(VkPipelineShaderStageCreateInfo) *
            (config->NEW_VULKAN_SHADER_COUNT),
        32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_STAGE_FLAG_BITS,
                 sizeof(VkShaderStageFlagBits) *
                     (config->NEW_VULKAN_SHADER_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_PATH,
                 sizeof(char) * (config->NEW_VULKAN_SHADER_COUNT *
                                 config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE,
                 sizeof(VkPipeline) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_LAYOUT,
                 sizeof(VkPipelineLayout) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_SET_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_SHADER_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_SHADER_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_STRUCTURE_TYPE,
                 sizeof(VkStructureType) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PUSH_CONSTANT_SHADER_STAGE_FLAGS,
        sizeof(VkShaderStageFlags) * (config->NEW_VULKAN_PIPELINE_COUNT),
        32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PUSH_CONSTANT_SIZE,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_BIND_POINT,
                 sizeof(VkPipelineBindPoint) *
                     (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SIZE,
                 sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_DISPATCH_GROUP,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_GROUPS),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_LOCAL_SIZE,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_GROUPS),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VIEWPORT,
                 sizeof(VkViewport) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_RECT_2D,
                 sizeof(VkRect2D) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IN_DESCRIPTOR_SET,
                 sizeof(uint8_t) * (config->NEW_VULKAN_RESOURCE_COUNT *
                                    config->NEW_VULKAN_DESCRIPTOR_SET_COUNT),
                 32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_INPUT_BINDING_DESCRIPTION,
        sizeof(VkVertexInputBindingDescription) *
            (config->NEW_VULKAN_RESOURCE_COUNT),
        32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION,
        sizeof(VkVertexInputAttributeDescription*) *
            (config->NEW_VULKAN_RESOURCE_COUNT),
        32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_INPUT_RATE,
                 sizeof(VkVertexInputRate) *
                     (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_ATTRIBUTE_COUNT,
                 sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_STRIDE,
                 sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_OFFSETS,
                 sizeof(uint32_t*) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_FORMATS,
                 sizeof(VkFormat*) * (config->NEW_VULKAN_RESOURCE_COUNT),
                 32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_VERTEX_INPUT_BINDING_IDX,
        sizeof(uint32_t) * (config->NEW_VULKAN_RESOURCE_COUNT *
                            config->NEW_VULKAN_PIPELINE_COUNT),
        32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_GLYPH_INFO,
                 sizeof(war_glyph_info) * (config->NEW_VULKAN_GLYPH_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_PUSH_CONSTANT,
                 sizeof(void*) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        sizeof(VkPipelineDepthStencilStateCreateInfo) *
            (config->NEW_VULKAN_PIPELINE_COUNT),
        32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        sizeof(VkPipelineColorBlendStateCreateInfo) *
            (config->NEW_VULKAN_PIPELINE_COUNT),
        32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_COLOR_BLEND_ATTACHMENT_STATE,
        sizeof(VkPipelineColorBlendAttachmentState) *
            (config->NEW_VULKAN_PIPELINE_COUNT),
        32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_INSTANCE_STAGE_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_Z_LAYER,
                 sizeof(float) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DIRTY_BUFFER,
                 sizeof(uint8_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                    config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DRAW_BUFFER,
                 sizeof(uint8_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                    config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_INSTANCE_COUNT,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_FIRST_INSTANCE,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_PIPELINE_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_FIRST_INSTANCE,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_INSTANCE_COUNT,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_SIZE,
                 sizeof(VkDeviceSize) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                         config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_SRC_OFFSET,
                 sizeof(VkDeviceSize) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                         config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_ACCESS_FLAGS,
                 sizeof(VkAccessFlags) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                          config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_PIPELINE_STAGE_FLAGS,
        sizeof(VkPipelineStageFlags) *
            (config->NEW_VULKAN_PIPELINE_COUNT * config->NEW_VULKAN_BUFFER_MAX),
        32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_IMAGE_LAYOUT,
                 sizeof(VkImageLayout) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                          config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_SRC_RESOURCE_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_DST_RESOURCE_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PIPELINE_IDX,
                 sizeof(uint32_t) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_X,
                 sizeof(double) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_Y,
                 sizeof(double) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_FACTOR_X,
                 sizeof(double) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_FACTOR_Y,
                 sizeof(double) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PTRS_BUFFER_PUSH_CONSTANT,
                 sizeof(void*) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_NOTE,
                 sizeof(war_new_vulkan_note_push_constant) *
                     (config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_TEXT,
                 sizeof(war_new_vulkan_text_push_constant) *
                     (config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_LINE,
                 sizeof(war_new_vulkan_line_push_constant) *
                     (config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_CURSOR,
                 sizeof(war_new_vulkan_cursor_push_constant) *
                     (config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD,
                 sizeof(war_new_vulkan_hud_push_constant) *
                     (config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(
        pool,
        config,
        WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD_CURSOR,
        sizeof(war_new_vulkan_hud_cursor_push_constant) *
            (config->NEW_VULKAN_BUFFER_MAX),
        32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD_TEXT,
                 sizeof(war_new_vulkan_hud_text_push_constant) *
                     (config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD_LINE,
                 sizeof(war_new_vulkan_hud_line_push_constant) *
                     (config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PTRS_BUFFER_VIEWPORT,
                 sizeof(void*) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_VIEWPORT,
                 sizeof(VkViewport) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                       config->NEW_VULKAN_BUFFER_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PTRS_BUFFER_RECT_2D,
                 sizeof(void*) * (config->NEW_VULKAN_PIPELINE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_RECT_2D,
                 sizeof(VkRect2D) * (config->NEW_VULKAN_PIPELINE_COUNT *
                                     config->NEW_VULKAN_BUFFER_MAX),
                 32);
    // env
    war_pool_set(pool, config, WAR_POOL_ID_ENV, sizeof(war_env) * (1), 32);
    // command context
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_COMMAND,
                 sizeof(war_command_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_COMMAND_INPUT,
                 sizeof(int) * (config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_COMMAND_TEXT,
                 sizeof(char) * (config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_COMMAND_PROMPT,
                 sizeof(char) * (config->CONFIG_PATH_MAX),
                 32);
    // char input
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CHAR_INPUT,
                 sizeof(char) * (config->CONFIG_PATH_MAX * 2),
                 32);
    // play context
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_PLAY,
                 sizeof(war_play_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_PLAY_KEY_LAYERS,
                 sizeof(uint64_t) * (config->A_NOTE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_PLAY_KEYS,
                 sizeof(uint8_t) * (config->A_NOTE_COUNT),
                 32);
    // capture context
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_CAPTURE,
                 sizeof(war_capture_context) * (1),
                 32);
    // capture_wav
    war_pool_set(
        pool, config, WAR_POOL_ID_MAIN_CAPTURE_WAV, sizeof(war_file) * (1), 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CAPTURE_WAV_FNAME,
                 sizeof(char) * (config->CONFIG_PATH_MAX),
                 32);
    // color
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_COLOR,
                 sizeof(war_color_context) * (1),
                 32);
    // layres
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_LAYERS_ACTIVE,
                 sizeof(char) * (config->A_LAYER_COUNT),
                 32);
    // Colors
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_COLORS,
                 sizeof(uint32_t) * (config->A_LAYER_COUNT),
                 32);
    // views
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_COL,
                 sizeof(uint32_t) * (config->WR_VIEWS_SAVED),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_ROW,
                 sizeof(uint32_t) * (config->WR_VIEWS_SAVED),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_LEFT_COL,
                 sizeof(uint32_t) * (config->WR_VIEWS_SAVED),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_RIGHT_COL,
                 sizeof(uint32_t) * (config->WR_VIEWS_SAVED),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_BOTTOM_ROW,
                 sizeof(uint32_t) * (config->WR_VIEWS_SAVED),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_TOP_ROW,
                 sizeof(uint32_t) * (config->WR_VIEWS_SAVED),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_WARPOON_TEXT,
                 sizeof(char*) * (config->WR_VIEWS_SAVED),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_VIEWS_WARPOON_TEXT_ROWS,
                 sizeof(char) *
                     (config->WR_VIEWS_SAVED * config->WR_WARPOON_TEXT_COLS),
                 32);
    // FSM CONTEXT
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM,
                 sizeof(war_fsm_context) * (1),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_FUNCTION,
                 sizeof(war_function_union) *
                     (config->WR_STATES * config->WR_MODE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_FUNCTION_TYPE,
                 sizeof(uint8_t) * (config->WR_STATES * config->WR_MODE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_NAME,
                 sizeof(char) * (config->WR_STATES * config->WR_MODE_COUNT *
                                 config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_IS_TERMINAL,
                 sizeof(uint8_t) * (config->WR_STATES * config->WR_MODE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_HANDLE_RELEASE,
                 sizeof(uint8_t) * (config->WR_STATES * config->WR_MODE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_HANDLE_TIMEOUT,
                 sizeof(uint8_t) * (config->WR_STATES * config->WR_MODE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_HANDLE_REPEAT,
                 sizeof(uint8_t) * (config->WR_STATES * config->WR_MODE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_IS_PREFIX,
                 sizeof(uint8_t) * (config->WR_STATES * config->WR_MODE_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_NEXT_STATE,
                 sizeof(uint64_t) *
                     (config->WR_STATES * config->WR_KEYSYM_COUNT *
                      config->WR_MOD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_CWD,
                 sizeof(char) * (config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_CURRENT_FILE_PATH,
                 sizeof(char) * (config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_EXT,
                 sizeof(char) * (config->CONFIG_PATH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_KEY_DOWN,
                 sizeof(uint8_t) *
                     (config->WR_KEYSYM_COUNT * config->WR_MOD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_FSM_KEY_LAST_EVENT_US,
                 sizeof(uint64_t) *
                     (config->WR_KEYSYM_COUNT * config->WR_MOD_COUNT),
                 32);
    // note quads
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_ALIVE,
                 sizeof(uint8_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_ID,
                 sizeof(uint64_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_POS_X,
                 sizeof(double) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_POS_Y,
                 sizeof(double) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_LAYER,
                 sizeof(uint64_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_SIZE_X,
                 sizeof(double) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_NAVIGATION_X,
                 sizeof(double) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_NAVIGATION_X_NUMERATOR,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_NAVIGATION_X_DENOMINATOR,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_SIZE_X_NUMERATOR,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_SIZE_X_DENOMINATOR,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_COLOR,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_OUTLINE_COLOR,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_GAIN,
                 sizeof(float) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_VOICE,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_HIDDEN,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_NOTE_QUADS_MUTE,
                 sizeof(uint32_t) * (config->WR_NOTE_QUADS_MAX),
                 32);
    // keydown
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_KEY_DOWN,
                 sizeof(bool) *
                     (config->WR_KEYSYM_COUNT * config->WR_MOD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_KEY_LAST_EVENT_US,
                 sizeof(uint64_t) *
                     (config->WR_KEYSYM_COUNT * config->WR_MOD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_MSG_BUFFER,
                 sizeof(uint8_t) * (config->WR_WAYLAND_MSG_BUFFER_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_OBJ_OP,
                 sizeof(void*) * (config->WR_WAYLAND_MAX_OBJECTS *
                                  config->WR_WAYLAND_MAX_OP_CODES),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_PC_CONTROL_CMD,
                 sizeof(void*) * (config->CMD_COUNT),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CONTROL_PAYLOAD,
                 sizeof(uint8_t) * (config->PC_CONTROL_BUFFER_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_TMP_CONTROL_PAYLOAD,
                 sizeof(uint8_t) * (config->PC_CONTROL_BUFFER_SIZE),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_INPUT_SEQUENCE,
                 sizeof(char) * (config->WR_INPUT_SEQUENCE_LENGTH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_PROMPT,
                 sizeof(char) * (config->WR_INPUT_SEQUENCE_LENGTH_MAX),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CWD,
                 sizeof(char) * (config->CONFIG_PATH_MAX),
                 32);
    // ctx config
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_CONFIG_CONTEXT,
                 sizeof(war_config_context),
                 32);
    // ctx keymap
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_KEYMAP,
                 sizeof(war_keymap_context),
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_KEYMAP_FUNCTION,
                 sizeof(void*) * config->KEYMAP_STATE_CAPACITY *
                     config->KEYMAP_FUNCTION_CAPACITY,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_KEYMAP_FUNCTION_COUNT,
                 sizeof(uint8_t) * config->KEYMAP_STATE_CAPACITY,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_KEYMAP_FLAGS,
                 sizeof(war_keymap_flags) * config->KEYMAP_STATE_CAPACITY,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_MAIN_CTX_KEYMAP_NEXT_STATE,
                 sizeof(uint64_t) * config->KEYMAP_STATE_CAPACITY *
                     config->KEYMAP_MOD_CAPACITY *
                     config->KEYMAP_KEYSYM_CAPACITY,
                 32);
    // hot context
    war_pool_set(
        pool, config, WAR_POOL_ID_HOT_CONTEXT, sizeof(war_hot_context), 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_HOT_CONTEXT_FUNCTION,
                 sizeof(void*) * WAR_HOT_ID_COUNT,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_HOT_CONTEXT_HANDLE,
                 sizeof(void*) * WAR_HOT_ID_COUNT,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_HOT_CONTEXT_FN_ID,
                 sizeof(war_hot_id) * WAR_HOT_ID_COUNT,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_HOT_CONTEXT_NAME,
                 sizeof(char*) * WAR_HOT_ID_COUNT,
                 32);
    // pool context
    war_pool_set(
        pool, config, WAR_POOL_ID_POOL_CONTEXT, sizeof(war_pool_context), 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_POOL_CONTEXT_SIZE,
                 sizeof(uint64_t) * config->POOL_MAX_ALLOCATIONS,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_POOL_CONTEXT_OFFSET,
                 sizeof(uint64_t) * config->POOL_MAX_ALLOCATIONS,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_POOL_CONTEXT_ALIGNMENT,
                 sizeof(uint32_t) * config->POOL_MAX_ALLOCATIONS,
                 32);
    war_pool_set(pool,
                 config,
                 WAR_POOL_ID_POOL_CONTEXT_ID,
                 sizeof(war_pool_id) * config->POOL_MAX_ALLOCATIONS,
                 32);
}

void war_pool_override(war_pool_context* pool, war_config_context* config);

#endif // WAR_POOL_H
