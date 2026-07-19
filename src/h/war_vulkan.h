//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_vulkan.h
//-----------------------------------------------------------------------------

#ifndef WAR_VULKAN_H
#define WAR_VULKAN_H

#include "war_data.h"
#include "war_debug_macros.h"
#include "war_embed_shaders.h"
#include "war_functions.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libdrm/drm_fourcc.h>
#include <libdrm/drm_mode.h>
#include <linux/socket.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

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

static inline void war_cursor_init(war_cursor_context* ctx_cursor,
                                   war_pool_context* ctx_pool,
                                   war_config_context* ctx_config,
                                   war_vulkan_context* ctx_vk) {
    //-------------------------------------------------------------------------
    // ALLOCATE CPU ARRAYS FROM POOL
    //-------------------------------------------------------------------------
    uint32_t max_instances = (uint32_t)ctx_config->CURSOR_DEFAULT_INSTANCE_MAX;
    ctx_cursor->draw =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_DRAW);
    ctx_cursor->x_seconds =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_X_SECONDS);
    ctx_cursor->y_cells =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_Y_CELLS);
    ctx_cursor->instance =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_INSTANCE);
    ctx_cursor->instance_count = 0;

    //-------------------------------------------------------------------------
    // LOAD SHADER SPIR-V (embedded)
    //-------------------------------------------------------------------------
    WASSERT(build_spv_war_new_vulkan_vertex_cursor_spv_len > 0 &&
            build_spv_war_new_vulkan_vertex_cursor_spv_len % 4 == 0);
    WASSERT(build_spv_war_new_vulkan_fragment_cursor_spv_len > 0 &&
            build_spv_war_new_vulkan_fragment_cursor_spv_len % 4 == 0);
    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = build_spv_war_new_vulkan_vertex_cursor_spv_len,
        .pCode = (uint32_t*)build_spv_war_new_vulkan_vertex_cursor_spv,
    };
    VkResult res = vkCreateShaderModule(
        ctx_vk->device, &smci, NULL, &ctx_cursor->vert_module);
    WASSERT(res == VK_SUCCESS);
    smci.codeSize = build_spv_war_new_vulkan_fragment_cursor_spv_len;
    smci.pCode = (uint32_t*)build_spv_war_new_vulkan_fragment_cursor_spv;
    res = vkCreateShaderModule(
        ctx_vk->device, &smci, NULL, &ctx_cursor->frag_module);
    WASSERT(res == VK_SUCCESS);

    //-------------------------------------------------------------------------
    // PIPELINE LAYOUT (push constants only)
    //-------------------------------------------------------------------------
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    res = vkCreatePipelineLayout(
        ctx_vk->device, &plci, NULL, &ctx_cursor->pipeline_layout);
    WASSERT(res == VK_SUCCESS);

    //-------------------------------------------------------------------------
    // GRAPHICS PIPELINE
    //-------------------------------------------------------------------------
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = ctx_cursor->vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = ctx_cursor->frag_module,
         .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(war_vulkan_cursor_instance), VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[5] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1,
         1,
         VK_FORMAT_R32G32B32_SFLOAT,
         offsetof(war_vulkan_cursor_instance, pos)},
        {2,
         1,
         VK_FORMAT_R32G32_SFLOAT,
         offsetof(war_vulkan_cursor_instance, size)},
        {3,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_vulkan_cursor_instance, color)},
        {7, 1, VK_FORMAT_R32_UINT, offsetof(war_vulkan_cursor_instance, flags)},
    };
    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 5,
        .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState cba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vis,
        .pInputAssemblyState = &ias,
        .pViewportState = &vpsi,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDynamicState = &dsi,
        .pColorBlendState = &cbs,
        .layout = ctx_cursor->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    res = vkCreateGraphicsPipelines(
        ctx_vk->device, VK_NULL_HANDLE, 1, &gpci, NULL, &ctx_cursor->pipeline);
    WASSERT(res == VK_SUCCESS);

    //-------------------------------------------------------------------------
    // QUAD VERTEX BUFFER
    //-------------------------------------------------------------------------
    float quad_verts[] = {
        0.0f,
        0.0f,
        1.0f,
        0.0f,
        0.0f,
        1.0f,
        1.0f,
        1.0f,
    };
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    res = vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_cursor->quad_vbo);
    WASSERT(res == VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_cursor->quad_vbo, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props,
                                      mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    res = vkAllocateMemory(
        ctx_vk->device, &mai, NULL, &ctx_cursor->quad_vbo_memory);
    WASSERT(res == VK_SUCCESS);
    vkBindBufferMemory(
        ctx_vk->device, ctx_cursor->quad_vbo, ctx_cursor->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(ctx_vk->device,
                ctx_cursor->quad_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_cursor->quad_vbo_memory);

    //-------------------------------------------------------------------------
    // INSTANCE VERTEX BUFFER (host-visible, persistently mapped)
    //-------------------------------------------------------------------------
    VkDeviceSize instance_buf_size =
        sizeof(war_vulkan_cursor_instance) * max_instances;
    bci.size = instance_buf_size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    res = vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_cursor->instance_vbo);
    WASSERT(res == VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_cursor->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props,
                             mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    res = vkAllocateMemory(
        ctx_vk->device, &mai, NULL, &ctx_cursor->instance_vbo_memory);
    WASSERT(res == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device,
                       ctx_cursor->instance_vbo,
                       ctx_cursor->instance_vbo_memory,
                       0);
    vkMapMemory(ctx_vk->device,
                ctx_cursor->instance_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &ctx_cursor->instance_mapped);
}

static inline void war_piano_gutter_init(war_piano_gutter_context* ctx_pg,
                                         war_pool_context* ctx_pool,
                                         war_config_context* ctx_config,
                                         war_vulkan_context* ctx_vk) {
    uint32_t max_instances = (uint32_t)ctx_config->HUD_PIANO_INSTANCE_MAX;
    ctx_pg->draw =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_DRAW);
    ctx_pg->x_cells =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_X_CELLS);
    ctx_pg->y_cells =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_Y_CELLS);
    ctx_pg->x_width =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_X_WIDTH);
    ctx_pg->instance = war_pool_alloc_new(
        ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_INSTANCE);
    ctx_pg->instance_count = 0;

    WASSERT(build_spv_war_new_vulkan_vertex_piano_gutter_spv_len > 0 &&
            build_spv_war_new_vulkan_vertex_piano_gutter_spv_len % 4 == 0);
    WASSERT(build_spv_war_new_vulkan_fragment_piano_gutter_spv_len > 0 &&
            build_spv_war_new_vulkan_fragment_piano_gutter_spv_len % 4 == 0);

    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = build_spv_war_new_vulkan_vertex_piano_gutter_spv_len,
        .pCode = (uint32_t*)build_spv_war_new_vulkan_vertex_piano_gutter_spv,
    };
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_pg->vert_module) ==
            VK_SUCCESS);
    smci.codeSize = build_spv_war_new_vulkan_fragment_piano_gutter_spv_len;
    smci.pCode = (uint32_t*)build_spv_war_new_vulkan_fragment_piano_gutter_spv;
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_pg->frag_module) ==
            VK_SUCCESS);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    WASSERT(vkCreatePipelineLayout(
                ctx_vk->device, &plci, NULL, &ctx_pg->pipeline_layout) ==
            VK_SUCCESS);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = ctx_pg->vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = ctx_pg->frag_module,
         .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1,
         sizeof(war_vulkan_piano_gutter_instance),
         VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[8] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1,
         1,
         VK_FORMAT_R32G32B32_SFLOAT,
         offsetof(war_vulkan_piano_gutter_instance, pos)},
        {2,
         1,
         VK_FORMAT_R32G32_SFLOAT,
         offsetof(war_vulkan_piano_gutter_instance, size)},
        {3,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_vulkan_piano_gutter_instance, color)},
        {4,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_vulkan_piano_gutter_instance, outline_color)},
        {5,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_vulkan_piano_gutter_instance, foreground_color)},
        {6,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_vulkan_piano_gutter_instance, foreground_outline_color)},
        {7,
         1,
         VK_FORMAT_R32_UINT,
         offsetof(war_vulkan_piano_gutter_instance, flags)},
    };
    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 8,
        .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState cba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vis,
        .pInputAssemblyState = &ias,
        .pViewportState = &vpsi,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDynamicState = &dsi,
        .pColorBlendState = &cbs,
        .layout = ctx_pg->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    WASSERT(vkCreateGraphicsPipelines(ctx_vk->device,
                                      VK_NULL_HANDLE,
                                      1,
                                      &gpci,
                                      NULL,
                                      &ctx_pg->pipeline) == VK_SUCCESS);

    float quad_verts[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_pg->quad_vbo) ==
            VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_pg->quad_vbo, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props,
                                      mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_pg->quad_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(
        ctx_vk->device, ctx_pg->quad_vbo, ctx_pg->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(
        ctx_vk->device, ctx_pg->quad_vbo_memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_pg->quad_vbo_memory);

    VkDeviceSize instance_buf_size =
        sizeof(war_vulkan_piano_gutter_instance) * max_instances;
    bci.size = instance_buf_size;
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_pg->instance_vbo) ==
            VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_pg->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props,
                             mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_pg->instance_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(
        ctx_vk->device, ctx_pg->instance_vbo, ctx_pg->instance_vbo_memory, 0);
    vkMapMemory(ctx_vk->device,
                ctx_pg->instance_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &ctx_pg->instance_mapped);
}

static inline void war_piano_gutter_generate(war_piano_gutter_context* ctx_pg,
                                             uint32_t gutter_cols,
                                             uint32_t gutter_rows) {
    int idx = 0;
    // MIDI keys 0-127 at rows gutter_rows to gutter_rows+127
    for (uint32_t i = 0; i < 128; i++) {
        uint32_t c = i % 12;
        int black = (c == 1 || c == 3 || c == 6 || c == 8 || c == 10);
        ctx_pg->instance[idx].color[0] = black ? 0 : 1;
        ctx_pg->instance[idx].color[1] = black ? 0 : 1;
        ctx_pg->instance[idx].color[2] = black ? 0 : 1;
        ctx_pg->instance[idx].color[3] = 1;
        ctx_pg->instance[idx].pos[0] = 0.0f;
        ctx_pg->instance[idx].pos[1] = (float)(gutter_rows + i);
        ctx_pg->instance[idx].pos[2] = 0;
        ctx_pg->instance[idx].size[0] = (float)gutter_cols;
        ctx_pg->instance[idx].size[1] = 1;
        ctx_pg->instance[idx].flags = 0;
        idx++;
    }
    ctx_pg->instance_count = (uint32_t)idx;
}

static inline void war_piano_gutter_render(VkCommandBuffer cmd,
                                           war_piano_gutter_context* ctx_pg,
                                           war_wayland_context* ctx_wayland,
                                           war_cursor_context* ctx_cursor,
                                           float screen_w,
                                           float screen_h) {
    if (!ctx_pg || !ctx_pg->instance_count) return;
    memcpy(ctx_pg->instance_mapped,
           ctx_pg->instance,
           sizeof(war_vulkan_piano_gutter_instance) * ctx_pg->instance_count);
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    int32_t gutter_bottom =
        (int32_t)((float)ctx_cursor->cell_height * ctx_wayland->zoom *
                  (ctx_wayland->gutter_rows - 0.5f));
    if (gutter_bottom < 0) gutter_bottom = 0;
    if (gutter_bottom > (int32_t)screen_h) gutter_bottom = (int32_t)screen_h;
    VkRect2D scissor = {{0, 0}, {(uint32_t)screen_w, (uint32_t)(screen_h - gutter_bottom)}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx_pg->pipeline);
    float pc_data[] = {
        (float)ctx_cursor->cell_width,
        (float)ctx_cursor->cell_height,
        0.0f,
        -ctx_wayland->panning[1] * ctx_wayland->zoom,
        ctx_wayland->zoom,
        0,
        screen_w,
        screen_h,
        0,
        0,
    };
    vkCmdPushConstants(cmd,
                       ctx_pg->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(pc_data),
                       pc_data);
    VkBuffer bufs[] = {ctx_pg->quad_vbo, ctx_pg->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, ctx_pg->instance_count, 0, 0);
}

static inline void war_gridlines_init(war_gridlines_context* ctx_gl,
                                      war_pool_context* ctx_pool,
                                      war_config_context* ctx_config,
                                      war_vulkan_context* ctx_vk) {
    uint32_t max_instances = (uint32_t)ctx_config->HUD_GRIDLINES_INSTANCE_MAX;
    ctx_gl->draw =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_GRIDLINES_DRAW);
    ctx_gl->x_cells =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_GRIDLINES_X_CELLS);
    ctx_gl->y_cells =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_GRIDLINES_Y_CELLS);
    ctx_gl->x_width =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_GRIDLINES_X_WIDTH);
    ctx_gl->instance =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_GRIDLINES_INSTANCE);
    ctx_gl->instance_count = 0;

    WASSERT(build_spv_war_new_vulkan_vertex_gridlines_spv_len > 0 &&
            build_spv_war_new_vulkan_vertex_gridlines_spv_len % 4 == 0);
    WASSERT(build_spv_war_new_vulkan_fragment_gridlines_spv_len > 0 &&
            build_spv_war_new_vulkan_fragment_gridlines_spv_len % 4 == 0);
    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = build_spv_war_new_vulkan_vertex_gridlines_spv_len,
        .pCode = (uint32_t*)build_spv_war_new_vulkan_vertex_gridlines_spv,
    };
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_gl->vert_module) ==
            VK_SUCCESS);
    smci.codeSize = build_spv_war_new_vulkan_fragment_gridlines_spv_len;
    smci.pCode = (uint32_t*)build_spv_war_new_vulkan_fragment_gridlines_spv;
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_gl->frag_module) ==
            VK_SUCCESS);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    WASSERT(vkCreatePipelineLayout(
                ctx_vk->device, &plci, NULL, &ctx_gl->pipeline_layout) ==
            VK_SUCCESS);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = ctx_gl->vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = ctx_gl->frag_module,
         .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1,
         sizeof(war_vulkan_gridlines_instance),
         VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[5] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1,
         1,
         VK_FORMAT_R32G32B32_SFLOAT,
         offsetof(war_vulkan_gridlines_instance, pos)},
        {2,
         1,
         VK_FORMAT_R32G32_SFLOAT,
         offsetof(war_vulkan_gridlines_instance, size)},
        {3,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_vulkan_gridlines_instance, color)},
        {4,
         1,
         VK_FORMAT_R32_UINT,
         offsetof(war_vulkan_gridlines_instance, flags)},
    };
    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 5,
        .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState cba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vis,
        .pInputAssemblyState = &ias,
        .pViewportState = &vpsi,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDynamicState = &dsi,
        .pColorBlendState = &cbs,
        .layout = ctx_gl->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    WASSERT(vkCreateGraphicsPipelines(ctx_vk->device,
                                      VK_NULL_HANDLE,
                                      1,
                                      &gpci,
                                      NULL,
                                      &ctx_gl->pipeline) == VK_SUCCESS);

    float quad_verts[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_gl->quad_vbo) ==
            VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_gl->quad_vbo, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props,
                                      mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_gl->quad_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(
        ctx_vk->device, ctx_gl->quad_vbo, ctx_gl->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(
        ctx_vk->device, ctx_gl->quad_vbo_memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_gl->quad_vbo_memory);

    VkDeviceSize instance_buf_size =
        sizeof(war_vulkan_gridlines_instance) * max_instances;
    bci.size = instance_buf_size;
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_gl->instance_vbo) ==
            VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_gl->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props,
                             mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_gl->instance_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(
        ctx_vk->device, ctx_gl->instance_vbo, ctx_gl->instance_vbo_memory, 0);
    vkMapMemory(ctx_vk->device,
                ctx_gl->instance_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &ctx_gl->instance_mapped);
}

static inline void war_gridlines_generate(war_gridlines_context* ctx_gl,
                                          double start_col,
                                          double end_col,
                                          uint32_t max_rows,
                                          uint32_t max_instances,
                                          uint32_t gutter_rows) {
    uint32_t count = 0;
    float h_thick = 0.06f;
    float v_thick = 0.06f;
    float v_thick_beat = 0.10f;
    for (uint32_t y = 0; y <= max_rows; y++) {
        if (count >= max_instances) break;
        if (y < gutter_rows) continue;
        ctx_gl->instance[count].pos[0] = (float)start_col;
        ctx_gl->instance[count].pos[1] = (float)y;
        ctx_gl->instance[count].pos[2] = 0.0f;
        ctx_gl->instance[count].size[0] = (float)(end_col - start_col);
        {
            ctx_gl->instance[count].size[1] = h_thick;
            ctx_gl->instance[count].color[0] = 0.2f;
            ctx_gl->instance[count].color[1] = 0.2f;
            ctx_gl->instance[count].color[2] = 0.2f;
        }
        ctx_gl->instance[count].color[3] = 1.0f;
        ctx_gl->instance[count].flags = 0;
        count++;
    }
    uint32_t x_start = (uint32_t)ceil(start_col);
    uint32_t x_end = (uint32_t)floor(end_col);
    uint32_t x_span = x_end > x_start ? x_end - x_start + 1 : 0;
    uint32_t remaining = max_instances > count ? max_instances - count : 0;
    // distribute vertical lines evenly across visible range within budget
    uint32_t step = 1;
    if (remaining > 0 && x_span > remaining)
        step = (x_span + remaining - 1) / remaining;
    for (uint32_t x = x_start; x <= x_end && count < max_instances; x += step) {
        ctx_gl->instance[count].pos[0] = (float)x;
        ctx_gl->instance[count].pos[1] = gutter_rows;
        ctx_gl->instance[count].pos[2] = 0.0f;
        ctx_gl->instance[count].size[0] = x % 4 == 0 ? v_thick_beat : v_thick;
        ctx_gl->instance[count].size[1] = (float)max_rows - gutter_rows;
        if (x % 4 == 0) {
            ctx_gl->instance[count].color[0] = 0.6f;
            ctx_gl->instance[count].color[1] = 0.0f;
            ctx_gl->instance[count].color[2] = 0.0f;
        } else {
            ctx_gl->instance[count].color[0] = 0.2f;
            ctx_gl->instance[count].color[1] = 0.2f;
            ctx_gl->instance[count].color[2] = 0.2f;
        }
        ctx_gl->instance[count].color[3] = 1.0f;
        ctx_gl->instance[count].flags = 0;
        count++;
    }
    ctx_gl->instance_count = count;
}

static inline void war_gridlines_render(VkCommandBuffer cmd,
                                        war_gridlines_context* ctx_gl,
                                        war_wayland_context* ctx_wayland,
                                        war_cursor_context* ctx_cursor,
                                        float screen_w,
                                        float screen_h) {
    if (!ctx_gl || !ctx_gl->instance_count) return;
    memcpy(ctx_gl->instance_mapped,
           ctx_gl->instance,
           sizeof(war_vulkan_gridlines_instance) * ctx_gl->instance_count);
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    int32_t gutter_right =
        (int32_t)(ctx_wayland->gutter_cols * ctx_cursor->cell_width *
                  ctx_wayland->zoom);
    if (gutter_right < 0) gutter_right = 0;
    if (gutter_right > (int32_t)screen_w) gutter_right = (int32_t)screen_w;
    int32_t gutter_bottom =
        (int32_t)((float)ctx_cursor->cell_height * ctx_wayland->zoom *
                  (ctx_wayland->gutter_rows - 0.5f));
    if (gutter_bottom < 0) gutter_bottom = 0;
    if (gutter_bottom > (int32_t)screen_h) gutter_bottom = (int32_t)screen_h;
    VkRect2D scissor = {
        {gutter_right, 0},
        {(uint32_t)(screen_w - gutter_right), (uint32_t)(screen_h - gutter_bottom)}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx_gl->pipeline);
    float pc_data[] = {
        (float)ctx_cursor->cell_width,
        (float)ctx_cursor->cell_height,
        -ctx_wayland->panning[0] * ctx_wayland->zoom,
        -ctx_wayland->panning[1] * ctx_wayland->zoom,
        ctx_wayland->zoom,
        0,
        screen_w,
        screen_h,
        0,
        0,
    };
    vkCmdPushConstants(cmd,
                       ctx_gl->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(pc_data),
                       pc_data);
    VkBuffer bufs[] = {ctx_gl->quad_vbo, ctx_gl->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, ctx_gl->instance_count, 0, 0);
}

static inline void war_font_init(war_font_context* font,
                                 war_vulkan_context* ctx_vk,
                                 FT_Face face,
                                 double cell_px_w,
                                 double cell_px_h) {
    font->instance_count = 2048;
    font->cmd_instance_count = 0;

    // load '*' at the display size to get display metrics
    if (FT_Load_Char(face, '*', FT_LOAD_DEFAULT))
        call_king_terry("freetype: failed to load glyph for metrics");
    float disp_w = (float)(face->glyph->metrics.width >> 6);
    float disp_h = (float)(face->glyph->metrics.height >> 6);
    float disp_advance = (float)(face->glyph->advance.x >> 6);

    font->glyph_px_w = disp_w;
    font->glyph_px_h = disp_h;
    font->glyph_advance = disp_advance;
    font->glyph_ascent = (float)(face->size->metrics.ascender >> 6);
    font->glyph_descent = (float)(-face->size->metrics.descender >> 6);

    // render atlas at moderate resolution (2-3x display size)
    // bilinear filtering at 2-3x downscale yields sharp results
    FT_Set_Pixel_Sizes(face, 0, 64);

#define ATLAS_COLS 10
    int atlas_glyph_size = 64;
    int atlas_rows = 10;
    int atlas_w = ATLAS_COLS * atlas_glyph_size;
    int atlas_h = atlas_rows * atlas_glyph_size;
    unsigned char* atlas_data = calloc(1, (size_t)atlas_w * atlas_h);

    for (int c = 32; c <= 126; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            continue;
        int col = (c - 32) % ATLAS_COLS;
        int row = (c - 32) / ATLAS_COLS;
        int dst_x = col * atlas_glyph_size;
        int dst_y = row * atlas_glyph_size;
        unsigned char* src = face->glyph->bitmap.buffer;
        int src_w = face->glyph->bitmap.width;
        int src_h = face->glyph->bitmap.rows;
        int src_pitch = face->glyph->bitmap.pitch;
        for (int y = 0; y < src_h && dst_y + y < atlas_h; y++) {
            memcpy(atlas_data + (dst_y + y) * atlas_w + dst_x,
                   src + y * src_pitch, src_w);
        }
        font->glyph_uv[c][0] = (float)dst_x / atlas_w;
        font->glyph_uv[c][1] = (float)dst_y / atlas_h;
        font->glyph_uv[c][2] = (float)(dst_x + src_w) / atlas_w;
        font->glyph_uv[c][3] = (float)(dst_y + src_h) / atlas_h;
    }
    font->glyph_uv[0][0] = font->glyph_uv['*'][0];
    font->glyph_uv[0][1] = font->glyph_uv['*'][1];
    font->glyph_uv[0][2] = font->glyph_uv['*'][2];
    font->glyph_uv[0][3] = font->glyph_uv['*'][3];

    // restore display size and load per-glyph metrics at display resolution
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)cell_px_h);
    for (int c = 32; c <= 126; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            continue;
        float gw = (float)(face->glyph->metrics.width >> 6);
        float gh = (float)(face->glyph->metrics.height >> 6);
        font->glyph_norm_width[c] = gw / (float)cell_px_w;
        font->glyph_norm_height[c] = gh / (float)cell_px_h;
        font->glyph_norm_ascent[c] = 0.5f + gh / (2.0f * (float)cell_px_h);
        font->glyph_norm_descent[c] = 0;
        font->glyph_norm_baseline[c] = 0;
    }
    font->glyph_norm_width[0] = font->glyph_norm_width['*'];
    font->glyph_norm_height[0] = font->glyph_norm_height['*'];
    font->glyph_norm_ascent[0] = font->glyph_norm_ascent['*'];
    font->glyph_norm_descent[0] = font->glyph_norm_descent['*'];
    font->glyph_norm_baseline[0] = font->glyph_norm_baseline['*'];

    // atlas image (R8 at display resolution)
    VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .extent = {(uint32_t)atlas_w, (uint32_t)atlas_h, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkResult res = vkCreateImage(ctx_vk->device, &ici, NULL, &font->atlas_image);
    WASSERT(res == VK_SUCCESS);

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(ctx_vk->device, font->atlas_image, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    res = vkAllocateMemory(ctx_vk->device, &mai, NULL, &font->atlas_memory);
    WASSERT(res == VK_SUCCESS);
    vkBindImageMemory(ctx_vk->device, font->atlas_image, font->atlas_memory, 0);

    // upload atlas data
    void* mapped;
    vkMapMemory(ctx_vk->device, font->atlas_memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    VkImageSubresource sub = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(ctx_vk->device, font->atlas_image, &sub, &layout);
    uint8_t* dst = (uint8_t*)mapped + layout.offset;
    for (int y = 0; y < atlas_h && y < (int)(layout.size / layout.rowPitch); y++) {
        memcpy(dst + y * layout.rowPitch, atlas_data + y * atlas_w, atlas_w);
    }
    vkUnmapMemory(ctx_vk->device, font->atlas_memory);
    free(atlas_data);

    // image view
    VkImageViewCreateInfo ivci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = font->atlas_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    res = vkCreateImageView(ctx_vk->device, &ivci, NULL, &font->atlas_view);
    WASSERT(res == VK_SUCCESS);

    // sampler
    VkSamplerCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
    res = vkCreateSampler(ctx_vk->device, &sci, NULL, &font->sampler);
    WASSERT(res == VK_SUCCESS);

    // descriptor set layout
    VkDescriptorSetLayoutBinding dslb = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutCreateInfo dslci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &dslb,
    };
    res = vkCreateDescriptorSetLayout(ctx_vk->device, &dslci, NULL, &font->desc_set_layout);
    WASSERT(res == VK_SUCCESS);

    // descriptor pool
    VkDescriptorPoolSize dps = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo dpci = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &dps,
    };
    res = vkCreateDescriptorPool(ctx_vk->device, &dpci, NULL, &font->desc_pool);
    WASSERT(res == VK_SUCCESS);

    // allocate descriptor set
    VkDescriptorSetAllocateInfo dsai = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = font->desc_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &font->desc_set_layout,
    };
    res = vkAllocateDescriptorSets(ctx_vk->device, &dsai, &font->desc_set);
    WASSERT(res == VK_SUCCESS);

    // update descriptor set
    VkDescriptorImageInfo dii = {
        .sampler = font->sampler,
        .imageView = font->atlas_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkWriteDescriptorSet wds = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = font->desc_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &dii,
    };
    vkUpdateDescriptorSets(ctx_vk->device, 1, &wds, 0, NULL);

    // load text vertex/fragment shaders (embedded)
    WASSERT(build_spv_war_new_vulkan_vertex_text_spv_len > 0 &&
            build_spv_war_new_vulkan_vertex_text_spv_len % 4 == 0);
    WASSERT(build_spv_war_new_vulkan_fragment_text_spv_len > 0 &&
            build_spv_war_new_vulkan_fragment_text_spv_len % 4 == 0);

    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = build_spv_war_new_vulkan_vertex_text_spv_len,
        .pCode = (uint32_t*)build_spv_war_new_vulkan_vertex_text_spv,
    };
    res = vkCreateShaderModule(ctx_vk->device, &smci, NULL, &font->vert_module);
    WASSERT(res == VK_SUCCESS);
    smci.codeSize = build_spv_war_new_vulkan_fragment_text_spv_len;
    smci.pCode = (uint32_t*)build_spv_war_new_vulkan_fragment_text_spv;
    res = vkCreateShaderModule(ctx_vk->device, &smci, NULL, &font->frag_module);
    WASSERT(res == VK_SUCCESS);

    // pipeline layout (push constants + descriptor set)
    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &font->desc_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    res = vkCreatePipelineLayout(ctx_vk->device, &plci, NULL, &font->pipeline_layout);
    WASSERT(res == VK_SUCCESS);

    // graphics pipeline
    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = font->vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = font->frag_module,
         .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(war_vulkan_text_instance), VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(war_vulkan_text_instance, pos)},
        {2, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(war_vulkan_text_instance, size)},
        {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_vulkan_text_instance, uv)},
        {4, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(war_vulkan_text_instance, glyph_scale)},
        {5, 1, VK_FORMAT_R32_SFLOAT, offsetof(war_vulkan_text_instance, baseline)},
        {6, 1, VK_FORMAT_R32_SFLOAT, offsetof(war_vulkan_text_instance, ascent)},
        {7, 1, VK_FORMAT_R32_SFLOAT, offsetof(war_vulkan_text_instance, descent)},
        {8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_vulkan_text_instance, color)},
        {12, 1, VK_FORMAT_R32_UINT, offsetof(war_vulkan_text_instance, flags)},
    };

    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 10,
        .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState cba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vis,
        .pInputAssemblyState = &ias,
        .pViewportState = &vpsi,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDynamicState = &dsi,
        .pColorBlendState = &cbs,
        .layout = font->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    res = vkCreateGraphicsPipelines(ctx_vk->device, VK_NULL_HANDLE, 1, &gpci, NULL, &font->pipeline);
    WASSERT(res == VK_SUCCESS);

    // quad VBO
    float quad_verts[] = {0,0,1,0,0,1,1,1};
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    res = vkCreateBuffer(ctx_vk->device, &bci, NULL, &font->quad_vbo);
    WASSERT(res == VK_SUCCESS);
    vkGetBufferMemoryRequirements(ctx_vk->device, font->quad_vbo, &mem_req);
    mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    res = vkAllocateMemory(ctx_vk->device, &mai, NULL, &font->quad_vbo_memory);
    WASSERT(res == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, font->quad_vbo, font->quad_vbo_memory, 0);
    vkMapMemory(ctx_vk->device, font->quad_vbo_memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, font->quad_vbo_memory);

    // instance VBO
    VkDeviceSize instance_buf_size = sizeof(war_vulkan_text_instance) * font->instance_count;
    bci.size = instance_buf_size;
    res = vkCreateBuffer(ctx_vk->device, &bci, NULL, &font->instance_vbo);
    WASSERT(res == VK_SUCCESS);
    vkGetBufferMemoryRequirements(ctx_vk->device, font->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    res = vkAllocateMemory(ctx_vk->device, &mai, NULL, &font->instance_vbo_memory);
    WASSERT(res == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, font->instance_vbo, font->instance_vbo_memory, 0);
    vkMapMemory(ctx_vk->device, font->instance_vbo_memory, 0, VK_WHOLE_SIZE, 0, &font->instance_mapped);

    // glyph_uv is now set per-char above; glyph_uv[0] = '*' fallback
}

static inline void war_font_render(VkCommandBuffer cmd,
                                   war_font_context* font,
                                   war_wayland_context* ctx_wayland,
                                   war_cursor_context* cur,
                                   float screen_w, float screen_h) {
    if (!font || !font->instance_count || !cur->instance_count) return;

    double cw = cur->cell_width, ch = cur->cell_height;
    float zoom = ctx_wayland->zoom;

    war_vulkan_text_instance inst = {0};
    inst.pos[0] = cur->instance[0].pos[0];
    inst.pos[1] = cur->instance[0].pos[1];
    inst.size[0] = 1.0f;
    inst.size[1] = 1.0f;
    inst.uv[0] = font->glyph_uv['*'][0];
    inst.uv[1] = font->glyph_uv['*'][1];
    inst.uv[2] = font->glyph_uv['*'][2];
    inst.uv[3] = font->glyph_uv['*'][3];
    inst.glyph_scale[0] = font->glyph_norm_width['*'];
    inst.glyph_scale[1] = font->glyph_norm_height['*'];
    inst.ascent = font->glyph_norm_ascent['*'];
    inst.descent = font->glyph_norm_descent['*'];
    inst.baseline = font->glyph_norm_baseline['*'];
    inst.color[0] = 1; inst.color[1] = 1; inst.color[2] = 1; inst.color[3] = 1;
    inst.flags = 0;

    memcpy(font->instance_mapped, &inst, sizeof(inst));

    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    int32_t gutter_bottom =
        (int32_t)((float)ch * ctx_wayland->zoom * (ctx_wayland->gutter_rows - 0.5f));
    if (gutter_bottom < 0) gutter_bottom = 0;
    if (gutter_bottom > (int32_t)screen_h) gutter_bottom = (int32_t)screen_h;
    VkRect2D scissor = {{0, 0}, {(uint32_t)screen_w, (uint32_t)(screen_h - gutter_bottom)}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, font->pipeline);
        float pc_data[] = {
            (float)cw, (float)ch,
            -ctx_wayland->panning[0] * zoom, -ctx_wayland->panning[1] * zoom,
            zoom, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 0,
        };
        vkCmdPushConstants(cmd, font->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                font->pipeline_layout, 0, 1, &font->desc_set, 0, NULL);
    VkBuffer bufs[] = {font->quad_vbo, font->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, 1, 0, 0);
}

static inline void war_font_render_cmd(VkCommandBuffer cmd,
                                       war_font_context* font,
                                       war_wayland_context* ctx_wayland,
                                       war_cursor_context* cur,
                                       war_env* env,
                                       float screen_w, float screen_h) {
    if (!font || !env->cmd_active || !env->cmd_len) return;
    if (!font->instance_count) return;

    double cw = cur->cell_width, ch = cur->cell_height;
    float zoom = ctx_wayland->zoom;
    uint32_t count = env->cmd_len;

    font->cmd_instance_count = count;
    war_vulkan_text_instance* inst = font->instance_mapped;
    // skip index 0 (used by cursor glyph)

    // position: middle of the 3 gutter rows, 3 cells left of gutter edge
    float gutter_row = ctx_wayland->panning[1] + 1.0f;

    for (uint32_t i = 0; i < count; i++) {
        unsigned char c = (unsigned char)env->cmd_buf[i];
        war_vulkan_text_instance* ti = &inst[1 + i];
        ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)i - 4.0f;
        ti->pos[1] = gutter_row;
        ti->pos[2] = 0;
        ti->size[0] = 1.0f;
        ti->size[1] = 1.0f;
        if (c >= 32 && c <= 126) {
            ti->uv[0] = font->glyph_uv[c][0];
            ti->uv[1] = font->glyph_uv[c][1];
            ti->uv[2] = font->glyph_uv[c][2];
            ti->uv[3] = font->glyph_uv[c][3];
            ti->glyph_scale[0] = font->glyph_norm_width[c];
            ti->glyph_scale[1] = font->glyph_norm_height[c];
            ti->ascent = font->glyph_norm_ascent[c];
            ti->descent = font->glyph_norm_descent[c];
            ti->baseline = font->glyph_norm_baseline[c];
        } else {
            ti->uv[0] = font->glyph_uv[0][0];
            ti->uv[1] = font->glyph_uv[0][1];
            ti->uv[2] = font->glyph_uv[0][2];
            ti->uv[3] = font->glyph_uv[0][3];
            ti->glyph_scale[0] = font->glyph_norm_width[0];
            ti->glyph_scale[1] = font->glyph_norm_height[0];
            ti->ascent = font->glyph_norm_ascent[0];
            ti->descent = font->glyph_norm_descent[0];
            ti->baseline = font->glyph_norm_baseline[0];
        }
        ti->color[0] = 1; ti->color[1] = 1; ti->color[2] = 1; ti->color[3] = 1;
        ti->flags = 0;
    }

    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    VkRect2D scissor = {{0, 0}, {(uint32_t)screen_w, (uint32_t)screen_h}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, font->pipeline);
    float pc_data[] = {
        (float)cw, (float)ch,
        -ctx_wayland->panning[0] * zoom, -ctx_wayland->panning[1] * zoom,
        zoom, 0, screen_w, screen_h, 0, 0,
    };
    vkCmdPushConstants(cmd, font->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            font->pipeline_layout, 0, 1, &font->desc_set, 0, NULL);
    VkBuffer bufs[] = {font->quad_vbo, font->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, count, 0, 1);

    // restore bottom scissor for subsequent passes (none follow, but be tidy)
}

static inline void war_cursor_render(VkCommandBuffer cmd,
                                     war_cursor_context* ctx_cursor,
                                     war_wayland_context* ctx_wayland,
                                     float screen_w,
                                     float screen_h) {
    if (!ctx_cursor || !ctx_cursor->instance_count) return;
    memcpy(ctx_cursor->instance_mapped,
           ctx_cursor->instance,
           sizeof(war_vulkan_cursor_instance) * ctx_cursor->instance_count);
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    int32_t gutter_bottom =
        (int32_t)((float)ctx_cursor->cell_height * ctx_wayland->zoom *
                  (ctx_wayland->gutter_rows - 0.5f));
    if (gutter_bottom < 0) gutter_bottom = 0;
    if (gutter_bottom > (int32_t)screen_h) gutter_bottom = (int32_t)screen_h;
    VkRect2D scissor = {{0, 0}, {(uint32_t)screen_w, (uint32_t)(screen_h - gutter_bottom)}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx_cursor->pipeline);
    float pc_data[] = {
        (float)ctx_cursor->cell_width,
        (float)ctx_cursor->cell_height,
        -ctx_wayland->panning[0] * ctx_wayland->zoom,
        -ctx_wayland->panning[1] * ctx_wayland->zoom,
        ctx_wayland->zoom,
        0,
        screen_w,
        screen_h,
        0,
        0,
    };
    vkCmdPushConstants(cmd,
                       ctx_cursor->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(pc_data),
                       pc_data);
    VkBuffer bufs[] = {ctx_cursor->quad_vbo, ctx_cursor->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, ctx_cursor->instance_count, 0, 0);
}

static inline void war_render_init(war_wayland_context* ctx_wayland,
                                   war_vulkan_context* ctx_vk) {
    VkAttachmentDescription attach = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_GENERAL,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };
    VkRenderPassCreateInfo rpci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attach,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };
    vkCreateRenderPass(ctx_vk->device, &rpci, NULL, &ctx_vk->render_pass);

    VkImageViewCreateInfo ivci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx_vk->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    vkCreateImageView(ctx_vk->device, &ivci, NULL, &ctx_vk->image_view);

    VkFramebufferCreateInfo fbci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ctx_vk->render_pass,
        .attachmentCount = 1,
        .pAttachments = &ctx_vk->image_view,
        .width = ctx_wayland->width,
        .height = ctx_wayland->height,
        .layers = 1,
    };
    vkCreateFramebuffer(ctx_vk->device, &fbci, NULL, &ctx_vk->framebuffer);
}

static inline void war_note_init(war_note_context* ctx_note,
                                 war_pool_context* ctx_pool,
                                 war_vulkan_context* ctx_vk) {
    ctx_note->draw =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_DRAW);
    ctx_note->x_seconds =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_X_SECONDS);
    ctx_note->y_cells =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_Y_CELLS);
    ctx_note->x_width =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_X_WIDTH);
    ctx_note->instance =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_INSTANCE);
    ctx_note->instance_count = 0;
    ctx_note->max_instances = 1024;
    ctx_note->tick_counter = 0;

    WASSERT(build_spv_war_new_vulkan_vertex_note_spv_len > 0 &&
            build_spv_war_new_vulkan_vertex_note_spv_len % 4 == 0);
    WASSERT(build_spv_war_new_vulkan_fragment_note_spv_len > 0 &&
            build_spv_war_new_vulkan_fragment_note_spv_len % 4 == 0);
    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = build_spv_war_new_vulkan_vertex_note_spv_len,
        .pCode = (uint32_t*)build_spv_war_new_vulkan_vertex_note_spv,
    };
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_note->vert_module) ==
            VK_SUCCESS);
    smci.codeSize = build_spv_war_new_vulkan_fragment_note_spv_len;
    smci.pCode = (uint32_t*)build_spv_war_new_vulkan_fragment_note_spv;
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_note->frag_module) ==
            VK_SUCCESS);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    WASSERT(vkCreatePipelineLayout(
                ctx_vk->device, &plci, NULL, &ctx_note->pipeline_layout) ==
            VK_SUCCESS);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = ctx_note->vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = ctx_note->frag_module,
         .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1,
         sizeof(war_new_vulkan_note_instance),
         VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[6] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1,
         1,
         VK_FORMAT_R32G32B32_SFLOAT,
         offsetof(war_new_vulkan_note_instance, pos)},
        {2,
         1,
         VK_FORMAT_R32G32_SFLOAT,
         offsetof(war_new_vulkan_note_instance, size)},
        {3,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_new_vulkan_note_instance, color)},
        {4,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_new_vulkan_note_instance, outline_color)},
        {7,
         1,
         VK_FORMAT_R32_UINT,
         offsetof(war_new_vulkan_note_instance, flags)},
    };
    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 6,
        .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState cba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vis,
        .pInputAssemblyState = &ias,
        .pViewportState = &vpsi,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDynamicState = &dsi,
        .pColorBlendState = &cbs,
        .layout = ctx_note->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    WASSERT(vkCreateGraphicsPipelines(ctx_vk->device,
                                      VK_NULL_HANDLE,
                                      1,
                                      &gpci,
                                      NULL,
                                      &ctx_note->pipeline) == VK_SUCCESS);

    float quad_verts[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_note->quad_vbo) ==
            VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_note->quad_vbo, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props,
                                      mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_note->quad_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(
        ctx_vk->device, ctx_note->quad_vbo, ctx_note->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(ctx_vk->device,
                ctx_note->quad_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_note->quad_vbo_memory);

    VkDeviceSize instance_buf_size =
        sizeof(war_new_vulkan_note_instance) * ctx_note->max_instances;
    bci.size = instance_buf_size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    WASSERT(
        vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_note->instance_vbo) ==
        VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_note->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props,
                             mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_note->instance_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device,
                       ctx_note->instance_vbo,
                       ctx_note->instance_vbo_memory,
                       0);
    vkMapMemory(ctx_vk->device,
                ctx_note->instance_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &ctx_note->instance_mapped);
}

static inline void war_note_render(VkCommandBuffer cmd,
                                    war_note_context* ctx_note,
                                    war_wayland_context* ctx_wayland,
                                    float screen_w,
                                    float screen_h) {
    if (!ctx_note || !ctx_note->instance_count) return;
    memcpy(ctx_note->instance_mapped,
           ctx_note->instance,
           sizeof(war_new_vulkan_note_instance) * ctx_note->instance_count);
    // hide notes on invisible layers (zero their size)
    uint16_t _lv_mask = ctx_wayland->env->layer_visible;
    if (_lv_mask != 0x1FF) {
        war_new_vulkan_note_instance* _inst = ctx_note->instance_mapped;
        for (uint32_t _i = 0; _i < ctx_note->instance_count; _i++) {
            uint32_t _li = (_inst[_i].flags >> 4) & 0xF;
            if ((_inst[_i].flags & WAR_NEW_VULKAN_FLAGS_MUTE)) continue;
            if (_li < 1 || _li > 9 || !(_lv_mask & (1 << (_li - 1))))
                _inst[_i].size[0] = 0.0f;
        }
    }
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    int32_t gutter_right =
        (int32_t)(ctx_wayland->gutter_cols * ctx_wayland->env->ctx_cursor->cell_width *
                  ctx_wayland->zoom);
    if (gutter_right < 0) gutter_right = 0;
    if (gutter_right > (int32_t)screen_w) gutter_right = (int32_t)screen_w;
    int32_t gutter_bottom =
        (int32_t)(ctx_wayland->env->ctx_cursor->cell_height * ctx_wayland->zoom *
                  (ctx_wayland->gutter_rows - 0.5f));
    if (gutter_bottom < 0) gutter_bottom = 0;
    if (gutter_bottom > (int32_t)screen_h) gutter_bottom = (int32_t)screen_h;
    VkRect2D scissor = {{gutter_right, 0}, {(uint32_t)(screen_w - gutter_right), (uint32_t)(screen_h - gutter_bottom)}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx_note->pipeline);
    float pc_data[] = {
        (float)ctx_wayland->env->ctx_cursor->cell_width,
        (float)ctx_wayland->env->ctx_cursor->cell_height,
        -ctx_wayland->panning[0] * ctx_wayland->zoom,
        -ctx_wayland->panning[1] * ctx_wayland->zoom,
        ctx_wayland->zoom,
        0,
        screen_w,
        screen_h,
        0,
        0,
    };
    vkCmdPushConstants(cmd,
                       ctx_note->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(pc_data),
                       pc_data);
    VkBuffer bufs[] = {ctx_note->quad_vbo, ctx_note->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, ctx_note->instance_count, 0, 0);
}

static inline void war_line_init(war_simple_line_context* ctx_line,
                                 war_pool_context* ctx_pool,
                                 war_vulkan_context* ctx_vk,
                                 uint32_t max_instances) {
    ctx_line->draw =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE_DRAW);
    ctx_line->x_seconds =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE_X_SECONDS);
    ctx_line->y_cells =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE_Y_CELLS);
    ctx_line->x_size =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE_WIDTH_SECONDS);
    ctx_line->y_size =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE_HEIGHT_CELLS);
    ctx_line->width = war_pool_alloc_new(
        ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE_LINE_WIDTH_SECONDS);
    ctx_line->instance =
        war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_LINE_INSTANCE);
    ctx_line->instance_count = 0;

    WASSERT(build_spv_war_new_vulkan_vertex_line_spv_len > 0 &&
            build_spv_war_new_vulkan_vertex_line_spv_len % 4 == 0);
    WASSERT(build_spv_war_new_vulkan_fragment_line_spv_len > 0 &&
            build_spv_war_new_vulkan_fragment_line_spv_len % 4 == 0);
    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = build_spv_war_new_vulkan_vertex_line_spv_len,
        .pCode = (uint32_t*)build_spv_war_new_vulkan_vertex_line_spv,
    };
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_line->vert_module) ==
            VK_SUCCESS);
    smci.codeSize = build_spv_war_new_vulkan_fragment_line_spv_len;
    smci.pCode = (uint32_t*)build_spv_war_new_vulkan_fragment_line_spv;
    WASSERT(vkCreateShaderModule(
                ctx_vk->device, &smci, NULL, &ctx_line->frag_module) ==
            VK_SUCCESS);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pc_range,
    };
    WASSERT(vkCreatePipelineLayout(
                ctx_vk->device, &plci, NULL, &ctx_line->pipeline_layout) ==
            VK_SUCCESS);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT,
         .module = ctx_line->vert_module,
         .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
         .module = ctx_line->frag_module,
         .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(war_vulkan_line_instance), VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[6] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1,
         1,
         VK_FORMAT_R32G32B32_SFLOAT,
         offsetof(war_vulkan_line_instance, pos)},
        {2,
         1,
         VK_FORMAT_R32G32_SFLOAT,
         offsetof(war_vulkan_line_instance, size)},
        {3, 1, VK_FORMAT_R32_SFLOAT, offsetof(war_vulkan_line_instance, width)},
        {4,
         1,
         VK_FORMAT_R32G32B32A32_SFLOAT,
         offsetof(war_vulkan_line_instance, color)},
        {8, 1, VK_FORMAT_R32_UINT, offsetof(war_vulkan_line_instance, flags)},
    };
    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 6,
        .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1,
    };
    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState cba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vis,
        .pInputAssemblyState = &ias,
        .pViewportState = &vpsi,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pDynamicState = &dsi,
        .pColorBlendState = &cbs,
        .layout = ctx_line->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    WASSERT(vkCreateGraphicsPipelines(ctx_vk->device,
                                      VK_NULL_HANDLE,
                                      1,
                                      &gpci,
                                      NULL,
                                      &ctx_line->pipeline) == VK_SUCCESS);

    float quad_verts[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_line->quad_vbo) ==
            VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_line->quad_vbo, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props,
                                      mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type,
    };
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_line->quad_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(
        ctx_vk->device, ctx_line->quad_vbo, ctx_line->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(ctx_vk->device,
                ctx_line->quad_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_line->quad_vbo_memory);

    VkDeviceSize instance_buf_size =
        sizeof(war_vulkan_line_instance) * max_instances;
    bci.size = instance_buf_size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    WASSERT(
        vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_line->instance_vbo) ==
        VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_line->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props,
                             mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    WASSERT(vkAllocateMemory(
                ctx_vk->device, &mai, NULL, &ctx_line->instance_vbo_memory) ==
            VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device,
                       ctx_line->instance_vbo,
                       ctx_line->instance_vbo_memory,
                       0);
    vkMapMemory(ctx_vk->device,
                ctx_line->instance_vbo_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &ctx_line->instance_mapped);
}

static inline void war_line_render(VkCommandBuffer cmd,
                                   war_simple_line_context* ctx_line,
                                   war_wayland_context* ctx_wayland,
                                   float screen_w,
                                   float screen_h) {
    if (!ctx_line || !ctx_line->instance_count) return;
    memcpy(ctx_line->instance_mapped,
           ctx_line->instance,
           sizeof(war_vulkan_line_instance) * ctx_line->instance_count);
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    int32_t gutter_right =
        (int32_t)(ctx_wayland->gutter_cols *
                  ctx_wayland->env->ctx_cursor->cell_width * ctx_wayland->zoom);
    if (gutter_right < 0) gutter_right = 0;
    if (gutter_right > (int32_t)screen_w) gutter_right = (int32_t)screen_w;
    int32_t gutter_bottom =
        (int32_t)(ctx_wayland->env->ctx_cursor->cell_height * ctx_wayland->zoom *
                  (ctx_wayland->gutter_rows - 0.5f));
    if (gutter_bottom < 0) gutter_bottom = 0;
    if (gutter_bottom > (int32_t)screen_h) gutter_bottom = (int32_t)screen_h;
    VkRect2D scissor = {
        {gutter_right, 0},
        {(uint32_t)(screen_w - gutter_right), (uint32_t)(screen_h - gutter_bottom)}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx_line->pipeline);
    float pc_data[] = {
        (float)ctx_wayland->env->ctx_cursor->cell_width,
        (float)ctx_wayland->env->ctx_cursor->cell_height,
        -ctx_wayland->panning[0] * ctx_wayland->zoom,
        -ctx_wayland->panning[1] * ctx_wayland->zoom,
        ctx_wayland->zoom,
        0,
        screen_w,
        screen_h,
        0,
        0,
    };
    vkCmdPushConstants(cmd,
                       ctx_line->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(pc_data),
                       pc_data);
    VkBuffer bufs[] = {ctx_line->quad_vbo, ctx_line->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, ctx_line->instance_count, 0, 0);
}

static inline void war_render_frame(war_wayland_context* ctx_wayland,
                                    war_vulkan_context* ctx_vk, war_color_context* ctx_color) {
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx_vk->device, &ctx_vk->cbai, &cmd);
    VkCommandBufferBeginInfo cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &cbbi);
    VkClearValue clear = {.color = {{
        ((ctx_color->background >> 24) & 0xFF) / 255.0f,
        ((ctx_color->background >> 16) & 0xFF) / 255.0f,
        ((ctx_color->background >> 8) & 0xFF) / 255.0f,
        (ctx_color->background & 0xFF) / 255.0f
    }}};
    VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx_vk->render_pass,
        .framebuffer = ctx_vk->framebuffer,
        .renderArea = {{0, 0}, {ctx_wayland->width, ctx_wayland->height}},
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    war_piano_gutter_render(cmd,
                            ctx_wayland->env->ctx_piano_gutter,
                            ctx_wayland,
                            ctx_wayland->env->ctx_cursor,
                            (float)ctx_wayland->width,
                            (float)ctx_wayland->height);
    if (ctx_wayland->env->ctx_gridlines) {
        war_cursor_context* cur = ctx_wayland->env->ctx_cursor;
        double vis_cols =
            (double)ctx_wayland->width / (cur->cell_width * ctx_wayland->zoom) -
            ctx_wayland->gutter_cols;
        if (vis_cols < 1) vis_cols = 1;
        double start_col = ctx_wayland->panning[0];
        double end_col = start_col + vis_cols + ctx_wayland->gutter_cols + 1;
        if (end_col > ctx_wayland->right_bound)
            end_col = ctx_wayland->right_bound;
        war_gridlines_generate(
            ctx_wayland->env->ctx_gridlines,
            start_col,
            end_col,
            128 + ctx_wayland->gutter_rows,
            ctx_wayland->env->ctx_config->HUD_GRIDLINES_INSTANCE_MAX,
            ctx_wayland->gutter_rows);
        war_gridlines_render(cmd,
                             ctx_wayland->env->ctx_gridlines,
                             ctx_wayland,
                             cur,
                             (float)ctx_wayland->width,
                             (float)ctx_wayland->height);
    }
    if (ctx_wayland->env->ctx_note)
        war_note_render(cmd,
                        ctx_wayland->env->ctx_note,
                        ctx_wayland,
                        (float)ctx_wayland->width,
                        (float)ctx_wayland->height);
    // mute note layer labels (behind cursor, scissored to avoid gutter)
    if (ctx_wayland->env->ctx_font && ctx_wayland->env->ctx_note) {
        war_font_context* font = ctx_wayland->env->ctx_font;
        uint32_t _mnc = ctx_wayland->env->ctx_note->instance_count;
        float _cw = (float)ctx_wayland->env->ctx_cursor->cell_width;
        float _ch = (float)ctx_wayland->env->ctx_cursor->cell_height;
        float _zoom = ctx_wayland->zoom;
        VkViewport _mvp = {0, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 1};
        int32_t _gr = (int32_t)(ctx_wayland->gutter_cols * _cw * _zoom);
        if (_gr < 0) _gr = 0;
        if (_gr > (int32_t)ctx_wayland->width) _gr = (int32_t)ctx_wayland->width;
        int32_t _gb = (int32_t)(_ch * _zoom * (ctx_wayland->gutter_rows - 0.5f));
        if (_gb < 0) _gb = 0;
        if (_gb > (int32_t)ctx_wayland->height) _gb = (int32_t)ctx_wayland->height;
        VkRect2D _msc = {{_gr, 0}, {(uint32_t)(ctx_wayland->width - _gr), (uint32_t)(ctx_wayland->height - _gb)}};
        vkCmdSetViewport(cmd, 0, 1, &_mvp);
        vkCmdSetScissor(cmd, 0, 1, &_msc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, font->pipeline);
        float _mpc[] = {_cw, _ch, -ctx_wayland->panning[0] * _zoom, -ctx_wayland->panning[1] * _zoom, _zoom, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 0};
        vkCmdPushConstants(cmd, font->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(_mpc), _mpc);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, font->pipeline_layout, 0, 1, &font->desc_set, 0, NULL);
        VkBuffer _mbufs[] = {font->quad_vbo, font->instance_vbo};
        VkDeviceSize _moffs[] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, _mbufs, _moffs);
        war_vulkan_text_instance* _mdst = (war_vulkan_text_instance*)font->instance_mapped;
#define MUTE_LABEL_OFFSET 420
        uint32_t _mli = 0;
        for (uint32_t _mi = 0; _mi < _mnc; _mi++) {
            war_new_vulkan_note_instance* _mn = &ctx_wayland->env->ctx_note->instance[_mi];
            if (!(_mn->flags & WAR_NEW_VULKAN_FLAGS_MUTE)) continue;
            uint16_t _mm = (_mn->flags >> 8) & 0x1FF;
            char _mlb[16];
            int _mln = 0;
            for (int _b = 0; _b < 9; _b++)
                if (_mm & (1 << _b))
                    _mlb[_mln++] = (char)('1' + _b);
            _mlb[_mln] = '\0';
            if (_mln == 0) continue;
            int _maxc = (int)((_mn->size[0] - 0.05f) / 0.7f);
            if (_maxc < 1) _maxc = 1;
            if (_mln > _maxc) _mln = _maxc;
            float _ml_x = _mn->pos[0] + 0.05f;
            float _ml_y = _mn->pos[1] - 0.20f;
            for (int _j = 0; _j < _mln && MUTE_LABEL_OFFSET + _mli + _j < 512; _j++) {
                unsigned char _c = (unsigned char)_mlb[_j];
                war_vulkan_text_instance* _ti = &_mdst[MUTE_LABEL_OFFSET + _mli + _j];
                _ti->pos[0] = _ml_x + (float)_j * 0.7f;
                _ti->pos[1] = _ml_y;
                _ti->pos[2] = 0;
                _ti->size[0] = 0.7f;
                _ti->size[1] = 0.7f;
                _ti->uv[0] = font->glyph_uv[_c][0];
                _ti->uv[1] = font->glyph_uv[_c][1];
                _ti->uv[2] = font->glyph_uv[_c][2];
                _ti->uv[3] = font->glyph_uv[_c][3];
                _ti->glyph_scale[0] = font->glyph_norm_width[_c];
                _ti->glyph_scale[1] = font->glyph_norm_height[_c];
                _ti->ascent = font->glyph_norm_ascent[_c];
                _ti->descent = font->glyph_norm_descent[_c];
                _ti->baseline = font->glyph_norm_baseline[_c];
                _ti->color[0] = 0.0f; _ti->color[1] = 0.0f; _ti->color[2] = 0.0f; _ti->color[3] = 1.0f;
                _ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)_mln, 0, MUTE_LABEL_OFFSET + _mli);
            _mli += _mln;
        }
#undef MUTE_LABEL_OFFSET
    }
    if (ctx_wayland->env->ctx_line)
        war_line_render(cmd,
                        ctx_wayland->env->ctx_line,
                        ctx_wayland,
                        (float)ctx_wayland->width,
                        (float)ctx_wayland->height);
    if (ctx_wayland->env->ctx_cursor->visual_active) {
        // override cursor color for visual mode before the render copies it
        ctx_wayland->env->ctx_cursor->instance[0].color[0] = 1.0f;
        ctx_wayland->env->ctx_cursor->instance[0].color[1] = 1.0f;
        ctx_wayland->env->ctx_cursor->instance[0].color[2] = 0.3f;
        ctx_wayland->env->ctx_cursor->instance[0].color[3] = 0.6f;
    }
    // device selector HUD overlay (over full screen, panning-independent)
    if (ctx_wayland->env->dev_sel_active && ctx_wayland->env->ctx_font) {
        war_font_context* _df = ctx_wayland->env->ctx_font;
        double _dcw = ctx_wayland->env->ctx_cursor->cell_width;
        double _dch = ctx_wayland->env->ctx_cursor->cell_height;
        float _dz = ctx_wayland->zoom;
        float _dsw = (float)ctx_wayland->width;
        float _dsh = (float)ctx_wayland->height;
        VkViewport _dvp = {0, 0, _dsw, _dsh, 0, 1};
        VkRect2D _dsc = {{0, 0}, {(uint32_t)_dsw, (uint32_t)_dsh}};
        vkCmdSetViewport(cmd, 0, 1, &_dvp);
        vkCmdSetScissor(cmd, 0, 1, &_dsc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _df->pipeline);
        float _dpc[] = {(float)_dcw, (float)_dch,
                        -ctx_wayland->panning[0] * _dz, 0,
                        _dz, 0, _dsw, _dsh, 0, 0};
        vkCmdPushConstants(cmd, _df->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(_dpc), _dpc);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                _df->pipeline_layout, 0, 1, &_df->desc_set, 0, NULL);
        VkBuffer _dbufs[] = {_df->quad_vbo, _df->instance_vbo};
        VkDeviceSize _doffs[] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, _dbufs, _doffs);
        war_vulkan_text_instance* _ddst = (war_vulkan_text_instance*)_df->instance_mapped;
        const char* _dcur = ctx_wayland->env->dev_nodes[ctx_wayland->env->capture_mode - 1];
        uint32_t _dwritten = 0;
        uint32_t _dmaxrow = 15;
        for (uint32_t _dr = ctx_wayland->env->dev_sel_offset;
             _dr < ctx_wayland->env->dev_count && _dr < ctx_wayland->env->dev_sel_offset + _dmaxrow;
             _dr++) {
            char _dline[256];
            int _dp = 0;
            if ((int32_t)_dr == ctx_wayland->env->dev_sel_cursor)
                _dline[_dp++] = '>';
            if (_dcur && strcmp(ctx_wayland->env->dev_names[_dr], _dcur) == 0)
                _dline[_dp++] = '*';
            int _dn = snprintf(_dline + _dp, sizeof(_dline) - _dp, "%s", ctx_wayland->env->dev_names[_dr]);
            if (_dn < 0) _dn = 0;
            if (_dp + _dn > (int)sizeof(_dline) - 1) _dn = (int)sizeof(_dline) - 1 - _dp;
            _dn += _dp;
            if (_dn > 60) _dn = 60;
            for (int _di = 0; _di < _dn; _di++) {
                unsigned char _dc = (unsigned char)_dline[_di];
                if (_dc < 32) _dc = ' ';
                war_vulkan_text_instance* _dti = &_ddst[_dwritten + _di];
                _dti->pos[0] = (float)ctx_wayland->gutter_cols + 2.0f + (float)_di;
                _dti->pos[1] = (float)ctx_wayland->gutter_rows + (float)(_dr - ctx_wayland->env->dev_sel_offset);
                _dti->pos[2] = 0;
                _dti->size[0] = 1.0f;
                _dti->size[1] = 1.0f;
                _dti->uv[0] = _df->glyph_uv[_dc][0];
                _dti->uv[1] = _df->glyph_uv[_dc][1];
                _dti->uv[2] = _df->glyph_uv[_dc][2];
                _dti->uv[3] = _df->glyph_uv[_dc][3];
                _dti->glyph_scale[0] = _df->glyph_norm_width[_dc];
                _dti->glyph_scale[1] = _df->glyph_norm_height[_dc];
                _dti->ascent = _df->glyph_norm_ascent[_dc];
                _dti->descent = _df->glyph_norm_descent[_dc];
                _dti->baseline = _df->glyph_norm_baseline[_dc];
                _dti->color[0] = 1.0f; _dti->color[1] = 1.0f; _dti->color[2] = 1.0f; _dti->color[3] = 1.0f;
                _dti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)_dn, 0, _dwritten);
            _dwritten += _dn;
        }
    }
    war_cursor_render(cmd,
                      ctx_wayland->env->ctx_cursor,
                      ctx_wayland,
                      (float)ctx_wayland->width,
                      (float)ctx_wayland->height);
    // --- 4 status bar backgrounds at bottom gutter rows ---
    {
        war_cursor_context* cur = ctx_wayland->env->ctx_cursor;
        double cw = cur->cell_width;
        double ch = cur->cell_height;
        float zoom = ctx_wayland->zoom;
        uint32_t bar_colors[4] = {
            ctx_color->extra_status_bar,
            ctx_color->top_status_bar,
            ctx_color->middle_status_bar,
            ctx_color->bottom_status_bar,
        };
        war_vulkan_cursor_instance status[4] = {0};
        for (int i = 0; i < 4; i++) {
            // Y is inverted: 0 = bottom of screen, gutter_rows-1 = first row above gutter
            status[i].pos[0] = ctx_wayland->panning[0];
            status[i].pos[1] = ctx_wayland->panning[1] + ctx_wayland->gutter_rows - 1 - i;
            status[i].size[0] = (float)ctx_wayland->width / (float)(cw * zoom);
            status[i].size[1] = 1.0f;
            status[i].color[0] = (float)((bar_colors[i] >> 24) & 0xFF) / 255.0f;
            status[i].color[1] = (float)((bar_colors[i] >> 16) & 0xFF) / 255.0f;
            status[i].color[2] = (float)((bar_colors[i] >> 8) & 0xFF) / 255.0f;
            status[i].color[3] = (float)(bar_colors[i] & 0xFF) / 255.0f;
            status[i].flags = 0;
        }
        memcpy((char*)cur->instance_mapped +
                   sizeof(war_vulkan_cursor_instance) * cur->instance_count,
               status, sizeof(status));
        VkViewport svp = {0, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 1};
        VkRect2D sscissor = {{0, 0}, {ctx_wayland->width, ctx_wayland->height}};
        vkCmdSetViewport(cmd, 0, 1, &svp);
        vkCmdSetScissor(cmd, 0, 1, &sscissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cur->pipeline);
        float pc_data[] = {
            (float)cw, (float)ch,
            -ctx_wayland->panning[0] * zoom, -ctx_wayland->panning[1] * zoom,
            zoom, 0,
            (float)ctx_wayland->width, (float)ctx_wayland->height,
            0, 0,
        };
        vkCmdPushConstants(cmd, cur->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
        VkBuffer s_bufs[] = {cur->quad_vbo, cur->instance_vbo};
        VkDeviceSize s_offsets[] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, s_bufs, s_offsets);
        vkCmdDraw(cmd, 4, 4, 0, cur->instance_count);
    }
    // --- visual mode selection highlight ---
    if (ctx_wayland->env->ctx_cursor->visual_active) {
        war_cursor_context* vcur = ctx_wayland->env->ctx_cursor;
        double vcw = vcur->cell_width;
        double vch = vcur->cell_height;
        float vzoom = ctx_wayland->zoom;
        float vax = vcur->visual_anchor_col;
        float vay = vcur->visual_anchor_row;
        float vcx = vcur->instance[0].pos[0];
        float vcy = vcur->instance[0].pos[1];
        {
            war_vulkan_cursor_instance sel = {0};
            if (vcx > vax) { sel.pos[0] = vax; sel.size[0] = vcx - vax + 1.0f; }
            else           { sel.pos[0] = vcx; sel.size[0] = vax - vcx + 1.0f; }
            if (vcy > vay) { sel.pos[1] = vay; sel.size[1] = vcy - vay + 1.0f; }
            else           { sel.pos[1] = vcy; sel.size[1] = vay - vcy + 1.0f; }
            sel.pos[2] = 0.0f;
            sel.color[0] = 0.3f; sel.color[1] = 0.3f; sel.color[2] = 1.0f; sel.color[3] = 0.4f;
            sel.flags = 0;
            // write to cursor instance buffer (past cursor + 4 status bars)
            memcpy((char*)vcur->instance_mapped +
                       sizeof(war_vulkan_cursor_instance) * (vcur->instance_count + 4),
                   &sel, sizeof(sel));
            VkViewport svp = {0, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 1};
            int32_t sgutter = (int32_t)(vch * vzoom * (ctx_wayland->gutter_rows - 0.5f));
            if (sgutter < 0) sgutter = 0;
            if (sgutter > (int32_t)ctx_wayland->height) sgutter = (int32_t)ctx_wayland->height;
            VkRect2D sscissor = {{0, 0}, {(uint32_t)ctx_wayland->width, (uint32_t)(ctx_wayland->height - sgutter)}};
            vkCmdSetViewport(cmd, 0, 1, &svp);
            vkCmdSetScissor(cmd, 0, 1, &sscissor);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vcur->pipeline);
            float pc_data[] = {
                (float)vcw, (float)vch,
                -ctx_wayland->panning[0] * vzoom, -ctx_wayland->panning[1] * vzoom,
                vzoom, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 0,
            };
            vkCmdPushConstants(cmd, vcur->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
            VkBuffer bufs[] = {vcur->quad_vbo, vcur->instance_vbo};
            VkDeviceSize offsets[] = {0, 0};
            vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
            vkCmdDraw(cmd, 4, 1, 0, vcur->instance_count + 4);
        }
    }
    // --- waveform viewer ---
    if (ctx_wayland->env->wave_view_active) {
        war_cursor_context* wcur = ctx_wayland->env->ctx_cursor;
        double wcw = wcur->cell_width, wch = wcur->cell_height;
        float wz = ctx_wayland->zoom;
        uint32_t wp = ctx_wayland->env->wave_view_pitch;
        uint32_t wl = ctx_wayland->env->wave_view_layer;
        uint32_t widx = wp * WAR_CAPTURE_SLOT_LAYERS + (wl - 1);
        float* ws = ctx_wayland->env->capture_slots[widx].samples;
        uint64_t wcnt = ctx_wayland->env->capture_slots[widx].count;
        if (ws && wcnt >= 4) {
            double wrows = (double)ctx_wayland->height / (wch * wz) - ctx_wayland->gutter_rows;
            if (wrows < 2) wrows = 2;
            uint64_t wframes = wcnt / 2;
            float nw = ctx_wayland->env->wave_view_note_width;
            if (nw < 0.5f) nw = 1.0f;
            // 4 bars per cell so waveform spans exactly the note width
            #define WAVE_MAX_BARS 1000
            uint32_t wbars = (uint32_t)(nw * 4.0f);
            if (wbars < 1) wbars = 1;
            if (wbars > WAVE_MAX_BARS) wbars = WAVE_MAX_BARS;
            uint64_t wstep = wframes / wbars;
            if (wstep < 1) wstep = 1;
            double wcy = (double)ctx_wayland->env->wave_view_pitch + (double)ctx_wayland->gutter_rows;
            double was = wrows * 0.45;
            double wx0 = (double)ctx_wayland->gutter_cols;
            uint32_t wcount = 0;
            war_vulkan_cursor_instance wquads[WAVE_MAX_BARS];
            for (uint32_t b = 0; b < wbars && wcount + 2 < WAVE_MAX_BARS; b++) {
                uint64_t fs = b * wstep;
                uint64_t fe = fs + wstep;
                if (fe > wframes) fe = wframes;
                float pp = 0.0f, pn = 0.0f;
                for (uint64_t f = fs; f < fe; f++) {
                    float m = (ws[f*2] + ws[f*2+1]) * 0.5f;
                    if (m > pp) pp = m;
                    if (m < pn) pn = m;
                }
                double bx = wx0 + (double)b / 4.0;
                if (pp > 0.0001f) {
                    float bh = pp * (float)was;
                    if (bh < 0.3f) bh = 0.3f;
                    wquads[wcount].pos[0] = (float)bx;
                    wquads[wcount].pos[1] = (float)wcy;
                    wquads[wcount].size[0] = 0.15f;
                    wquads[wcount].size[1] = bh;
                    wquads[wcount].color[0] = 0.2f; wquads[wcount].color[1] = 0.9f; wquads[wcount].color[2] = 0.3f; wquads[wcount].color[3] = 1.0f;
                    wquads[wcount].flags = 0;
                    wcount++;
                }
                if (pn < -0.0001f) {
                    float bh = -pn * (float)was;
                    if (bh < 0.3f) bh = 0.3f;
                    wquads[wcount].pos[0] = (float)bx;
                    wquads[wcount].pos[1] = (float)(wcy - bh);
                    wquads[wcount].size[0] = 0.15f;
                    wquads[wcount].size[1] = bh;
                    wquads[wcount].color[0] = 0.2f; wquads[wcount].color[1] = 0.9f; wquads[wcount].color[2] = 0.3f; wquads[wcount].color[3] = 1.0f;
                    wquads[wcount].flags = 0;
                    wcount++;
                }
            }
            if (wcount > 0) {
                memcpy((char*)wcur->instance_mapped +
                           sizeof(war_vulkan_cursor_instance) * (wcur->instance_count + 4),
                       wquads, sizeof(war_vulkan_cursor_instance) * wcount);
                VkViewport wvp = {0, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 1};
                int32_t wgb = (int32_t)(wch * wz * (ctx_wayland->gutter_rows - 0.5f));
                if (wgb < 0) wgb = 0;
                if (wgb > (int32_t)ctx_wayland->height) wgb = (int32_t)ctx_wayland->height;
                VkRect2D wsc = {{0, 0}, {(uint32_t)ctx_wayland->width, (uint32_t)(ctx_wayland->height - wgb)}};
                vkCmdSetViewport(cmd, 0, 1, &wvp);
                vkCmdSetScissor(cmd, 0, 1, &wsc);
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, wcur->pipeline);
                float wpc[] = {(float)wcw, (float)wch, -ctx_wayland->panning[0]*wz, -ctx_wayland->panning[1]*wz, wz, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 0};
                vkCmdPushConstants(cmd, wcur->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(wpc), wpc);
                VkBuffer wbuf[] = {wcur->quad_vbo, wcur->instance_vbo};
                VkDeviceSize woffs[] = {0, 0};
                vkCmdBindVertexBuffers(cmd, 0, 2, wbuf, woffs);
                vkCmdDraw(cmd, 4, wcount, 0, wcur->instance_count + 4);
            }
            #undef WAVE_MAX_BARS
        }
    }
    if (ctx_wayland->env->ctx_font) {
        war_font_render_cmd(cmd,
                            ctx_wayland->env->ctx_font,
                            ctx_wayland,
                            ctx_wayland->env->ctx_cursor,
                            ctx_wayland->env,
                            (float)ctx_wayland->width,
                            (float)ctx_wayland->height);
        // gutter label: cursor row, col
        war_font_context* font = ctx_wayland->env->ctx_font;
        double cw = ctx_wayland->env->ctx_cursor->cell_width;
        double ch = ctx_wayland->env->ctx_cursor->cell_height;
        float zoom = ctx_wayland->zoom;
        char label[32];
        int n = snprintf(label, sizeof(label), "%.0f, %.0f",
                         ctx_wayland->env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows,
                         ctx_wayland->env->ctx_cursor->instance[0].pos[0] - (double)(ctx_wayland->gutter_cols - 1));
        if (n < 0 || n > (int)sizeof(label)) n = 0;
        // top status bar, panning-independent
        float label_row = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 2.0f;
#define LABEL_OFFSET 256
        war_vulkan_text_instance* dst = font->instance_mapped;
        for (int i = 0; i < n; i++) {
            unsigned char c = (unsigned char)label[i];
            war_vulkan_text_instance* ti = &dst[LABEL_OFFSET + i];
            ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)i;
            ti->pos[1] = label_row;
            ti->pos[2] = 0;
            ti->size[0] = 1.0f;
            ti->size[1] = 1.0f;
            ti->uv[0] = font->glyph_uv[c][0];
            ti->uv[1] = font->glyph_uv[c][1];
            ti->uv[2] = font->glyph_uv[c][2];
            ti->uv[3] = font->glyph_uv[c][3];
            ti->glyph_scale[0] = font->glyph_norm_width[c];
            ti->glyph_scale[1] = font->glyph_norm_height[c];
            ti->ascent = font->glyph_norm_ascent[c];
            ti->descent = font->glyph_norm_descent[c];
            ti->baseline = font->glyph_norm_baseline[c];
            ti->color[0] = 1; ti->color[1] = 1; ti->color[2] = 1; ti->color[3] = 1;
            ti->flags = 0;
        }
        VkViewport vp = {0, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 1};
        VkRect2D scissor = {{0, 0}, {ctx_wayland->width, ctx_wayland->height}};
        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, font->pipeline);
        float pc_data[] = {
            (float)cw, (float)ch,
            -ctx_wayland->panning[0] * zoom, -ctx_wayland->panning[1] * zoom,
            zoom, 0, (float)ctx_wayland->width, (float)ctx_wayland->height, 0, 0,
        };
        vkCmdPushConstants(cmd, font->pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                font->pipeline_layout, 0, 1, &font->desc_set, 0, NULL);
        VkBuffer bufs[] = {font->quad_vbo, font->instance_vbo};
        VkDeviceSize offsets[] = {0, 0};
        vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
        vkCmdDraw(cmd, 4, (uint32_t)n, 0, LABEL_OFFSET);
#undef LABEL_OFFSET
        // layer visibility label on top status bar
        {
            uint16_t _lv = ctx_wayland->env->layer_visible;
            char _lvb[16];
            int _lvn = 0;
            for (int _lvi = 1; _lvi <= 9; _lvi++)
                if (_lv & (1 << (_lvi - 1)))
                    _lvb[_lvn++] = (char)('0' + _lvi);
            _lvb[_lvn] = '\0';
            if (_lvn > 0) {
                float _lvr = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 2.0f;
#define LV_OFFSET 410
                for (int _lvi2 = 0; _lvi2 < _lvn; _lvi2++) {
                    unsigned char _lvc = (unsigned char)_lvb[_lvi2];
                    war_vulkan_text_instance* _lti = &dst[LV_OFFSET + _lvi2];
                    _lti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 20 + _lvi2);
                    _lti->pos[1] = _lvr;
                    _lti->pos[2] = 0;
                    _lti->size[0] = 1.0f;
                    _lti->size[1] = 1.0f;
                    _lti->uv[0] = font->glyph_uv[_lvc][0];
                    _lti->uv[1] = font->glyph_uv[_lvc][1];
                    _lti->uv[2] = font->glyph_uv[_lvc][2];
                    _lti->uv[3] = font->glyph_uv[_lvc][3];
                    _lti->glyph_scale[0] = font->glyph_norm_width[_lvc];
                    _lti->glyph_scale[1] = font->glyph_norm_height[_lvc];
                    _lti->ascent = font->glyph_norm_ascent[_lvc];
                    _lti->descent = font->glyph_norm_descent[_lvc];
                    _lti->baseline = font->glyph_norm_baseline[_lvc];
                    _lti->color[0] = 0.6f; _lti->color[1] = 0.8f; _lti->color[2] = 1.0f; _lti->color[3] = 1.0f;
                    _lti->flags = 0;
                }
                vkCmdDraw(cmd, 4, (uint32_t)_lvn, 0, LV_OFFSET);
#undef LV_OFFSET
            }
        }
        // master gain label on extra status bar (topmost)
        {
            float _mg = ctx_wayland->env->master_gain;
            char _mt[16];
            int _mn = snprintf(_mt, sizeof(_mt), "MG%+.0f", _mg);
            if (_mn > 0) {
                    float _mrow = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 1.0f;
#define MG_OFFSET 276
                    for (int _mi2 = 0; _mi2 < _mn; _mi2++) {
                        unsigned char _mc = (unsigned char)_mt[_mi2];
                        war_vulkan_text_instance* _ti = &dst[MG_OFFSET + _mi2];
                        _ti->pos[0] = ctx_wayland->panning[0] + (float)(25 + _mi2);
                        _ti->pos[1] = _mrow;
                        _ti->pos[2] = 0;
                        _ti->size[0] = 1.0f;
                        _ti->size[1] = 1.0f;
                        _ti->uv[0] = font->glyph_uv[_mc][0];
                        _ti->uv[1] = font->glyph_uv[_mc][1];
                        _ti->uv[2] = font->glyph_uv[_mc][2];
                        _ti->uv[3] = font->glyph_uv[_mc][3];
                        _ti->glyph_scale[0] = font->glyph_norm_width[_mc];
                        _ti->glyph_scale[1] = font->glyph_norm_height[_mc];
                        _ti->ascent = font->glyph_norm_ascent[_mc];
                        _ti->descent = font->glyph_norm_descent[_mc];
                        _ti->baseline = font->glyph_norm_baseline[_mc];
                        _ti->color[0] = 1.0f; _ti->color[1] = 0.8f; _ti->color[2] = 0.2f; _ti->color[3] = 1.0f;
                        _ti->flags = 0;
                    }
                    vkCmdDraw(cmd, 4, (uint32_t)_mn, 0, MG_OFFSET);
#undef MG_OFFSET
            }
        }
        // ADSR label on extra status bar (attack/sustain/release of current slot)
        {
            war_cursor_context* _ac2 = ctx_wayland->env->ctx_cursor;
            if (_ac2 && _ac2->instance_count) {
                double _agr = _ac2->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
                uint32_t _agp = _agr > 0 ? (uint32_t)(_agr + 0.5) : 0;
                if (_agp > 127) _agp = 127;
                uint32_t _agl2 = _ac2->layer;
                if (_agl2 < 1 || _agl2 > 9) _agl2 = 1;
                uint32_t _agi = _agp * WAR_CAPTURE_SLOT_LAYERS + (_agl2 - 1);
                war_capture_slot* _cslot = &ctx_wayland->env->capture_slots[_agi];
                float _aatk2 = _cslot->attack;
                float _asus2 = _cslot->sustain;
                float _arel2 = _cslot->release;
                char _abuf2[32];
                int _abn2 = snprintf(_abuf2, sizeof(_abuf2), "A%+-6.0f S%+-6.0f R%+-6.0f", _aatk2, _asus2, _arel2);
                if (_abn2 > 0) {
                    float _arow2 = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 1.0f;
#define ADSR_OFFSET 304
                    for (int _abi2 = 0; _abi2 < _abn2; _abi2++) {
                        unsigned char _abc2 = (unsigned char)_abuf2[_abi2];
                        war_vulkan_text_instance* _bti2 = &dst[ADSR_OFFSET + _abi2];
                        _bti2->pos[0] = ctx_wayland->panning[0] + (float)_abi2;
                        _bti2->pos[1] = _arow2;
                        _bti2->pos[2] = 0;
                        _bti2->size[0] = 1.0f;
                        _bti2->size[1] = 1.0f;
                        _bti2->uv[0] = font->glyph_uv[_abc2][0];
                        _bti2->uv[1] = font->glyph_uv[_abc2][1];
                        _bti2->uv[2] = font->glyph_uv[_abc2][2];
                        _bti2->uv[3] = font->glyph_uv[_abc2][3];
                        _bti2->glyph_scale[0] = font->glyph_norm_width[_abc2];
                        _bti2->glyph_scale[1] = font->glyph_norm_height[_abc2];
                        _bti2->ascent = font->glyph_norm_ascent[_abc2];
                        _bti2->descent = font->glyph_norm_descent[_abc2];
                        _bti2->baseline = font->glyph_norm_baseline[_abc2];
                        _bti2->color[0] = 1.0f; _bti2->color[1] = 0.6f; _bti2->color[2] = 0.2f; _bti2->color[3] = 1.0f;
                        _bti2->flags = 0;
                    }
                    vkCmdDraw(cmd, 4, (uint32_t)_abn2, 0, ADSR_OFFSET);
#undef ADSR_OFFSET
                }
            }
        }
        // gain label on top status bar
        {
            double _gr = ctx_wayland->env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
            uint32_t _gp = _gr > 0 ? (uint32_t)(_gr + 0.5) : 0;
            if (_gp > 127) _gp = 127;
            uint32_t _gl = ctx_wayland->env->ctx_cursor->layer;
            if (_gl < 1 || _gl > 9) _gl = 1;
            uint32_t _gi = _gp * WAR_CAPTURE_SLOT_LAYERS + (_gl - 1);
            war_capture_slot* _gs = &ctx_wayland->env->capture_slots[_gi];
            {
                char _gt[16];
                int _gn = snprintf(_gt, sizeof(_gt), "G%+.0f", _gs->gain);
                if (_gn > 0) {
                    float _grow = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 4.0f;
#define GAIN_OFFSET 284
                    for (int _gi2 = 0; _gi2 < _gn; _gi2++) {
                        unsigned char _gc = (unsigned char)_gt[_gi2];
                        war_vulkan_text_instance* _ti = &dst[GAIN_OFFSET + _gi2];
                        _ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 1 + _gi2);
                        _ti->pos[1] = _grow;
                        _ti->pos[2] = 0;
                        _ti->size[0] = 1.0f;
                        _ti->size[1] = 1.0f;
                        _ti->uv[0] = font->glyph_uv[_gc][0];
                        _ti->uv[1] = font->glyph_uv[_gc][1];
                        _ti->uv[2] = font->glyph_uv[_gc][2];
                        _ti->uv[3] = font->glyph_uv[_gc][3];
                        _ti->glyph_scale[0] = font->glyph_norm_width[_gc];
                        _ti->glyph_scale[1] = font->glyph_norm_height[_gc];
                        _ti->ascent = font->glyph_norm_ascent[_gc];
                        _ti->descent = font->glyph_norm_descent[_gc];
                        _ti->baseline = font->glyph_norm_baseline[_gc];
                        _ti->color[0] = 0.2f; _ti->color[1] = 1.0f; _ti->color[2] = 0.6f; _ti->color[3] = 1.0f;
                        _ti->flags = 0;
                    }
                    vkCmdDraw(cmd, 4, (uint32_t)_gn, 0, GAIN_OFFSET);
#undef GAIN_OFFSET
                }
            }
        }
        // pan label on bottom status bar
        {
            double _pr2 = ctx_wayland->env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
            uint32_t _pp2 = _pr2 > 0 ? (uint32_t)(_pr2 + 0.5) : 0;
            if (_pp2 > 127) _pp2 = 127;
            uint32_t _pl2 = ctx_wayland->env->ctx_cursor->layer;
            if (_pl2 < 1 || _pl2 > 9) _pl2 = 1;
            uint32_t _pi2 = _pp2 * WAR_CAPTURE_SLOT_LAYERS + (_pl2 - 1);
            war_capture_slot* _ps2 = &ctx_wayland->env->capture_slots[_pi2];
            char _pt2[16];
            int _pn2 = snprintf(_pt2, sizeof(_pt2), "P%+d", _ps2->pan);
            if (_pn2 > 0) {
                float _prow2 = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 4.0f;
#define PAN_OFFSET 292
                for (int _pi3 = 0; _pi3 < _pn2; _pi3++) {
                    unsigned char _pc2 = (unsigned char)_pt2[_pi3];
                    war_vulkan_text_instance* _ti2 = &dst[PAN_OFFSET + _pi3];
                    _ti2->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 6 + _pi3);
                    _ti2->pos[1] = _prow2;
                    _ti2->pos[2] = 0;
                    _ti2->size[0] = 1.0f;
                    _ti2->size[1] = 1.0f;
                    _ti2->uv[0] = font->glyph_uv[_pc2][0];
                    _ti2->uv[1] = font->glyph_uv[_pc2][1];
                    _ti2->uv[2] = font->glyph_uv[_pc2][2];
                    _ti2->uv[3] = font->glyph_uv[_pc2][3];
                    _ti2->glyph_scale[0] = font->glyph_norm_width[_pc2];
                    _ti2->glyph_scale[1] = font->glyph_norm_height[_pc2];
                    _ti2->ascent = font->glyph_norm_ascent[_pc2];
                    _ti2->descent = font->glyph_norm_descent[_pc2];
                    _ti2->baseline = font->glyph_norm_baseline[_pc2];
                    _ti2->color[0] = 0.2f; _ti2->color[1] = 0.8f; _ti2->color[2] = 1.0f; _ti2->color[3] = 1.0f;
                    _ti2->flags = 0;
                }
                vkCmdDraw(cmd, 4, (uint32_t)_pn2, 0, PAN_OFFSET);
#undef PAN_OFFSET
            }
        }
        // eq label on bottom status bar
        {
            double _er2 = ctx_wayland->env->ctx_cursor->instance[0].pos[1] - (double)ctx_wayland->gutter_rows;
            uint32_t _ep2 = _er2 > 0 ? (uint32_t)(_er2 + 0.5) : 0;
            if (_ep2 > 127) _ep2 = 127;
            uint32_t _el2 = ctx_wayland->env->ctx_cursor->layer;
            if (_el2 < 1 || _el2 > 9) _el2 = 1;
            uint32_t _ei2 = _ep2 * WAR_CAPTURE_SLOT_LAYERS + (_el2 - 1);
            war_capture_slot* _es2 = &ctx_wayland->env->capture_slots[_ei2];
            char _et2[16];
            int _en2 = snprintf(_et2, sizeof(_et2), "P%+d", _es2->eq);
            if (_en2 > 0) {
                float _erow2 = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 4.0f;
#define EQ_OFFSET 298
                for (int _ei3 = 0; _ei3 < _en2; _ei3++) {
                    unsigned char _ec2 = (unsigned char)_et2[_ei3];
                    war_vulkan_text_instance* _ti2 = &dst[EQ_OFFSET + _ei3];
                    _ti2->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 11 + _ei3);
                    _ti2->pos[1] = _erow2;
                    _ti2->pos[2] = 0;
                    _ti2->size[0] = 1.0f;
                    _ti2->size[1] = 1.0f;
                    _ti2->uv[0] = font->glyph_uv[_ec2][0];
                    _ti2->uv[1] = font->glyph_uv[_ec2][1];
                    _ti2->uv[2] = font->glyph_uv[_ec2][2];
                    _ti2->uv[3] = font->glyph_uv[_ec2][3];
                    _ti2->glyph_scale[0] = font->glyph_norm_width[_ec2];
                    _ti2->glyph_scale[1] = font->glyph_norm_height[_ec2];
                    _ti2->ascent = font->glyph_norm_ascent[_ec2];
                    _ti2->descent = font->glyph_norm_descent[_ec2];
                    _ti2->baseline = font->glyph_norm_baseline[_ec2];
                    _ti2->color[0] = 1.0f; _ti2->color[1] = 0.6f; _ti2->color[2] = 0.8f; _ti2->color[3] = 1.0f;
                    _ti2->flags = 0;
                }
                vkCmdDraw(cmd, 4, (uint32_t)_en2, 0, EQ_OFFSET);
#undef EQ_OFFSET
            }
        }
        // resample label on bottom status bar
        if (!ctx_wayland->env->across_resample) {
            const char* _rst = "RESAMPLE";
            int _rsn = 8;
            float _rsr = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 4.0f;
#define RSMP_OFFSET 390
            for (int _rsi = 0; _rsi < _rsn; _rsi++) {
                unsigned char _rsc = (unsigned char)_rst[_rsi];
                war_vulkan_text_instance* _ti = &dst[RSMP_OFFSET + _rsi];
                _ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 26 + _rsi);
                _ti->pos[1] = _rsr;
                _ti->pos[2] = 0;
                _ti->size[0] = 1.0f;
                _ti->size[1] = 1.0f;
                _ti->uv[0] = font->glyph_uv[_rsc][0];
                _ti->uv[1] = font->glyph_uv[_rsc][1];
                _ti->uv[2] = font->glyph_uv[_rsc][2];
                _ti->uv[3] = font->glyph_uv[_rsc][3];
                _ti->glyph_scale[0] = font->glyph_norm_width[_rsc];
                _ti->glyph_scale[1] = font->glyph_norm_height[_rsc];
                _ti->ascent = font->glyph_norm_ascent[_rsc];
                _ti->descent = font->glyph_norm_descent[_rsc];
                _ti->baseline = font->glyph_norm_baseline[_rsc];
                _ti->color[0] = 0.2f; _ti->color[1] = 0.8f; _ti->color[2] = 1.0f; _ti->color[3] = 1.0f;
                _ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)_rsn, 0, RSMP_OFFSET);
#undef RSMP_OFFSET
        }
        // playbar loop label on bottom status bar (right of RESAMPLE)
        if (ctx_wayland->env->play_bar_loop) {
            const char* _plt = "PB LOOP";
            int _pln = 7;
            float _plr = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 4.0f;
#define PBLOOP_OFFSET 400
            for (int _pli = 0; _pli < _pln; _pli++) {
                unsigned char _pc = (unsigned char)_plt[_pli];
                war_vulkan_text_instance* _ti = &dst[PBLOOP_OFFSET + _pli];
                _ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 35 + _pli);
                _ti->pos[1] = _plr;
                _ti->pos[2] = 0;
                _ti->size[0] = 1.0f;
                _ti->size[1] = 1.0f;
                _ti->uv[0] = font->glyph_uv[_pc][0];
                _ti->uv[1] = font->glyph_uv[_pc][1];
                _ti->uv[2] = font->glyph_uv[_pc][2];
                _ti->uv[3] = font->glyph_uv[_pc][3];
                _ti->glyph_scale[0] = font->glyph_norm_width[_pc];
                _ti->glyph_scale[1] = font->glyph_norm_height[_pc];
                _ti->ascent = font->glyph_norm_ascent[_pc];
                _ti->descent = font->glyph_norm_descent[_pc];
                _ti->baseline = font->glyph_norm_baseline[_pc];
                _ti->color[0] = 0.2f; _ti->color[1] = 1.0f; _ti->color[2] = 0.2f; _ti->color[3] = 1.0f;
                _ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)_pln, 0, PBLOOP_OFFSET);
#undef PBLOOP_OFFSET
        }
        // loop mode label
        if (ctx_wayland->env->loop_mode && ctx_wayland->env->active_mode == WAR_MODE_ID_MIDI) {
            const char* loop_text = "LOOP";
            int loop_n = 4;
#define LOOP_OFFSET 335
            for (int i = 0; i < loop_n; i++) {
                unsigned char c = (unsigned char)loop_text[i];
                war_vulkan_text_instance* ti = &dst[LOOP_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 26 + i);
                ti->pos[1] = label_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f;
                ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0]; ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2]; ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 0.2f; ti->color[1] = 1.0f; ti->color[2] = 0.2f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)loop_n, 0, LOOP_OFFSET);
#undef LOOP_OFFSET
        }
        // across mode label
        if (ctx_wayland->env->across_mode && ctx_wayland->env->active_mode != WAR_MODE_ID_MIDI) {
            const char* atext = "ACROSS";
            int an = 6;
#define ACROSS_OFFSET 328
            for (int i = 0; i < an; i++) {
                unsigned char c = (unsigned char)atext[i];
                war_vulkan_text_instance* ti = &dst[ACROSS_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 31 + i);
                ti->pos[1] = label_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f;
                ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0];
                ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2];
                ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 0.4f; ti->color[2] = 0.4f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)an, 0, ACROSS_OFFSET);
#undef ACROSS_OFFSET
        }
        // toggle label (purple)
        if (ctx_wayland->env->midi_toggle && ctx_wayland->env->active_mode == WAR_MODE_ID_MIDI) {
            const char* tog_txt = "TOGGLE";
            int tog_n = 6;
#define TOG_OFFSET 344
            for (int i = 0; i < tog_n; i++) {
                unsigned char c = (unsigned char)tog_txt[i];
                war_vulkan_text_instance* ti = &dst[TOG_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 34 + i);
                ti->pos[1] = label_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f; ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0]; ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2]; ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 0.7f; ti->color[1] = 0.3f; ti->color[2] = 0.9f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)tog_n, 0, TOG_OFFSET);
#undef TOG_OFFSET
        }
        // crop label
        if (ctx_wayland->env->crop_active) {
            const char* croptxt = "CROP";
            int cropn = 4;
#define CROP_OFFSET 325
            for (int i = 0; i < cropn; i++) {
                unsigned char c = (unsigned char)croptxt[i];
                war_vulkan_text_instance* ti = &dst[CROP_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(n + 32 + i);
                ti->pos[1] = label_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f; ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0]; ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2]; ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 0.7f; ti->color[2] = 0.2f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)cropn, 0, CROP_OFFSET);
#undef CROP_OFFSET
        }
        // capture label on middle status bar
        if (ctx_wayland->env->atomics->capture) {
            char captxt[16];
            int capn = snprintf(captxt, sizeof(captxt), "CAPTURE %u", ctx_wayland->env->capture_mode);
            float caprow = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 3.0f;
#define CAP_OFFSET 349
            for (int i = 0; i < capn; i++) {
                unsigned char c = (unsigned char)captxt[i];
                war_vulkan_text_instance* ti = &dst[CAP_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)i;
                ti->pos[1] = caprow;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f; ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0]; ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2]; ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 0.3f; ti->color[2] = 0.3f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)capn, 0, CAP_OFFSET);
#undef CAP_OFFSET
        }
        // midi mode label on middle status bar
        if (ctx_wayland->env->active_mode == WAR_MODE_ID_MIDI) {
            char midi_text[32];
            int midi_n = ctx_wayland->env->recording_active 
                ? snprintf(midi_text, sizeof(midi_text), "MIDI RECORD C%u", ctx_wayland->env->capture_mode)
                : snprintf(midi_text, sizeof(midi_text), "MIDI");
            float midi_row = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 3.0f;
#define MIDI_OFFSET 326
            for (int i = 0; i < midi_n; i++) {
                unsigned char c = (unsigned char)midi_text[i];
                war_vulkan_text_instance* ti = &dst[MIDI_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)i;
                ti->pos[1] = midi_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f;
                ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0];
                ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2];
                ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 1.0f; ti->color[2] = 0.2f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)midi_n, 0, MIDI_OFFSET);
#undef MIDI_OFFSET
        }
        // master mode label on middle status bar
        if (ctx_wayland->env->active_mode == WAR_MODE_ID_MASTER) {
            const char* mast_text = "MASTER";
            int mast_n = 6;
            float mast_row = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 3.0f;
#define MASTER_OFFSET 372
            for (int i = 0; i < mast_n; i++) {
                unsigned char c = (unsigned char)mast_text[i];
                war_vulkan_text_instance* ti = &dst[MASTER_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)i;
                ti->pos[1] = mast_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f;
                ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0];
                ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2];
                ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 0.3f; ti->color[2] = 0.3f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)mast_n, 0, MASTER_OFFSET);
#undef MASTER_OFFSET
        }
        // tap tempo label on middle status bar
        if (ctx_wayland->env->tap_tempo_active) {
            const char* tap_txt = "TAP TEMPO";
            int tap_n = 9;
            float tap_row = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 3.0f;
#define TAP_OFFSET 354
            for (int i = 0; i < tap_n; i++) {
                unsigned char c = (unsigned char)tap_txt[i];
                war_vulkan_text_instance* ti = &dst[TAP_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)i;
                ti->pos[1] = tap_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f; ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0]; ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2]; ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 0.3f; ti->color[1] = 1.0f; ti->color[2] = 1.0f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)tap_n, 0, TAP_OFFSET);
#undef TAP_OFFSET
        }
        // visual mode label
        if (ctx_wayland->env->ctx_cursor->visual_active) {
            const char* vis_text = "VISUAL";
            int vis_n = 6;
            float vis_row = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 3.0f;
#define VIS_OFFSET 332
            for (int i = 0; i < vis_n; i++) {
                unsigned char c = (unsigned char)vis_text[i];
                war_vulkan_text_instance* ti = &dst[VIS_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)i;
                ti->pos[1] = vis_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f;
                ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0];
                ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2];
                ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 0.6f; ti->color[2] = 0.2f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)vis_n, 0, VIS_OFFSET);
#undef VIS_OFFSET
        }
        // stretch mode label
        if (ctx_wayland->env->ctx_cursor->visual_stretch_active) {
            const char* str_text = "STRETCH";
            int str_n = 7;
            float str_row = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 3.0f;
#define STR_OFFSET 358
            for (int i = 0; i < str_n; i++) {
                unsigned char c = (unsigned char)str_text[i];
                war_vulkan_text_instance* ti = &dst[STR_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(7 + i);
                ti->pos[1] = str_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f;
                ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0];
                ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2];
                ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 0.4f; ti->color[2] = 0.4f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)str_n, 0, STR_OFFSET);
#undef STR_OFFSET
        }
        // status message on middle status bar (e.g. "test loaded")
        if (ctx_wayland->env->status_msg[0]) {
            const char* sm = ctx_wayland->env->status_msg;
            int sm_n = (int)strlen(sm);
            if (sm_n > 60) sm_n = 60;
            float sm_row = ctx_wayland->panning[1] + (float)ctx_wayland->gutter_rows - 3.0f;
#define SM_OFFSET 363
            for (int i = 0; i < sm_n; i++) {
                unsigned char c = (unsigned char)sm[i];
                if (c < 32 || c > 126) c = '?';
                war_vulkan_text_instance* ti = &dst[SM_OFFSET + i];
                ti->pos[0] = ctx_wayland->panning[0] + (float)ctx_wayland->gutter_cols + (float)(10 + i);
                ti->pos[1] = sm_row;
                ti->pos[2] = 0;
                ti->size[0] = 1.0f;
                ti->size[1] = 1.0f;
                ti->uv[0] = font->glyph_uv[c][0];
                ti->uv[1] = font->glyph_uv[c][1];
                ti->uv[2] = font->glyph_uv[c][2];
                ti->uv[3] = font->glyph_uv[c][3];
                ti->glyph_scale[0] = font->glyph_norm_width[c];
                ti->glyph_scale[1] = font->glyph_norm_height[c];
                ti->ascent = font->glyph_norm_ascent[c];
                ti->descent = font->glyph_norm_descent[c];
                ti->baseline = font->glyph_norm_baseline[c];
                ti->color[0] = 1.0f; ti->color[1] = 1.0f; ti->color[2] = 1.0f; ti->color[3] = 1.0f;
                ti->flags = 0;
            }
            vkCmdDraw(cmd, 4, (uint32_t)sm_n, 0, SM_OFFSET);
#undef SM_OFFSET
        }
    }
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                       .commandBufferCount = 1,
                       .pCommandBuffers = &cmd};
    vkQueueSubmit(ctx_vk->queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_vk->queue);
    vkFreeCommandBuffers(ctx_vk->device, ctx_vk->cmd_pool, 1, &cmd);
}

static inline void war_render_init_frame(war_wayland_context* ctx_wayland,
                                         war_vulkan_context* ctx_vk,
                                         war_color_context* ctx_color) {
    war_render_init(ctx_wayland, ctx_vk);
    war_render_frame(ctx_wayland, ctx_vk, ctx_color);
}

static inline void war_vulkan_init(war_wayland_context* ctx_wayland,
                                   war_vulkan_context* ctx_vk) {
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
    VkResult result = vkCreateInstance(&instance_info, NULL, &ctx_vk->instance);
    WASSERT(result == VK_SUCCESS);

    uint32_t gpu_count;
    vkEnumeratePhysicalDevices(ctx_vk->instance, &gpu_count, NULL);
    WASSERT(gpu_count > 0);
    vkEnumeratePhysicalDevices(
        ctx_vk->instance, &gpu_count, &ctx_vk->physical_device);

    uint32_t qfam_count;
    vkGetPhysicalDeviceQueueFamilyProperties(
        ctx_vk->physical_device, &qfam_count, NULL);
    VkQueueFamilyProperties qprops[32];
    vkGetPhysicalDeviceQueueFamilyProperties(
        ctx_vk->physical_device, &qfam_count, qprops);
    uint32_t graphics_family = UINT32_MAX;
    for (uint32_t i = 0; i < qfam_count; i++) {
        if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i;
            break;
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
    result =
        vkCreateDevice(ctx_vk->physical_device, &dci, NULL, &ctx_vk->device);
    WASSERT(result == VK_SUCCESS);

    vkGetDeviceQueue(ctx_vk->device, graphics_family, 0, &ctx_vk->queue);

    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);

    VkImageCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = {ctx_wayland->width, ctx_wayland->height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    result = vkCreateImage(ctx_vk->device, &ici, NULL, &ctx_vk->image);
    WASSERT(result == VK_SUCCESS);

    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(ctx_vk->device, ctx_vk->image, &mem_req);
    uint32_t mem_type = find_mem_type(
        mem_props, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
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
    result = vkAllocateMemory(ctx_vk->device, &mai, NULL, &ctx_vk->memory);
    WASSERT(result == VK_SUCCESS);
    result =
        vkBindImageMemory(ctx_vk->device, ctx_vk->image, ctx_vk->memory, 0);
    WASSERT(result == VK_SUCCESS);

    VkImageSubresource sub = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT};
    vkGetImageSubresourceLayout(
        ctx_vk->device, ctx_vk->image, &sub, &ctx_vk->img_layout);

    VkImageMemoryBarrier init_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .image = ctx_vk->image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    VkCommandPoolCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = graphics_family,
    };
    vkCreateCommandPool(ctx_vk->device, &cpci, NULL, &ctx_vk->cmd_pool);
    ctx_vk->cbai = (VkCommandBufferAllocateInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx_vk->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer init_cmd;
    vkAllocateCommandBuffers(ctx_vk->device, &ctx_vk->cbai, &init_cmd);
    VkCommandBufferBeginInfo init_cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(init_cmd, &init_cbbi);
    vkCmdPipelineBarrier(init_cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &init_barrier);
    vkEndCommandBuffer(init_cmd);
    VkSubmitInfo init_si = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &init_cmd,
    };
    vkQueueSubmit(ctx_vk->queue, 1, &init_si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_vk->queue);
    vkFreeCommandBuffers(ctx_vk->device, ctx_vk->cmd_pool, 1, &init_cmd);

    PFN_vkGetMemoryFdKHR pfn_vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(ctx_vk->device,
                                                  "vkGetMemoryFdKHR");
    WASSERT(pfn_vkGetMemoryFdKHR);
    ctx_vk->dmabuf_fd = -1;
    VkMemoryGetFdInfoKHR fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = ctx_vk->memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    result = pfn_vkGetMemoryFdKHR(ctx_vk->device, &fd_info, &ctx_vk->dmabuf_fd);
    WASSERT(result == VK_SUCCESS && ctx_vk->dmabuf_fd >= 0);
}

#endif // WAR_VULKAN_H
