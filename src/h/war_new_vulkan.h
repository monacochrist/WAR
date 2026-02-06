//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_new_vulkan.h
//-----------------------------------------------------------------------------

#ifndef WAR_NEW_VULKAN_H
#define WAR_NEW_VULKAN_H

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_functions.h"

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

static inline uint8_t war_new_vulkan_get_shader_module(
    VkDevice device, VkShaderModule* shader_module, const char* path) {
    call_king_terry("war_vulkan_get_shader_module");
    FILE* file = fopen(path, "rb");
    if (!file) {
        call_king_terry("failed to open file: %s", path);
        return 0;
    }
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);
    char* code = malloc(size);
    if (!code) {
        call_king_terry("failed to allocate memory for shader: %s", path);
        fclose(file);
        return 0;
    }
    fread(code, 1, size, file);
    fclose(file);
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (uint32_t*)code};
    if (vkCreateShaderModule(device, &create_info, NULL, shader_module) !=
        VK_SUCCESS) {
        call_king_terry("Failed to create shader module: %s", path);
        return 0;
    }
    free(code);
    return 1;
}

static inline VkDeviceSize war_new_vulkan_align_size_up(VkDeviceSize size,
                                                        VkDeviceSize alignment,
                                                        VkDeviceSize capacity) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        call_king_terry("ERROR: alignment is not power-of-2");
        return 0;
    }
    VkDeviceSize aligned = (size + alignment - 1) & ~(alignment - 1);
    if (aligned > capacity) { aligned = capacity; }
    return aligned;
}

static inline VkDeviceSize war_new_vulkan_align_offset_down(
    VkDeviceSize offset, VkDeviceSize alignment, VkDeviceSize capacity) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        call_king_terry("ERROR: alignment is not power-of-2");
        return 0;
    }
    VkDeviceSize aligned = offset & ~(alignment - 1);
    if (aligned > capacity) {
        call_king_terry("ERROR: aligned offset exceeds capacity");
        return 0;
    }
    return aligned;
}

static inline void
war_new_vulkan_flush(uint32_t idx_count,
                     uint32_t* idx,
                     VkDeviceSize* offset,
                     VkDeviceSize* size,
                     VkDevice device,
                     war_new_vulkan_context* ctx_new_vulkan) {
    for (uint32_t i = 0; i < idx_count; i++) {
        VkDeviceSize off = offset ? offset[i] : 0;
        VkDeviceSize sz = size ? size[i] : ctx_new_vulkan->capacity[idx[i]];
        VkDeviceSize aligned_offset = war_new_vulkan_align_offset_down(
            off,
            ctx_new_vulkan->memory_requirements[idx[i]].alignment,
            ctx_new_vulkan->capacity[idx[i]]);
        VkDeviceSize aligned_size = war_new_vulkan_align_size_up(
            sz + (off - aligned_offset),
            ctx_new_vulkan->memory_requirements[idx[i]].alignment,
            ctx_new_vulkan->capacity[idx[i]] - aligned_offset);
        ctx_new_vulkan->mapped_memory_range[i] = (VkMappedMemoryRange){
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext = NULL,
            .memory = ctx_new_vulkan->device_memory[idx[i]],
            .offset = aligned_offset,
            .size = aligned_size,
        };
    }
    VkResult result = vkFlushMappedMemoryRanges(
        device, idx_count, ctx_new_vulkan->mapped_memory_range);
    assert(result == VK_SUCCESS);
}

static inline void
war_new_vulkan_invalidate(uint32_t idx_count,
                          uint32_t* idx,
                          VkDeviceSize* offset,
                          VkDeviceSize* size,
                          VkDevice device,
                          war_new_vulkan_context* ctx_new_vulkan) {
    for (uint32_t i = 0; i < idx_count; i++) {
        VkDeviceSize off = offset ? offset[i] : 0;
        VkDeviceSize sz = size ? size[i] : ctx_new_vulkan->capacity[idx[i]];
        VkDeviceSize aligned_offset = war_new_vulkan_align_offset_down(
            off,
            ctx_new_vulkan->memory_requirements[idx[i]].alignment,
            ctx_new_vulkan->capacity[idx[i]]);
        VkDeviceSize aligned_size = war_new_vulkan_align_size_up(
            sz + (off - aligned_offset),
            ctx_new_vulkan->memory_requirements[idx[i]].alignment,
            ctx_new_vulkan->capacity[idx[i]] - aligned_offset);
        ctx_new_vulkan->mapped_memory_range[i] = (VkMappedMemoryRange){
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext = NULL,
            .memory = ctx_new_vulkan->device_memory[idx[i]],
            .offset = aligned_offset,
            .size = aligned_size,
        };
    }
    VkResult result = vkInvalidateMappedMemoryRanges(
        device, idx_count, ctx_new_vulkan->mapped_memory_range);
    assert(result == VK_SUCCESS);
}

static inline void war_new_vulkan_copy(uint32_t idx_count,
                                       uint32_t* idx_src,
                                       uint32_t* idx_dst,
                                       VkDeviceSize* src_offset,
                                       VkDeviceSize* dst_offset,
                                       VkDeviceSize* size,
                                       VkDeviceSize* size_2,
                                       VkCommandBuffer cmd,
                                       war_new_vulkan_context* ctx_new_vulkan) {
    for (uint32_t i = 0; i < idx_count; i++) {
        if (ctx_new_vulkan->image[idx_src[i]] &&
            ctx_new_vulkan->image[idx_dst[i]]) {
            goto war_label_copy_image;
        } else if (ctx_new_vulkan->image[idx_dst[i]]) {
            goto war_label_copy_buffer_image;
        }
        VkDeviceSize src_offset_temp = src_offset ? src_offset[i] : 0;
        VkDeviceSize dst_offset_temp = dst_offset ? dst_offset[i] : 0;
        VkDeviceSize size_temp = size[i];
        if (!size_temp) {
            src_offset_temp = 0;
            dst_offset_temp = 0;
            VkDeviceSize dst_capacity = ctx_new_vulkan->capacity[idx_dst[i]];
            size_temp = ctx_new_vulkan->capacity[idx_src[i]];
            if (size_temp > dst_capacity) { size_temp = dst_capacity; }
        }
        if (dst_offset_temp + size_temp >
            ctx_new_vulkan->capacity[idx_dst[i]]) {
            src_offset_temp = 0;
            dst_offset_temp = 0;
            VkDeviceSize dst_capacity = ctx_new_vulkan->capacity[idx_dst[i]];
            size_temp = ctx_new_vulkan->capacity[idx_src[i]];
            if (size_temp > dst_capacity) { size_temp = dst_capacity; }
        }
        VkBufferCopy copy = {
            .srcOffset = src_offset_temp,
            .dstOffset = dst_offset_temp,
            .size = size_temp,
        };
        vkCmdCopyBuffer(cmd,
                        ctx_new_vulkan->buffer[idx_src[i]],
                        ctx_new_vulkan->buffer[idx_dst[i]],
                        1,
                        &copy);
        continue;
    war_label_copy_image: {
        VkImage src_img = ctx_new_vulkan->image[idx_src[i]];
        VkImage dst_img = ctx_new_vulkan->image[idx_dst[i]];

        VkImageSubresourceLayers subresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        // Get size in bytes and convert to number of pixels
        VkDeviceSize num_bytes = size ? size[i] : 0;
        if (num_bytes == 0) continue;

        uint32_t num_pixels = (uint32_t)(num_bytes / sizeof(float));
        if (num_pixels == 0) continue;

        // Compute linear pixel offset in destination
        uint32_t linear_offset =
            (dst_offset ? dst_offset[i] / sizeof(float) : 0);

        // Image dimensions
        uint32_t image_width = ctx_new_vulkan->extent_3d[idx_src[i]].width;
        uint32_t image_height = ctx_new_vulkan->extent_3d[idx_src[i]].height;

        if (linear_offset >= image_width * image_height) continue;

        // Interpret dst_offset as a sample offset (column-major: bin_capacity
        // floats per sample) and append a rectangle of width=diff_samples,
        // height=bin_capacity without wrapping vertically or duplicating rows.
        uint32_t start_sample = linear_offset / image_height;
        uint32_t start_x = start_sample % image_width;

        // Clamp so we never wrap horizontally beyond frame_capacity
        uint32_t max_pixels = (image_width - start_x) * image_height;
        if (num_pixels > max_pixels) num_pixels = max_pixels;

        uint32_t width_per_row =
            num_pixels / image_height; // expected: diff_samples
        if (width_per_row == 0) continue;

        for (uint32_t row = 0; row < image_height; row++) {
            uint32_t remaining = width_per_row;
            uint32_t src_x = 0;
            uint32_t dst_x_row = start_x;

            while (remaining > 0) {
                uint32_t row_space = image_width - dst_x_row;
                uint32_t chunk =
                    (remaining < row_space) ? remaining : row_space;

                VkImageCopy copy_region = {
                    .srcSubresource = subresource,
                    .srcOffset = {(int32_t)src_x, (int32_t)row, 0},
                    .dstSubresource = subresource,
                    .dstOffset = {(int32_t)dst_x_row, (int32_t)row, 0},
                    .extent = {chunk, 1, 1},
                };

                vkCmdCopyImage(cmd,
                               src_img,
                               ctx_new_vulkan->image_layout[idx_src[i]],
                               dst_img,
                               ctx_new_vulkan->image_layout[idx_dst[i]],
                               1,
                               &copy_region);

                remaining -= chunk;
                src_x += chunk;
                dst_x_row += chunk;
                if (dst_x_row >= image_width) {
                    // stop if we run out of horizontal space
                    remaining = 0;
                }
            }
        }
        continue;
    }
    war_label_copy_buffer_image: {
        VkBuffer src_buffer = ctx_new_vulkan->buffer[idx_src[i]];
        VkImage dst_img = ctx_new_vulkan->image[idx_dst[i]];
        VkBufferImageCopy buffer_image_copy = {
            .bufferOffset = 0,
            .bufferRowLength = 0, // tightly packed
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageOffset = {0, 0, 0},
            .imageExtent =
                {
                    .width = size[i],
                    .height = size_2[i],
                    .depth = 1,
                },
        };
        vkCmdCopyBufferToImage(cmd,
                               src_buffer,
                               dst_img,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &buffer_image_copy);
        continue;
    }
    }
}

static inline void
war_new_vulkan_barrier(uint32_t idx_count,
                       uint32_t* idx,
                       VkDeviceSize* dst_offset,
                       VkDeviceSize* dst_size,
                       VkPipelineStageFlags* dst_stage,
                       VkAccessFlags* dst_access,
                       VkImageLayout* dst_image_layout,
                       VkCommandBuffer cmd,
                       war_new_vulkan_context* ctx_new_vulkan) {
    for (uint32_t i = 0; i < idx_count; i++) {
        if (ctx_new_vulkan->image[idx[i]]) { goto war_label_image_barrier; }
        ctx_new_vulkan->buffer_memory_barrier[i] = (VkBufferMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = ctx_new_vulkan->access_flags[idx[i]],
            .dstAccessMask = dst_access[i],
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = ctx_new_vulkan->buffer[idx[i]],
            .offset = dst_offset ? dst_offset[i] : 0,
            .size = dst_size ? dst_size[i] : VK_WHOLE_SIZE,
        };
        vkCmdPipelineBarrier(cmd,
                             ctx_new_vulkan->pipeline_stage_flags[idx[i]],
                             dst_stage[i],
                             0, // dependencyFlags
                             0,
                             NULL, // memory barriers
                             1,
                             &ctx_new_vulkan->buffer_memory_barrier[i],
                             0,
                             NULL); // image barriers
        ctx_new_vulkan->access_flags[idx[i]] = dst_access[i];
        ctx_new_vulkan->pipeline_stage_flags[idx[i]] = dst_stage[i];
        continue;
    war_label_image_barrier:
        ctx_new_vulkan->image_memory_barrier[i] = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = ctx_new_vulkan->access_flags[idx[i]],
            .dstAccessMask = dst_access[i],
            .oldLayout = ctx_new_vulkan->image_layout[idx[i]],
            .newLayout = dst_image_layout[i],
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = ctx_new_vulkan->image[idx[i]],
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        vkCmdPipelineBarrier(cmd,
                             ctx_new_vulkan->pipeline_stage_flags[idx[i]],
                             dst_stage[i],
                             0,
                             0,
                             NULL,
                             0,
                             NULL,
                             1,
                             &ctx_new_vulkan->image_memory_barrier[i]);
        ctx_new_vulkan->image_layout[idx[i]] = dst_image_layout[i];
        ctx_new_vulkan->access_flags[idx[i]] = dst_access[i];
        ctx_new_vulkan->pipeline_stage_flags[idx[i]] = dst_stage[i];
    }
}

static inline void
war_new_vulkan_clear(uint32_t idx_count,
                     uint32_t* idx,
                     VkDeviceSize* dst_offset,
                     VkDeviceSize* dst_size,
                     VkCommandBuffer cmd,
                     war_new_vulkan_context* ctx_new_vulkan) {
    for (uint32_t i = 0; i < idx_count; i++) {
        if (ctx_new_vulkan->image[idx[i]]) { goto war_label_clear_image; }
        VkDeviceSize dst_offset_temp = dst_offset ? dst_offset[i] : 0;
        VkDeviceSize dst_size_temp =
            dst_size ? dst_size[i] : ctx_new_vulkan->capacity[idx[i]];
        vkCmdFillBuffer(cmd,
                        ctx_new_vulkan->buffer[idx[i]],
                        dst_offset_temp,
                        dst_size_temp,
                        0);
        continue;
    war_label_clear_image:
        VkClearColorValue clear_color_value;
        clear_color_value.float32[0] = 0.0f;
        clear_color_value.float32[1] = 0.0f;
        clear_color_value.float32[2] = 0.0f;
        clear_color_value.float32[3] = 0.0f;
        VkImageSubresourceRange image_subresource_range;
        image_subresource_range.layerCount = 1;
        image_subresource_range.baseArrayLayer = 0;
        image_subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_subresource_range.baseMipLevel = 0;
        image_subresource_range.levelCount = 1;
        vkCmdClearColorImage(cmd,
                             ctx_new_vulkan->image[idx[i]],
                             ctx_new_vulkan->image_layout[idx[i]],
                             &clear_color_value,
                             1,
                             &image_subresource_range);
    }
}

static inline void
war_new_vulkan_buffer_draw(uint32_t idx_count,
                           uint32_t* pipeline_idx,
                           uint32_t* buffer_idx,
                           VkCommandBuffer cmd,
                           war_new_vulkan_context* ctx_new_vulkan) {
    uint32_t bound_pipeline_idx = UINT32_MAX;
    for (uint32_t i = 0; i < idx_count; i++) {
        uint32_t instance_count =
            ctx_new_vulkan->buffer_instance_count
                [pipeline_idx[i] * ctx_new_vulkan->buffer_max + buffer_idx[i]];
        if (!instance_count ||
            !ctx_new_vulkan
                 ->draw_buffer[pipeline_idx[i] * ctx_new_vulkan->buffer_max +
                               buffer_idx[i]]) {
            continue;
        }
        uint32_t instance_buffer_idx =
            ctx_new_vulkan->pipeline_vertex_input_binding_idx
                [pipeline_idx[i] * ctx_new_vulkan->resource_count + 1];
        uint32_t first_instance =
            ctx_new_vulkan->buffer_first_instance
                [pipeline_idx[i] * ctx_new_vulkan->buffer_max + buffer_idx[i]];
        if (bound_pipeline_idx == pipeline_idx[i]) { goto war_label_draw; }
        bound_pipeline_idx = pipeline_idx[i];
        vkCmdBindPipeline(cmd,
                          ctx_new_vulkan->pipeline_bind_point[pipeline_idx[i]],
                          ctx_new_vulkan->pipeline[pipeline_idx[i]]);
        VkDeviceSize offsets[2] = {0, 0};
        uint32_t vertex_buffer_idx =
            ctx_new_vulkan->pipeline_vertex_input_binding_idx
                [pipeline_idx[i] * ctx_new_vulkan->resource_count];
        vkCmdBindVertexBuffers(cmd,
                               vertex_buffer_idx,
                               1,
                               &ctx_new_vulkan->buffer[vertex_buffer_idx],
                               offsets);
        vkCmdBindVertexBuffers(cmd,
                               instance_buffer_idx,
                               1,
                               &ctx_new_vulkan->buffer[instance_buffer_idx],
                               offsets);
        uint32_t descriptor_set_idx =
            ctx_new_vulkan
                ->pipeline_set_idx[pipeline_idx[i] *
                                   ctx_new_vulkan->descriptor_set_count];
        if (descriptor_set_idx != UINT32_MAX) {
            vkCmdBindDescriptorSets(
                cmd,
                ctx_new_vulkan->pipeline_bind_point[pipeline_idx[i]],
                ctx_new_vulkan->pipeline_layout[pipeline_idx[i]],
                0,
                1,
                &ctx_new_vulkan->descriptor_set[descriptor_set_idx],
                0,
                NULL);
        }
    war_label_draw:
        vkCmdSetViewport(
            cmd,
            0,
            1,
            &ctx_new_vulkan->buffer_viewport[pipeline_idx[i]][buffer_idx[i]]);
        vkCmdSetScissor(
            cmd,
            0,
            1,
            &ctx_new_vulkan->buffer_rect_2d[pipeline_idx[i]][buffer_idx[i]]);
        vkCmdPushConstants(
            cmd,
            ctx_new_vulkan->pipeline_layout[pipeline_idx[i]],
            ctx_new_vulkan->push_constant_shader_stage_flags[pipeline_idx[i]],
            0,
            ctx_new_vulkan->push_constant_size[pipeline_idx[i]],
            &ctx_new_vulkan->buffer_push_constant
                 [pipeline_idx[i]]
                 [buffer_idx[i] *
                  ctx_new_vulkan->push_constant_size[pipeline_idx[i]]]);
        // call_king_terry("[DRAW] pip=%u buf=%u inst_buf=%u first=%u count=%u",
        //                 pipeline_idx[i],
        //                 buffer_idx[i],
        //                 instance_buffer_idx,
        //                 first_instance,
        //                 instance_count);
        vkCmdDraw(cmd, 4, instance_count, 0, first_instance);
    }
}

static inline void
war_new_vulkan_buffer_flush_copy(uint32_t idx_count,
                                 uint32_t* pipeline_idx,
                                 uint32_t* buffer_idx,
                                 war_new_vulkan_context* ctx_new_vulkan,
                                 VkDevice device,
                                 VkCommandBuffer cmd_buffer) {
    if (idx_count == 0) { return; }
    for (uint32_t i = 0; i < idx_count; i++) {
        uint32_t pip_idx = pipeline_idx[i];
        uint32_t buffer_idx_2d =
            pip_idx * ctx_new_vulkan->buffer_max + buffer_idx[i];
        uint32_t src_resource_idx =
            ctx_new_vulkan->pipeline_instance_stage_idx[pip_idx];
        uint32_t dst_resource_idx =
            ctx_new_vulkan->pipeline_vertex_input_binding_idx
                [pip_idx * ctx_new_vulkan->resource_count +
                 1]; // 1 for instance
        uint32_t stride = ctx_new_vulkan->stride[dst_resource_idx];
        ctx_new_vulkan->fn_buffer_size[i] =
            ctx_new_vulkan->buffer_instance_count[buffer_idx_2d] * stride;
        if (ctx_new_vulkan->fn_buffer_size[i] == 0) {
            call_king_terry("early continue: size of 0");
            continue;
        }
        ctx_new_vulkan->fn_buffer_src_offset[i] =
            ctx_new_vulkan->buffer_first_instance[buffer_idx_2d] * stride;
        ctx_new_vulkan->fn_buffer_src_resource_idx[i] = src_resource_idx;
        ctx_new_vulkan->fn_buffer_dst_resource_idx[i] = dst_resource_idx;
        ctx_new_vulkan->fn_buffer_access_flags[i] = VK_ACCESS_TRANSFER_READ_BIT;
        ctx_new_vulkan->fn_buffer_pipeline_stage_flags[i] =
            VK_PIPELINE_STAGE_TRANSFER_BIT;
        ctx_new_vulkan->fn_buffer_image_layout[i] = 0;
        // call_king_terry("[COPY PREP] i=%u pip=%u buf=%u src_res=%u dst_res=%u
        // "
        //                 "first=%u stride=%u size=%u src_off=%u",
        //                 i,
        //                 pip_idx,
        //                 buffer_idx[i],
        //                 src_resource_idx,
        //                 dst_resource_idx,
        //                 ctx_new_vulkan->buffer_first_instance[buffer_idx_2d],
        //                 stride,
        //                 ctx_new_vulkan->fn_buffer_size[i],
        //                 ctx_new_vulkan->fn_buffer_src_offset[i]);
    }
    war_new_vulkan_flush(idx_count,
                         ctx_new_vulkan->fn_buffer_src_resource_idx,
                         ctx_new_vulkan->fn_buffer_src_offset,
                         ctx_new_vulkan->fn_buffer_size,
                         device,
                         ctx_new_vulkan);
    war_new_vulkan_barrier(idx_count,
                           ctx_new_vulkan->fn_buffer_src_resource_idx,
                           ctx_new_vulkan->fn_buffer_src_offset,
                           ctx_new_vulkan->fn_buffer_size,
                           ctx_new_vulkan->fn_buffer_pipeline_stage_flags,
                           ctx_new_vulkan->fn_buffer_access_flags,
                           ctx_new_vulkan->fn_buffer_image_layout,
                           cmd_buffer,
                           ctx_new_vulkan);
    for (uint32_t i = 0; i < idx_count; i++) {
        ctx_new_vulkan->fn_buffer_access_flags[i] =
            VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    war_new_vulkan_barrier(idx_count,
                           ctx_new_vulkan->fn_buffer_dst_resource_idx,
                           ctx_new_vulkan->fn_buffer_src_offset,
                           ctx_new_vulkan->fn_buffer_size,
                           ctx_new_vulkan->fn_buffer_pipeline_stage_flags,
                           ctx_new_vulkan->fn_buffer_access_flags,
                           ctx_new_vulkan->fn_buffer_image_layout,
                           cmd_buffer,
                           ctx_new_vulkan);
    war_new_vulkan_copy(idx_count,
                        ctx_new_vulkan->fn_buffer_src_resource_idx,
                        ctx_new_vulkan->fn_buffer_dst_resource_idx,
                        ctx_new_vulkan->fn_buffer_src_offset,
                        ctx_new_vulkan->fn_buffer_src_offset,
                        ctx_new_vulkan->fn_buffer_size,
                        0,
                        cmd_buffer,
                        ctx_new_vulkan);
    for (uint32_t i = 0; i < idx_count; i++) {
        ctx_new_vulkan->fn_buffer_access_flags[i] =
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        ctx_new_vulkan->fn_buffer_pipeline_stage_flags[i] =
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    }
    war_new_vulkan_barrier(idx_count,
                           ctx_new_vulkan->fn_buffer_dst_resource_idx,
                           ctx_new_vulkan->fn_buffer_src_offset,
                           ctx_new_vulkan->fn_buffer_size,
                           ctx_new_vulkan->fn_buffer_pipeline_stage_flags,
                           ctx_new_vulkan->fn_buffer_access_flags,
                           ctx_new_vulkan->fn_buffer_image_layout,
                           cmd_buffer,
                           ctx_new_vulkan);
}

static inline void war_new_vulkan_init(war_new_vulkan_context* ctx_new_vulkan,
                                       war_pool* pool_wr,
                                       war_lua_context* ctx_lua) {
    header("war_new_vulkan_init");
    ctx_new_vulkan->physical_width = 2560;
    ctx_new_vulkan->physical_height = 1600;
    uint32_t instance_extension_count = 0;
    vkEnumerateInstanceExtensionProperties(
        NULL, &instance_extension_count, NULL);
    VkExtensionProperties* instance_extensions_properties =
        malloc(sizeof(VkExtensionProperties) * instance_extension_count);
    vkEnumerateInstanceExtensionProperties(
        NULL, &instance_extension_count, instance_extensions_properties);

    const char* validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    const char* instance_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo =
            &(VkApplicationInfo){
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "WAR",
                .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                .pEngineName = "war-engine",
                .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                .apiVersion = VK_API_VERSION_1_2,
            },
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = validation_layers,
        .enabledExtensionCount =
            sizeof(instance_extensions) / sizeof(instance_extensions[0]),
        .ppEnabledExtensionNames = instance_extensions,
    };
    VkResult result =
        vkCreateInstance(&instance_info, NULL, &ctx_new_vulkan->instance);
    assert(result == VK_SUCCESS);
    enum {
        max_gpu_count = 10,
    };
    uint32_t gpu_count = 0;
    VkPhysicalDevice physical_devices[max_gpu_count];
    vkEnumeratePhysicalDevices(ctx_new_vulkan->instance, &gpu_count, NULL);
    assert(gpu_count != 0 && gpu_count <= max_gpu_count);
    vkEnumeratePhysicalDevices(
        ctx_new_vulkan->instance, &gpu_count, physical_devices);
    ctx_new_vulkan->physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties device_props;
    for (uint32_t i = 0; i < gpu_count; i++) {
        vkGetPhysicalDeviceProperties(physical_devices[i], &device_props);
        call_king_terry("Found GPU %u: %s (vendorID=0x%x, deviceID=0x%x)",
                        i,
                        device_props.deviceName,
                        device_props.vendorID,
                        device_props.deviceID);
        if (device_props.vendorID == 0x8086) {
            ctx_new_vulkan->physical_device = physical_devices[i];
            call_king_terry("Selected Intel GPU: %s", device_props.deviceName);
            break;
        }
    }
    if (ctx_new_vulkan->physical_device == VK_NULL_HANDLE) {
        ctx_new_vulkan->physical_device = physical_devices[0];
        vkGetPhysicalDeviceProperties(ctx_new_vulkan->physical_device,
                                      &device_props);
        call_king_terry("Fallback GPU selected: %s (vendorID=0x%x)",
                        device_props.deviceName,
                        device_props.vendorID);
    }
    assert(ctx_new_vulkan->physical_device != VK_NULL_HANDLE);
    uint32_t device_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(
        ctx_new_vulkan->physical_device, NULL, &device_extension_count, NULL);
    VkExtensionProperties* device_extensions_properties =
        malloc(sizeof(VkExtensionProperties) * device_extension_count);
    vkEnumerateDeviceExtensionProperties(ctx_new_vulkan->physical_device,
                                         NULL,
                                         &device_extension_count,
                                         device_extensions_properties);
    const char* device_extensions[] = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
        VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
    };
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(
        ctx_new_vulkan->physical_device, NULL, &extension_count, NULL);
    VkExtensionProperties* available_extensions =
        malloc(sizeof(VkExtensionProperties) * extension_count);
    vkEnumerateDeviceExtensionProperties(ctx_new_vulkan->physical_device,
                                         NULL,
                                         &extension_count,
                                         available_extensions);
    uint8_t has_external_memory = 0;
    uint8_t has_external_memory_fd = 0;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(available_extensions[i].extensionName,
                   VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0) {
            has_external_memory = 1;
        }
        if (strcmp(available_extensions[i].extensionName,
                   VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) == 0) {
            has_external_memory_fd = 1;
        }
    }
    free(available_extensions);
    assert(has_external_memory && has_external_memory_fd);
    enum {
        max_family_count = 16,
    };
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        ctx_new_vulkan->physical_device, &queue_family_count, NULL);
    if (queue_family_count > max_family_count) {
        queue_family_count = max_family_count;
    }
    VkQueueFamilyProperties queue_families[max_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(
        ctx_new_vulkan->physical_device, &queue_family_count, queue_families);
    uint32_t graphics_family_index = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_index = i;
        }
    }
    assert(graphics_family_index != UINT32_MAX);
    float queue_priority = 1.0f;
    VkDeviceCreateInfo device_info;
    VkDeviceQueueCreateInfo* queue_infos;
    queue_infos = (VkDeviceQueueCreateInfo[1]){
        (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = ctx_new_vulkan->queue_family_index,
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        },
    };
    VkPhysicalDeviceVulkan12Features physical_device_vulkan_12_features = {0};
    physical_device_vulkan_12_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    physical_device_vulkan_12_features.timelineSemaphore = VK_TRUE;
    device_info = (VkDeviceCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = queue_infos,
        .enabledExtensionCount =
            sizeof(device_extensions) / sizeof(device_extensions[0]),
        .ppEnabledExtensionNames = device_extensions,
        .pNext = &physical_device_vulkan_12_features,
    };
    result = vkCreateDevice(ctx_new_vulkan->physical_device,
                            &device_info,
                            NULL,
                            &ctx_new_vulkan->device);
    assert(result == VK_SUCCESS);
    ctx_new_vulkan->get_semaphore_fd_khr =
        (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(ctx_new_vulkan->device,
                                                     "vkGetSemaphoreFdKHR");
    assert(ctx_new_vulkan->get_semaphore_fd_khr &&
           "failed to load vkGetSemaphoreFdKHR");
    ctx_new_vulkan->import_semaphore_fd_khr =
        (PFN_vkImportSemaphoreFdKHR)vkGetDeviceProcAddr(
            ctx_new_vulkan->device, "vkImportSemaphoreFdKHR");
    assert(ctx_new_vulkan->get_semaphore_fd_khr &&
           "failed to load vkImportSemaphoreFdKHR");
    VkFormat quad_depth_format = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo quad_depth_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = quad_depth_format,
        .extent = {ctx_new_vulkan->physical_width,
                   ctx_new_vulkan->physical_height,
                   1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage quad_depth_image;
    result = vkCreateImage(ctx_new_vulkan->device,
                           &quad_depth_image_info,
                           NULL,
                           &quad_depth_image);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements quad_depth_mem_reqs;
    vkGetImageMemoryRequirements(
        ctx_new_vulkan->device, quad_depth_image, &quad_depth_mem_reqs);
    VkPhysicalDeviceMemoryProperties quad_depth_mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_new_vulkan->physical_device,
                                        &quad_depth_mem_props);
    int quad_depth_memory_type_index = -1;
    for (uint32_t i = 0; i < quad_depth_mem_props.memoryTypeCount; i++) {
        if ((quad_depth_mem_reqs.memoryTypeBits & (1u << i)) &&
            (quad_depth_mem_props.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            quad_depth_memory_type_index = i;
            break;
        }
    }
    assert(quad_depth_memory_type_index != -1);
    VkMemoryAllocateInfo quad_depth_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quad_depth_mem_reqs.size,
        .memoryTypeIndex = quad_depth_memory_type_index,
    };
    VkDeviceMemory quad_depth_image_memory;
    result = vkAllocateMemory(ctx_new_vulkan->device,
                              &quad_depth_alloc_info,
                              NULL,
                              &quad_depth_image_memory);
    assert(result == VK_SUCCESS);
    result = vkBindImageMemory(
        ctx_new_vulkan->device, quad_depth_image, quad_depth_image_memory, 0);
    assert(result == VK_SUCCESS);
    // --- create ctx_new_vulkan->image view ---
    VkImageViewCreateInfo quad_depth_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = quad_depth_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = quad_depth_format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    VkImageView quad_depth_image_view;
    result = vkCreateImageView(ctx_new_vulkan->device,
                               &quad_depth_view_info,
                               NULL,
                               &quad_depth_image_view);
    assert(result == VK_SUCCESS);
    VkAttachmentDescription quad_depth_attachment = {
        .format = quad_depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    vkGetDeviceQueue(ctx_new_vulkan->device,
                     ctx_new_vulkan->queue_family_index,
                     0,
                     &ctx_new_vulkan->queue);
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ctx_new_vulkan->queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    result = vkCreateCommandPool(
        ctx_new_vulkan->device, &pool_info, NULL, &ctx_new_vulkan->cmd_pool);
    assert(result == VK_SUCCESS);
    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx_new_vulkan->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    result = vkAllocateCommandBuffers(
        ctx_new_vulkan->device, &cmd_buf_info, &ctx_new_vulkan->cmd_buffer);
    assert(result == VK_SUCCESS);
    VkExternalMemoryImageCreateInfo ext_mem_image_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = &ext_mem_image_info,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = {ctx_new_vulkan->physical_width,
                   ctx_new_vulkan->physical_height,
                   1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    result = vkCreateImage(ctx_new_vulkan->device,
                           &image_create_info,
                           NULL,
                           &ctx_new_vulkan->core_image);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(
        ctx_new_vulkan->device, ctx_new_vulkan->core_image, &mem_reqs);
    VkExportMemoryAllocateInfo export_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_new_vulkan->physical_device,
                                        &mem_properties);
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    uint32_t memory_type = 0;
    uint8_t found_memory_type = 0;
    call_king_terry(
        "Looking for ctx_new_vulkan->memory type with properties: 0x%x",
        properties);
    call_king_terry("Available ctx_new_vulkan->memory types:");
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags =
            mem_properties.memoryTypes[i].propertyFlags;
        call_king_terry("Type %u: flags=0x%x", i, flags);

        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (flags & properties) == properties) {
            call_king_terry("-> Selected ctx_new_vulkan->memory type %u", i);
            memory_type = i;
            found_memory_type = 1;
            break;
        }
    }
    assert(found_memory_type);
    VkMemoryAllocateInfo mem_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &export_alloc_info,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = memory_type,
    };
    result = vkAllocateMemory(ctx_new_vulkan->device,
                              &mem_alloc_info,
                              NULL,
                              &ctx_new_vulkan->core_memory);
    assert(result == VK_SUCCESS);
    result = vkBindImageMemory(ctx_new_vulkan->device,
                               ctx_new_vulkan->core_image,
                               ctx_new_vulkan->core_memory,
                               0);
    assert(result == VK_SUCCESS);
    VkMemoryGetFdInfoKHR get_fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = ctx_new_vulkan->core_memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    int dmabuf_fd = -1;
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(ctx_new_vulkan->device,
                                                  "vkGetMemoryFdKHR");
    result = vkGetMemoryFdKHR(
        ctx_new_vulkan->device, &get_fd_info, &ctx_new_vulkan->dmabuf_fd);
    assert(result == VK_SUCCESS);
    assert(ctx_new_vulkan->dmabuf_fd > 0);
    int flags = fcntl(ctx_new_vulkan->dmabuf_fd, F_GETFD);
    assert(flags != -1);
    VkAttachmentDescription color_attachment = {
        .flags = 0,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    VkAttachmentDescription quad_attachments[2] = {color_attachment,
                                                   quad_depth_attachment};
    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference quad_depth_ref = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pDepthStencilAttachment = &quad_depth_ref,
    };
    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = quad_attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };
    result = vkCreateRenderPass(ctx_new_vulkan->device,
                                &render_pass_info,
                                NULL,
                                &ctx_new_vulkan->render_pass);
    assert(result == VK_SUCCESS);
    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx_new_vulkan->core_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    result = vkCreateImageView(ctx_new_vulkan->device,
                               &image_view_info,
                               NULL,
                               &ctx_new_vulkan->core_image_view);
    assert(result == VK_SUCCESS);
    VkImageView quad_fb_attachments[2] = {ctx_new_vulkan->core_image_view,
                                          quad_depth_image_view};
    VkFramebufferCreateInfo frame_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ctx_new_vulkan->render_pass,
        .attachmentCount = 2,
        .pAttachments = quad_fb_attachments,
        .width = ctx_new_vulkan->physical_width,
        .height = ctx_new_vulkan->physical_height,
        .layers = 1,
    };
    result = vkCreateFramebuffer(ctx_new_vulkan->device,
                                 &frame_buffer_info,
                                 NULL,
                                 &ctx_new_vulkan->frame_buffer);
    assert(result == VK_SUCCESS);
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx_new_vulkan->core_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkImageMemoryBarrier quad_depth_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = quad_depth_image, // <-- the depth VkImage you created
        .subresourceRange =
            {
                .aspectMask =
                    VK_IMAGE_ASPECT_DEPTH_BIT, // use DEPTH_BIT, add STENCIL_BIT
                                               // if format has stencil
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    };
    VkImageMemoryBarrier quad_barriers[2] = {barrier, quad_depth_barrier};
    vkBeginCommandBuffer(
        ctx_new_vulkan->cmd_buffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        });
    vkCmdPipelineBarrier(ctx_new_vulkan->cmd_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         2,
                         quad_barriers);
    vkEndCommandBuffer(ctx_new_vulkan->cmd_buffer);
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx_new_vulkan->cmd_buffer,
    };
    vkQueueSubmit(ctx_new_vulkan->queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_new_vulkan->queue);
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    result = vkCreateFence(
        ctx_new_vulkan->device, &fence_info, NULL, &ctx_new_vulkan->fence);
    assert(result == VK_SUCCESS);
    //-------------------------------------------------------------------------
    // NEW VULKAN INIT
    //-------------------------------------------------------------------------
    vkGetPhysicalDeviceProperties(ctx_new_vulkan->physical_device,
                                  &ctx_new_vulkan->physical_device_properties);
    ctx_new_vulkan->max_image_dimension_2d =
        ctx_new_vulkan->physical_device_properties.limits.maxImageDimension2D;
    ctx_new_vulkan->optimal_buffer_copy_row_pitch_alignment =
        ctx_new_vulkan->physical_device_properties.limits
            .optimalBufferCopyRowPitchAlignment;
    ctx_new_vulkan->descriptor_count = 0;
    ctx_new_vulkan->descriptor_image_count = 0;
    ctx_new_vulkan->config_path_max = atomic_load(&ctx_lua->CONFIG_PATH_MAX);
    ctx_new_vulkan->resource_count =
        atomic_load(&ctx_lua->NEW_VULKAN_RESOURCE_COUNT);
    ctx_new_vulkan->descriptor_set_count =
        atomic_load(&ctx_lua->NEW_VULKAN_DESCRIPTOR_SET_COUNT);
    ctx_new_vulkan->shader_count =
        atomic_load(&ctx_lua->NEW_VULKAN_SHADER_COUNT);
    ctx_new_vulkan->pipeline_count =
        atomic_load(&ctx_lua->NEW_VULKAN_PIPELINE_COUNT);
    ctx_new_vulkan->groups = atomic_load(&ctx_lua->NEW_VULKAN_GROUPS);
    ctx_new_vulkan->note_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_NOTE_INSTANCE_MAX);
    ctx_new_vulkan->text_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_TEXT_INSTANCE_MAX);
    ctx_new_vulkan->line_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_LINE_INSTANCE_MAX);
    ctx_new_vulkan->cursor_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_CURSOR_INSTANCE_MAX);
    ctx_new_vulkan->hud_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_HUD_INSTANCE_MAX);
    ctx_new_vulkan->hud_cursor_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_HUD_CURSOR_INSTANCE_MAX);
    ctx_new_vulkan->hud_text_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_HUD_TEXT_INSTANCE_MAX);
    ctx_new_vulkan->hud_line_instance_max =
        atomic_load(&ctx_lua->NEW_VULKAN_HUD_LINE_INSTANCE_MAX);
    ctx_new_vulkan->atlas_width = atomic_load(&ctx_lua->NEW_VULKAN_ATLAS_WIDTH);
    ctx_new_vulkan->atlas_height =
        atomic_load(&ctx_lua->NEW_VULKAN_ATLAS_HEIGHT);
    ctx_new_vulkan->font_pixel_height =
        atomic_load(&ctx_lua->NEW_VULKAN_FONT_PIXEL_HEIGHT);
    ctx_new_vulkan->glyph_count = atomic_load(&ctx_lua->NEW_VULKAN_GLYPH_COUNT);
    ctx_new_vulkan->sdf_scale = atomic_load(&ctx_lua->NEW_VULKAN_SDF_SCALE);
    ctx_new_vulkan->sdf_padding = atomic_load(&ctx_lua->NEW_VULKAN_SDF_PADDING);
    ctx_new_vulkan->sdf_range = atomic_load(&ctx_lua->NEW_VULKAN_SDF_RANGE);
    ctx_new_vulkan->sdf_large = atomic_load(&ctx_lua->NEW_VULKAN_SDF_LARGE);
    ctx_new_vulkan->buffer_max = atomic_load(&ctx_lua->NEW_VULKAN_BUFFER_MAX);
    ctx_new_vulkan->note_capacity = ctx_new_vulkan->note_instance_max *
                                    sizeof(war_new_vulkan_note_instance);
    ctx_new_vulkan->text_capacity = ctx_new_vulkan->text_instance_max *
                                    sizeof(war_new_vulkan_text_instance);
    ctx_new_vulkan->line_capacity = ctx_new_vulkan->line_instance_max *
                                    sizeof(war_new_vulkan_line_instance);
    ctx_new_vulkan->cursor_capacity = ctx_new_vulkan->cursor_instance_max *
                                      sizeof(war_new_vulkan_cursor_instance);
    ctx_new_vulkan->hud_capacity =
        ctx_new_vulkan->hud_instance_max * sizeof(war_new_vulkan_hud_instance);
    ctx_new_vulkan->hud_cursor_capacity =
        ctx_new_vulkan->cursor_instance_max *
        sizeof(war_new_vulkan_hud_cursor_instance);
    ctx_new_vulkan->hud_text_capacity =
        ctx_new_vulkan->hud_text_instance_max *
        sizeof(war_new_vulkan_hud_text_instance);
    ctx_new_vulkan->hud_line_capacity =
        ctx_new_vulkan->hud_line_instance_max *
        sizeof(war_new_vulkan_hud_line_instance);
    // pool alloc
    ctx_new_vulkan->memory_property_flags = war_pool_alloc(
        pool_wr,
        sizeof(VkMemoryPropertyFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->usage_flags = war_pool_alloc(
        pool_wr, sizeof(VkBufferUsageFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->buffer = war_pool_alloc(
        pool_wr, sizeof(VkBuffer) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->memory_requirements = war_pool_alloc(
        pool_wr, sizeof(VkMemoryRequirements) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->device_memory = war_pool_alloc(
        pool_wr, sizeof(VkDeviceMemory) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->map =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->capacity = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->descriptor_buffer_info = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorBufferInfo) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->descriptor_image_info = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorImageInfo) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->image = war_pool_alloc(
        pool_wr, sizeof(VkImage) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->image_view = war_pool_alloc(
        pool_wr, sizeof(VkImageView) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->format = war_pool_alloc(
        pool_wr, sizeof(VkFormat) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->extent_3d = war_pool_alloc(
        pool_wr, sizeof(VkExtent3D) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->mapped_memory_range = war_pool_alloc(
        pool_wr, sizeof(VkMappedMemoryRange) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->buffer_memory_barrier = war_pool_alloc(
        pool_wr,
        sizeof(VkBufferMemoryBarrier) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->image_memory_barrier = war_pool_alloc(
        pool_wr, sizeof(VkImageMemoryBarrier) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->image_usage_flags = war_pool_alloc(
        pool_wr, sizeof(VkImageUsageFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->shader_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkShaderStageFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->descriptor_set_layout_binding = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorSetLayoutBinding) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->write_descriptor_set = war_pool_alloc(
        pool_wr, sizeof(VkWriteDescriptorSet) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->image_layout = war_pool_alloc(
        pool_wr, sizeof(VkImageLayout) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->access_flags = war_pool_alloc(
        pool_wr, sizeof(VkAccessFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->pipeline_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkPipelineStageFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_src_idx = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_dst_idx = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_size = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_size_2 = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_src_offset = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_dst_offset = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->descriptor_set = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorSet) * ctx_new_vulkan->descriptor_set_count);
    ctx_new_vulkan->descriptor_set_layout = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorSetLayout) * ctx_new_vulkan->descriptor_set_count);
    ctx_new_vulkan->image_descriptor_type = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorType) * ctx_new_vulkan->descriptor_set_count);
    ctx_new_vulkan->descriptor_pool = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorPool) * ctx_new_vulkan->descriptor_set_count);
    ctx_new_vulkan->descriptor_image_layout = war_pool_alloc(
        pool_wr, sizeof(VkImageLayout) * ctx_new_vulkan->descriptor_set_count);
    ctx_new_vulkan->shader_module = war_pool_alloc(
        pool_wr, sizeof(VkShaderModule) * ctx_new_vulkan->shader_count);
    ctx_new_vulkan->pipeline = war_pool_alloc(
        pool_wr, sizeof(VkPipeline) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_layout = war_pool_alloc(
        pool_wr, sizeof(VkPipelineLayout) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_set_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->descriptor_set_count);
    memset(ctx_new_vulkan->pipeline_set_idx,
           UINT32_MAX,
           ctx_new_vulkan->pipeline_count *
               ctx_new_vulkan->descriptor_set_count * sizeof(uint32_t));
    ctx_new_vulkan->pipeline_shader_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->shader_count);
    memset(ctx_new_vulkan->pipeline_shader_idx,
           UINT32_MAX,
           ctx_new_vulkan->pipeline_count * ctx_new_vulkan->shader_count *
               sizeof(uint32_t));
    ctx_new_vulkan->pipeline_shader_stage_create_info = war_pool_alloc(
        pool_wr,
        sizeof(VkPipelineShaderStageCreateInfo) * ctx_new_vulkan->shader_count);
    ctx_new_vulkan->shader_stage_flag_bits = war_pool_alloc(
        pool_wr, sizeof(VkShaderStageFlagBits) * ctx_new_vulkan->shader_count);
    ctx_new_vulkan->shader_path =
        war_pool_alloc(pool_wr,
                       sizeof(char) * ctx_new_vulkan->shader_count *
                           ctx_new_vulkan->config_path_max);
    ctx_new_vulkan->structure_type = war_pool_alloc(
        pool_wr, sizeof(VkStructureType) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->push_constant_shader_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkShaderStageFlags) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->push_constant_size = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_bind_point = war_pool_alloc(
        pool_wr, sizeof(VkPipelineBindPoint) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->size = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_data = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_data_2 = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->pipeline_dispatch_group =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->groups);
    ctx_new_vulkan->pipeline_local_size =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->groups);
    for (uint32_t i = 0;
         i < ctx_new_vulkan->pipeline_count * ctx_new_vulkan->groups;
         i++) {
        ctx_new_vulkan->pipeline_dispatch_group[i] = 1;
        ctx_new_vulkan->pipeline_local_size[i] = 1;
    }
    ctx_new_vulkan->fn_image_layout = war_pool_alloc(
        pool_wr, sizeof(VkImageLayout) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_access_flags = war_pool_alloc(
        pool_wr, sizeof(VkAccessFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->fn_pipeline_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkPipelineStageFlags) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->viewport = war_pool_alloc(
        pool_wr, sizeof(VkViewport) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->rect_2d = war_pool_alloc(
        pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->sampler = war_pool_alloc(
        pool_wr, sizeof(VkSampler) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->in_descriptor_set = war_pool_alloc(
        pool_wr, sizeof(uint8_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->vertex_input_binding_description =
        war_pool_alloc(pool_wr,
                       sizeof(VkVertexInputBindingDescription) *
                           ctx_new_vulkan->resource_count);
    ctx_new_vulkan->vertex_input_attribute_description =
        war_pool_alloc(pool_wr,
                       sizeof(VkVertexInputAttributeDescription*) *
                           ctx_new_vulkan->resource_count);
    ctx_new_vulkan->vertex_input_rate = war_pool_alloc(
        pool_wr, sizeof(VkVertexInputRate) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->stride = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->attribute_count = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->resource_count);
    ctx_new_vulkan->pipeline_vertex_input_binding_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->resource_count *
                           ctx_new_vulkan->pipeline_count);
    memset(ctx_new_vulkan->pipeline_vertex_input_binding_idx,
           UINT32_MAX,
           sizeof(uint32_t) * ctx_new_vulkan->resource_count *
               ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_push_constant =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_depth_stencil_state_create_info =
        war_pool_alloc(pool_wr,
                       sizeof(VkPipelineDepthStencilStateCreateInfo) *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_color_blend_state_create_info =
        war_pool_alloc(pool_wr,
                       sizeof(VkPipelineColorBlendStateCreateInfo) *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_color_blend_attachment_state =
        war_pool_alloc(pool_wr,
                       sizeof(VkPipelineColorBlendAttachmentState) *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->pipeline_instance_stage_idx = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->z_layer =
        war_pool_alloc(pool_wr, sizeof(float) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->dirty_buffer =
        war_pool_alloc(pool_wr,
                       sizeof(uint8_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->draw_buffer =
        war_pool_alloc(pool_wr,
                       sizeof(uint8_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    for (uint32_t i = 0;
         i < ctx_new_vulkan->buffer_max * ctx_new_vulkan->pipeline_count;
         i++) {
        ctx_new_vulkan->draw_buffer[i] = 1;
    }
    ctx_new_vulkan->fn_buffer_pipeline_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_buffer_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_buffer_first_instance =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_buffer_instance_count =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_buffer_size =
        war_pool_alloc(pool_wr,
                       sizeof(VkDeviceSize) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->fn_buffer_src_offset =
        war_pool_alloc(pool_wr,
                       sizeof(VkDeviceSize) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->fn_buffer_access_flags =
        war_pool_alloc(pool_wr,
                       sizeof(VkAccessFlags) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->fn_buffer_pipeline_stage_flags = war_pool_alloc(
        pool_wr,
        sizeof(VkPipelineStageFlags) * ctx_new_vulkan->pipeline_count *
            ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->fn_buffer_image_layout =
        war_pool_alloc(pool_wr,
                       sizeof(VkImageLayout) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_instance_count =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_first_instance =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->pipeline_count *
                           ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->fn_pipeline_idx = war_pool_alloc(
        pool_wr, sizeof(uint32_t) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_pan_x = war_pool_alloc(
        pool_wr, sizeof(double) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_pan_y = war_pool_alloc(
        pool_wr, sizeof(double) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_pan_factor_x = war_pool_alloc(
        pool_wr, sizeof(double) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_pan_factor_y = war_pool_alloc(
        pool_wr, sizeof(double) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_buffer_src_resource_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->fn_buffer_dst_resource_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_new_vulkan->buffer_max *
                           ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->buffer_push_constant =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->buffer_viewport =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_new_vulkan->pipeline_count);
    ctx_new_vulkan->buffer_rect_2d =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_new_vulkan->pipeline_count);
    //-------------------------------------------------------------------------
    // NEW VULKAN IDX
    //-------------------------------------------------------------------------
    // resource
    ctx_new_vulkan->idx_image_text = 0;
    ctx_new_vulkan->idx_vertex = 1;
    ctx_new_vulkan->idx_note = 2;
    ctx_new_vulkan->idx_text = 3;
    ctx_new_vulkan->idx_line = 4;
    ctx_new_vulkan->idx_cursor = 5;
    ctx_new_vulkan->idx_hud = 6;
    ctx_new_vulkan->idx_hud_cursor = 7;
    ctx_new_vulkan->idx_hud_text = 8;
    ctx_new_vulkan->idx_hud_line = 9;
    ctx_new_vulkan->idx_vertex_stage = 10;
    ctx_new_vulkan->idx_note_stage = 11;
    ctx_new_vulkan->idx_text_stage = 12;
    ctx_new_vulkan->idx_line_stage = 13;
    ctx_new_vulkan->idx_cursor_stage = 14;
    ctx_new_vulkan->idx_hud_stage = 15;
    ctx_new_vulkan->idx_hud_cursor_stage = 16;
    ctx_new_vulkan->idx_hud_text_stage = 17;
    ctx_new_vulkan->idx_hud_line_stage = 18;
    ctx_new_vulkan->idx_image_text_stage = 19;
    // descriptor set
    ctx_new_vulkan->set_idx_text = 0;
    // text
    ctx_new_vulkan->image_descriptor_type[ctx_new_vulkan->set_idx_text] =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ctx_new_vulkan->descriptor_image_layout[ctx_new_vulkan->set_idx_text] =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // shader
    ctx_new_vulkan->shader_idx_vertex_note = 0;
    ctx_new_vulkan->shader_idx_vertex_text = 1;
    ctx_new_vulkan->shader_idx_vertex_line = 2;
    ctx_new_vulkan->shader_idx_vertex_cursor = 3;
    ctx_new_vulkan->shader_idx_vertex_hud = 4;
    ctx_new_vulkan->shader_idx_vertex_hud_cursor = 5;
    ctx_new_vulkan->shader_idx_vertex_hud_text = 6;
    ctx_new_vulkan->shader_idx_vertex_hud_line = 7;
    ctx_new_vulkan->shader_idx_fragment_note = 8;
    ctx_new_vulkan->shader_idx_fragment_text = 9;
    ctx_new_vulkan->shader_idx_fragment_line = 10;
    ctx_new_vulkan->shader_idx_fragment_cursor = 11;
    ctx_new_vulkan->shader_idx_fragment_hud = 12;
    ctx_new_vulkan->shader_idx_fragment_hud_cursor = 13;
    ctx_new_vulkan->shader_idx_fragment_hud_text = 14;
    ctx_new_vulkan->shader_idx_fragment_hud_line = 15;
    // war_new_vulkan_vertex_note
    char* path_vertex_note = "build/spv/war_new_vulkan_vertex_note.spv";
    memcpy(&ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_vertex_note *
                                        ctx_new_vulkan->config_path_max],
           path_vertex_note,
           strlen(path_vertex_note));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_note] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_vertex_text
    char* path_vertex_text = "build/spv/war_new_vulkan_vertex_text.spv";
    memcpy(&ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_vertex_text *
                                        ctx_new_vulkan->config_path_max],
           path_vertex_text,
           strlen(path_vertex_text));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_text] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_vertex_line
    char* path_vertex_line = "build/spv/war_new_vulkan_vertex_line.spv";
    memcpy(&ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_vertex_line *
                                        ctx_new_vulkan->config_path_max],
           path_vertex_line,
           strlen(path_vertex_line));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_line] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_vertex_cursor
    char* path_vertex_cursor = "build/spv/war_new_vulkan_vertex_cursor.spv";
    memcpy(
        &ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_vertex_cursor *
                                     ctx_new_vulkan->config_path_max],
        path_vertex_cursor,
        strlen(path_vertex_cursor));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_cursor] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_vertex_hud
    char* path_vertex_hud = "build/spv/war_new_vulkan_vertex_hud.spv";
    memcpy(&ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_vertex_hud *
                                        ctx_new_vulkan->config_path_max],
           path_vertex_hud,
           strlen(path_vertex_hud));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_hud] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_vertex_hud_cursor
    char* path_vertex_hud_cursor =
        "build/spv/war_new_vulkan_vertex_hud_cursor.spv";
    memcpy(&ctx_new_vulkan
                ->shader_path[ctx_new_vulkan->shader_idx_vertex_hud_cursor *
                              ctx_new_vulkan->config_path_max],
           path_vertex_hud_cursor,
           strlen(path_vertex_hud_cursor));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_hud_cursor] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_vertex_hud_text
    char* path_vertex_hud_text = "build/spv/war_new_vulkan_vertex_hud_text.spv";
    memcpy(&ctx_new_vulkan
                ->shader_path[ctx_new_vulkan->shader_idx_vertex_hud_text *
                              ctx_new_vulkan->config_path_max],
           path_vertex_hud_text,
           strlen(path_vertex_hud_text));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_hud_text] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_vertex_hud_line
    char* path_vertex_hud_line = "build/spv/war_new_vulkan_vertex_hud_line.spv";
    memcpy(&ctx_new_vulkan
                ->shader_path[ctx_new_vulkan->shader_idx_vertex_hud_line *
                              ctx_new_vulkan->config_path_max],
           path_vertex_hud_line,
           strlen(path_vertex_hud_line));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_vertex_hud_line] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_new_vulkan_fragment_note
    char* path_fragment_note = "build/spv/war_new_vulkan_fragment_note.spv";
    memcpy(
        &ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_fragment_note *
                                     ctx_new_vulkan->config_path_max],
        path_fragment_note,
        strlen(path_fragment_note));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_fragment_note] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // war_new_vulkan_fragment_text
    char* path_fragment_text = "build/spv/war_new_vulkan_fragment_text.spv";
    memcpy(
        &ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_fragment_text *
                                     ctx_new_vulkan->config_path_max],
        path_fragment_text,
        strlen(path_fragment_text));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_fragment_text] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // war_new_vulkan_fragment_line
    char* path_fragment_line = "build/spv/war_new_vulkan_fragment_line.spv";
    memcpy(
        &ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_fragment_line *
                                     ctx_new_vulkan->config_path_max],
        path_fragment_line,
        strlen(path_fragment_line));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_fragment_line] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // war_new_vulkan_fragment_cursor
    char* path_fragment_cursor = "build/spv/war_new_vulkan_fragment_cursor.spv";
    memcpy(&ctx_new_vulkan
                ->shader_path[ctx_new_vulkan->shader_idx_fragment_cursor *
                              ctx_new_vulkan->config_path_max],
           path_fragment_cursor,
           strlen(path_fragment_cursor));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_fragment_cursor] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // war_new_vulkan_fragment_hud
    char* path_fragment_hud = "build/spv/war_new_vulkan_fragment_hud.spv";
    memcpy(
        &ctx_new_vulkan->shader_path[ctx_new_vulkan->shader_idx_fragment_hud *
                                     ctx_new_vulkan->config_path_max],
        path_fragment_hud,
        strlen(path_fragment_hud));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_fragment_hud] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // war_new_vulkan_fragment_hud_cursor
    char* path_fragment_hud_cursor =
        "build/spv/war_new_vulkan_fragment_hud_cursor.spv";
    memcpy(&ctx_new_vulkan
                ->shader_path[ctx_new_vulkan->shader_idx_fragment_hud_cursor *
                              ctx_new_vulkan->config_path_max],
           path_fragment_hud_cursor,
           strlen(path_fragment_hud_cursor));
    ctx_new_vulkan->shader_stage_flag_bits
        [ctx_new_vulkan->shader_idx_fragment_hud_cursor] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // war_new_vulkan_fragment_hud_text
    char* path_fragment_hud_text =
        "build/spv/war_new_vulkan_fragment_hud_text.spv";
    memcpy(&ctx_new_vulkan
                ->shader_path[ctx_new_vulkan->shader_idx_fragment_hud_text *
                              ctx_new_vulkan->config_path_max],
           path_fragment_hud_text,
           strlen(path_fragment_hud_text));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_fragment_hud_text] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // war_new_vulkan_fragment_hud_line
    char* path_fragment_hud_line =
        "build/spv/war_new_vulkan_fragment_hud_line.spv";
    memcpy(&ctx_new_vulkan
                ->shader_path[ctx_new_vulkan->shader_idx_fragment_hud_line *
                              ctx_new_vulkan->config_path_max],
           path_fragment_hud_line,
           strlen(path_fragment_hud_line));
    ctx_new_vulkan
        ->shader_stage_flag_bits[ctx_new_vulkan->shader_idx_fragment_hud_line] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // pipeline idx
    ctx_new_vulkan->pipeline_idx_note = 0;
    ctx_new_vulkan->pipeline_idx_text = 1;
    ctx_new_vulkan->pipeline_idx_line = 2;
    ctx_new_vulkan->pipeline_idx_cursor = 3;
    ctx_new_vulkan->pipeline_idx_hud = 4;
    ctx_new_vulkan->pipeline_idx_hud_cursor = 5;
    ctx_new_vulkan->pipeline_idx_hud_text = 6;
    ctx_new_vulkan->pipeline_idx_hud_line = 7;
    // pipeline note
    ctx_new_vulkan->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_note] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_note *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = UINT32_MAX;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_note *
                                            ctx_new_vulkan->shader_count +
                                        0] =
        ctx_new_vulkan->shader_idx_vertex_note;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_note *
                                            ctx_new_vulkan->shader_count +
                                        1] =
        ctx_new_vulkan->shader_idx_fragment_note;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_note] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan
        ->push_constant_shader_stage_flags[ctx_new_vulkan->pipeline_idx_note] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->push_constant_size[ctx_new_vulkan->pipeline_idx_note] =
        sizeof(war_new_vulkan_note_push_constant);
    ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_note] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan
                    ->push_constant_size[ctx_new_vulkan->pipeline_idx_note] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_note] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_note] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_note *
                                                ctx_new_vulkan->resource_count +
                                            0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_note *
                                                ctx_new_vulkan->resource_count +
                                            1] = ctx_new_vulkan->idx_note;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_note] =
        ctx_new_vulkan->idx_note_stage;
    ctx_new_vulkan->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_note] =
        &ctx_new_vulkan->push_constant_note;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_note].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_note].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_note].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_note].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_note].minDepth = 0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_note].maxDepth = 1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_note].offset.x = 0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_note].extent.width =
        (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_note].extent.height =
        (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_note]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_note]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_note]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_note]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_note]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_note]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_note]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_note]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_note]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_note];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_note] = 0.1f;
    // pipeline text
    ctx_new_vulkan->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_text] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_text *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = ctx_new_vulkan->set_idx_text;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_text *
                                            ctx_new_vulkan->shader_count +
                                        0] =
        ctx_new_vulkan->shader_idx_vertex_text;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_text *
                                            ctx_new_vulkan->shader_count +
                                        1] =
        ctx_new_vulkan->shader_idx_fragment_text;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_text] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan
        ->push_constant_shader_stage_flags[ctx_new_vulkan->pipeline_idx_text] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->push_constant_size[ctx_new_vulkan->pipeline_idx_text] =
        sizeof(war_new_vulkan_text_push_constant);
    ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_text] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan
                    ->push_constant_size[ctx_new_vulkan->pipeline_idx_text] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_text] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_text] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_text *
                                                ctx_new_vulkan->resource_count +
                                            0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_text *
                                                ctx_new_vulkan->resource_count +
                                            1] = ctx_new_vulkan->idx_text;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_text] =
        ctx_new_vulkan->idx_text_stage;
    ctx_new_vulkan->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_text] =
        &ctx_new_vulkan->push_constant_text;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_text].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_text].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_text].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_text].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_text].minDepth = 0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_text].maxDepth = 1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_text].offset.x = 0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_text].extent.width =
        (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_text].extent.height =
        (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_text]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_text]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_text]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_text]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_text]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_text]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_text]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_text]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_text]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_text];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_text] = 0.1f;
    // pipeline line
    ctx_new_vulkan->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_line] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_line *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = UINT32_MAX;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_line *
                                            ctx_new_vulkan->shader_count +
                                        0] =
        ctx_new_vulkan->shader_idx_vertex_line;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_line *
                                            ctx_new_vulkan->shader_count +
                                        1] =
        ctx_new_vulkan->shader_idx_fragment_line;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_line] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan
        ->push_constant_shader_stage_flags[ctx_new_vulkan->pipeline_idx_line] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->push_constant_size[ctx_new_vulkan->pipeline_idx_line] =
        sizeof(war_new_vulkan_line_push_constant);
    ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_line] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan
                    ->push_constant_size[ctx_new_vulkan->pipeline_idx_line] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_line] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_line] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_line *
                                                ctx_new_vulkan->resource_count +
                                            0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_line *
                                                ctx_new_vulkan->resource_count +
                                            1] = ctx_new_vulkan->idx_line;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_line] =
        ctx_new_vulkan->idx_line_stage;
    ctx_new_vulkan->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_line] =
        &ctx_new_vulkan->push_constant_line;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_line].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_line].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_line].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_line].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_line].minDepth = 0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_line].maxDepth = 1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_line].offset.x = 0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_line].extent.width =
        (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_line].extent.height =
        (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_line]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_line]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_line]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_line]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_line]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_line]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_line]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_line]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_line]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_line];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_line] = 0.1f;
    // pipeline cursor
    ctx_new_vulkan->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_cursor] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_cursor *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = UINT32_MAX;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_cursor *
                                            ctx_new_vulkan->shader_count +
                                        0] =
        ctx_new_vulkan->shader_idx_vertex_cursor;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_cursor *
                                            ctx_new_vulkan->shader_count +
                                        1] =
        ctx_new_vulkan->shader_idx_fragment_cursor;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_cursor] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan->push_constant_shader_stage_flags
        [ctx_new_vulkan->pipeline_idx_cursor] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->push_constant_size[ctx_new_vulkan->pipeline_idx_cursor] =
        sizeof(war_new_vulkan_cursor_push_constant);
    ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_cursor] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan
                    ->push_constant_size[ctx_new_vulkan->pipeline_idx_cursor] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_cursor] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_cursor] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_cursor * ctx_new_vulkan->resource_count +
         0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_cursor * ctx_new_vulkan->resource_count +
         1] = ctx_new_vulkan->idx_cursor;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_cursor] =
        ctx_new_vulkan->idx_cursor_stage;
    ctx_new_vulkan
        ->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_cursor] =
        &ctx_new_vulkan->push_constant_cursor;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_cursor].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_cursor].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_cursor].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_cursor].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_cursor].minDepth =
        0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_cursor].maxDepth =
        1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_cursor].offset.x = 0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_cursor].extent.width =
        (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_cursor].extent.height =
        (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_cursor]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_cursor]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_cursor]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_cursor]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_cursor]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_cursor]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_cursor]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_cursor]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_cursor]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_cursor];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_cursor] = 0.1f;
    // pipeline hud
    ctx_new_vulkan->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_hud] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_hud *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = UINT32_MAX;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud *
                                            ctx_new_vulkan->shader_count +
                                        0] =
        ctx_new_vulkan->shader_idx_vertex_hud;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud *
                                            ctx_new_vulkan->shader_count +
                                        1] =
        ctx_new_vulkan->shader_idx_fragment_hud;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_hud] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan
        ->push_constant_shader_stage_flags[ctx_new_vulkan->pipeline_idx_hud] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->push_constant_size[ctx_new_vulkan->pipeline_idx_hud] =
        sizeof(war_new_vulkan_hud_push_constant);
    ctx_new_vulkan->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan
                    ->push_constant_size[ctx_new_vulkan->pipeline_idx_hud] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_hud *
                                                ctx_new_vulkan->resource_count +
                                            0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan
        ->pipeline_vertex_input_binding_idx[ctx_new_vulkan->pipeline_idx_hud *
                                                ctx_new_vulkan->resource_count +
                                            1] = ctx_new_vulkan->idx_hud;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_hud] =
        ctx_new_vulkan->idx_hud_stage;
    ctx_new_vulkan->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_hud] =
        &ctx_new_vulkan->push_constant_hud;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud].minDepth = 0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud].maxDepth = 1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud].offset.x = 0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud].extent.width =
        (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud].extent.height =
        (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_hud];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_hud] = 0.1f;
    // pipeline hud cursor
    ctx_new_vulkan
        ->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_hud_cursor *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = UINT32_MAX;
    ctx_new_vulkan
        ->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud_cursor *
                                  ctx_new_vulkan->shader_count +
                              0] = ctx_new_vulkan->shader_idx_vertex_hud_cursor;
    ctx_new_vulkan
        ->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud_cursor *
                                  ctx_new_vulkan->shader_count +
                              1] =
        ctx_new_vulkan->shader_idx_fragment_hud_cursor;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan->push_constant_shader_stage_flags
        [ctx_new_vulkan->pipeline_idx_hud_cursor] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan
        ->push_constant_size[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        sizeof(war_new_vulkan_hud_cursor_push_constant);
    ctx_new_vulkan
        ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan->push_constant_size[ctx_new_vulkan
                                                   ->pipeline_idx_hud_cursor] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_hud_cursor *
             ctx_new_vulkan->resource_count +
         0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_hud_cursor *
             ctx_new_vulkan->resource_count +
         1] = ctx_new_vulkan->idx_hud_cursor;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        ctx_new_vulkan->idx_hud_cursor_stage;
    ctx_new_vulkan
        ->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_hud_cursor] =
        &ctx_new_vulkan->push_constant_hud_cursor;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_cursor].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_cursor].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_cursor].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_cursor].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_cursor].minDepth =
        0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_cursor].maxDepth =
        1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_cursor].offset.x =
        0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_cursor]
        .extent.width = (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_cursor]
        .extent.height = (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info
            [ctx_new_vulkan->pipeline_idx_hud_cursor]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info
            [ctx_new_vulkan->pipeline_idx_hud_cursor]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info
            [ctx_new_vulkan->pipeline_idx_hud_cursor]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info
            [ctx_new_vulkan->pipeline_idx_hud_cursor]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud_cursor]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud_cursor]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_cursor]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_cursor]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_cursor]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_hud_cursor];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_hud_cursor] = 0.1f;
    // pipeline hud text
    ctx_new_vulkan->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_hud_text] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_hud_text *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = ctx_new_vulkan->set_idx_text;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud_text *
                                            ctx_new_vulkan->shader_count +
                                        0] =
        ctx_new_vulkan->shader_idx_vertex_hud_text;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud_text *
                                            ctx_new_vulkan->shader_count +
                                        1] =
        ctx_new_vulkan->shader_idx_fragment_hud_text;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_hud_text] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan->push_constant_shader_stage_flags
        [ctx_new_vulkan->pipeline_idx_hud_text] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->push_constant_size[ctx_new_vulkan->pipeline_idx_hud_text] =
        sizeof(war_new_vulkan_hud_text_push_constant);
    ctx_new_vulkan
        ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud_text] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan->push_constant_size[ctx_new_vulkan
                                                   ->pipeline_idx_hud_text] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud_text] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud_text] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_hud_text *
             ctx_new_vulkan->resource_count +
         0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_hud_text *
             ctx_new_vulkan->resource_count +
         1] = ctx_new_vulkan->idx_hud_text;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_hud_text] =
        ctx_new_vulkan->idx_hud_text_stage;
    ctx_new_vulkan
        ->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_hud_text] =
        &ctx_new_vulkan->push_constant_hud_text;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_text].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_text].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_text].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_text].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_text].minDepth =
        0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_text].maxDepth =
        1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_text].offset.x = 0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_text]
        .extent.width = (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_text]
        .extent.height = (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_text]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_text]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_text]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_text]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud_text]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud_text]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_text]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_text]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_text]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_hud_text];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_hud_text] = 0.1f;
    // pipeline hud line
    ctx_new_vulkan->pipeline_bind_point[ctx_new_vulkan->pipeline_idx_hud_line] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_new_vulkan->pipeline_set_idx[ctx_new_vulkan->pipeline_idx_hud_line *
                                         ctx_new_vulkan->descriptor_set_count +
                                     0] = UINT32_MAX;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud_line *
                                            ctx_new_vulkan->shader_count +
                                        0] =
        ctx_new_vulkan->shader_idx_vertex_hud_line;
    ctx_new_vulkan->pipeline_shader_idx[ctx_new_vulkan->pipeline_idx_hud_line *
                                            ctx_new_vulkan->shader_count +
                                        1] =
        ctx_new_vulkan->shader_idx_fragment_hud_line;
    ctx_new_vulkan->structure_type[ctx_new_vulkan->pipeline_idx_hud_line] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_new_vulkan->push_constant_shader_stage_flags
        [ctx_new_vulkan->pipeline_idx_hud_line] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->push_constant_size[ctx_new_vulkan->pipeline_idx_hud_line] =
        sizeof(war_new_vulkan_hud_line_push_constant);
    ctx_new_vulkan
        ->buffer_push_constant[ctx_new_vulkan->pipeline_idx_hud_line] =
        war_pool_alloc(
            pool_wr,
            ctx_new_vulkan->push_constant_size[ctx_new_vulkan
                                                   ->pipeline_idx_hud_line] *
                ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_viewport[ctx_new_vulkan->pipeline_idx_hud_line] =
        war_pool_alloc(pool_wr,
                       sizeof(VkViewport) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->buffer_rect_2d[ctx_new_vulkan->pipeline_idx_hud_line] =
        war_pool_alloc(pool_wr, sizeof(VkRect2D) * ctx_new_vulkan->buffer_max);
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_hud_line *
             ctx_new_vulkan->resource_count +
         0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan->pipeline_vertex_input_binding_idx
        [ctx_new_vulkan->pipeline_idx_hud_line *
             ctx_new_vulkan->resource_count +
         1] = ctx_new_vulkan->idx_hud_line;
    ctx_new_vulkan
        ->pipeline_instance_stage_idx[ctx_new_vulkan->pipeline_idx_hud_line] =
        ctx_new_vulkan->idx_hud_line_stage;
    ctx_new_vulkan
        ->pipeline_push_constant[ctx_new_vulkan->pipeline_idx_hud_line] =
        &ctx_new_vulkan->push_constant_hud_line;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_line].x = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_line].y = 0.0f;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_line].width =
        (float)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_line].height =
        (float)ctx_new_vulkan->physical_height;
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_line].minDepth =
        0.0f,
    ctx_new_vulkan->viewport[ctx_new_vulkan->pipeline_idx_hud_line].maxDepth =
        1.0f,
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_line].offset.x = 0;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_line]
        .extent.width = (uint32_t)ctx_new_vulkan->physical_width;
    ctx_new_vulkan->rect_2d[ctx_new_vulkan->pipeline_idx_hud_line]
        .extent.height = (uint32_t)ctx_new_vulkan->physical_height;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_line]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_line]
        .depthTestEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_line]
        .depthWriteEnable = VK_TRUE;
    ctx_new_vulkan
        ->pipeline_depth_stencil_state_create_info[ctx_new_vulkan
                                                       ->pipeline_idx_hud_line]
        .depthCompareOp = VK_COMPARE_OP_LESS;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud_line]
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    ctx_new_vulkan
        ->pipeline_color_blend_attachment_state[ctx_new_vulkan
                                                    ->pipeline_idx_hud_line]
        .blendEnable = VK_FALSE;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_line]
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_line]
        .attachmentCount = 1;
    ctx_new_vulkan
        ->pipeline_color_blend_state_create_info[ctx_new_vulkan
                                                     ->pipeline_idx_hud_line]
        .pAttachments = &ctx_new_vulkan->pipeline_color_blend_attachment_state
                             [ctx_new_vulkan->pipeline_idx_hud_line];
    ctx_new_vulkan->z_layer[ctx_new_vulkan->pipeline_idx_hud_line] = 0.1f;
    // vertex
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_vertex] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_vertex] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_vertex] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_vertex] =
        4 * sizeof(war_new_vulkan_vertex);
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_vertex] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_vertex] =
        VK_VERTEX_INPUT_RATE_VERTEX;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_vertex] =
        sizeof(war_new_vulkan_vertex);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_vertex] = 1;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_vertex] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_vertex]);
    VkVertexInputAttributeDescription* ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_vertex];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_vertex,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_vertex, pos),
    };
    // note
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_note] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_note] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_note] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_note] =
        ctx_new_vulkan->note_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_note] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_note] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_note] =
        sizeof(war_new_vulkan_note_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_note] = 7;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_note] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_note]);
    ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_note];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_note,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_note_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_note,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_note_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_note,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_note_instance, color),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_note,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_note_instance, outline_color),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_note,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 4,
        .offset = offsetof(war_new_vulkan_note_instance, foreground_color),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_note,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 5,
        .offset =
            offsetof(war_new_vulkan_note_instance, foreground_outline_color),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_note,
        .format = VK_FORMAT_R32_UINT,
        .location = 6,
        .offset = offsetof(war_new_vulkan_note_instance, flags),
    };
    // cursor
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_cursor] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_cursor] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_cursor] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_cursor] =
        ctx_new_vulkan->note_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_cursor] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_cursor] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_cursor] =
        sizeof(war_new_vulkan_cursor_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_cursor] = 7;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_cursor] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_cursor]);
    ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_cursor];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_cursor,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_cursor_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_cursor,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_cursor_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_cursor_instance, color),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_cursor_instance, outline_color),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 4,
        .offset = offsetof(war_new_vulkan_cursor_instance, foreground_color),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 5,
        .offset =
            offsetof(war_new_vulkan_cursor_instance, foreground_outline_color),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_cursor,
        .format = VK_FORMAT_R32_UINT,
        .location = 6,
        .offset = offsetof(war_new_vulkan_cursor_instance, flags),
    };
    // text
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_text] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_text] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_text] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_text] =
        ctx_new_vulkan->text_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_text] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_text] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_text] =
        sizeof(war_new_vulkan_text_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_text] = 12;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_text] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_text]);
    ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_text];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_text_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_text_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_text_instance, uv),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_text_instance, glyph_scale),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 4,
        .offset = offsetof(war_new_vulkan_text_instance, baseline),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 5,
        .offset = offsetof(war_new_vulkan_text_instance, ascent),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 6,
        .offset = offsetof(war_new_vulkan_text_instance, descent),
    };
    ptr_vk_attr_desc[7] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 7,
        .offset = offsetof(war_new_vulkan_text_instance, color),
    };
    ptr_vk_attr_desc[8] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 8,
        .offset = offsetof(war_new_vulkan_text_instance, outline_color),
    };
    ptr_vk_attr_desc[9] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 9,
        .offset = offsetof(war_new_vulkan_text_instance, foreground_color),
    };
    ptr_vk_attr_desc[10] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 10,
        .offset =
            offsetof(war_new_vulkan_text_instance, foreground_outline_color),
    };
    ptr_vk_attr_desc[11] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_text,
        .format = VK_FORMAT_R32_UINT,
        .location = 11,
        .offset = offsetof(war_new_vulkan_text_instance, flags),
    };
    // line
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_line] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_line] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_line] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_line] =
        ctx_new_vulkan->line_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_line] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_line] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_line] =
        sizeof(war_new_vulkan_line_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_line] = 8;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_line] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_line]);
    ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_line];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_line_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_line_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_line_instance, width),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_line_instance, color),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 4,
        .offset = offsetof(war_new_vulkan_line_instance, outline_color),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 5,
        .offset = offsetof(war_new_vulkan_line_instance, foreground_color),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 6,
        .offset =
            offsetof(war_new_vulkan_line_instance, foreground_outline_color),
    };
    ptr_vk_attr_desc[7] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_line,
        .format = VK_FORMAT_R32_UINT,
        .location = 7,
        .offset = offsetof(war_new_vulkan_line_instance, flags),
    };
    // hud
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_hud] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_hud] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud] =
        ctx_new_vulkan->hud_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_hud] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_hud] =
        sizeof(war_new_vulkan_hud_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud] = 7;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_hud] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud]);
    ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_hud];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_hud_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_hud_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_hud_instance, color),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_hud_instance, outline_color),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 4,
        .offset = offsetof(war_new_vulkan_hud_instance, foreground_color),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 5,
        .offset =
            offsetof(war_new_vulkan_hud_instance, foreground_outline_color),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud,
        .format = VK_FORMAT_R32_UINT,
        .location = 6,
        .offset = offsetof(war_new_vulkan_hud_instance, flags),
    };
    // hud cursor
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_hud_cursor] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud_cursor] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_hud_cursor] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud_cursor] =
        ctx_new_vulkan->hud_cursor_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud_cursor] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_hud_cursor] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_hud_cursor] =
        sizeof(war_new_vulkan_hud_cursor_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud_cursor] = 7;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_hud_cursor] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud_cursor]);
    ptr_vk_attr_desc = ctx_new_vulkan->vertex_input_attribute_description
                           [ctx_new_vulkan->idx_hud_cursor];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_cursor,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_hud_cursor_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_cursor,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_hud_cursor_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_hud_cursor_instance, color),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_hud_cursor_instance, outline_color),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 4,
        .offset =
            offsetof(war_new_vulkan_hud_cursor_instance, foreground_color),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_cursor,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 5,
        .offset = offsetof(war_new_vulkan_hud_cursor_instance,
                           foreground_outline_color),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_cursor,
        .format = VK_FORMAT_R32_UINT,
        .location = 6,
        .offset = offsetof(war_new_vulkan_hud_cursor_instance, flags),
    };
    // hud text
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_hud_text] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud_text] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_hud_text] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud_text] =
        ctx_new_vulkan->hud_text_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud_text] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_hud_text] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_hud_text] =
        sizeof(war_new_vulkan_hud_text_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud_text] = 12;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_hud_text] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud_text]);
    ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_hud_text];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_hud_text_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_hud_text_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_hud_text_instance, uv),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_hud_text_instance, glyph_scale),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 4,
        .offset = offsetof(war_new_vulkan_hud_text_instance, baseline),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 5,
        .offset = offsetof(war_new_vulkan_hud_text_instance, ascent),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 6,
        .offset = offsetof(war_new_vulkan_hud_text_instance, descent),
    };
    ptr_vk_attr_desc[7] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 7,
        .offset = offsetof(war_new_vulkan_hud_text_instance, color),
    };
    ptr_vk_attr_desc[8] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 8,
        .offset = offsetof(war_new_vulkan_hud_text_instance, outline_color),
    };
    ptr_vk_attr_desc[9] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 9,
        .offset = offsetof(war_new_vulkan_hud_text_instance, foreground_color),
    };
    ptr_vk_attr_desc[10] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 10,
        .offset = offsetof(war_new_vulkan_hud_text_instance,
                           foreground_outline_color),
    };
    ptr_vk_attr_desc[11] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_text,
        .format = VK_FORMAT_R32_UINT,
        .location = 11,
        .offset = offsetof(war_new_vulkan_hud_text_instance, flags),
    };
    // hud line
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_hud_line] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud_line] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_hud_line] =
        VK_SHADER_STAGE_VERTEX_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud_line] =
        ctx_new_vulkan->hud_line_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud_line] = 0;
    ctx_new_vulkan->vertex_input_rate[ctx_new_vulkan->idx_hud_line] =
        VK_VERTEX_INPUT_RATE_INSTANCE;
    ctx_new_vulkan->stride[ctx_new_vulkan->idx_hud_line] =
        sizeof(war_new_vulkan_hud_line_instance);
    ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud_line] = 8;
    ctx_new_vulkan
        ->vertex_input_attribute_description[ctx_new_vulkan->idx_hud_line] =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               ctx_new_vulkan->attribute_count[ctx_new_vulkan->idx_hud_line]);
    ptr_vk_attr_desc =
        ctx_new_vulkan
            ->vertex_input_attribute_description[ctx_new_vulkan->idx_hud_line];
    ptr_vk_attr_desc[0] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .location = 0,
        .offset = offsetof(war_new_vulkan_hud_line_instance, pos),
    };
    ptr_vk_attr_desc[1] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .location = 1,
        .offset = offsetof(war_new_vulkan_hud_line_instance, size),
    };
    ptr_vk_attr_desc[2] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32_SFLOAT,
        .location = 2,
        .offset = offsetof(war_new_vulkan_hud_line_instance, width),
    };
    ptr_vk_attr_desc[3] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 3,
        .offset = offsetof(war_new_vulkan_hud_line_instance, color),
    };
    ptr_vk_attr_desc[4] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 4,
        .offset = offsetof(war_new_vulkan_hud_line_instance, outline_color),
    };
    ptr_vk_attr_desc[5] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 5,
        .offset = offsetof(war_new_vulkan_hud_line_instance, foreground_color),
    };
    ptr_vk_attr_desc[6] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .location = 6,
        .offset = offsetof(war_new_vulkan_hud_line_instance,
                           foreground_outline_color),
    };
    ptr_vk_attr_desc[7] = (VkVertexInputAttributeDescription){
        .binding = ctx_new_vulkan->idx_hud_line,
        .format = VK_FORMAT_R32_UINT,
        .location = 7,
        .offset = offsetof(war_new_vulkan_hud_line_instance, flags),
    };
    // vertex stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_vertex_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_vertex_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_vertex_stage] =
        sizeof(war_new_vulkan_vertex) * 4;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_vertex_stage] = 0;
    // note stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_note_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_note_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_note_stage] =
        ctx_new_vulkan->note_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_note_stage] = 0;
    // text stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_text_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_text_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_text_stage] =
        ctx_new_vulkan->text_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_text_stage] = 0;
    // line stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_line_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_line_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_line_stage] =
        ctx_new_vulkan->line_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_line_stage] = 0;
    // cursor stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_cursor_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_cursor_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_cursor_stage] =
        ctx_new_vulkan->cursor_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_cursor_stage] = 0;
    // hud stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_hud_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud_stage] =
        ctx_new_vulkan->hud_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud_stage] = 0;
    // hud_cursor stage
    ctx_new_vulkan
        ->memory_property_flags[ctx_new_vulkan->idx_hud_cursor_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud_cursor_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud_cursor_stage] =
        ctx_new_vulkan->hud_cursor_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud_cursor_stage] = 0;
    // hud_text stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_hud_text_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud_text_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud_text_stage] =
        ctx_new_vulkan->hud_text_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud_text_stage] = 0;
    // hud_line stage
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_hud_line_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_hud_line_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_hud_line_stage] =
        ctx_new_vulkan->hud_line_capacity;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_hud_line_stage] = 0;
    // image text stage
    ctx_new_vulkan
        ->memory_property_flags[ctx_new_vulkan->idx_image_text_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_new_vulkan->usage_flags[ctx_new_vulkan->idx_image_text_stage] =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    ctx_new_vulkan->capacity[ctx_new_vulkan->idx_image_text_stage] =
        ctx_new_vulkan->atlas_width * ctx_new_vulkan->atlas_height;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_image_text_stage] = 0;
    // image_text
    ctx_new_vulkan->memory_property_flags[ctx_new_vulkan->idx_image_text] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_new_vulkan->shader_stage_flags[ctx_new_vulkan->idx_image_text] =
        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_new_vulkan->format[ctx_new_vulkan->idx_image_text] = VK_FORMAT_R8_UNORM;
    ctx_new_vulkan->extent_3d[ctx_new_vulkan->idx_image_text] =
        (VkExtent3D){.width = ctx_new_vulkan->atlas_width,
                     .height = ctx_new_vulkan->atlas_height,
                     .depth = 1};
    ctx_new_vulkan->image_usage_flags[ctx_new_vulkan->idx_image_text] =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ctx_new_vulkan->in_descriptor_set[ctx_new_vulkan->idx_image_text] = 1;
    // sampler text
    VkSamplerCreateInfo graphics_sampler_info = {0};
    graphics_sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    graphics_sampler_info.magFilter =
        VK_FILTER_LINEAR; // magnification (upsampling)
    graphics_sampler_info.minFilter =
        VK_FILTER_LINEAR; // minification (downsampling)
    graphics_sampler_info.mipmapMode =
        VK_SAMPLER_MIPMAP_MODE_LINEAR; // optional
    graphics_sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    graphics_sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    graphics_sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    graphics_sampler_info.mipLodBias = 0.0f;
    graphics_sampler_info.anisotropyEnable = VK_FALSE;
    graphics_sampler_info.maxAnisotropy = 1.0f;
    graphics_sampler_info.compareEnable = VK_FALSE;
    graphics_sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    graphics_sampler_info.unnormalizedCoordinates =
        VK_FALSE; // use [0,1] UV coords
    graphics_sampler_info.minLod = 0.0f;
    graphics_sampler_info.maxLod = 1.0f; // no mipmaps
    result = vkCreateSampler(ctx_new_vulkan->device,
                             &graphics_sampler_info,
                             NULL,
                             &ctx_new_vulkan->sampler);
    assert(result == VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_new_vulkan->physical_device,
                                        &physical_device_memory_properties);
    for (VkDeviceSize i = 0; i < ctx_new_vulkan->resource_count; i++) {
        ctx_new_vulkan->pipeline_stage_flags[i] =
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkImageUsageFlags image_usage_flags =
            ctx_new_vulkan->image_usage_flags[i];
        if ((image_usage_flags &
             (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) != 0) {
            goto image;
        }
        VkMemoryPropertyFlags memory_property_flags =
            ctx_new_vulkan->memory_property_flags[i];
        uint8_t device_local =
            (memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        VkBufferCreateInfo buffer_create_info = {0};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = ctx_new_vulkan->capacity[i];
        buffer_create_info.usage = ctx_new_vulkan->usage_flags[i];
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        result = vkCreateBuffer(ctx_new_vulkan->device,
                                &buffer_create_info,
                                NULL,
                                &ctx_new_vulkan->buffer[i]);
        assert(result == VK_SUCCESS);
        vkGetBufferMemoryRequirements(ctx_new_vulkan->device,
                                      ctx_new_vulkan->buffer[i],
                                      &ctx_new_vulkan->memory_requirements[i]);
        uint32_t memory_type_index = UINT32_MAX;
        VkMemoryRequirements memory_requirements =
            ctx_new_vulkan->memory_requirements[i];
        for (uint32_t j = 0;
             j < physical_device_memory_properties.memoryTypeCount;
             j++) {
            if ((memory_requirements.memoryTypeBits & (1 << j)) &&
                (physical_device_memory_properties.memoryTypes[j]
                     .propertyFlags &
                 ctx_new_vulkan->memory_property_flags[i]) ==
                    ctx_new_vulkan->memory_property_flags[i]) {
                memory_type_index = j;
                break;
            }
        }
        assert(memory_type_index != UINT32_MAX);
        VkMemoryAllocateInfo memory_allocate_info = {0};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type_index;
        result = vkAllocateMemory(ctx_new_vulkan->device,
                                  &memory_allocate_info,
                                  NULL,
                                  &ctx_new_vulkan->device_memory[i]);
        assert(result == VK_SUCCESS);
        result = vkBindBufferMemory(ctx_new_vulkan->device,
                                    ctx_new_vulkan->buffer[i],
                                    ctx_new_vulkan->device_memory[i],
                                    0);
        assert(result == VK_SUCCESS);
        if (!device_local) {
            result = vkMapMemory(ctx_new_vulkan->device,
                                 ctx_new_vulkan->device_memory[i],
                                 0,
                                 ctx_new_vulkan->capacity[i],
                                 0,
                                 &ctx_new_vulkan->map[i]);
            assert(result == VK_SUCCESS);
            continue;
        }
        if (!ctx_new_vulkan->in_descriptor_set[i]) { continue; }
        // descriptor_set_layout_binding
        ctx_new_vulkan->descriptor_set_layout_binding[i].binding = i;
        ctx_new_vulkan->descriptor_set_layout_binding[i].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ctx_new_vulkan->descriptor_set_layout_binding[i].descriptorCount = 1;
        ctx_new_vulkan->descriptor_set_layout_binding[i].stageFlags =
            ctx_new_vulkan->shader_stage_flags[i];
        ctx_new_vulkan->descriptor_count++;
        continue;
    image:
        VkImageCreateInfo image_create_info = {0};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = ctx_new_vulkan->format[i];
        image_create_info.extent = ctx_new_vulkan->extent_3d[i];
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = ctx_new_vulkan->image_usage_flags[i];
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        result = vkCreateImage(ctx_new_vulkan->device,
                               &image_create_info,
                               NULL,
                               &ctx_new_vulkan->image[i]);
        assert(result == VK_SUCCESS);
        vkGetImageMemoryRequirements(ctx_new_vulkan->device,
                                     ctx_new_vulkan->image[i],
                                     &ctx_new_vulkan->memory_requirements[i]);
        memory_requirements = ctx_new_vulkan->memory_requirements[i];
        memory_type_index = UINT32_MAX;
        for (uint32_t j = 0;
             j < physical_device_memory_properties.memoryTypeCount;
             j++) {
            if ((memory_requirements.memoryTypeBits & (1 << j)) &&
                (physical_device_memory_properties.memoryTypes[j]
                     .propertyFlags &
                 ctx_new_vulkan->memory_property_flags[i]) ==
                    ctx_new_vulkan->memory_property_flags[i]) {
                memory_type_index = j;
                break;
            }
        }
        assert(memory_type_index != UINT32_MAX);
        memory_allocate_info = (VkMemoryAllocateInfo){0};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type_index;
        result = vkAllocateMemory(ctx_new_vulkan->device,
                                  &memory_allocate_info,
                                  NULL,
                                  &ctx_new_vulkan->device_memory[i]);
        assert(result == VK_SUCCESS);
        result = vkBindImageMemory(ctx_new_vulkan->device,
                                   ctx_new_vulkan->image[i],
                                   ctx_new_vulkan->device_memory[i],
                                   0);
        assert(result == VK_SUCCESS);
        VkImageViewCreateInfo view_info = {0};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = ctx_new_vulkan->image[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = ctx_new_vulkan->format[i];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        result = vkCreateImageView(ctx_new_vulkan->device,
                                   &view_info,
                                   NULL,
                                   &ctx_new_vulkan->image_view[i]);
        assert(result == VK_SUCCESS);
        if (!ctx_new_vulkan->in_descriptor_set[i]) { continue; }
        // descriptor_set_layout_binding
        ctx_new_vulkan->descriptor_set_layout_binding[i].binding = i;
        ctx_new_vulkan->descriptor_set_layout_binding[i].descriptorCount = 1;
        ctx_new_vulkan->descriptor_set_layout_binding[i].stageFlags =
            ctx_new_vulkan->shader_stage_flags[i];
        ctx_new_vulkan->descriptor_count++;
        ctx_new_vulkan->descriptor_image_count++;
    }
    for (uint32_t i = 0; i < ctx_new_vulkan->descriptor_set_count; i++) {
        for (uint32_t k = 0; k < ctx_new_vulkan->descriptor_count; k++) {
            VkImageUsageFlags image_usage_flags =
                ctx_new_vulkan->image_usage_flags[k];
            if ((image_usage_flags & (VK_IMAGE_USAGE_STORAGE_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT)) != 0) {
                goto image_descriptor_type;
            }
            continue;
        image_descriptor_type:
            ctx_new_vulkan->descriptor_set_layout_binding[k].descriptorType =
                ctx_new_vulkan->image_descriptor_type[i];
        }
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = ctx_new_vulkan->descriptor_count,
            .pBindings = ctx_new_vulkan->descriptor_set_layout_binding,
        };
        result = vkCreateDescriptorSetLayout(
            ctx_new_vulkan->device,
            &descriptor_set_layout_create_info,
            NULL,
            &ctx_new_vulkan->descriptor_set_layout[i]);
        assert(result == VK_SUCCESS);
        VkDescriptorPoolSize descriptor_pool_size[1] = {
            (VkDescriptorPoolSize){
                .type = ctx_new_vulkan->image_descriptor_type[i],
                .descriptorCount = ctx_new_vulkan->descriptor_image_count,
            },
        };
        VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = 1,
            .pPoolSizes = descriptor_pool_size,
            .maxSets = 1,
        };
        result = vkCreateDescriptorPool(ctx_new_vulkan->device,
                                        &descriptor_pool_create_info,
                                        NULL,
                                        &ctx_new_vulkan->descriptor_pool[i]);
        assert(result == VK_SUCCESS);
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = ctx_new_vulkan->descriptor_pool[i],
            .descriptorSetCount = 1,
            .pSetLayouts = &ctx_new_vulkan->descriptor_set_layout[i],
        };
        result = vkAllocateDescriptorSets(ctx_new_vulkan->device,
                                          &descriptor_set_allocate_info,
                                          &ctx_new_vulkan->descriptor_set[i]);
        assert(result == VK_SUCCESS);
        for (uint32_t k = 0; k < ctx_new_vulkan->descriptor_count; k++) {
            VkImageUsageFlags image_usage_flags =
                ctx_new_vulkan->image_usage_flags[k];
            if ((image_usage_flags & (VK_IMAGE_USAGE_STORAGE_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT)) != 0) {
                goto descriptor_image;
            }
            // descriptor buffer info
            ctx_new_vulkan->descriptor_buffer_info[k].buffer =
                ctx_new_vulkan->buffer[k];
            ctx_new_vulkan->descriptor_buffer_info[k].offset = 0;
            ctx_new_vulkan->descriptor_buffer_info[k].range =
                ctx_new_vulkan->capacity[k];
            /// write descriptor set
            ctx_new_vulkan->write_descriptor_set[k].sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ctx_new_vulkan->write_descriptor_set[k].dstBinding = k;
            ctx_new_vulkan->write_descriptor_set[k].descriptorCount = 1;
            ctx_new_vulkan->write_descriptor_set[k].descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            ctx_new_vulkan->write_descriptor_set[k].pBufferInfo =
                &ctx_new_vulkan->descriptor_buffer_info[k];
            ctx_new_vulkan->write_descriptor_set[k].dstSet =
                ctx_new_vulkan->descriptor_set[i];
            // pipeline stage flags
            ctx_new_vulkan->pipeline_stage_flags[k] =
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            continue;
        descriptor_image:
            // descriptor_image_info
            ctx_new_vulkan->descriptor_image_info[k].imageView =
                ctx_new_vulkan->image_view[k];
            ctx_new_vulkan->descriptor_image_info[k].sampler =
                ctx_new_vulkan->sampler;
            ctx_new_vulkan->descriptor_image_info[k].imageLayout =
                ctx_new_vulkan->descriptor_image_layout[i];
            // write_descriptor_set
            ctx_new_vulkan->write_descriptor_set[k].sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ctx_new_vulkan->write_descriptor_set[k].dstBinding = k;
            ctx_new_vulkan->write_descriptor_set[k].descriptorCount = 1;
            ctx_new_vulkan->write_descriptor_set[k].pImageInfo =
                &ctx_new_vulkan->descriptor_image_info[k];
            ctx_new_vulkan->write_descriptor_set[k].descriptorType =
                ctx_new_vulkan->image_descriptor_type[i];
            ctx_new_vulkan->write_descriptor_set[k].dstSet =
                ctx_new_vulkan->descriptor_set[i];
            // image layout
            ctx_new_vulkan->image_layout[k] = VK_IMAGE_LAYOUT_UNDEFINED;
            // pipeline stage flags
            ctx_new_vulkan->pipeline_stage_flags[k] =
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        vkUpdateDescriptorSets(ctx_new_vulkan->device,
                               ctx_new_vulkan->descriptor_count,
                               ctx_new_vulkan->write_descriptor_set,
                               0,
                               NULL);
    }
    for (uint32_t i = 0; i < ctx_new_vulkan->shader_count; i++) {
        uint8_t fn_result = war_new_vulkan_get_shader_module(
            ctx_new_vulkan->device,
            &ctx_new_vulkan->shader_module[i],
            &ctx_new_vulkan->shader_path[i * ctx_new_vulkan->config_path_max]);
        assert(fn_result);
    }
    uint32_t total_across_pipeline_attribute_count = 0;
    for (uint32_t i = 0; i < ctx_new_vulkan->pipeline_count; i++) {
        for (uint32_t k = 0; k < ctx_new_vulkan->resource_count; k++) {
            uint32_t idx_vertex_input =
                ctx_new_vulkan->pipeline_vertex_input_binding_idx
                    [i * ctx_new_vulkan->resource_count + k];
            if (idx_vertex_input == UINT32_MAX) { break; }
            total_across_pipeline_attribute_count +=
                ctx_new_vulkan->attribute_count[idx_vertex_input];
        }
    }
    VkVertexInputAttributeDescription* vtx_attrs =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               total_across_pipeline_attribute_count);
    for (uint32_t i = 0; i < ctx_new_vulkan->pipeline_count; i++) {
        VkPushConstantRange push_constant_range;
        push_constant_range.stageFlags =
            ctx_new_vulkan->push_constant_shader_stage_flags[i];
        push_constant_range.offset = 0;
        push_constant_range.size = ctx_new_vulkan->push_constant_size[i];
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {0};
        pipeline_layout_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        uint32_t pipeline_set_idx =
            ctx_new_vulkan
                ->pipeline_set_idx[i * ctx_new_vulkan->descriptor_set_count];
        if (pipeline_set_idx == UINT32_MAX) {
            pipeline_layout_create_info.setLayoutCount = 0;
        } else {
            pipeline_layout_create_info.setLayoutCount = 1;
            pipeline_layout_create_info.pSetLayouts =
                &ctx_new_vulkan->descriptor_set_layout[pipeline_set_idx];
        }
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
        result = vkCreatePipelineLayout(ctx_new_vulkan->device,
                                        &pipeline_layout_create_info,
                                        NULL,
                                        &ctx_new_vulkan->pipeline_layout[i]);
        assert(result == VK_SUCCESS);
        uint32_t pipeline_shader_count = 0;
        uint32_t pipeline_shader_idx = 0;
        uint8_t graphics_pipeline = 0;
        for (uint32_t k = 0; k < ctx_new_vulkan->shader_count; k++) {
            uint32_t tmp_pipeline_shader_idx =
                ctx_new_vulkan
                    ->pipeline_shader_idx[i * ctx_new_vulkan->shader_count + k];
            if (tmp_pipeline_shader_idx == UINT32_MAX) { continue; }
            pipeline_shader_idx = tmp_pipeline_shader_idx;
            ctx_new_vulkan
                ->pipeline_shader_stage_create_info[pipeline_shader_count]
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            ctx_new_vulkan
                ->pipeline_shader_stage_create_info[pipeline_shader_count]
                .stage =
                ctx_new_vulkan->shader_stage_flag_bits[pipeline_shader_idx];
            ctx_new_vulkan
                ->pipeline_shader_stage_create_info[pipeline_shader_count]
                .module = ctx_new_vulkan->shader_module[pipeline_shader_idx];
            ctx_new_vulkan
                ->pipeline_shader_stage_create_info[pipeline_shader_count]
                .pName = "main";
            pipeline_shader_count++;
            if (ctx_new_vulkan->shader_stage_flag_bits[pipeline_shader_idx] ==
                VK_SHADER_STAGE_VERTEX_BIT) {
                graphics_pipeline = 1;
            }
        }
        if (graphics_pipeline) { goto graphics_pipeline; }
        // compute pipeline
        assert(pipeline_shader_count == 1 &&
               ctx_new_vulkan->shader_stage_flag_bits[pipeline_shader_idx] ==
                   VK_SHADER_STAGE_COMPUTE_BIT);
        VkComputePipelineCreateInfo compute_pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage =
                (VkPipelineShaderStageCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module =
                        ctx_new_vulkan->shader_module[pipeline_shader_idx],
                    .pName = "main",
                },
            .layout = ctx_new_vulkan->pipeline_layout[i],
        };
        result = vkCreateComputePipelines(ctx_new_vulkan->device,
                                          VK_NULL_HANDLE,
                                          1,
                                          &compute_pipeline_create_info,
                                          NULL,
                                          &ctx_new_vulkan->pipeline[i]);
        assert(result == VK_SUCCESS);
        continue;
    graphics_pipeline:
        uint32_t binding_count = 0;
        uint32_t attribute_count = 0;
        uint32_t binding_location_offset = 0;
        for (uint32_t k = 0; k < ctx_new_vulkan->resource_count; k++) {
            uint32_t idx_vertex_input =
                ctx_new_vulkan->pipeline_vertex_input_binding_idx
                    [i * ctx_new_vulkan->resource_count + k];
            if (idx_vertex_input == UINT32_MAX) { break; }
            ctx_new_vulkan->vertex_input_binding_description[k].binding =
                idx_vertex_input;
            ctx_new_vulkan->vertex_input_binding_description[k].inputRate =
                ctx_new_vulkan->vertex_input_rate[idx_vertex_input];
            ctx_new_vulkan->vertex_input_binding_description[k].stride =
                ctx_new_vulkan->stride[idx_vertex_input];
            binding_count++;
            for (uint32_t j = 0;
                 j < ctx_new_vulkan->attribute_count[idx_vertex_input];
                 j++) {
                vtx_attrs[attribute_count] =
                    ctx_new_vulkan
                        ->vertex_input_attribute_description[idx_vertex_input]
                                                            [j];
                vtx_attrs[attribute_count].location += binding_location_offset;
                attribute_count++;
            }
            binding_location_offset +=
                ctx_new_vulkan->attribute_count[idx_vertex_input];
        }
        VkPipelineVertexInputStateCreateInfo
            graphics_pipeline_vertex_input_state_create_info = {
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = binding_count,
                .pVertexBindingDescriptions =
                    ctx_new_vulkan->vertex_input_binding_description,
                .vertexAttributeDescriptionCount = attribute_count,
                .pVertexAttributeDescriptions = vtx_attrs,
            };
        if (!binding_count) {
            graphics_pipeline_vertex_input_state_create_info =
                (VkPipelineVertexInputStateCreateInfo){0};
        }
        VkPipelineInputAssemblyStateCreateInfo
            graphics_pipeline_input_assembly_state_create_info = {0};
        graphics_pipeline_input_assembly_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        graphics_pipeline_input_assembly_state_create_info.topology =
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        VkPipelineViewportStateCreateInfo
            graphics_pipeline_viewport_state_create_info = {0};
        graphics_pipeline_viewport_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        graphics_pipeline_viewport_state_create_info.viewportCount = 1;
        graphics_pipeline_viewport_state_create_info.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo
            graphics_pipeline_rasterization_state_create_info = {0};
        graphics_pipeline_rasterization_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        graphics_pipeline_rasterization_state_create_info.polygonMode =
            VK_POLYGON_MODE_FILL;
        graphics_pipeline_rasterization_state_create_info.cullMode =
            VK_CULL_MODE_NONE;
        graphics_pipeline_rasterization_state_create_info.frontFace =
            VK_FRONT_FACE_COUNTER_CLOCKWISE;
        graphics_pipeline_rasterization_state_create_info.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo
            graphics_pipeline_multisample_state_create_info = {0};
        graphics_pipeline_multisample_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        graphics_pipeline_multisample_state_create_info.rasterizationSamples =
            VK_SAMPLE_COUNT_1_BIT;
        VkDynamicState graphics_dynamic_state[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                   VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo
            graphics_pipeline_dynamic_state_create_info = {0};
        graphics_pipeline_dynamic_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        graphics_pipeline_dynamic_state_create_info.dynamicStateCount = 2;
        graphics_pipeline_dynamic_state_create_info.pDynamicStates =
            graphics_dynamic_state;
        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = pipeline_shader_count,
            .pStages = ctx_new_vulkan->pipeline_shader_stage_create_info,
            .layout = ctx_new_vulkan->pipeline_layout[i],
            .pVertexInputState =
                &graphics_pipeline_vertex_input_state_create_info,
            .pInputAssemblyState =
                &graphics_pipeline_input_assembly_state_create_info,
            .pViewportState = &graphics_pipeline_viewport_state_create_info,
            .pRasterizationState =
                &graphics_pipeline_rasterization_state_create_info,
            .pMultisampleState =
                &graphics_pipeline_multisample_state_create_info,
            .pDepthStencilState =
                &ctx_new_vulkan->pipeline_depth_stencil_state_create_info[i],
            .pColorBlendState =
                &ctx_new_vulkan->pipeline_color_blend_state_create_info[i],
            .pDynamicState = &graphics_pipeline_dynamic_state_create_info,
            .renderPass = ctx_new_vulkan->render_pass,
            .subpass = 0,
        };
        result = vkCreateGraphicsPipelines(ctx_new_vulkan->device,
                                           VK_NULL_HANDLE,
                                           1,
                                           &graphics_pipeline_create_info,
                                           NULL,
                                           &ctx_new_vulkan->pipeline[i]);
        assert(result == VK_SUCCESS);
    }
    free(vtx_attrs);
    //-------------------------------------------------------------------------
    // FREETYPE TEXT SDF (LATER DO PRECOMPUTE 2-PASS)
    //-------------------------------------------------------------------------
    FT_Library ft_library;
    FT_Face ft_regular;
    FT_Init_FreeType(&ft_library);
    FT_New_Face(ft_library, "assets/fonts/FreeMono.otf", 0, &ft_regular);
    FT_Set_Pixel_Sizes(ft_regular, 0, ctx_new_vulkan->font_pixel_height);
    ctx_new_vulkan->ascent = ft_regular->size->metrics.ascender / 64.0f;
    ctx_new_vulkan->descent = ft_regular->size->metrics.descender / 64.0f;
    ctx_new_vulkan->cell_height = ft_regular->size->metrics.height / 64.0f;
    ctx_new_vulkan->line_gap =
        ctx_new_vulkan->cell_height - ctx_new_vulkan->font_pixel_height;
    ctx_new_vulkan->baseline =
        ctx_new_vulkan->ascent + ctx_new_vulkan->line_gap / 2.0f;
    ctx_new_vulkan->cell_width = 0;
    uint8_t* atlas_pixels =
        malloc(ctx_new_vulkan->atlas_width * ctx_new_vulkan->atlas_height);
    memset(atlas_pixels,
           0,
           ctx_new_vulkan->atlas_width * ctx_new_vulkan->atlas_height);
    ctx_new_vulkan->glyph_info = war_pool_alloc(
        pool_wr, sizeof(war_glyph_info) * ctx_new_vulkan->glyph_count);
    int pen_x = 0;
    int pen_y = 0;
    int row_height = 0;
    for (int c = 0; c < (int)ctx_new_vulkan->glyph_count; c++) {
        FT_Load_Char(ft_regular, c, FT_LOAD_RENDER);
        if (c == 'M') {
            call_king_terry("for monospaced fonts");
            ctx_new_vulkan->cell_width = ft_regular->glyph->advance.x / 64.0f;
        }
        FT_Bitmap* bmp = &ft_regular->glyph->bitmap;
        int w = bmp->width;
        int h = bmp->rows;
        if (pen_x + w >= (int)ctx_new_vulkan->atlas_width) {
            pen_x = 0;
            pen_y += row_height + 1;
            row_height = 0;
        }
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                atlas_pixels[(pen_x + x) +
                             (pen_y + y) * ctx_new_vulkan->atlas_width] =
                    bmp->buffer[x + y * bmp->width];
            }
        }
        ctx_new_vulkan->glyph_info[c].advance_x =
            ft_regular->glyph->advance.x / 64.0f;
        ctx_new_vulkan->glyph_info[c].advance_y =
            ft_regular->glyph->advance.y / 64.0f;
        ctx_new_vulkan->glyph_info[c].bearing_x =
            ft_regular->glyph->bitmap_left;
        ctx_new_vulkan->glyph_info[c].bearing_y = ft_regular->glyph->bitmap_top;
        ctx_new_vulkan->glyph_info[c].width = w;
        ctx_new_vulkan->glyph_info[c].height = h;
        ctx_new_vulkan->glyph_info[c].uv_x0 =
            (float)pen_x / ctx_new_vulkan->atlas_width;
        ctx_new_vulkan->glyph_info[c].uv_y0 =
            (float)pen_y / ctx_new_vulkan->atlas_height;
        ctx_new_vulkan->glyph_info[c].uv_x1 =
            (float)(pen_x + w) / ctx_new_vulkan->atlas_width;
        ctx_new_vulkan->glyph_info[c].uv_y1 =
            (float)(pen_y + h) / ctx_new_vulkan->atlas_height;
        ctx_new_vulkan->glyph_info[c].ascent =
            ft_regular->glyph->metrics.horiBearingY / 64.0f;
        ctx_new_vulkan->glyph_info[c].descent =
            (ft_regular->glyph->metrics.height / 64.0f) -
            ctx_new_vulkan->glyph_info[c].ascent;
        pen_x += w + 1;
        if (h > row_height) { row_height = h; }
    }
    assert(ctx_new_vulkan->cell_width != 0);
    for (int c = 0; c < (int)ctx_new_vulkan->glyph_count; c++) {
        ctx_new_vulkan->glyph_info[c].norm_width =
            ctx_new_vulkan->glyph_info[c].width / ctx_new_vulkan->cell_width;
        ctx_new_vulkan->glyph_info[c].norm_height =
            ctx_new_vulkan->glyph_info[c].height / ctx_new_vulkan->cell_height;

        // Distance from bottom of cell to glyph quad bottom
        float glyph_bottom = ctx_new_vulkan->glyph_info[c].ascent -
                             ctx_new_vulkan->glyph_info[c].height;
        ctx_new_vulkan->glyph_info[c].norm_baseline =
            (ctx_new_vulkan->baseline - glyph_bottom) /
            ctx_new_vulkan->cell_height;
        ctx_new_vulkan->glyph_info[c].norm_ascent =
            ctx_new_vulkan->glyph_info[c].ascent / ctx_new_vulkan->cell_height;
        ctx_new_vulkan->glyph_info[c].norm_descent =
            ctx_new_vulkan->glyph_info[c].descent / ctx_new_vulkan->cell_height;
    }
    memcpy(ctx_new_vulkan->map[ctx_new_vulkan->idx_image_text_stage],
           atlas_pixels,
           ctx_new_vulkan->atlas_width * ctx_new_vulkan->atlas_height);
    ctx_new_vulkan->fn_idx_count = 1;
    ctx_new_vulkan->fn_src_idx[0] = ctx_new_vulkan->idx_image_text_stage;
    war_new_vulkan_flush(ctx_new_vulkan->fn_idx_count,
                         ctx_new_vulkan->fn_src_idx,
                         NULL,
                         NULL,
                         ctx_new_vulkan->device,
                         ctx_new_vulkan);
    VkCommandBufferBeginInfo text_cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(ctx_new_vulkan->cmd_buffer, &text_cmd_begin_info);
    ctx_new_vulkan->fn_idx_count = 1;
    ctx_new_vulkan->fn_dst_idx[0] = ctx_new_vulkan->idx_image_text;
    ctx_new_vulkan->fn_src_idx[0] = ctx_new_vulkan->idx_image_text_stage;
    ctx_new_vulkan->fn_pipeline_stage_flags[0] = VK_PIPELINE_STAGE_TRANSFER_BIT;
    ctx_new_vulkan->fn_access_flags[0] = VK_ACCESS_TRANSFER_WRITE_BIT;
    ctx_new_vulkan->fn_image_layout[0] = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    war_new_vulkan_barrier(ctx_new_vulkan->fn_idx_count,
                           ctx_new_vulkan->fn_dst_idx,
                           0,
                           0,
                           ctx_new_vulkan->fn_pipeline_stage_flags,
                           ctx_new_vulkan->fn_access_flags,
                           ctx_new_vulkan->fn_image_layout,
                           ctx_new_vulkan->cmd_buffer,
                           ctx_new_vulkan);
    ctx_new_vulkan->fn_access_flags[0] = VK_ACCESS_TRANSFER_READ_BIT;
    war_new_vulkan_barrier(ctx_new_vulkan->fn_idx_count,
                           ctx_new_vulkan->fn_src_idx,
                           0,
                           0,
                           ctx_new_vulkan->fn_pipeline_stage_flags,
                           ctx_new_vulkan->fn_access_flags,
                           ctx_new_vulkan->fn_image_layout,
                           ctx_new_vulkan->cmd_buffer,
                           ctx_new_vulkan);
    ctx_new_vulkan->fn_size[0] = ctx_new_vulkan->atlas_width;
    ctx_new_vulkan->fn_size_2[0] = ctx_new_vulkan->atlas_height;
    war_new_vulkan_copy(ctx_new_vulkan->fn_idx_count,
                        ctx_new_vulkan->fn_src_idx,
                        ctx_new_vulkan->fn_dst_idx,
                        0,
                        0,
                        ctx_new_vulkan->fn_size,
                        ctx_new_vulkan->fn_size_2,
                        ctx_new_vulkan->cmd_buffer,
                        ctx_new_vulkan);
    ctx_new_vulkan->fn_idx_count = 1;
    ctx_new_vulkan->fn_dst_idx[0] = ctx_new_vulkan->idx_image_text;
    ctx_new_vulkan->fn_pipeline_stage_flags[0] =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    ctx_new_vulkan->fn_access_flags[0] = VK_ACCESS_SHADER_READ_BIT;
    ctx_new_vulkan->fn_image_layout[0] =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    war_new_vulkan_barrier(ctx_new_vulkan->fn_idx_count,
                           ctx_new_vulkan->fn_dst_idx,
                           0,
                           0,
                           ctx_new_vulkan->fn_pipeline_stage_flags,
                           ctx_new_vulkan->fn_access_flags,
                           ctx_new_vulkan->fn_image_layout,
                           ctx_new_vulkan->cmd_buffer,
                           ctx_new_vulkan);
    vkEndCommandBuffer(ctx_new_vulkan->cmd_buffer);
    VkSubmitInfo sdf_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx_new_vulkan->cmd_buffer,
    };
    vkQueueSubmit(ctx_new_vulkan->queue, 1, &sdf_submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_new_vulkan->queue);
    free(atlas_pixels);
    FT_Done_Face(ft_regular);
    FT_Done_FreeType(ft_library);
    //-------------------------------------------------------------------------
    // VERTEX STAGE
    //-------------------------------------------------------------------------
    war_new_vulkan_vertex* ptr_vtx =
        (war_new_vulkan_vertex*)
            ctx_new_vulkan->map[ctx_new_vulkan->idx_vertex_stage];
    ptr_vtx[0] = (war_new_vulkan_vertex){
        .pos = {0, 0},
    };
    ptr_vtx[1] = (war_new_vulkan_vertex){
        .pos = {1, 0},
    };
    ptr_vtx[2] = (war_new_vulkan_vertex){
        .pos = {0, 1},
    };
    ptr_vtx[3] = (war_new_vulkan_vertex){
        .pos = {1, 1},
    };
    ctx_new_vulkan->fn_idx_count = 1;
    ctx_new_vulkan->fn_src_idx[0] = ctx_new_vulkan->idx_vertex_stage;
    ctx_new_vulkan->fn_dst_idx[0] = ctx_new_vulkan->idx_vertex;
    ctx_new_vulkan->fn_pipeline_stage_flags[0] = VK_PIPELINE_STAGE_TRANSFER_BIT;
    ctx_new_vulkan->fn_access_flags[0] = VK_ACCESS_TRANSFER_READ_BIT;
    ctx_new_vulkan->fn_image_layout[0] = 0;
    ctx_new_vulkan->fn_src_offset[0] = 0;
    ctx_new_vulkan->fn_size[0] = sizeof(war_new_vulkan_vertex) * 4;
    war_new_vulkan_flush(ctx_new_vulkan->fn_idx_count,
                         ctx_new_vulkan->fn_src_idx,
                         ctx_new_vulkan->fn_src_offset,
                         ctx_new_vulkan->fn_size,
                         ctx_new_vulkan->device,
                         ctx_new_vulkan);
    VkCommandBufferBeginInfo vertex_cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(ctx_new_vulkan->cmd_buffer, &vertex_cmd_begin_info);
    war_new_vulkan_barrier(ctx_new_vulkan->fn_idx_count,
                           ctx_new_vulkan->fn_src_idx,
                           ctx_new_vulkan->fn_src_offset,
                           ctx_new_vulkan->fn_size,
                           ctx_new_vulkan->fn_pipeline_stage_flags,
                           ctx_new_vulkan->fn_access_flags,
                           ctx_new_vulkan->fn_image_layout,
                           ctx_new_vulkan->cmd_buffer,
                           ctx_new_vulkan);
    ctx_new_vulkan->fn_access_flags[0] = VK_ACCESS_TRANSFER_WRITE_BIT;
    war_new_vulkan_barrier(ctx_new_vulkan->fn_idx_count,
                           ctx_new_vulkan->fn_dst_idx,
                           ctx_new_vulkan->fn_src_offset,
                           ctx_new_vulkan->fn_size,
                           ctx_new_vulkan->fn_pipeline_stage_flags,
                           ctx_new_vulkan->fn_access_flags,
                           ctx_new_vulkan->fn_image_layout,
                           ctx_new_vulkan->cmd_buffer,
                           ctx_new_vulkan);
    war_new_vulkan_copy(ctx_new_vulkan->fn_idx_count,
                        ctx_new_vulkan->fn_src_idx,
                        ctx_new_vulkan->fn_dst_idx,
                        ctx_new_vulkan->fn_src_offset,
                        0,
                        ctx_new_vulkan->fn_size,
                        0,
                        ctx_new_vulkan->cmd_buffer,
                        ctx_new_vulkan);
    ctx_new_vulkan->fn_access_flags[0] = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    ctx_new_vulkan->fn_pipeline_stage_flags[0] =
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    war_new_vulkan_barrier(ctx_new_vulkan->fn_idx_count,
                           ctx_new_vulkan->fn_dst_idx,
                           ctx_new_vulkan->fn_src_offset,
                           ctx_new_vulkan->fn_size,
                           ctx_new_vulkan->fn_pipeline_stage_flags,
                           ctx_new_vulkan->fn_access_flags,
                           ctx_new_vulkan->fn_image_layout,
                           ctx_new_vulkan->cmd_buffer,
                           ctx_new_vulkan);
    vkEndCommandBuffer(ctx_new_vulkan->cmd_buffer);
    VkSubmitInfo vertex_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx_new_vulkan->cmd_buffer,
    };
    vkQueueSubmit(
        ctx_new_vulkan->queue, 1, &vertex_submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_new_vulkan->queue);
    end("war_new_vulkan_init");
}

#endif // WAR_NEW_VULKAN_H
