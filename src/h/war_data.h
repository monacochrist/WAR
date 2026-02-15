//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_data.h
//-----------------------------------------------------------------------------

#ifndef WAR_DATA_H
#define WAR_DATA_H

#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <ft2build.h>
#include <locale.h>
#include <luajit-2.1/lauxlib.h>
#include <luajit-2.1/lua.h>
#include <luajit-2.1/lualib.h>
#include <pipewire/stream.h>
#include <spa-0.2/spa/param/audio/raw.h>
#include <spa-0.2/spa/pod/builder.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include FT_FREETYPE_H

enum war_mods {
    MOD_NONE = 0,
    MOD_SHIFT = (1 << 0),
    MOD_CTRL = (1 << 1),
    MOD_ALT = (1 << 2),
    MOD_LOGO = (1 << 3),
    MOD_CAPS = (1 << 4),
    MOD_NUM = (1 << 5),
    MOD_FN = (1 << 6),
};

enum war_misc {
    max_objects = 1000,
    max_opcodes = 20,
    max_instances_per_quad = 1,
    max_instances_per_sdf_quad = 1,
    max_fds = 50,
    OLED_MODE = 0,
    MAX_MIDI_NOTES = 128,
    MAX_SAMPLES_PER_NOTE = 128,
    UNSET = 0,
    MAX_DIGITS = 10,
    NUM_STATUS_BARS = 3,
    MAX_GRIDLINE_SPLITS = 4,
    MAX_VIEWS_SAVED = 13,
    MAX_WARPOON_TEXT_COLS = 25,
    MAX_STATUS_BAR_COLS = 200,
    PROMPT_LAYER = 1,
    PROMPT_NOTE = 2,
    PROMPT_NAME = 3,
    ALL_NOTE_LAYERS = -13,
    ARGB8888 = 0,
};

enum war_hud {
    HUD_PIANO = 0,
    HUD_LINE_NUMBERS = 1,
    HUD_PIANO_AND_LINE_NUMBERS = 2,
};

enum war_fsm {
    MAX_NODES = 1024,
    MAX_SEQUENCE_LENGTH = 7,
    MAX_CHILDREN = 32,
    SEQUENCE_COUNT = 140,
    MAX_STATES = 256,
    MAX_COMMAND_BUFFER_LENGTH = 128,
};

enum war_pipelines {
    PIPELINE_NONE = 0,
    PIPELINE_QUAD = 1,
    PIPELINE_SDF = 2,
    PIPELINE_NSGT = 3,
};

enum war_cursor {
    CURSOR_BLINK_BPM = 1,
    CURSOR_BLINK = 2,
    DEFAULT_CURSOR_BLINK_DURATION = 700000,
};

enum war_undo_commands {
    CMD_ADD_NOTE = 0,
    CMD_DELETE_NOTE = 1,
    CMD_ADD_NOTES = 2,
    CMD_DELETE_NOTES = 3,
    CMD_SWAP_ADD_NOTES = 4,
    CMD_SWAP_DELETE_NOTES = 5,
    CMD_ADD_NOTES_SAME = 6,
    CMD_DELETE_NOTES_SAME = 7,
};

enum war_control_commands {
    CONTROL_END_WAR = 0,
};

typedef struct __attribute__((packed)) war_riff_header {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
} war_riff_header;

typedef struct __attribute__((packed)) war_fmt_chunk {
    char subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} war_fmt_chunk;

typedef struct __attribute__((packed)) war_data_chunk {
    char subchunk2_id[4];
    uint32_t subchunk2_size;
} war_data_chunk;

typedef struct war_lua_context {
    // audio
    _Atomic int A_SAMPLE_RATE;
    _Atomic double A_SAMPLE_DURATION;
    _Atomic double A_TARGET_SAMPLES_FACTOR;
    _Atomic int A_CHANNEL_COUNT;
    _Atomic int A_NOTE_COUNT;
    _Atomic float WR_CAPTURE_THRESHOLD;
    _Atomic int A_LAYER_COUNT;
    _Atomic int A_LAYERS_IN_RAM;
    _Atomic double A_BPM;
    _Atomic double BPM_SECONDS_PER_CELL;
    _Atomic double SUBDIVISION_SECONDS_PER_CELL;
    _Atomic int A_BASE_FREQUENCY;
    _Atomic int A_SCHED_FIFO_PRIORITY;
    _Atomic int A_BASE_NOTE;
    _Atomic int A_EDO;
    _Atomic int A_NOTES_MAX;
    _Atomic float A_DEFAULT_ATTACK;
    _Atomic float A_DEFAULT_SUSTAIN;
    _Atomic float A_DEFAULT_RELEASE;
    _Atomic float A_DEFAULT_GAIN;
    _Atomic double A_DEFAULT_COLUMNS_PER_BEAT;
    _Atomic int A_BYTES_NEEDED;
    _Atomic int A_BUILDER_DATA_SIZE;
    _Atomic int A_PLAY_DATA_SIZE;
    _Atomic int A_CAPTURE_DATA_SIZE;
    _Atomic int CACHE_FILE_CAPACITY;
    _Atomic int CONFIG_PATH_MAX;
    _Atomic int A_WARMUP_FRAMES_FACTOR;
    // window render
    _Atomic int WR_VIEWS_SAVED;
    _Atomic float WR_COLOR_STEP;
    _Atomic double WR_PLAY_CALLBACK_FPS;
    _Atomic double WR_CAPTURE_CALLBACK_FPS;
    _Atomic int WR_WARPOON_TEXT_COLS;
    _Atomic int WR_STATES;
    _Atomic int WR_SEQUENCE_COUNT;
    _Atomic int WR_SEQUENCE_LENGTH_MAX;
    _Atomic int WR_CALLBACK_SIZE;
    _Atomic int WR_MODE_COUNT;
    _Atomic int WR_KEYSYM_COUNT;
    _Atomic int WR_MOD_COUNT;
    _Atomic int WR_NOTE_QUADS_MAX;
    _Atomic int WR_STATUS_BAR_COLS_MAX;
    _Atomic int WR_TEXT_QUADS_MAX;
    _Atomic int WR_QUADS_MAX;
    _Atomic char* WR_LEADER;
    _Atomic int WR_WAYLAND_MSG_BUFFER_SIZE;
    _Atomic int WR_WAYLAND_MAX_OBJECTS;
    _Atomic int WR_WAYLAND_MAX_OP_CODES;
    _Atomic int WR_FN_NAME_LIMIT;
    _Atomic int WR_UNDO_NODES_MAX;
    _Atomic int WR_UNDO_NODES_CHILDREN_MAX;
    _Atomic int WR_TIMESTAMP_LENGTH_MAX;
    _Atomic int WR_REPEAT_DELAY_US;
    _Atomic int WR_REPEAT_RATE_US;
    _Atomic int WR_CURSOR_BLINK_DURATION_US;
    _Atomic double WR_FPS;
    _Atomic int WR_UNDO_NOTES_BATCH_MAX;
    _Atomic int WR_INPUT_SEQUENCE_LENGTH_MAX;
    _Atomic int ROLL_POSITION_X_Y;
    // pool
    _Atomic int POOL_ALIGNMENT;
    // cmd
    _Atomic int CMD_COUNT;
    // pc
    _Atomic int PC_CONTROL_BUFFER_SIZE;
    _Atomic int PC_PLAY_BUFFER_SIZE;
    _Atomic int PC_CAPTURE_BUFFER_SIZE;
    // vk
    _Atomic int VK_ATLAS_WIDTH;
    _Atomic int VK_ATLAS_HEIGHT;
    _Atomic float VK_FONT_PIXEL_HEIGHT;
    _Atomic int VK_MAX_FRAMES;
    _Atomic int VK_GLYPH_COUNT;
    _Atomic int VK_NSGT_DIFF_CAPACITY;
    _Atomic int VK_ALIGNMENT;
    // nsgt
    _Atomic int NSGT_BIN_CAPACITY;
    _Atomic int NSGT_FRAME_CAPACITY;
    _Atomic int NSGT_FREQUENCY_MIN;
    _Atomic int NSGT_FREQUENCY_MAX;
    _Atomic float NSGT_ALPHA;
    _Atomic float NSGT_SHAPE_FACTOR;
    _Atomic int NSGT_WINDOW_LENGTH_MIN;
    _Atomic int NSGT_RESOURCE_COUNT;
    _Atomic int NSGT_DESCRIPTOR_SET_COUNT;
    _Atomic int NSGT_SHADER_COUNT;
    _Atomic int NSGT_PIPELINE_COUNT;
    _Atomic int NSGT_GRAPHICS_FPS;
    _Atomic int NSGT_GROUPS;
    // new_vulkan
    _Atomic int NEW_VULKAN_RESOURCE_COUNT;
    _Atomic int NEW_VULKAN_DESCRIPTOR_SET_COUNT;
    _Atomic int NEW_VULKAN_SHADER_COUNT;
    _Atomic int NEW_VULKAN_PIPELINE_COUNT;
    _Atomic int NEW_VULKAN_GROUPS;
    _Atomic int NEW_VULKAN_NOTE_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_TEXT_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_LINE_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_CURSOR_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_HUD_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_HUD_CURSOR_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_HUD_TEXT_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_HUD_LINE_INSTANCE_MAX;
    _Atomic int NEW_VULKAN_ATLAS_WIDTH;
    _Atomic int NEW_VULKAN_ATLAS_HEIGHT;
    _Atomic int NEW_VULKAN_FONT_PIXEL_HEIGHT;
    _Atomic int NEW_VULKAN_GLYPH_COUNT;
    _Atomic int NEW_VULKAN_SDF_SCALE;
    _Atomic int NEW_VULKAN_SDF_PADDING;
    _Atomic int NEW_VULKAN_BUFFER_MAX;
    _Atomic float NEW_VULKAN_SDF_RANGE;
    _Atomic float NEW_VULKAN_SDF_LARGE;
    // cache nsgt
    _Atomic int CACHE_NSGT_CAPACITY;
    // nsgt visual
    _Atomic int VK_NSGT_VISUAL_QUAD_CAPACITY;
    _Atomic int VK_NSGT_VISUAL_RESOURCE_COUNT;
    // misc
    _Atomic float DEFAULT_ALPHA_SCALE;
    _Atomic float DEFAULT_CURSOR_ALPHA_SCALE;
    _Atomic float DEFAULT_PLAYBACK_BAR_THICKNESS;
    _Atomic float DEFAULT_TEXT_FEATHER;
    _Atomic float DEFAULT_TEXT_THICKNESS;
    _Atomic float DEFAULT_BOLD_TEXT_FEATHER;
    _Atomic float DEFAULT_BOLD_TEXT_THICKNESS;
    _Atomic float WINDOWED_TEXT_FEATHER;
    _Atomic float WINDOWED_TEXT_THICKNESS;
    _Atomic float DEFAULT_WINDOWED_ALPHA_SCALE;
    _Atomic float DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE;
    // hud
    _Atomic int HUD_COUNT;
    _Atomic int HUD_STATUS_BOTTOM_INSTANCE_MAX;
    _Atomic int HUD_STATUS_TOP_INSTANCE_MAX;
    _Atomic int HUD_STATUS_MIDDLE_INSTANCE_MAX;
    _Atomic int HUD_LINE_NUMBERS_INSTANCE_MAX;
    _Atomic int HUD_PIANO_INSTANCE_MAX;
    _Atomic int HUD_EXPLORE_INSTANCE_MAX;
    // cursor
    _Atomic int CURSOR_COUNT;
    _Atomic int CURSOR_DEFAULT_INSTANCE_MAX;
    // line
    _Atomic int LINE_COUNT;
    _Atomic int LINE_CELL_INSTANCE_MAX;
    _Atomic int LINE_BPM_INSTANCE_MAX;
    // text
    _Atomic int TEXT_COUNT;
    _Atomic int TEXT_STATUS_BOTTOM_INSTANCE_MAX;
    _Atomic int TEXT_STATUS_TOP_INSTANCE_MAX;
    _Atomic int TEXT_STATUS_MIDDLE_INSTANCE_MAX;
    _Atomic int TEXT_PIANO_INSTANCE_MAX;
    _Atomic int TEXT_LINE_NUMBERS_INSTANCE_MAX;
    _Atomic int TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX;
    _Atomic int TEXT_EXPLORE_INSTANCE_MAX;
    _Atomic int TEXT_ERROR_INSTANCE_MAX;
    // sequence
    _Atomic int NOTE_COUNT;
    _Atomic int NOTE_GRID_INSTANCE_MAX;
    // hud line
    _Atomic int HUD_LINE_COUNT;
    _Atomic int HUD_LINE_PIANO_INSTANCE_MAX;
    // hud text
    _Atomic int HUD_TEXT_COUNT;
    _Atomic int HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX;
    _Atomic int HUD_TEXT_STATUS_TOP_INSTANCE_MAX;
    _Atomic int HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX;
    _Atomic int HUD_TEXT_PIANO_INSTANCE_MAX;
    _Atomic int HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX;
    _Atomic int HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX;
    _Atomic int HUD_TEXT_EXPLORE_INSTANCE_MAX;
    _Atomic int HUD_TEXT_ERROR_INSTANCE_MAX;
    // hud cursor
    _Atomic int HUD_CURSOR_COUNT;
    _Atomic int HUD_CURSOR_DEFAULT_INSTANCE_MAX;
    //_Atomic(char*) CWD
    // state
    lua_State* L;
} war_lua_context;

typedef struct war_atomics {
    _Atomic uint64_t play_clock;
    _Atomic uint64_t play_frames;
    _Atomic uint64_t capture_frames;
    _Atomic float capture_threshold;
    _Atomic uint8_t capture;
    _Atomic uint8_t play;
    _Atomic double play_reader_rate;
    _Atomic double play_writer_rate;
    _Atomic double capture_reader_rate;
    _Atomic double capture_writer_rate;
    _Atomic float bpm;
    _Atomic int16_t map_note;
    _Atomic uint64_t layer;
    _Atomic uint64_t map_layer;
    _Atomic uint8_t map;
    _Atomic uint8_t capture_monitor;
    _Atomic float play_gain;
    _Atomic float capture_gain;
    _Atomic uint8_t* notes_on;
    _Atomic uint8_t* notes_on_previous;
    _Atomic uint8_t loop;
    _Atomic uint8_t repeat_section;
    _Atomic uint64_t repeat_start_frames;
    _Atomic uint64_t repeat_end_frames;
    _Atomic uint8_t start_war;
    _Atomic uint8_t resample;
    _Atomic uint64_t note_next_id;
    _Atomic uint64_t cache_next_id;
    _Atomic uint64_t cache_next_timestamp;
    _Atomic uint32_t bytes_needed;
} war_atomics;

typedef struct war_glyph_info {
    float advance_x;
    float advance_y;
    float bearing_x;
    float bearing_y;
    float width;
    float height;
    float uv_x0, uv_y0, uv_x1, uv_y1;
    float ascent;
    float descent;
    //
    float norm_width;
    float norm_height;
    float norm_baseline;
    float norm_ascent;
    float norm_descent;
} war_glyph_info;

typedef uint32_t war_new_vulkan_flags;
typedef enum war_new_vulkan_flags_bits {
    WAR_NEW_VULKAN_FLAGS_HIDDEN = 1 << 0,
    WAR_NEW_VULKAN_FLAGS_OUTLINE = 1 << 1,
    WAR_NEW_VULKAN_FLAGS_FOREGROUND = 1 << 2,
} war_new_vulkan_flags_bits;

typedef struct war_new_vulkan_vertex {
    float pos[2];
} war_new_vulkan_vertex;
typedef struct war_new_vulkan_note_instance {
    float pos[3];
    float size[2];
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_note_instance;
typedef struct war_new_vulkan_text_instance {
    float pos[3];
    float size[2];
    float uv[4];
    float glyph_scale[2];
    float baseline;
    float ascent;
    float descent;
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_text_instance;
typedef struct war_new_vulkan_line_instance {
    float pos[3];
    float size[2];
    float width;
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_line_instance;
typedef struct war_new_vulkan_cursor_instance {
    float pos[3];
    float size[2];
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_cursor_instance;
typedef struct war_new_vulkan_hud_instance {
    float pos[3];
    float size[2];
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_hud_instance;
typedef struct war_new_vulkan_hud_text_instance {
    float pos[3];
    float size[2];
    float uv[4];
    float glyph_scale[2];
    float baseline;
    float ascent;
    float descent;
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_hud_text_instance;
typedef struct war_new_vulkan_hud_line_instance {
    float pos[3];
    float size[2];
    float width;
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_hud_line_instance;
typedef struct war_new_vulkan_hud_cursor_instance {
    float pos[3];
    float size[2];
    float color[4];
    float outline_color[4];
    float foreground_color[4];
    float foreground_outline_color[4];
    war_new_vulkan_flags flags;
} war_new_vulkan_hud_cursor_instance;

typedef struct war_new_vulkan_note_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_note_push_constant;
typedef struct war_new_vulkan_text_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_text_push_constant;
typedef struct war_new_vulkan_line_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_line_push_constant;
typedef struct war_new_vulkan_cursor_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_cursor_push_constant;
typedef struct war_new_vulkan_hud_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_hud_push_constant;
typedef struct war_new_vulkan_hud_cursor_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_hud_cursor_push_constant;
typedef struct war_new_vulkan_hud_text_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_hud_text_push_constant;
typedef struct war_new_vulkan_hud_line_push_constant {
    float cell_size[2];
    float panning[2];
    float zoom;
    uint32_t _pad1;
    float screen_size[2];
    float cell_offset[2];
} war_new_vulkan_hud_line_push_constant;

typedef struct war_new_vulkan_context {
    // core
    int dmabuf_fd;
    double physical_width;
    double physical_height;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t queue_family_index;
    VkQueue queue;
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;
    VkImage depth_image;
    VkDeviceMemory depth_image_memory;
    VkImageView depth_image_view;
    VkRenderPass render_pass;
    VkImage color_image;
    VkDeviceMemory color_image_memory;
    VkFramebuffer frame_buffer;
    VkFence fence;
    VkImage color_linear_image;
    VkDeviceMemory color_linear_memory;
    VkDeviceMemory core_memory;
    VkImage core_image;
    VkImageView core_image_view;
    // pipeline
    uint32_t pipeline_idx_note;
    uint32_t pipeline_idx_text;
    uint32_t pipeline_idx_line;
    uint32_t pipeline_idx_cursor;
    uint32_t pipeline_idx_hud;
    uint32_t pipeline_idx_hud_cursor;
    uint32_t pipeline_idx_hud_text;
    uint32_t pipeline_idx_hud_line;
    VkPipeline* pipeline;
    VkPipelineLayout* pipeline_layout;
    VkStructureType* structure_type;
    VkPipelineShaderStageCreateInfo* pipeline_shader_stage_create_info;
    void** pipeline_push_constant;
    uint32_t* pipeline_set_idx;
    uint32_t* pipeline_shader_idx;
    uint32_t* pipeline_dispatch_group;
    uint32_t* pipeline_local_size;
    uint32_t* pipeline_instance_stage_idx;
    VkPipelineDepthStencilStateCreateInfo*
        pipeline_depth_stencil_state_create_info;
    VkPipelineColorBlendStateCreateInfo* pipeline_color_blend_state_create_info;
    VkPipelineColorBlendAttachmentState* pipeline_color_blend_attachment_state;
    VkPipelineBindPoint* pipeline_bind_point;
    war_new_vulkan_note_push_constant push_constant_note;
    war_new_vulkan_text_push_constant push_constant_text;
    war_new_vulkan_line_push_constant push_constant_line;
    war_new_vulkan_cursor_push_constant push_constant_cursor;
    war_new_vulkan_hud_push_constant push_constant_hud;
    war_new_vulkan_hud_cursor_push_constant push_constant_hud_cursor;
    war_new_vulkan_hud_text_push_constant push_constant_hud_text;
    war_new_vulkan_hud_line_push_constant push_constant_hud_line;
    uint32_t* push_constant_size;
    VkShaderStageFlags* push_constant_shader_stage_flags;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    VkDeviceSize pipeline_count;
    // shaders
    uint32_t shader_idx_vertex_note;
    uint32_t shader_idx_vertex_text;
    uint32_t shader_idx_vertex_line;
    uint32_t shader_idx_vertex_cursor;
    uint32_t shader_idx_vertex_hud;
    uint32_t shader_idx_vertex_hud_cursor;
    uint32_t shader_idx_vertex_hud_text;
    uint32_t shader_idx_vertex_hud_line;
    uint32_t shader_idx_fragment_note;
    uint32_t shader_idx_fragment_text;
    uint32_t shader_idx_fragment_line;
    uint32_t shader_idx_fragment_cursor;
    uint32_t shader_idx_fragment_hud;
    uint32_t shader_idx_fragment_hud_cursor;
    uint32_t shader_idx_fragment_hud_text;
    uint32_t shader_idx_fragment_hud_line;
    VkShaderModule* shader_module;
    VkShaderStageFlagBits* shader_stage_flag_bits;
    char* shader_path;
    VkDeviceSize shader_count;
    // resources
    uint32_t idx_vertex;
    uint32_t idx_note;
    uint32_t idx_text;
    uint32_t idx_image_text;
    uint32_t idx_line;
    uint32_t idx_cursor;
    uint32_t idx_hud;
    uint32_t idx_hud_cursor;
    uint32_t idx_hud_text;
    uint32_t idx_hud_line;
    uint32_t idx_vertex_stage;
    uint32_t idx_note_stage;
    uint32_t idx_text_stage;
    uint32_t idx_line_stage;
    uint32_t idx_cursor_stage;
    uint32_t idx_hud_stage;
    uint32_t idx_hud_cursor_stage;
    uint32_t idx_hud_text_stage;
    uint32_t idx_hud_line_stage;
    uint32_t idx_image_text_stage;
    VkDeviceSize* capacity;
    VkMemoryRequirements* memory_requirements;
    void** map;
    VkDeviceMemory* device_memory;
    uint32_t* size;
    VkMemoryPropertyFlags* memory_property_flags;
    VkBufferUsageFlags* usage_flags;
    VkBuffer* buffer;
    VkImage* image;
    uint32_t sampler_idx_text; // sampler
    VkSampler sampler;
    VkImageView* image_view;
    VkFormat* format;
    VkExtent3D* extent_3d;
    VkImageUsageFlags* image_usage_flags;
    VkDeviceSize resource_count;
    // descriptor set
    uint8_t* in_descriptor_set;
    uint32_t set_idx_text;
    VkShaderStageFlags* shader_stage_flags;
    VkDescriptorBufferInfo* descriptor_buffer_info;
    VkDescriptorImageInfo* descriptor_image_info;
    VkDescriptorSetLayoutBinding* descriptor_set_layout_binding;
    VkWriteDescriptorSet* write_descriptor_set;
    VkDescriptorSet* descriptor_set;
    VkDescriptorSetLayout* descriptor_set_layout;
    VkDescriptorType* image_descriptor_type;
    VkDescriptorPool* descriptor_pool;
    VkImageLayout* descriptor_image_layout;
    VkDeviceSize descriptor_count;
    VkDeviceSize descriptor_image_count;
    VkDeviceSize descriptor_set_count;
    // vertex input
    VkVertexInputBindingDescription* vertex_input_binding_description;
    VkVertexInputAttributeDescription** vertex_input_attribute_description;
    VkVertexInputRate* vertex_input_rate;
    uint32_t* stride;
    uint32_t* attribute_count;
    uint32_t* pipeline_vertex_input_binding_idx;
    // logic
    float* z_layer;
    VkMappedMemoryRange* mapped_memory_range;
    VkBufferMemoryBarrier* buffer_memory_barrier;
    VkImageMemoryBarrier* image_memory_barrier;
    VkImageLayout* image_layout;
    VkImageLayout* fn_image_layout;
    VkAccessFlags* access_flags;
    VkAccessFlags* fn_access_flags;
    VkPipelineStageFlags* pipeline_stage_flags;
    VkPipelineStageFlags* fn_pipeline_stage_flags;
    uint32_t* fn_src_idx;
    uint32_t* fn_dst_idx;
    VkDeviceSize* fn_size;
    VkDeviceSize* fn_size_2;
    VkDeviceSize* fn_src_offset;
    VkDeviceSize* fn_dst_offset;
    uint32_t* fn_data;
    uint32_t* fn_data_2;
    uint32_t fn_idx_count;
    war_glyph_info* glyph_info;
    float ascent;
    float descent;
    float line_gap;
    float baseline;
    float cell_height;
    float cell_width;
    // misc
    VkPhysicalDeviceProperties physical_device_properties;
    VkDeviceSize max_image_dimension_2d;
    VkDeviceSize optimal_buffer_copy_row_pitch_alignment;
    VkDeviceSize image_count;
    uint32_t groups;
    VkDeviceSize config_path_max;
    VkDeviceSize note_instance_max;
    VkDeviceSize text_instance_max;
    VkDeviceSize line_instance_max;
    VkDeviceSize cursor_instance_max;
    VkDeviceSize hud_instance_max;
    VkDeviceSize hud_cursor_instance_max;
    VkDeviceSize hud_text_instance_max;
    VkDeviceSize hud_line_instance_max;
    VkDeviceSize note_capacity;
    VkDeviceSize text_capacity;
    VkDeviceSize line_capacity;
    VkDeviceSize cursor_capacity;
    VkDeviceSize hud_capacity;
    VkDeviceSize hud_cursor_capacity;
    VkDeviceSize hud_text_capacity;
    VkDeviceSize hud_line_capacity;
    VkDeviceSize atlas_width;
    VkDeviceSize atlas_height;
    VkDeviceSize font_pixel_height;
    VkDeviceSize glyph_count;
    VkDeviceSize sdf_scale;
    VkDeviceSize sdf_padding;
    float sdf_range;
    float sdf_large;
    PFN_vkGetSemaphoreFdKHR get_semaphore_fd_khr;
    PFN_vkImportSemaphoreFdKHR import_semaphore_fd_khr;
    uint32_t* fn_pipeline_idx;
    double* fn_pan_x;
    double* fn_pan_y;
    double* fn_pan_factor_x;
    double* fn_pan_factor_y;
    // instance buffers
    uint8_t* dirty_buffer;
    uint8_t* draw_buffer;
    VkRect2D** buffer_rect_2d;
    VkViewport** buffer_viewport;
    void** buffer_push_constant;
    uint32_t* buffer_instance_count;
    uint32_t* buffer_first_instance;
    uint32_t* fn_buffer_pipeline_idx;
    uint32_t* fn_buffer_idx;
    uint32_t* fn_buffer_src_resource_idx;
    uint32_t* fn_buffer_dst_resource_idx;
    uint32_t* fn_buffer_first_instance;
    uint32_t* fn_buffer_instance_count;
    VkDeviceSize* fn_buffer_size;
    VkDeviceSize* fn_buffer_src_offset;
    VkAccessFlags* fn_buffer_access_flags;
    VkPipelineStageFlags* fn_buffer_pipeline_stage_flags;
    VkImageLayout* fn_buffer_image_layout;
    uint32_t fn_buffer_idx_count;
    uint32_t buffer_max;
} war_new_vulkan_context;

typedef struct war_producer_consumer {
    uint8_t* to_a;
    uint8_t* to_wr;
    uint32_t i_to_a;
    uint32_t i_to_wr;
    uint32_t i_from_a;
    uint32_t i_from_wr;
    uint64_t size;
} war_producer_consumer;

typedef struct war_pool {
    void* pool;
    uint8_t* pool_ptr;
    size_t pool_size;
    size_t pool_alignment;
} war_pool;

typedef uint32_t war_diff_type_u32;
typedef uint64_t war_diff_type_u64;
typedef enum war_diff_type_bits {
    WAR_DIFF_TYPE_NONE = 0,
} war_diff_type_bits;

typedef uint32_t war_file_type_u32;
typedef uint64_t war_file_type_u64;
typedef enum war_file_type_bits {
    WAR_FILE_TYPE_NONE = 0,
    WAR_FILE_TYPE_WAV = 1,
    WAR_FILE_TYPE_SEQUENCE = 2,
    WAR_FILE_TYPE_MAP = 3,
    WAR_FILE_TYPE_UNDO = 4,
    WAR_FILE_TYPE_COLOR = 5,
    WAR_FILE_TYPE_JUMPLIST = 6,
    WAR_FILE_TYPE_WARPOON = 7,
} war_file_type_bits;

typedef struct war_file {
    uint8_t* file;
    war_file_type_u32 type;
    int memfd;
    uint64_t memfd_size;
    uint64_t memfd_capacity;
    int fd;
    uint64_t fd_size;
    char* path;
    uint32_t path_size;
    uint32_t path_capacity;
    dev_t* st_dev;
    ino_t* st_ino;
    struct timespec st_mtim;
} war_file;

typedef struct __attribute__((packed)) war_jumplist_header {
    char magic[4];
    uint32_t version; // 0
} war_jumplist_header;

typedef struct __attribute__((packed)) war_jumplist_entry {
    uint64_t cursor_x;
    uint64_t cursor_y;
    uint64_t bottom_bound;
    uint64_t top_bound;
    uint64_t left_bound;
    uint64_t right_bound;
    war_file_type_u64 type;
    uint64_t path_size;
    uint64_t path_offset;
} war_jumplist_entry;

typedef struct war_jumplist_context {
    // jumplist
    char** path;
    uint32_t* path_size;
    uint32_t* path_capacity;
    uint8_t** head;
    uint64_t* head_size;
    uint64_t* head_capacity;
    uint8_t** body;
    uint64_t* body_capacity;
    // misc
    uint64_t* id;
    uint64_t* timestamp;
    war_file_type_u32* file_type;
    dev_t* st_dev;
    ino_t* st_ino;
    struct timespec* st_mtim;
    int* memfd;
    int* fd;
    uint32_t config_path_max;
    uint32_t* free;
    uint32_t free_count;
    uint32_t count;
    uint32_t capacity;
    uint64_t next_id;
    uint64_t next_timestamp;
} war_jumplist_context;

typedef struct __attribute__((packed)) war_warpoon_header {
    char magic[4];
    uint32_t version; // 0
} war_warpoon_header;

typedef struct __attribute__((packed)) war_warpoon_entry {
    uint64_t cursor_x;
    uint64_t cursor_y;
    uint64_t bottom_bound;
    uint64_t top_bound;
    uint64_t left_bound;
    uint64_t right_bound;
    war_file_type_u64 type;
    uint64_t path_size;
    uint64_t path_offset;
} war_warpoon_entry;

typedef struct war_warpoon_context {
    // warpoon
    char** path;
    uint32_t* path_size;
    uint32_t* path_capacity;
    uint8_t** head;
    uint64_t* head_size;
    uint64_t* head_capacity;
    uint8_t** body;
    uint64_t* body_capacity;
    // misc
    uint64_t* id;
    uint64_t* timestamp;
    war_file_type_u32* file_type;
    dev_t* st_dev;
    ino_t* st_ino;
    struct timespec* st_mtim;
    int* memfd;
    int* fd;
    uint32_t config_path_max;
    uint32_t* free;
    uint32_t free_count;
    uint32_t count;
    uint32_t capacity;
    uint64_t next_id;
    uint64_t next_timestamp;
} war_warpoon_context;

typedef struct __attribute__((packed)) war_sequence_header {
    char magic[4];
    uint32_t version; // 0
} war_sequence_header;

typedef struct __attribute__((packed)) war_sequence_entry {
    uint64_t start_seconds;
    uint64_t duration_seconds;
    uint64_t key;
    war_file_type_u64 type;
    uint64_t path_size;
    uint64_t path_offset;
} war_sequence_entry;

typedef struct war_sequence_context {
    // sequence
    char** path;
    uint32_t* path_size;
    uint32_t* path_capacity;
    uint8_t** head;
    uint64_t* head_size;
    uint64_t* head_capacity;
    uint8_t** body;
    uint64_t* body_capacity;
    //
    uint32_t idx_grid;
    war_sequence_entry** sequence;
    war_new_vulkan_note_instance** stage;
    war_new_vulkan_note_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* buffer_capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* instance_count;
    uint32_t* first;
    uint32_t buffer_count;
    // misc
    uint64_t* id;
    uint64_t* timestamp;
    war_file_type_u32* file_type;
    dev_t* st_dev;
    ino_t* st_ino;
    struct timespec* st_mtim;
    int* memfd;
    int* fd;
    uint32_t config_path_max;
    uint32_t* free;
    uint32_t free_count;
    uint32_t count;
    uint32_t capacity;
    uint64_t next_id;
    uint64_t next_timestamp;
} war_sequence_context;

typedef struct __attribute__((packed)) war_undo_header {
    char magic[4];
    uint32_t version; // 0
    uint32_t current_node_id_lo;
    uint32_t current_node_id_hi;
    war_file_type_u32 src_file_type;
    uint32_t src_path_size;
    uint32_t src_path_offset;
} war_undo_header;

typedef struct __attribute__((packed)) war_undo_node {
    uint64_t node_id;
    uint64_t timestamp;
    uint64_t seq_num;
    uint64_t branch_id;
    uint64_t command;
    uint64_t cursor_x;
    uint64_t cursor_y;
    uint64_t left_col;
    uint64_t right_col;
    uint64_t top_row;
    uint64_t bottom_row;
    uint64_t parent_id;
    uint64_t next_id;
    uint64_t prev_id;
    uint64_t alt_next_id;
    uint64_t alt_prev_id;
    war_diff_type_u64 diff_type;
    uint64_t diff_size;
    uint64_t diff_size_frames;
    uint64_t diff_src_offset;
    uint64_t diff_src_offset_frames;
    uint64_t diff_offset;
} war_undo_node;

typedef struct war_undo_context {
    // undo
    char** path;
    uint32_t* path_size;
    uint32_t* path_capacity;
    uint8_t** head;
    uint64_t* head_size;
    uint64_t* head_capacity;
    uint64_t* body_offset;
    uint64_t* body_capacity;
    uint8_t** node; // into og. file body
    //  misc
    uint64_t* id;
    uint64_t* timestamp;
    war_file_type_u32* file_type;
    dev_t* st_dev;
    ino_t* st_ino;
    struct timespec* st_mtim;
    int* memfd;
    int* fd;
    uint32_t config_path_max;
    uint32_t* free;
    uint32_t free_count;
    uint32_t count;
    uint32_t capacity;
    uint64_t next_id;
    uint64_t next_timestamp;
} war_undo_context;

typedef struct __attribute__((packed)) war_map_header {
    char magic[4];
    uint32_t version; // 0
} war_map_header;

typedef struct __attribute__((packed)) war_map_entry {
    uint32_t key;
    uint32_t layer;
    uint32_t path_size;
    uint32_t path_offset;
} war_map_entry;

typedef struct war_map_context {
    // map file
    char** path;
    uint32_t* path_size;
    uint32_t* path_capacity;
    uint8_t** head;
    uint64_t* head_size;
    uint64_t* head_capacity;
    uint8_t** body;
    uint64_t* body_capacity;
    // key, layer, path_size, path
    // 4 + 4 + 4 + ((path_size + alignment - 1) & ~(alignment - 1))
    // misc
    uint64_t* id;
    uint64_t* timestamp;
    war_file_type_u32* file_type;
    dev_t* st_dev;
    ino_t* st_ino;
    struct timespec* st_mtim;
    int* memfd;
    int* fd;
    uint32_t config_path_max;
    uint32_t* free;
    uint32_t free_count;
    uint32_t count;
    uint32_t capacity;
    uint64_t next_id;
    uint64_t next_timestamp;
} war_map_context;

typedef struct war_part_context {
    // slice
    char** path;
    uint32_t* path_size;
    uint32_t* path_capacity;
    uint8_t** head;
    uint64_t* head_size;
    uint64_t* head_capacity;
    uint8_t** tail;
    uint64_t* tail_size;
    uint64_t* tail_capacity;
    uint64_t* body_offset;
    uint64_t* body_capacity;
    uint64_t* body_offset_frames;
    uint64_t* body_capacity_frames;
    uint8_t** part; // into og. file body
    uint64_t* part_edit_size;
    uint64_t* part_edit_frames;
    uint64_t* part_edit_offset;
    uint64_t* part_edit_offset_frames;
    uint64_t* part_capacity;
    uint64_t* part_capacity_frames;
    float** pcm_l;
    float** pcm_r;
    uint64_t* pcm_edit_size;
    uint64_t* pcm_edit_frames;
    uint64_t* pcm_edit_offset;
    uint64_t* pcm_edit_offset_frames;
    uint64_t* pcm_capacity;
    uint64_t* pcm_capacity_frames;
    float** pcm_temp_l;
    float** pcm_temp_r;
    uint64_t* pcm_temp_edit_size;
    uint64_t* pcm_temp_edit_frames;
    uint64_t* pcm_temp_edit_offset;
    uint64_t* pcm_temp_capacity;
    uint64_t* pcm_temp_edit_offset_frames;
    uint64_t* pcm_temp_capacity_frames;
    // float** video;
    // misc
    uint64_t* id;
    uint64_t* timestamp;
    war_file_type_u32* file_type;
    dev_t* st_dev;
    ino_t* st_ino;
    struct timespec* st_mtim;
    int* memfd;
    int* fd;
    uint32_t config_path_max;
    uint32_t* free;
    uint32_t free_count;
    uint32_t count;
    uint32_t capacity;
    uint64_t next_id;
    uint64_t next_timestamp;
} war_part_context;

typedef struct __attribute__((packed)) war_color_header {
    char magic[4];
    uint32_t version;
} war_color_header;

typedef struct __attribute__((packed)) war_color_body {
    uint32_t top_status_bar;
    uint32_t middle_status_bar;
    uint32_t bottom_status_bar;
    uint32_t top_status_bar_cursor;
    uint32_t middle_status_bar_cursor;
    uint32_t bottom_status_bar_cursor;
    uint32_t top_status_bar_text;
    uint32_t middle_status_bar_text;
    uint32_t bottom_status_bar_text;
    uint32_t top_status_bar_line;
    uint32_t middle_status_bar_line;
    uint32_t bottom_status_bar_line;
    uint32_t top_status_bar_text_foreground;
    uint32_t middle_status_bar_text_foreground;
    uint32_t bottom_status_bar_text_foreground;
    uint32_t explore_header_text;
    uint32_t explore_header_text_foreground;
    uint32_t explore_text;
    uint32_t explore_text_foreground;
    uint32_t explore_editable_text;            // editable by war
    uint32_t explore_editable_text_foreground; // editable by war
    uint32_t explore_line;
    uint32_t explore_directory_text;
    uint32_t explore_directory_text_foreground;
    uint32_t explore_cursor;
    uint32_t warpoon_text;
    uint32_t warpoon_text_foreground;
    uint32_t warpoon_outline;
    uint32_t warpoon_background;
    uint32_t warpoon_gutter;
    uint32_t warpoon_gutter_text;
    uint32_t warpoon_line;
    uint32_t warpoon_cursor;
    uint32_t preview_outline;
    uint32_t preview_text;
    uint32_t preview_background;
    uint32_t preview_line;
    uint32_t preview_cursor;
    uint32_t mode_text;
    uint32_t error;
    uint32_t error_text;
    uint32_t background;
    uint32_t line;
    uint32_t line_foreground;
    uint32_t line_stressed_1;
    uint32_t line_stressed_2;
    uint32_t line_stressed_3;
    uint32_t line_stressed_4;
    uint32_t line_stressed_1_foreground;
    uint32_t line_stressed_2_foreground;
    uint32_t line_stressed_3_foreground;
    uint32_t line_stressed_4_foreground;
    uint32_t layer_none;
    uint32_t layer_1;
    uint32_t layer_2;
    uint32_t layer_3;
    uint32_t layer_4;
    uint32_t layer_5;
    uint32_t layer_6;
    uint32_t layer_7;
    uint32_t layer_8;
    uint32_t layer_9;
    uint32_t layer_multiple;
    uint32_t layer_none_foreground;
    uint32_t layer_1_foreground;
    uint32_t layer_2_foreground;
    uint32_t layer_3_foreground;
    uint32_t layer_4_foreground;
    uint32_t layer_5_foreground;
    uint32_t layer_6_foreground;
    uint32_t layer_7_foreground;
    uint32_t layer_8_foreground;
    uint32_t layer_9_foreground;
    uint32_t layer_multiple_foreground;
    uint32_t layer_none_outline;
    uint32_t layer_1_outline;
    uint32_t layer_2_outline;
    uint32_t layer_3_outline;
    uint32_t layer_4_outline;
    uint32_t layer_5_outline;
    uint32_t layer_6_outline;
    uint32_t layer_7_outline;
    uint32_t layer_8_outline;
    uint32_t layer_9_outline;
    uint32_t layer_multiple_outline;
    uint32_t layer_none_outline_foreground;
    uint32_t layer_1_outline_foreground;
    uint32_t layer_2_outline_foreground;
    uint32_t layer_3_outline_foreground;
    uint32_t layer_4_outline_foreground;
    uint32_t layer_5_outline_foreground;
    uint32_t layer_6_outline_foreground;
    uint32_t layer_7_outline_foreground;
    uint32_t layer_8_outline_foreground;
    uint32_t layer_9_outline_foreground;
    uint32_t layer_multiple_outline_foreground;
    uint32_t layer_none_text;
    uint32_t layer_1_text;
    uint32_t layer_2_text;
    uint32_t layer_3_text;
    uint32_t layer_4_text;
    uint32_t layer_5_text;
    uint32_t layer_6_text;
    uint32_t layer_7_text;
    uint32_t layer_8_text;
    uint32_t layer_9_text;
    uint32_t layer_multiple_text;
    uint32_t layer_none_text_foreground;
    uint32_t layer_1_text_foreground;
    uint32_t layer_2_text_foreground;
    uint32_t layer_3_text_foreground;
    uint32_t layer_4_text_foreground;
    uint32_t layer_5_text_foreground;
    uint32_t layer_6_text_foreground;
    uint32_t layer_7_text_foreground;
    uint32_t layer_8_text_foreground;
    uint32_t layer_9_text_foreground;
    uint32_t layer_multiple_text_foreground;
    uint32_t audio_line;
    uint32_t audio_background;
    uint32_t audio_cursor;
    uint32_t video_cursor;
    uint32_t gutter;
    uint32_t gutter_text;
    uint32_t gutter_text_foreground;
    uint32_t gutter_line;
    uint32_t piano_white_key;
    uint32_t piano_white_key_text;
    uint32_t piano_black_key;
    uint32_t piano_line;
    uint32_t playhead;
} war_color_body;

// typedef struct war_color_context {
//     // color
//     char** path;
//     uint32_t* path_size;
//     uint32_t* path_capacity;
//     uint8_t** head;
//     uint64_t* head_size;
//     uint64_t* head_capacity;
//     uint8_t** body;
//     uint64_t* body_capacity;
//     // misc
//     uint64_t* id;
//     uint64_t* timestamp;
//     war_file_type_u32* file_type;
//     dev_t* st_dev;
//     ino_t* st_ino;
//     struct timespec* st_mtim;
//     int* memfd;
//     int* fd;
//     uint32_t config_path_max;
//     uint32_t* free;
//     uint32_t free_count;
//     uint32_t count;
//     uint32_t capacity;
//     uint64_t next_id;
//     uint64_t next_timestamp;
// } war_color_context;

typedef struct war_pipewire_context {
    struct pw_loop* loop;
    struct pw_stream* play_stream;
    struct pw_stream* capture_stream;
    const struct spa_pod* play_params;
    const struct spa_pod* capture_params;
    struct spa_audio_info_raw play_info;
    struct spa_audio_info_raw capture_info;
    struct spa_pod_builder play_builder;
    struct spa_pod_builder capture_builder;
    struct pw_stream_events play_events;
    struct pw_stream_events capture_events;
    struct pw_properties* play_properties;
    struct pw_properties* capture_properties;
    uint8_t* play_builder_data;
    uint8_t* capture_builder_data;
    void** play_data;
    void** capture_data;
} war_pipewire_context;

enum war_command_prompt_types {
    WAR_COMMAND_PROMPT_NONE = 0,
    WAR_COMMAND_PROMPT_CAPTURE_FNAME = 1,
    WAR_COMMAND_PROMPT_CAPTURE_NOTE = 2,
    WAR_COMMAND_PROMPT_CAPTURE_LAYER = 3,
};

// typedef struct war_command_context {
//     int* input;
//     uint32_t input_write_index;
//     uint32_t input_read_index;
//     char* text;
//     uint32_t text_write_index;
//     uint32_t text_size;
//     uint8_t prompt_type;
//     char* prompt_text;
//     uint32_t prompt_text_size;
//     uint32_t capacity;
// } war_command_context;

typedef struct war_misc_context {
    double bpm;
    double fps;
    double bpm_seconds_per_cell;
    double subdivision_seconds_per_cell;
    double seconds_per_beat;
    double seconds_per_cell;
    double epsilon;
    uint32_t stride;
    uint32_t logical_width;
    uint32_t logical_height;
    double scale_factor;
    uint64_t now;
    war_riff_header init_riff_header;
    war_fmt_chunk init_fmt_chunk;
    war_data_chunk init_data_chunk;
    uint64_t last_frame_time;
    uint64_t frame_duration_us;
    uint8_t trinity;
} war_misc_context;

typedef struct war_play_context {
    // rate
    uint64_t last_frame_time;
    uint64_t last_write_time;
    uint64_t write_count;
    uint64_t rate_us;
    // misc
    uint8_t play;
    uint32_t fps;
    // midi
    uint8_t* keys;
    uint64_t* key_layers;
    int32_t octave;
    // limits
    uint32_t note_count;
    uint32_t layer_count;
} war_play_context;

enum capture_state {
    CAPTURE_WAITING = 0,
    CAPTURE_CAPTURING = 1,
};

typedef struct war_capture_context {
    // rate
    uint64_t last_frame_time;
    uint64_t last_read_time;
    uint64_t read_count;
    uint64_t rate_us;
    // misc
    uint8_t capture_wait;
    uint32_t capture_delay;
    uint8_t state;
    float threshold;
    uint8_t monitor;
    double fps;
    // prompts
    uint8_t prompt;
    char* prompt_fname_text;
    uint32_t prompt_fname_text_size;
    char* prompt_note_text;
    uint32_t prompt_note_text_size;
    char* prompt_layer_text;
    uint32_t prompt_layer_text_size;
    // data
    char* fname;
    uint32_t fname_size;
    uint32_t note;
    uint32_t layer;
    // limit
    uint32_t name_limit;
} war_capture_context;

typedef struct war_nsgt_compute_push_constant {
    uint arg_1; // frame_count
    uint arg_2; // base_sample (mono samples)
    uint arg_3; // bin_capacity
    uint arg_4; // hop (samples per frame)
} war_nsgt_compute_push_constant;

typedef struct war_nsgt_graphics_push_constant {
    int channel;
    int blend;
    int _pad0[2];
    float color_l[4];
    float color_r[4];
    float time_offset;
    float freq_scale;
    float time_scale;
    int bin_capacity;
    int frame_capacity;
    float z_layer;
    int frame_offset;
    int frame_count;
    int frame_filled;
} war_nsgt_graphics_push_constant;

typedef struct war_nsgt_cache {
    VkImage* image;
    uint32_t* frame_offset;
    uint64_t* id;
    uint64_t* timestamp;
    uint32_t* frame_count;
    uint32_t* frame_capacity;
    uint32_t* free;
    uint32_t free_count;
    uint32_t count;
    uint64_t next_id;
    uint64_t next_timestamp;
    uint32_t capacity;
} war_nsgt_cache;

typedef struct war_nsgt_context {
    // pipeline
    uint32_t pipeline_idx_compute_nsgt;
    uint32_t pipeline_idx_compute_magnitude;
    uint32_t pipeline_idx_compute_transient;
    uint32_t pipeline_idx_compute_image;
    uint32_t pipeline_idx_compute_wav;
    uint32_t pipeline_idx_graphics;
    VkPipeline* pipeline;
    VkPipelineLayout* pipeline_layout;
    VkStructureType* structure_type;
    VkPipelineShaderStageCreateInfo* pipeline_shader_stage_create_info;
    VkShaderStageFlags* push_constant_shader_stage_flags;
    uint32_t* push_constant_size;
    uint32_t* pipeline_set_idx;
    uint32_t* pipeline_shader_idx;
    uint32_t* pipeline_dispatch_group;
    uint32_t* pipeline_local_size;
    VkPipelineBindPoint* pipeline_bind_point;
    war_nsgt_compute_push_constant compute_push_constant;
    war_nsgt_graphics_push_constant graphics_push_constant;
    VkViewport graphics_viewport;
    VkRect2D graphics_rect_2d;
    VkDeviceSize pipeline_count;
    // shaders
    uint32_t shader_idx_compute_nsgt;
    uint32_t shader_idx_compute_image;
    uint32_t shader_idx_compute_magnitude;
    uint32_t shader_idx_compute_wav;
    uint32_t shader_idx_compute_transient;
    uint32_t shader_idx_vertex;
    uint32_t shader_idx_fragment;
    VkShaderModule* shader_module;
    VkShaderStageFlagBits* shader_stage_flag_bits;
    char* shader_path;
    VkDeviceSize shader_count;
    // resources
    uint32_t idx_offset;
    uint32_t idx_hop;
    uint32_t idx_length;
    uint32_t idx_window;
    uint32_t idx_dual_window;
    uint32_t idx_frequency;
    uint32_t idx_cis;
    uint32_t idx_wav_temp;
    uint32_t idx_wav;
    uint32_t idx_nsgt_temp;
    uint32_t idx_nsgt;
    uint32_t idx_magnitude_temp;
    uint32_t idx_magnitude;
    uint32_t idx_transient_temp;
    uint32_t idx_transient;
    uint32_t idx_image_temp;
    uint32_t idx_image;
    uint32_t idx_wav_stage;
    uint32_t idx_offset_stage;
    uint32_t idx_hop_stage;
    uint32_t idx_length_stage;
    uint32_t idx_window_stage;
    uint32_t idx_dual_window_stage;
    uint32_t idx_frequency_stage;
    uint32_t idx_cis_stage;
    VkDeviceSize* capacity;
    VkMemoryRequirements* memory_requirements;
    void** map;
    VkDeviceMemory* device_memory;
    uint32_t* size;
    VkMemoryPropertyFlags* memory_property_flags;
    VkBufferUsageFlags* usage_flags;
    VkBuffer* buffer;
    VkImage* image;
    VkImageView* image_view;
    VkFormat* format;
    VkExtent3D* extent_3d;
    VkImageUsageFlags* image_usage_flags;
    VkDeviceSize resource_count;
    // descriptor set
    uint32_t set_idx_compute;
    uint32_t set_idx_graphics;
    VkShaderStageFlags* shader_stage_flags;
    VkDescriptorBufferInfo* descriptor_buffer_info;
    VkDescriptorImageInfo* descriptor_image_info;
    VkDescriptorSetLayoutBinding* descriptor_set_layout_binding;
    VkWriteDescriptorSet* write_descriptor_set;
    VkDescriptorSet* descriptor_set;
    VkDescriptorSetLayout* descriptor_set_layout;
    VkDescriptorType* image_descriptor_type;
    VkDescriptorPool* descriptor_pool;
    VkImageLayout* descriptor_image_layout;
    VkDeviceSize descriptor_count;
    VkDeviceSize descriptor_image_count;
    VkDeviceSize descriptor_set_count;
    // logic
    VkMappedMemoryRange* mapped_memory_range;
    VkBufferMemoryBarrier* buffer_memory_barrier;
    VkImageMemoryBarrier* image_memory_barrier;
    VkImageLayout* image_layout;
    VkImageLayout* fn_image_layout;
    VkAccessFlags* access_flags;
    VkAccessFlags* fn_access_flags;
    VkPipelineStageFlags* pipeline_stage_flags;
    VkPipelineStageFlags* fn_pipeline_stage_flags;
    uint32_t* fn_src_idx;
    uint32_t* fn_dst_idx;
    VkDeviceSize* fn_size;
    VkDeviceSize* fn_src_offset;
    VkDeviceSize* fn_dst_offset;
    uint32_t* fn_data;
    uint32_t* fn_data_2;
    uint32_t fn_idx_count;
    uint8_t dirty_compute;
    // incremental visualization state
    uint32_t frame_cursor;
    uint32_t frame_filled;
    // misc
    VkPhysicalDeviceProperties physical_device_properties;
    VkSampler sampler;
    float alpha;
    float shape_factor;
    uint32_t window_length_min;
    uint32_t window_length_max;
    uint32_t hop_min;
    VkDeviceSize bin_capacity;
    VkDeviceSize frame_capacity;
    VkDeviceSize sample_rate;
    VkDeviceSize sample_duration;
    VkDeviceSize frequency_min;
    VkDeviceSize frequency_max;
    VkDeviceSize channel_count;
    VkDeviceSize wav_capacity;
    VkDeviceSize nsgt_capacity;
    VkDeviceSize magnitude_capacity;
    VkDeviceSize offset_capacity;
    VkDeviceSize length_capacity;
    VkDeviceSize window_capacity;
    VkDeviceSize frequency_capacity;
    VkDeviceSize hop_capacity;
    VkDeviceSize path_limit;
    VkDeviceSize cis_capacity;
    VkDeviceSize transient_capacity;
    VkDeviceSize frames_per_bin_capacity;
    VkDeviceSize frame_offset_capacity;
    VkDeviceSize image_frame_capacity;
    VkDeviceSize max_image_dimension_2d;
    VkDeviceSize optimal_buffer_copy_row_pitch_alignment;
    VkDeviceSize image_count;
    uint32_t groups;
    // graphics fps
    uint32_t graphics_fps;
    uint64_t last_frame_time;
    uint64_t last_write_time;
    uint64_t write_count;
    uint64_t rate_us;
} war_nsgt_context;

typedef struct war_env war_env;

typedef union war_function_union {
    void (*c)(war_env* env);
    int lua;
} war_function_union;

typedef struct war_fsm_context {
    // FSM data
    war_function_union* function;
    uint8_t* function_type;
    char* name;
    uint8_t* is_terminal;
    uint8_t* handle_release;
    uint8_t* handle_timeout;
    uint8_t* handle_repeat;
    uint8_t* is_prefix;
    uint64_t* next_state;
    // file
    char* cwd;
    uint32_t cwd_size;
    char* current_file_path;
    uint32_t current_file_path_size;
    uint32_t current_file_type;
    char* ext;
    uint32_t ext_size;
    // Runtime state
    uint8_t* key_down;
    uint64_t state_last_event_us;
    uint64_t* key_last_event_us;
    uint64_t current_state;
    uint32_t current_mode;
    uint32_t previous_mode;
    // Modifiers
    uint32_t mod_shift;
    uint32_t mod_ctrl;
    uint32_t mod_alt;
    uint32_t mod_logo;
    uint32_t mod_caps;
    uint32_t mod_num;
    uint32_t mod_fn;
    // XKB
    struct xkb_context* xkb_context;
    struct xkb_state* xkb_state;
    struct xkb_keymap* xkb_keymap;
    uint32_t keymap_format;
    uint32_t keymap_size;
    int keymap_fd;
    char* keymap_map;
    // counts
    uint32_t keysym_count;
    uint32_t mod_count;
    uint32_t state_count;
    uint32_t mode_count;
    uint32_t name_limit;
    // modes
    uint32_t MODE_ROLL;
    uint32_t MODE_VIEWS;
    uint32_t MODE_CAPTURE;
    uint32_t MODE_MIDI;
    uint32_t MODE_COMMAND;
    uint32_t MODE_WAV;
    uint32_t MODE_VISUAL;
    uint32_t MODE_CHORD;
    // cmd type
    int FUNCTION_NONE;
    int FUNCTION_C;
    int FUNCTION_LUA;
    // repeats
    uint64_t repeat_delay_us;
    uint64_t repeat_rate_us;
    uint32_t repeat_keysym;
    uint8_t repeat_mod;
    uint8_t repeating;
    uint8_t goto_cmd_repeat_done;
    // timeouts
    uint64_t timeout_duration_us;
    uint16_t timeout_state_index;
    uint64_t timeout_start_us;
    uint8_t timeout;
    uint8_t goto_cmd_timeout_done;
} war_fsm_context;

typedef struct war_hud_context {
    uint32_t idx_status_bottom;
    uint32_t idx_status_middle;
    uint32_t idx_status_top;
    uint32_t idx_line_numbers;
    uint32_t idx_piano;
    uint32_t idx_explore;
    uint32_t idx_error;
    double** x_seconds;
    double** y_cells;
    double** width_seconds;
    double** height_cells;
    war_new_vulkan_hud_instance** stage;
    war_new_vulkan_hud_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* count;
    uint32_t* first;
    uint32_t buffer_count;
} war_hud_context;

typedef struct war_hud_line_context {
    uint32_t idx_piano;
    double** x_seconds;
    double** y_cells;
    double** width_seconds;
    double** height_cells;
    double** line_width_seconds;
    war_new_vulkan_hud_line_instance** stage;
    war_new_vulkan_hud_line_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* count;
    uint32_t* first;
    uint32_t buffer_count;
} war_hud_line_context;

typedef struct war_hud_text_context {
    uint32_t idx_status_middle;
    uint32_t idx_status_bottom;
    uint32_t idx_status_top;
    uint32_t idx_line_numbers;
    uint32_t idx_relative_line_numbers;
    uint32_t idx_piano;
    uint32_t idx_explore;
    uint32_t idx_error;
    double** x_seconds;
    double** y_cells;
    double** width_seconds;
    double** height_cells;
    war_new_vulkan_hud_text_instance** stage;
    war_new_vulkan_hud_text_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* count;
    uint32_t* first;
    uint32_t buffer_count;
} war_hud_text_context;

typedef struct war_hud_cursor_context {
    uint32_t idx_cursor_default;
    uint32_t chord_idx_current;
    double** x_seconds;
    double** y_cells;
    double** width_seconds;
    double** height_cells;
    double** visual_x_seconds;
    double** visual_y_cells;
    double** visual_width_seconds;
    double** visual_height_cells;
    double top_bound_cells;
    double bottom_bound_cells;
    double right_bound_seconds;
    double left_bound_seconds;
    double max_cells_y;
    double max_seconds_x;
    double move_factor;
    double leap_cells;
    war_new_vulkan_hud_cursor_instance** stage;
    war_new_vulkan_hud_cursor_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* count;
    uint32_t* first;
    uint32_t buffer_count;
} war_hud_cursor_context;

typedef struct war_cursor_context {
    uint32_t idx_cursor_default;
    uint32_t chord_idx_current;
    double** x_seconds;
    double** y_cells;
    double** width_seconds;
    double** height_cells;
    double** visual_x_seconds;
    double** visual_y_cells;
    double** visual_width_seconds;
    double** visual_height_cells;
    double top_bound_cells;
    double bottom_bound_cells;
    double right_bound_seconds;
    double left_bound_seconds;
    double max_cells_y;
    double max_seconds_x;
    double move_factor;
    double leap_cells;
    war_new_vulkan_cursor_instance** stage;
    war_new_vulkan_cursor_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* count;
    uint32_t* first;
    uint32_t buffer_count;
} war_cursor_context;

typedef struct war_line_context {
    uint32_t idx_cell_grid;
    uint32_t idx_bpm_grid;
    double** x_seconds;
    double** y_cells;
    double** width_seconds;
    double** height_cells;
    double** line_width_seconds;
    war_new_vulkan_line_instance** stage;
    war_new_vulkan_line_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* count;
    uint32_t* first;
    uint32_t buffer_count;
} war_line_context;

typedef struct war_text_context {
    uint32_t idx_status_middle;
    uint32_t idx_status_bottom;
    uint32_t idx_status_top;
    uint32_t idx_line_numbers;
    uint32_t idx_relative_line_numbers;
    uint32_t idx_piano;
    uint32_t idx_explore;
    uint32_t idx_error;
    double** x_seconds;
    double** y_cells;
    double** width_seconds;
    double** height_cells;
    war_new_vulkan_text_instance** stage;
    war_new_vulkan_text_push_constant* push_constant;
    VkViewport* viewport;
    VkRect2D* rect_2d;
    //
    uint32_t* capacity;
    uint8_t* dirty;
    uint8_t* draw;
    uint32_t* count;
    uint32_t* first;
    uint32_t buffer_count;
} war_text_context;

typedef uint64_t war_mode_flags;
typedef enum war_mode_flags_bits {
    WAR_MODE_NONE = 0,
    WAR_MODE_ALL = 0xFFFFFFFFFFFFFFFFULL,
    WAR_MODE_ROLL = 1ULL << 0,
    WAR_MODE_VIEWS = 1ULL << 1,
    WAR_MODE_CAPTURE = 1ULL << 2,
    WAR_MODE_MIDI = 1ULL << 3,
    WAR_MODE_COMMAND = 1ULL << 4,
    WAR_MODE_WAV = 1ULL << 5,
    WAR_MODE_VISUAL = 1ULL << 6,
    WAR_MODE_CHORD = 1ULL << 7,
} war_mode_flags_bits;

typedef uint64_t war_keymap_flags;
typedef enum war_keymap_flags_bits {
    WAR_KEYMAP_NONE = 0,
    WAR_KEYMAP_ALL = 0xFFFFFFFFFFFFFFFFULL,
    WAR_KEYMAP_EXTEND = 1ULL << 0,
    WAR_KEYMAP_RELEASE = 1ULL << 1,
    WAR_KEYMAP_NO_REPEAT = 1ULL << 2,
    WAR_KEYMAP_NO_TIMEOUT = 1ULL << 3,
    //
    WAR_KEYMAP_SHARED_LIBRARY = 1ULL << 4,
    WAR_KEYMAP_PREFIX = 1ULL << 5,
} war_keymap_flags_bits;

typedef struct war_keymap_context {
    uint32_t version;
    //
    void (**function)(war_env* env);
    uint8_t* function_count;
    war_keymap_flags* flags;
    uint64_t* next_state;
    //
    uint32_t state_count;
    uint32_t state_capacity;
    uint32_t keysym_capacity;
    uint32_t mod_capacity;
    uint32_t function_capacity;
} war_keymap_context;

#define KEYMAP_3D_INDEX(state, keysym, mod, keymap)                            \
    ((state) * ((keymap)->keysym_capacity * (keymap)->mod_capacity) +          \
     (keysym) * (keymap)->mod_capacity + (mod))

typedef struct war_lock_context {
    atomic_flag config;
    atomic_flag keymap;
    atomic_flag color;
    atomic_flag plugin;
    atomic_flag command;
} war_lock_context;

typedef struct war_color_context {
    uint32_t version;
    //
    uint32_t top_status_bar;
    uint32_t middle_status_bar;
    uint32_t bottom_status_bar;
    uint32_t top_status_bar_cursor;
    uint32_t middle_status_bar_cursor;
    uint32_t bottom_status_bar_cursor;
    uint32_t top_status_bar_text;
    uint32_t middle_status_bar_text;
    uint32_t bottom_status_bar_text;
    uint32_t top_status_bar_line;
    uint32_t middle_status_bar_line;
    uint32_t bottom_status_bar_line;
    uint32_t top_status_bar_text_foreground;
    uint32_t middle_status_bar_text_foreground;
    uint32_t bottom_status_bar_text_foreground;
    uint32_t explore_header_text;
    uint32_t explore_header_text_foreground;
    uint32_t explore_text;
    uint32_t explore_text_foreground;
    uint32_t explore_editable_text;            // editable by war
    uint32_t explore_editable_text_foreground; // editable by war
    uint32_t explore_line;
    uint32_t explore_directory_text;
    uint32_t explore_directory_text_foreground;
    uint32_t explore_cursor;
    uint32_t warpoon_text;
    uint32_t warpoon_text_foreground;
    uint32_t warpoon_outline;
    uint32_t warpoon_background;
    uint32_t warpoon_gutter;
    uint32_t warpoon_gutter_text;
    uint32_t warpoon_line;
    uint32_t warpoon_cursor;
    uint32_t preview_outline;
    uint32_t preview_text;
    uint32_t preview_background;
    uint32_t preview_line;
    uint32_t preview_cursor;
    uint32_t mode_text;
    uint32_t error;
    uint32_t error_text;
    uint32_t background;
    uint32_t line;
    uint32_t line_foreground;
    uint32_t line_stressed_1;
    uint32_t line_stressed_2;
    uint32_t line_stressed_3;
    uint32_t line_stressed_4;
    uint32_t line_stressed_1_foreground;
    uint32_t line_stressed_2_foreground;
    uint32_t line_stressed_3_foreground;
    uint32_t line_stressed_4_foreground;
    uint32_t layer_none;
    uint32_t layer_1;
    uint32_t layer_2;
    uint32_t layer_3;
    uint32_t layer_4;
    uint32_t layer_5;
    uint32_t layer_6;
    uint32_t layer_7;
    uint32_t layer_8;
    uint32_t layer_9;
    uint32_t layer_multiple;
    uint32_t layer_none_foreground;
    uint32_t layer_1_foreground;
    uint32_t layer_2_foreground;
    uint32_t layer_3_foreground;
    uint32_t layer_4_foreground;
    uint32_t layer_5_foreground;
    uint32_t layer_6_foreground;
    uint32_t layer_7_foreground;
    uint32_t layer_8_foreground;
    uint32_t layer_9_foreground;
    uint32_t layer_multiple_foreground;
    uint32_t layer_none_outline;
    uint32_t layer_1_outline;
    uint32_t layer_2_outline;
    uint32_t layer_3_outline;
    uint32_t layer_4_outline;
    uint32_t layer_5_outline;
    uint32_t layer_6_outline;
    uint32_t layer_7_outline;
    uint32_t layer_8_outline;
    uint32_t layer_9_outline;
    uint32_t layer_multiple_outline;
    uint32_t layer_none_outline_foreground;
    uint32_t layer_1_outline_foreground;
    uint32_t layer_2_outline_foreground;
    uint32_t layer_3_outline_foreground;
    uint32_t layer_4_outline_foreground;
    uint32_t layer_5_outline_foreground;
    uint32_t layer_6_outline_foreground;
    uint32_t layer_7_outline_foreground;
    uint32_t layer_8_outline_foreground;
    uint32_t layer_9_outline_foreground;
    uint32_t layer_multiple_outline_foreground;
    uint32_t layer_none_text;
    uint32_t layer_1_text;
    uint32_t layer_2_text;
    uint32_t layer_3_text;
    uint32_t layer_4_text;
    uint32_t layer_5_text;
    uint32_t layer_6_text;
    uint32_t layer_7_text;
    uint32_t layer_8_text;
    uint32_t layer_9_text;
    uint32_t layer_multiple_text;
    uint32_t layer_none_text_foreground;
    uint32_t layer_1_text_foreground;
    uint32_t layer_2_text_foreground;
    uint32_t layer_3_text_foreground;
    uint32_t layer_4_text_foreground;
    uint32_t layer_5_text_foreground;
    uint32_t layer_6_text_foreground;
    uint32_t layer_7_text_foreground;
    uint32_t layer_8_text_foreground;
    uint32_t layer_9_text_foreground;
    uint32_t layer_multiple_text_foreground;
    uint32_t audio_line;
    uint32_t audio_background;
    uint32_t audio_cursor;
    uint32_t video_cursor;
    uint32_t gutter;
    uint32_t gutter_text;
    uint32_t gutter_text_foreground;
    uint32_t gutter_line;
    uint32_t piano_white_key;
    uint32_t piano_white_key_text;
    uint32_t piano_black_key;
    uint32_t piano_line;
    uint32_t playhead;
} war_color_context;

typedef struct war_command_context {
    uint32_t version;
    //
    void (**function)(war_env* env);
    uint8_t* function_count;
    war_keymap_flags* flags;
    uint64_t* next_state;
    //
    uint32_t state_count;
    uint32_t state_capacity;
    uint32_t keysym_capacity;
    uint32_t mod_capacity;
    uint32_t function_capacity;
} war_command_context;

typedef struct war_config_context {
    uint32_t version;
    //
    int A_SAMPLE_RATE;
    double A_SAMPLE_DURATION;
    double A_TARGET_SAMPLES_FACTOR;
    int A_CHANNEL_COUNT;
    int A_NOTE_COUNT;
    float WR_CAPTURE_THRESHOLD;
    int A_LAYER_COUNT;
    int A_LAYERS_IN_RAM;
    double A_BPM;
    double BPM_SECONDS_PER_CELL;
    double SUBDIVISION_SECONDS_PER_CELL;
    int A_BASE_FREQUENCY;
    int A_SCHED_FIFO_PRIORITY;
    int A_BASE_NOTE;
    int A_EDO;
    int A_NOTES_MAX;
    float A_DEFAULT_ATTACK;
    float A_DEFAULT_SUSTAIN;
    float A_DEFAULT_RELEASE;
    float A_DEFAULT_GAIN;
    double A_DEFAULT_COLUMNS_PER_BEAT;
    int A_BYTES_NEEDED;
    int A_BUILDER_DATA_SIZE;
    int A_PLAY_DATA_SIZE;
    int A_CAPTURE_DATA_SIZE;
    int CACHE_FILE_CAPACITY;
    int CONFIG_PATH_MAX;
    int A_WARMUP_FRAMES_FACTOR;
    int WR_VIEWS_SAVED;
    float WR_COLOR_STEP;
    double WR_PLAY_CALLBACK_FPS;
    double WR_CAPTURE_CALLBACK_FPS;
    int WR_WARPOON_TEXT_COLS;
    int WR_STATES;
    int WR_SEQUENCE_COUNT;
    int WR_SEQUENCE_LENGTH_MAX;
    int WR_CALLBACK_SIZE;
    int WR_MODE_COUNT;
    int WR_KEYSYM_COUNT;
    int WR_MOD_COUNT;
    int WR_NOTE_QUADS_MAX;
    int WR_STATUS_BAR_COLS_MAX;
    int WR_TEXT_QUADS_MAX;
    int WR_QUADS_MAX;
    char* WR_LEADER;
    int WR_WAYLAND_MSG_BUFFER_SIZE;
    int WR_WAYLAND_MAX_OBJECTS;
    int WR_WAYLAND_MAX_OP_CODES;
    int WR_FN_NAME_LIMIT;
    int WR_UNDO_NODES_MAX;
    int WR_UNDO_NODES_CHILDREN_MAX;
    int WR_TIMESTAMP_LENGTH_MAX;
    int WR_REPEAT_DELAY_US;
    int WR_REPEAT_RATE_US;
    int WR_CURSOR_BLINK_DURATION_US;
    double WR_FPS;
    int WR_UNDO_NOTES_BATCH_MAX;
    int WR_INPUT_SEQUENCE_LENGTH_MAX;
    int ROLL_POSITION_X_Y;
    int POOL_ALIGNMENT;
    int CMD_COUNT;
    int PC_CONTROL_BUFFER_SIZE;
    int PC_PLAY_BUFFER_SIZE;
    int PC_CAPTURE_BUFFER_SIZE;
    int VK_ATLAS_WIDTH;
    int VK_ATLAS_HEIGHT;
    float VK_FONT_PIXEL_HEIGHT;
    int VK_MAX_FRAMES;
    int VK_GLYPH_COUNT;
    int VK_NSGT_DIFF_CAPACITY;
    int VK_ALIGNMENT;
    int NSGT_BIN_CAPACITY;
    int NSGT_FRAME_CAPACITY;
    int NSGT_FREQUENCY_MIN;
    int NSGT_FREQUENCY_MAX;
    float NSGT_ALPHA;
    float NSGT_SHAPE_FACTOR;
    int NSGT_WINDOW_LENGTH_MIN;
    int NSGT_RESOURCE_COUNT;
    int NSGT_DESCRIPTOR_SET_COUNT;
    int NSGT_SHADER_COUNT;
    int NSGT_PIPELINE_COUNT;
    int NSGT_GRAPHICS_FPS;
    int NSGT_GROUPS;
    int NEW_VULKAN_RESOURCE_COUNT;
    int NEW_VULKAN_DESCRIPTOR_SET_COUNT;
    int NEW_VULKAN_SHADER_COUNT;
    int NEW_VULKAN_PIPELINE_COUNT;
    int NEW_VULKAN_GROUPS;
    int NEW_VULKAN_NOTE_INSTANCE_MAX;
    int NEW_VULKAN_TEXT_INSTANCE_MAX;
    int NEW_VULKAN_LINE_INSTANCE_MAX;
    int NEW_VULKAN_CURSOR_INSTANCE_MAX;
    int NEW_VULKAN_HUD_INSTANCE_MAX;
    int NEW_VULKAN_HUD_CURSOR_INSTANCE_MAX;
    int NEW_VULKAN_HUD_TEXT_INSTANCE_MAX;
    int NEW_VULKAN_HUD_LINE_INSTANCE_MAX;
    int NEW_VULKAN_ATLAS_WIDTH;
    int NEW_VULKAN_ATLAS_HEIGHT;
    int NEW_VULKAN_FONT_PIXEL_HEIGHT;
    int NEW_VULKAN_GLYPH_COUNT;
    int NEW_VULKAN_SDF_SCALE;
    int NEW_VULKAN_SDF_PADDING;
    int NEW_VULKAN_BUFFER_MAX;
    float NEW_VULKAN_SDF_RANGE;
    float NEW_VULKAN_SDF_LARGE;
    int CACHE_NSGT_CAPACITY;
    int VK_NSGT_VISUAL_QUAD_CAPACITY;
    int VK_NSGT_VISUAL_RESOURCE_COUNT;
    float DEFAULT_ALPHA_SCALE;
    float DEFAULT_CURSOR_ALPHA_SCALE;
    float DEFAULT_PLAYBACK_BAR_THICKNESS;
    float DEFAULT_TEXT_FEATHER;
    float DEFAULT_TEXT_THICKNESS;
    float DEFAULT_BOLD_TEXT_FEATHER;
    float DEFAULT_BOLD_TEXT_THICKNESS;
    float WINDOWED_TEXT_FEATHER;
    float WINDOWED_TEXT_THICKNESS;
    float DEFAULT_WINDOWED_ALPHA_SCALE;
    float DEFAULT_WINDOWED_CURSOR_ALPHA_SCALE;
    int HUD_COUNT;
    int HUD_STATUS_BOTTOM_INSTANCE_MAX;
    int HUD_STATUS_TOP_INSTANCE_MAX;
    int HUD_STATUS_MIDDLE_INSTANCE_MAX;
    int HUD_LINE_NUMBERS_INSTANCE_MAX;
    int HUD_PIANO_INSTANCE_MAX;
    int HUD_EXPLORE_INSTANCE_MAX;
    int CURSOR_COUNT;
    int CURSOR_DEFAULT_INSTANCE_MAX;
    int LINE_COUNT;
    int LINE_CELL_INSTANCE_MAX;
    int LINE_BPM_INSTANCE_MAX;
    int TEXT_COUNT;
    int TEXT_STATUS_BOTTOM_INSTANCE_MAX;
    int TEXT_STATUS_TOP_INSTANCE_MAX;
    int TEXT_STATUS_MIDDLE_INSTANCE_MAX;
    int TEXT_PIANO_INSTANCE_MAX;
    int TEXT_LINE_NUMBERS_INSTANCE_MAX;
    int TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX;
    int TEXT_EXPLORE_INSTANCE_MAX;
    int TEXT_ERROR_INSTANCE_MAX;
    int NOTE_COUNT;
    int NOTE_GRID_INSTANCE_MAX;
    int HUD_LINE_COUNT;
    int HUD_LINE_PIANO_INSTANCE_MAX;
    int HUD_TEXT_COUNT;
    int HUD_TEXT_STATUS_BOTTOM_INSTANCE_MAX;
    int HUD_TEXT_STATUS_TOP_INSTANCE_MAX;
    int HUD_TEXT_STATUS_MIDDLE_INSTANCE_MAX;
    int HUD_TEXT_PIANO_INSTANCE_MAX;
    int HUD_TEXT_LINE_NUMBERS_INSTANCE_MAX;
    int HUD_TEXT_RELATIVE_LINE_NUMBERS_INSTANCE_MAX;
    int HUD_TEXT_EXPLORE_INSTANCE_MAX;
    int HUD_TEXT_ERROR_INSTANCE_MAX;
    int HUD_CURSOR_COUNT;
    int HUD_CURSOR_DEFAULT_INSTANCE_MAX;
    // pool context
    int POOL_MAX_ALLOCATIONS;
    // keymap context
    uint32_t KEYMAP_STATE_CAPACITY;
    uint32_t KEYMAP_KEYSYM_CAPACITY;
    uint32_t KEYMAP_MOD_CAPACITY;
    uint32_t KEYMAP_FUNCTION_CAPACITY;
    // core directories
    char* DIR_CONFIG;
    char* DIR_CACHE;
    char* DIR_UNDO;
    char* DIR_OVERRIDE;
    char* DIR_JUMPLIST;
    char* DIR_WARPOON;
    // hot context
    uint32_t HOT_CONTEXT_NAME_LIMIT;
    uint32_t HOT_CONTEXT_CMD_LIMIT;
    char* HOT_CONTEXT_TEMPLATE;
} war_config_context;

typedef uint64_t war_event_flags;
typedef enum war_event_flags_bits {
    WAR_EVENT_NONE = 0,
    WAR_EVENT_ALL = 0xFFFFFFFFFFFFFFFFULL,
    WAR_EVENT_MOVE_CURSOR_UP = 1ULL << 0,
    WAR_EVENT_MOVE_CURSOR_DOWN = 1ULL << 1,
    WAR_EVENT_MOVE_CURSOR_LEFT = 1ULL << 2,
    WAR_EVENT_MOVE_CURSOR_RIGHT = 1ULL << 3,
    WAR_EVENT_MOVE_CURSOR_UP_LEAP = 1ULL << 4,
    WAR_EVENT_MOVE_CURSOR_DOWN_LEAP = 1ULL << 5,
    WAR_EVENT_MOVE_CURSOR_LEFT_LEAP = 1ULL << 6,
    WAR_EVENT_MOVE_CURSOR_RIGHT_LEAP = 1ULL << 7,
} war_event_flags_bits;

typedef struct war_hook_context {
    war_mode_flags* mode_flags;
    war_event_flags* event_flags;
    void* (*function)(war_env* env);
} war_hook_context;

typedef uint32_t war_pool_id;
typedef enum war_pool_id_enum {
    //-------------------------------------------------------------------------
    // AUDIO
    //-------------------------------------------------------------------------
    WAR_POOL_ID_AUDIO_CTX_PW,
    WAR_POOL_ID_AUDIO_CTX_PW_PLAY_BUILDER_DATA,
    WAR_POOL_ID_AUDIO_CTX_PW_CAPTURE_BUILDER_DATA,
    WAR_POOL_ID_AUDIO_CTX_PW_PLAY_DATA,
    WAR_POOL_ID_AUDIO_CTX_PW_CAPTURE_DATA,
    WAR_POOL_ID_AUDIO_CONTROL_PAYLOAD,
    WAR_POOL_ID_AUDIO_TMP_CONTROL_PAYLOAD,
    WAR_POOL_ID_AUDIO_PC_CONTROL_CMD,
    WAR_POOL_ID_AUDIO_PLAY_READ_COUNT,
    WAR_POOL_ID_AUDIO_PLAY_LAST_READ_TIME,
    WAR_POOL_ID_AUDIO_CAPTURE_READ_COUNT,
    WAR_POOL_ID_AUDIO_CAPTURE_LAST_READ_TIME,
    //-------------------------------------------------------------------------
    // MAIN
    //-------------------------------------------------------------------------
    // ctx sequence
    WAR_POOL_ID_MAIN_CTX_SEQUENCE,
    WAR_POOL_ID_MAIN_CTX_SEQUENCE_PTRS,
    WAR_POOL_ID_MAIN_CTX_SEQUENCE_SEQUENCE,
    WAR_POOL_ID_MAIN_CTX_SEQUENCE_BUFFER_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_SEQUENCE_STAGE,
    // ctx hud
    WAR_POOL_ID_MAIN_CTX_HUD,
    WAR_POOL_ID_MAIN_CTX_HUD_PTRS,
    WAR_POOL_ID_MAIN_CTX_HUD_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_HUD_STAGE,
    // ctx hud line
    WAR_POOL_ID_MAIN_CTX_HUD_LINE,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_PTRS,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_LINE_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_HUD_LINE_STAGE,
    // ctx cursor
    WAR_POOL_ID_MAIN_CTX_CURSOR,
    WAR_POOL_ID_MAIN_CTX_CURSOR_PTRS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_VISUAL_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_CURSOR_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_CURSOR_STAGE,
    // ctx hud cursor
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_PTRS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_VISUAL_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_HUD_CURSOR_STAGE,
    // ctx line
    WAR_POOL_ID_MAIN_CTX_LINE,
    WAR_POOL_ID_MAIN_CTX_LINE_PTRS,
    WAR_POOL_ID_MAIN_CTX_LINE_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_LINE_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_LINE_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_LINE_LINE_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_LINE_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_LINE_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_LINE_STAGE,
    // ctx text
    WAR_POOL_ID_MAIN_CTX_TEXT,
    WAR_POOL_ID_MAIN_CTX_TEXT_PTRS,
    WAR_POOL_ID_MAIN_CTX_TEXT_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_TEXT_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_TEXT_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_TEXT_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_TEXT_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_TEXT_STAGE,
    // ctx hud text
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT,
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT_PTRS,
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT_X_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT_Y_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT_WIDTH_SECONDS,
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT_HEIGHT_CELLS,
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_HUD_TEXT_STAGE,
    // misc context
    WAR_POOL_ID_MAIN_CTX_MISC,
    // vk context
    WAR_POOL_ID_MAIN_CTX_VK,
    WAR_POOL_ID_MAIN_CTX_VK_IN_FLIGHT_FENCES,
    WAR_POOL_ID_MAIN_CTX_VK_QUADS_VERTEX_BUFFER_MAPPED,
    WAR_POOL_ID_MAIN_CTX_VK_QUADS_INDEX_BUFFER_MAPPED,
    WAR_POOL_ID_MAIN_CTX_VK_QUADS_INSTANCE_BUFFER_MAPPED,
    WAR_POOL_ID_MAIN_CTX_VK_GLYPHS,
    WAR_POOL_ID_MAIN_CTX_VK_TEXT_VERTEX_BUFFER_MAPPED,
    WAR_POOL_ID_MAIN_CTX_VK_TEXT_INSTANCE_BUFFER_MAPPED,
    WAR_POOL_ID_MAIN_CTX_VK_TEXT_INDEX_BUFFER_MAPPED,
    // nsgt context
    WAR_POOL_ID_MAIN_CTX_NSGT,
    WAR_POOL_ID_MAIN_CTX_NSGT_MEMORY_PROPERTY_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_USAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_BUFFER,
    WAR_POOL_ID_MAIN_CTX_NSGT_MEMORY_REQUIREMENTS,
    WAR_POOL_ID_MAIN_CTX_NSGT_DEVICE_MEMORY,
    WAR_POOL_ID_MAIN_CTX_NSGT_MAP,
    WAR_POOL_ID_MAIN_CTX_NSGT_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_BUFFER_INFO,
    WAR_POOL_ID_MAIN_CTX_NSGT_COMPUTE_DESCRIPTOR_IMAGE_INFO,
    WAR_POOL_ID_MAIN_CTX_NSGT_GRAPHICS_DESCRIPTOR_IMAGE_INFO,
    WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE,
    WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_VIEW,
    WAR_POOL_ID_MAIN_CTX_NSGT_FORMAT,
    WAR_POOL_ID_MAIN_CTX_NSGT_EXTENT_3D,
    WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_USAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_SET_LAYOUT_BINDING,
    WAR_POOL_ID_MAIN_CTX_NSGT_WRITE_DESCRIPTOR_SET,
    WAR_POOL_ID_MAIN_CTX_NSGT_MAPPED_MEMORY_RANGE,
    WAR_POOL_ID_MAIN_CTX_NSGT_BUFFER_MEMORY_BARRIER,
    WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_MEMORY_BARRIER,
    WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_IMAGE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NSGT_ACCESS_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_ACCESS_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_PIPELINE_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_SRC_IDX,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_DST_IDX,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_SIZE,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_SRC_OFFSET,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_DST_OFFSET,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_DATA,
    WAR_POOL_ID_MAIN_CTX_NSGT_FN_DATA_2,
    WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_SET,
    WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_SET_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NSGT_IMAGE_DESCRIPTOR_TYPE,
    WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_POOL,
    WAR_POOL_ID_MAIN_CTX_NSGT_DESCRIPTOR_IMAGE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_MODULE,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_SHADER_STAGE_CREATE_INFO,
    WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_STAGE_FLAG_BITS,
    WAR_POOL_ID_MAIN_CTX_NSGT_SHADER_PATH,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_SET_IDX,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_SHADER_IDX,
    WAR_POOL_ID_MAIN_CTX_NSGT_STRUCTURE_TYPE,
    WAR_POOL_ID_MAIN_CTX_NSGT_PUSH_CONSTANT_SHADER_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NSGT_PUSH_CONSTANT_SIZE,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_BIND_POINT,
    WAR_POOL_ID_MAIN_CTX_NSGT_SIZE,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_DISPATCH_GROUP,
    WAR_POOL_ID_MAIN_CTX_NSGT_PIPELINE_LOCAL_SIZE,
    // new_vulkan context
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MEMORY_PROPERTY_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_USAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MEMORY_REQUIREMENTS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DEVICE_MEMORY,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MAP,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_CAPACITY,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_BUFFER_INFO,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_COMPUTE_DESCRIPTOR_IMAGE_INFO,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_GRAPHICS_DESCRIPTOR_IMAGE_INFO,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_VIEW,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FORMAT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_EXTENT_3D,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_USAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_SET_LAYOUT_BINDING,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_WRITE_DESCRIPTOR_SET,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_MAPPED_MEMORY_RANGE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_MEMORY_BARRIER,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_MEMORY_BARRIER,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_IMAGE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_ACCESS_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_ACCESS_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PIPELINE_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SRC_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DST_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SIZE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SIZE_2,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_SRC_OFFSET,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DST_OFFSET,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DATA,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_DATA_2,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_SET,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_SET_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IMAGE_DESCRIPTOR_TYPE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_POOL,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DESCRIPTOR_IMAGE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_MODULE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_SHADER_STAGE_CREATE_INFO,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_STAGE_FLAG_BITS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SHADER_PATH,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_SET_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_SHADER_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_STRUCTURE_TYPE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PUSH_CONSTANT_SHADER_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PUSH_CONSTANT_SIZE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_BIND_POINT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_SIZE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_DISPATCH_GROUP,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_LOCAL_SIZE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VIEWPORT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_RECT_2D,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_IN_DESCRIPTOR_SET,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_INPUT_BINDING_DESCRIPTION,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_INPUT_RATE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_ATTRIBUTE_COUNT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_STRIDE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_OFFSETS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_VERTEX_FORMATS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_VERTEX_INPUT_BINDING_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_GLYPH_INFO,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_PUSH_CONSTANT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_COLOR_BLEND_ATTACHMENT_STATE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PIPELINE_INSTANCE_STAGE_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_Z_LAYER,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DIRTY_BUFFER,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_DRAW_BUFFER,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_INSTANCE_COUNT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_FIRST_INSTANCE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_PIPELINE_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_FIRST_INSTANCE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_INSTANCE_COUNT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_SIZE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_SRC_OFFSET,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_ACCESS_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_PIPELINE_STAGE_FLAGS,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_IMAGE_LAYOUT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_SRC_RESOURCE_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_BUFFER_DST_RESOURCE_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PIPELINE_IDX,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_X,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_Y,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_FACTOR_X,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_FN_PAN_FACTOR_Y,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PTRS_BUFFER_PUSH_CONSTANT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_NOTE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_TEXT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_LINE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_CURSOR,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD_CURSOR,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD_TEXT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_PUSH_CONSTANT_HUD_LINE,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PTRS_BUFFER_VIEWPORT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_VIEWPORT,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_PTRS_BUFFER_RECT_2D,
    WAR_POOL_ID_MAIN_CTX_NEW_VULKAN_BUFFER_RECT_2D,
    // command context
    WAR_POOL_ID_MAIN_CTX_COMMAND,
    WAR_POOL_ID_MAIN_CTX_COMMAND_INPUT,
    WAR_POOL_ID_MAIN_CTX_COMMAND_TEXT,
    WAR_POOL_ID_MAIN_CTX_COMMAND_PROMPT,
    // status context
    WAR_POOL_ID_MAIN_CTX_STATUS,
    WAR_POOL_ID_MAIN_CTX_STATUS_TOP,
    WAR_POOL_ID_MAIN_CTX_STATUS_MIDDLE,
    WAR_POOL_ID_MAIN_CTX_STATUS_BOTTOM,
    // char input
    WAR_POOL_ID_MAIN_CHAR_INPUT,
    // play context
    WAR_POOL_ID_MAIN_CTX_PLAY,
    WAR_POOL_ID_MAIN_CTX_PLAY_KEY_LAYERS,
    WAR_POOL_ID_MAIN_CTX_PLAY_KEYS,
    // capture context
    WAR_POOL_ID_MAIN_CTX_CAPTURE,
    // capture_wav
    WAR_POOL_ID_MAIN_CAPTURE_WAV,
    WAR_POOL_ID_MAIN_CAPTURE_WAV_FNAME,
    // cache
    WAR_POOL_ID_MAIN_CACHE_FILE,
    WAR_POOL_ID_MAIN_CACHE_FILE_ID,
    WAR_POOL_ID_MAIN_CACHE_FILE_TIMESTAMP,
    WAR_POOL_ID_MAIN_CACHE_FILE_FILE,
    WAR_POOL_ID_MAIN_CACHE_FILE_TYPE,
    WAR_POOL_ID_MAIN_CACHE_FILE_INODE,
    WAR_POOL_ID_MAIN_CACHE_FILE_DEVICE,
    WAR_POOL_ID_MAIN_CACHE_FILE_FD_SIZE,
    WAR_POOL_ID_MAIN_CACHE_FILE_MEMFD_SIZE,
    WAR_POOL_ID_MAIN_CACHE_FILE_MEMFD_CAPACITY,
    WAR_POOL_ID_MAIN_CACHE_FILE_FD,
    WAR_POOL_ID_MAIN_CACHE_FILE_MEMFD,
    WAR_POOL_ID_MAIN_CACHE_FILE_FREE,
    // map_wav
    WAR_POOL_ID_MAIN_MAP_WAV,
    WAR_POOL_ID_MAIN_MAP_WAV_ID,
    WAR_POOL_ID_MAIN_MAP_WAV_FNAME,
    WAR_POOL_ID_MAIN_MAP_WAV_FNAME_SIZE,
    // color
    WAR_POOL_ID_MAIN_CTX_COLOR,
    // layres
    WAR_POOL_ID_MAIN_LAYERS_ACTIVE,
    // Colors
    WAR_POOL_ID_MAIN_COLORS,
    // views
    WAR_POOL_ID_MAIN_VIEWS_COL,
    WAR_POOL_ID_MAIN_VIEWS_ROW,
    WAR_POOL_ID_MAIN_VIEWS_LEFT_COL,
    WAR_POOL_ID_MAIN_VIEWS_RIGHT_COL,
    WAR_POOL_ID_MAIN_VIEWS_BOTTOM_ROW,
    WAR_POOL_ID_MAIN_VIEWS_TOP_ROW,
    WAR_POOL_ID_MAIN_VIEWS_WARPOON_TEXT,
    WAR_POOL_ID_MAIN_VIEWS_WARPOON_TEXT_ROWS,
    // FSM CONTEXT
    WAR_POOL_ID_MAIN_CTX_FSM,
    WAR_POOL_ID_MAIN_CTX_FSM_FUNCTION,
    WAR_POOL_ID_MAIN_CTX_FSM_FUNCTION_TYPE,
    WAR_POOL_ID_MAIN_CTX_FSM_NAME,
    WAR_POOL_ID_MAIN_CTX_FSM_IS_TERMINAL,
    WAR_POOL_ID_MAIN_CTX_FSM_HANDLE_RELEASE,
    WAR_POOL_ID_MAIN_CTX_FSM_HANDLE_TIMEOUT,
    WAR_POOL_ID_MAIN_CTX_FSM_HANDLE_REPEAT,
    WAR_POOL_ID_MAIN_CTX_FSM_IS_PREFIX,
    WAR_POOL_ID_MAIN_CTX_FSM_NEXT_STATE,
    WAR_POOL_ID_MAIN_CTX_FSM_CWD,
    WAR_POOL_ID_MAIN_CTX_FSM_CURRENT_FILE_PATH,
    WAR_POOL_ID_MAIN_CTX_FSM_EXT,
    WAR_POOL_ID_MAIN_CTX_FSM_KEY_DOWN,
    WAR_POOL_ID_MAIN_CTX_FSM_KEY_LAST_EVENT_US,
    // quads vertices
    WAR_POOL_ID_MAIN_QUAD_VERTICES,
    WAR_POOL_ID_MAIN_QUAD_INDICES,
    WAR_POOL_ID_MAIN_TRANSPARENT_QUAD_VERTICES,
    WAR_POOL_ID_MAIN_TRANSPARENT_QUAD_INDICES,
    WAR_POOL_ID_MAIN_TEXT_VERTICES,
    WAR_POOL_ID_MAIN_TEXT_INDICES,
    // note quads
    WAR_POOL_ID_MAIN_NOTE_QUADS_ALIVE,
    WAR_POOL_ID_MAIN_NOTE_QUADS_ID,
    WAR_POOL_ID_MAIN_NOTE_QUADS_POS_X,
    WAR_POOL_ID_MAIN_NOTE_QUADS_POS_Y,
    WAR_POOL_ID_MAIN_NOTE_QUADS_LAYER,
    WAR_POOL_ID_MAIN_NOTE_QUADS_SIZE_X,
    WAR_POOL_ID_MAIN_NOTE_QUADS_NAVIGATION_X,
    WAR_POOL_ID_MAIN_NOTE_QUADS_NAVIGATION_X_NUMERATOR,
    WAR_POOL_ID_MAIN_NOTE_QUADS_NAVIGATION_X_DENOMINATOR,
    WAR_POOL_ID_MAIN_NOTE_QUADS_SIZE_X_NUMERATOR,
    WAR_POOL_ID_MAIN_NOTE_QUADS_SIZE_X_DENOMINATOR,
    WAR_POOL_ID_MAIN_NOTE_QUADS_COLOR,
    WAR_POOL_ID_MAIN_NOTE_QUADS_OUTLINE_COLOR,
    WAR_POOL_ID_MAIN_NOTE_QUADS_GAIN,
    WAR_POOL_ID_MAIN_NOTE_QUADS_VOICE,
    WAR_POOL_ID_MAIN_NOTE_QUADS_HIDDEN,
    WAR_POOL_ID_MAIN_NOTE_QUADS_MUTE,
    // keydown, keylasteventus, msgbuffer, pc_window_render, payload, input
    // sequence
    WAR_POOL_ID_MAIN_KEY_DOWN,
    WAR_POOL_ID_MAIN_KEY_LAST_EVENT_US,
    WAR_POOL_ID_MAIN_MSG_BUFFER,
    WAR_POOL_ID_MAIN_OBJ_OP,
    WAR_POOL_ID_MAIN_PC_CONTROL_CMD,
    WAR_POOL_ID_MAIN_CONTROL_PAYLOAD,
    WAR_POOL_ID_MAIN_TMP_CONTROL_PAYLOAD,
    WAR_POOL_ID_MAIN_INPUT_SEQUENCE,
    WAR_POOL_ID_MAIN_PROMPT,
    WAR_POOL_ID_MAIN_CWD,
    // undo tree
    WAR_POOL_ID_MAIN_UNDO_TREE,
    WAR_POOL_ID_MAIN_UNDO_NODE_ID,
    WAR_POOL_ID_MAIN_UNDO_NODE_SEQ_NUM,
    WAR_POOL_ID_MAIN_UNDO_NODE_BRANCH_ID,
    WAR_POOL_ID_MAIN_UNDO_NODE_COMMAND,
    WAR_POOL_ID_MAIN_UNDO_NODE_PAYLOAD,
    WAR_POOL_ID_MAIN_UNDO_NODE_CURSOR_POS_X,
    WAR_POOL_ID_MAIN_UNDO_NODE_CURSOR_POS_Y,
    WAR_POOL_ID_MAIN_UNDO_NODE_LEFT_COL,
    WAR_POOL_ID_MAIN_UNDO_NODE_RIGHT_COL,
    WAR_POOL_ID_MAIN_UNDO_NODE_TOP_ROW,
    WAR_POOL_ID_MAIN_UNDO_NODE_BOTTOM_ROW,
    WAR_POOL_ID_MAIN_UNDO_NODE_TIMESTAMP,
    WAR_POOL_ID_MAIN_UNDO_NODE_PARENT,
    WAR_POOL_ID_MAIN_UNDO_NODE_NEXT,
    WAR_POOL_ID_MAIN_UNDO_NODE_PREV,
    WAR_POOL_ID_MAIN_UNDO_NODE_ALT_NEXT,
    WAR_POOL_ID_MAIN_UNDO_NODE_ALT_PREV,
    // ctx config
    WAR_POOL_ID_CONFIG_CONTEXT,
    // ctx keymap
    WAR_POOL_ID_MAIN_CTX_KEYMAP,
    WAR_POOL_ID_MAIN_CTX_KEYMAP_FUNCTION,
    WAR_POOL_ID_MAIN_CTX_KEYMAP_FUNCTION_COUNT,
    WAR_POOL_ID_MAIN_CTX_KEYMAP_FLAGS,
    WAR_POOL_ID_MAIN_CTX_KEYMAP_NEXT_STATE,
    // ctx hot
    WAR_POOL_ID_HOT_CONTEXT,
    WAR_POOL_ID_HOT_CONTEXT_FUNCTION,
    WAR_POOL_ID_HOT_CONTEXT_HANDLE,
    WAR_POOL_ID_HOT_CONTEXT_FN_ID,
    WAR_POOL_ID_HOT_CONTEXT_NAME,
    // env
    WAR_POOL_ID_ENV,
    // pool context
    WAR_POOL_ID_POOL_CONTEXT,
    WAR_POOL_ID_POOL_CONTEXT_SIZE,
    WAR_POOL_ID_POOL_CONTEXT_OFFSET,
    WAR_POOL_ID_POOL_CONTEXT_ALIGNMENT,
    WAR_POOL_ID_POOL_CONTEXT_ID,
    //
    WAR_POOL_ID_COUNT,
} war_pool_id_enum;

typedef struct war_pool_context {
    uint32_t version;
    //
    uint64_t* size;
    uint64_t* offset;
    uint32_t* alignment;
    war_pool_id* id;
    //
    uint8_t* pool;
    //
    uint32_t count;
    uint64_t total_size;
} war_pool_context;

typedef uint32_t war_hot_id;
typedef enum war_hot_id_enum {
    WAR_HOT_ID_CONFIG,
    WAR_HOT_ID_COMMAND,
    WAR_HOT_ID_COLOR,
    WAR_HOT_ID_PLUGIN,
    WAR_HOT_ID_POOL,
    WAR_HOT_ID_KEYMAP,
    //
    WAR_HOT_ID_COUNT,
} war_hot_id_enum;
typedef struct war_hot_context {
    void** function;
    void** handle;
    char** name;
    //
    uint32_t fn_count;
    war_hot_id* fn_id;
} war_hot_context;

struct war_env {
    war_atomics* atomics;
    war_lua_context* ctx_lua;
    war_play_context* ctx_play;
    war_capture_context* ctx_capture;
    war_pool* pool_wr;
    war_file* capture_wav;
    war_fsm_context* ctx_fsm;
    war_producer_consumer* pc_capture;
    war_nsgt_context* ctx_nsgt;
    war_new_vulkan_context* ctx_new_vulkan;
    war_cursor_context* ctx_cursor;
    war_misc_context* ctx_misc;
    // new
    war_config_context* ctx_config;
    war_command_context* ctx_command;
    war_keymap_context* ctx_keymap;
    war_pool_context* ctx_pool;
    war_hook_context* ctx_hook;
    void** plugins;
    war_color_context* ctx_color;
    war_hot_context* ctx_hot;
};

#endif // WAR_DATA_H
