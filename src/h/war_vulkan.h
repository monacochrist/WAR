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
    ctx_cursor->draw = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_DRAW);
    ctx_cursor->x_seconds = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_X_SECONDS);
    ctx_cursor->y_cells = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_Y_CELLS);
    ctx_cursor->instance = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_CURSOR_INSTANCE);
    ctx_cursor->instance_count = 0;

    //-------------------------------------------------------------------------
    // LOAD SHADER SPIR-V
    //-------------------------------------------------------------------------
    const char* vert_path = "build/spv/war_new_vulkan_vertex_cursor.spv";
    const char* frag_path = "build/spv/war_new_vulkan_fragment_cursor.spv";

    uint8_t vert_code[4096];
    uint8_t frag_code[4096];
    size_t vert_size = 0, frag_size = 0;

    FILE* f = fopen(vert_path, "rb");
    if (f) {
        vert_size = fread(vert_code, 1, sizeof(vert_code), f);
        fclose(f);
    }
    WASSERT(vert_size > 0 && vert_size % 4 == 0);
    f = fopen(frag_path, "rb");
    if (f) {
        frag_size = fread(frag_code, 1, sizeof(frag_code), f);
        fclose(f);
    }
    WASSERT(frag_size > 0 && frag_size % 4 == 0);

    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_size,
        .pCode = (uint32_t*)vert_code,
    };
    VkResult res = vkCreateShaderModule(ctx_vk->device, &smci, NULL, &ctx_cursor->vert_module);
    WASSERT(res == VK_SUCCESS);
    smci.codeSize = frag_size;
    smci.pCode = (uint32_t*)frag_code;
    res = vkCreateShaderModule(ctx_vk->device, &smci, NULL, &ctx_cursor->frag_module);
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
    res = vkCreatePipelineLayout(ctx_vk->device, &plci, NULL, &ctx_cursor->pipeline_layout);
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
        {1, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(war_vulkan_cursor_instance, pos)},
        {2, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(war_vulkan_cursor_instance, size)},
        {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_vulkan_cursor_instance, color)},
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
        .layout = ctx_cursor->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    res = vkCreateGraphicsPipelines(ctx_vk->device, VK_NULL_HANDLE, 1, &gpci, NULL, &ctx_cursor->pipeline);
    WASSERT(res == VK_SUCCESS);

    //-------------------------------------------------------------------------
    // QUAD VERTEX BUFFER
    //-------------------------------------------------------------------------
    float quad_verts[] = {
         0.0f,  0.0f,
         1.0f,  0.0f,
         0.0f,  1.0f,
         1.0f,  1.0f,
    };
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    res = vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_cursor->quad_vbo);
    WASSERT(res == VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_cursor->quad_vbo, &mem_req);
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
    res = vkAllocateMemory(ctx_vk->device, &mai, NULL, &ctx_cursor->quad_vbo_memory);
    WASSERT(res == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, ctx_cursor->quad_vbo, ctx_cursor->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(ctx_vk->device, ctx_cursor->quad_vbo_memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_cursor->quad_vbo_memory);

    //-------------------------------------------------------------------------
    // INSTANCE VERTEX BUFFER (host-visible, persistently mapped)
    //-------------------------------------------------------------------------
    VkDeviceSize instance_buf_size = sizeof(war_vulkan_cursor_instance) * max_instances;
    bci.size = instance_buf_size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    res = vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_cursor->instance_vbo);
    WASSERT(res == VK_SUCCESS);
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_cursor->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    res = vkAllocateMemory(ctx_vk->device, &mai, NULL, &ctx_cursor->instance_vbo_memory);
    WASSERT(res == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, ctx_cursor->instance_vbo, ctx_cursor->instance_vbo_memory, 0);
    vkMapMemory(ctx_vk->device, ctx_cursor->instance_vbo_memory, 0, VK_WHOLE_SIZE, 0, &ctx_cursor->instance_mapped);
}

static inline void war_piano_gutter_init(war_piano_gutter_context* ctx_pg,
                                         war_pool_context* ctx_pool,
                                         war_config_context* ctx_config,
                                         war_vulkan_context* ctx_vk) {
    uint32_t max_instances = (uint32_t)ctx_config->HUD_PIANO_INSTANCE_MAX;
    ctx_pg->draw = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_DRAW);
    ctx_pg->x_cells = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_X_CELLS);
    ctx_pg->y_cells = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_Y_CELLS);
    ctx_pg->x_width = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_X_WIDTH);
    ctx_pg->instance = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_PIANO_GUTTER_INSTANCE);
    ctx_pg->instance_count = 0;

    const char* vert_path = "build/spv/war_new_vulkan_vertex_piano_gutter.spv";
    const char* frag_path = "build/spv/war_new_vulkan_fragment_piano_gutter.spv";
    uint8_t vert_code[4096], frag_code[4096];
    size_t vert_size = 0, frag_size = 0;
    FILE* f = fopen(vert_path, "rb");
    if (f) { vert_size = fread(vert_code, 1, sizeof(vert_code), f); fclose(f); }
    WASSERT(vert_size > 0 && vert_size % 4 == 0);
    f = fopen(frag_path, "rb");
    if (f) { frag_size = fread(frag_code, 1, sizeof(frag_code), f); fclose(f); }
    WASSERT(frag_size > 0 && frag_size % 4 == 0);

    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_size, .pCode = (uint32_t*)vert_code,
    };
    WASSERT(vkCreateShaderModule(ctx_vk->device, &smci, NULL, &ctx_pg->vert_module) == VK_SUCCESS);
    smci.codeSize = frag_size; smci.pCode = (uint32_t*)frag_code;
    WASSERT(vkCreateShaderModule(ctx_vk->device, &smci, NULL, &ctx_pg->frag_module) == VK_SUCCESS);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &pc_range,
    };
    WASSERT(vkCreatePipelineLayout(ctx_vk->device, &plci, NULL, &ctx_pg->pipeline_layout) == VK_SUCCESS);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = ctx_pg->vert_module, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = ctx_pg->frag_module, .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(war_vulkan_piano_gutter_instance), VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[8] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(war_vulkan_piano_gutter_instance, pos)},
        {2, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(war_vulkan_piano_gutter_instance, size)},
        {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_vulkan_piano_gutter_instance, color)},
        {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_vulkan_piano_gutter_instance, outline_color)},
        {5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_vulkan_piano_gutter_instance, foreground_color)},
        {6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_vulkan_piano_gutter_instance, foreground_outline_color)},
        {7, 1, VK_FORMAT_R32_UINT, offsetof(war_vulkan_piano_gutter_instance, flags)},
    };
    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2, .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 8, .pVertexAttributeDescriptions = attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    };
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2, .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1,
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
        .attachmentCount = 1, .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vis, .pInputAssemblyState = &ias,
        .pViewportState = &vpsi, .pRasterizationState = &rs,
        .pMultisampleState = &ms, .pDynamicState = &dsi,
        .pColorBlendState = &cbs, .layout = ctx_pg->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    WASSERT(vkCreateGraphicsPipelines(ctx_vk->device, VK_NULL_HANDLE, 1, &gpci, NULL, &ctx_pg->pipeline) == VK_SUCCESS);

    float quad_verts[] = {0.0f,0.0f, 1.0f,0.0f, 0.0f,1.0f, 1.0f,1.0f};
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_pg->quad_vbo) == VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_pg->quad_vbo, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size, .memoryTypeIndex = mem_type,
    };
    WASSERT(vkAllocateMemory(ctx_vk->device, &mai, NULL, &ctx_pg->quad_vbo_memory) == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, ctx_pg->quad_vbo, ctx_pg->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(ctx_vk->device, ctx_pg->quad_vbo_memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_pg->quad_vbo_memory);

    VkDeviceSize instance_buf_size = sizeof(war_vulkan_piano_gutter_instance) * max_instances;
    bci.size = instance_buf_size;
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_pg->instance_vbo) == VK_SUCCESS);
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_pg->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    WASSERT(vkAllocateMemory(ctx_vk->device, &mai, NULL, &ctx_pg->instance_vbo_memory) == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, ctx_pg->instance_vbo, ctx_pg->instance_vbo_memory, 0);
    vkMapMemory(ctx_vk->device, ctx_pg->instance_vbo_memory, 0, VK_WHOLE_SIZE, 0, &ctx_pg->instance_mapped);
}

static inline void
war_piano_gutter_generate(war_piano_gutter_context* ctx_pg) {
    ctx_pg->instance_count = 128;
    for (uint32_t i = 0; i < 128; i++) {
        uint32_t c = i % 12;
        int black = (c == 1 || c == 3 || c == 6 || c == 8 || c == 10);
        ctx_pg->instance[i].color[0] = black ? 0 : 1;
        ctx_pg->instance[i].color[1] = black ? 0 : 1;
        ctx_pg->instance[i].color[2] = black ? 0 : 1;
        ctx_pg->instance[i].color[3] = 1;
        ctx_pg->instance[i].pos[0] = 1.0;
        ctx_pg->instance[i].pos[1] = (float)i;
        ctx_pg->instance[i].pos[2] = 0;
        ctx_pg->instance[i].size[0] = 3;
        ctx_pg->instance[i].size[1] = 1;
        ctx_pg->instance[i].flags = 0;
    }
}


static inline void war_piano_gutter_render(VkCommandBuffer cmd,
                                            war_piano_gutter_context* ctx_pg,
                                            war_wayland_context* ctx_wayland,
                                            war_cursor_context* ctx_cursor,
                                            float screen_w,
                                            float screen_h) {
    if (!ctx_pg || !ctx_pg->instance_count) return;
    memcpy(ctx_pg->instance_mapped, ctx_pg->instance,
           sizeof(war_vulkan_piano_gutter_instance) * ctx_pg->instance_count);
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    VkRect2D scissor = {{0, 0}, {(uint32_t)screen_w, (uint32_t)screen_h}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx_pg->pipeline);
    float pc_data[] = {
         (float)ctx_cursor->cell_width, (float)ctx_cursor->cell_height,
         -ctx_wayland->panning[0] * ctx_wayland->zoom,
         -ctx_wayland->panning[1] * ctx_wayland->zoom,
         ctx_wayland->zoom,
         0,
         screen_w, screen_h,
         0, 0,
    };
    vkCmdPushConstants(cmd, ctx_pg->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
    VkBuffer bufs[] = {ctx_pg->quad_vbo, ctx_pg->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, ctx_pg->instance_count, 0, 0);
}

static inline void war_cursor_render(VkCommandBuffer cmd,
                                     war_cursor_context* ctx_cursor,
                                     war_wayland_context* ctx_wayland,
                                     float screen_w,
                                     float screen_h) {
    if (!ctx_cursor || !ctx_cursor->instance_count) return;
    memcpy(ctx_cursor->instance_mapped, ctx_cursor->instance,
           sizeof(war_vulkan_cursor_instance) * ctx_cursor->instance_count);
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    VkRect2D scissor = {{0, 0}, {(uint32_t)screen_w, (uint32_t)screen_h}};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx_cursor->pipeline);
    float pc_data[] = {
         (float)ctx_cursor->cell_width, (float)ctx_cursor->cell_height, // cell_size (offset 0)
         -ctx_wayland->panning[0] * ctx_wayland->zoom,
         -ctx_wayland->panning[1] * ctx_wayland->zoom, // panning (offset 8)
         ctx_wayland->zoom, // zoom (offset 16)
         0,                 // padding (offset 20, aligns vec2)
         screen_w, screen_h, // screen_size (offset 24)
         0, 0,              // cell_offset (offset 32)
    };
    vkCmdPushConstants(cmd, ctx_cursor->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
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
    ctx_note->draw = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_DRAW);
    ctx_note->x_seconds = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_X_SECONDS);
    ctx_note->y_cells = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_Y_CELLS);
    ctx_note->x_width = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_X_WIDTH);
    ctx_note->instance = war_pool_alloc_new(ctx_pool, WAR_POOL_ID_MAIN_CTX_NOTE_INSTANCE);
    ctx_note->instance_count = 0;
    ctx_note->max_instances = 1024;
    ctx_note->tick_counter = 0;

    const char* vert_path = "build/spv/war_new_vulkan_vertex_note.spv";
    const char* frag_path = "build/spv/war_new_vulkan_fragment_note.spv";
    uint8_t vert_code[4096], frag_code[4096];
    size_t vert_size = 0, frag_size = 0;
    FILE* f = fopen(vert_path, "rb");
    if (f) { vert_size = fread(vert_code, 1, sizeof(vert_code), f); fclose(f); }
    WASSERT(vert_size > 0 && vert_size % 4 == 0);
    f = fopen(frag_path, "rb");
    if (f) { frag_size = fread(frag_code, 1, sizeof(frag_code), f); fclose(f); }
    WASSERT(frag_size > 0 && frag_size % 4 == 0);

    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vert_size, .pCode = (uint32_t*)vert_code,
    };
    WASSERT(vkCreateShaderModule(ctx_vk->device, &smci, NULL, &ctx_note->vert_module) == VK_SUCCESS);
    smci.codeSize = frag_size; smci.pCode = (uint32_t*)frag_code;
    WASSERT(vkCreateShaderModule(ctx_vk->device, &smci, NULL, &ctx_note->frag_module) == VK_SUCCESS);

    VkPushConstantRange pc_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = 40,
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &pc_range,
    };
    WASSERT(vkCreatePipelineLayout(ctx_vk->device, &plci, NULL, &ctx_note->pipeline_layout) == VK_SUCCESS);

    VkPipelineShaderStageCreateInfo stages[2] = {
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = ctx_note->vert_module, .pName = "main"},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
         .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = ctx_note->frag_module, .pName = "main"},
    };

    VkVertexInputBindingDescription bindings[2] = {
        {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(war_new_vulkan_note_instance), VK_VERTEX_INPUT_RATE_INSTANCE},
    };
    VkVertexInputAttributeDescription attrs[6] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(war_new_vulkan_note_instance, pos)},
        {2, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(war_new_vulkan_note_instance, size)},
        {3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_new_vulkan_note_instance, color)},
        {4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(war_new_vulkan_note_instance, outline_color)},
        {7, 1, VK_FORMAT_R32_UINT, offsetof(war_new_vulkan_note_instance, flags)},
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
    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2, .pDynamicStates = dyn_states,
    };
    VkPipelineViewportStateCreateInfo vpsi = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .scissorCount = 1,
    };
    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1,
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
        .attachmentCount = 1, .pAttachments = &cba,
    };
    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vis, .pInputAssemblyState = &ias,
        .pViewportState = &vpsi, .pRasterizationState = &rs,
        .pMultisampleState = &ms, .pDynamicState = &dsi,
        .pColorBlendState = &cbs, .layout = ctx_note->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
    };
    WASSERT(vkCreateGraphicsPipelines(ctx_vk->device, VK_NULL_HANDLE, 1, &gpci, NULL, &ctx_note->pipeline) == VK_SUCCESS);

    float quad_verts[] = {0.0f,0.0f, 1.0f,0.0f, 0.0f,1.0f, 1.0f,1.0f};
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(quad_verts), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    };
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_note->quad_vbo) == VK_SUCCESS);
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_note->quad_vbo, &mem_req);
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device, &mem_props);
    uint32_t mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    VkMemoryAllocateInfo mai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size, .memoryTypeIndex = mem_type,
    };
    WASSERT(vkAllocateMemory(ctx_vk->device, &mai, NULL, &ctx_note->quad_vbo_memory) == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, ctx_note->quad_vbo, ctx_note->quad_vbo_memory, 0);
    void* mapped;
    vkMapMemory(ctx_vk->device, ctx_note->quad_vbo_memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    memcpy(mapped, quad_verts, sizeof(quad_verts));
    vkUnmapMemory(ctx_vk->device, ctx_note->quad_vbo_memory);

    VkDeviceSize instance_buf_size = sizeof(war_new_vulkan_note_instance) * ctx_note->max_instances;
    bci.size = instance_buf_size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    WASSERT(vkCreateBuffer(ctx_vk->device, &bci, NULL, &ctx_note->instance_vbo) == VK_SUCCESS);
    vkGetBufferMemoryRequirements(ctx_vk->device, ctx_note->instance_vbo, &mem_req);
    mem_type = find_mem_type(mem_props, mem_req.memoryTypeBits,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    WASSERT(mem_type != UINT32_MAX);
    mai.allocationSize = mem_req.size;
    WASSERT(vkAllocateMemory(ctx_vk->device, &mai, NULL, &ctx_note->instance_vbo_memory) == VK_SUCCESS);
    vkBindBufferMemory(ctx_vk->device, ctx_note->instance_vbo, ctx_note->instance_vbo_memory, 0);
    vkMapMemory(ctx_vk->device, ctx_note->instance_vbo_memory, 0, VK_WHOLE_SIZE, 0, &ctx_note->instance_mapped);
}

static inline void war_note_render(VkCommandBuffer cmd,
                                   war_note_context* ctx_note,
                                   war_wayland_context* ctx_wayland,
                                   float screen_w,
                                   float screen_h) {
    if (!ctx_note || !ctx_note->instance_count) return;
    memcpy(ctx_note->instance_mapped, ctx_note->instance,
           sizeof(war_new_vulkan_note_instance) * ctx_note->instance_count);
    VkViewport vp = {0, 0, screen_w, screen_h, 0, 1};
    VkRect2D scissor = {{0, 0}, {(uint32_t)screen_w, (uint32_t)screen_h}};
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
         screen_w, screen_h,
         0, 0,
    };
    vkCmdPushConstants(cmd, ctx_note->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc_data), pc_data);
    VkBuffer bufs[] = {ctx_note->quad_vbo, ctx_note->instance_vbo};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 2, bufs, offsets);
    vkCmdDraw(cmd, 4, ctx_note->instance_count, 0, 0);
}

static inline void war_render_frame(war_wayland_context* ctx_wayland,
                                    war_vulkan_context* ctx_vk) {
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx_vk->device, &ctx_vk->cbai, &cmd);
    VkCommandBufferBeginInfo cbbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(cmd, &cbbi);
    VkClearValue clear = {.color = {{0.1, 0, 0, 0}}};
    VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx_vk->render_pass,
        .framebuffer = ctx_vk->framebuffer,
        .renderArea = {{0, 0}, {ctx_wayland->width, ctx_wayland->height}},
        .clearValueCount = 1,
        .pClearValues = &clear,
    };
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    war_piano_gutter_render(cmd, ctx_wayland->env->ctx_piano_gutter, ctx_wayland,
                            ctx_wayland->env->ctx_cursor,
                            (float)ctx_wayland->width, (float)ctx_wayland->height);
    if (ctx_wayland->env->ctx_note)
        war_note_render(cmd, ctx_wayland->env->ctx_note, ctx_wayland,
                        (float)ctx_wayland->width, (float)ctx_wayland->height);
    war_cursor_render(cmd, ctx_wayland->env->ctx_cursor, ctx_wayland,
                      (float)ctx_wayland->width, (float)ctx_wayland->height);
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
                                         war_vulkan_context* ctx_vk) {
    war_render_init(ctx_wayland, ctx_vk);
    war_render_frame(ctx_wayland, ctx_vk);
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
