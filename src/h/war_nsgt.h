//-----------------------------------------------------------------------------
//
// See LICENSE
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_nsgt.h
//-----------------------------------------------------------------------------

#ifndef WAR_NSGT_H
#define WAR_NSGT_H

#include "h/war_data.h"
#include "h/war_debug_macros.h"
#include "h/war_functions.h"
#include "h/war_vulkan.h"

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

static inline void war_nsgt_flush(uint32_t idx_count,
                                  uint32_t* idx,
                                  VkDeviceSize* offset,
                                  VkDeviceSize* size,
                                  VkDevice device,
                                  war_nsgt_context* ctx_nsgt) {
    for (uint32_t i = 0; i < idx_count; i++) {
        VkDeviceSize off = offset ? offset[i] : 0;
        VkDeviceSize sz = size ? size[i] : ctx_nsgt->capacity[idx[i]];
        VkDeviceSize aligned_offset = war_vulkan_align_offset_down(
            off,
            ctx_nsgt->memory_requirements[idx[i]].alignment,
            ctx_nsgt->capacity[idx[i]]);
        VkDeviceSize aligned_size = war_vulkan_align_size_up(
            sz + (off - aligned_offset),
            ctx_nsgt->memory_requirements[idx[i]].alignment,
            ctx_nsgt->capacity[idx[i]] - aligned_offset);
        ctx_nsgt->mapped_memory_range[i] = (VkMappedMemoryRange){
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext = NULL,
            .memory = ctx_nsgt->device_memory[idx[i]],
            .offset = aligned_offset,
            .size = aligned_size,
        };
    }
    VkResult result = vkFlushMappedMemoryRanges(
        device, idx_count, ctx_nsgt->mapped_memory_range);
    assert(result == VK_SUCCESS);
}

static inline void war_nsgt_invalidate(uint32_t idx_count,
                                       uint32_t* idx,
                                       VkDeviceSize* offset,
                                       VkDeviceSize* size,
                                       VkDevice device,
                                       war_nsgt_context* ctx_nsgt) {
    for (uint32_t i = 0; i < idx_count; i++) {
        VkDeviceSize off = offset ? offset[i] : 0;
        VkDeviceSize sz = size ? size[i] : ctx_nsgt->capacity[idx[i]];
        VkDeviceSize aligned_offset = war_vulkan_align_offset_down(
            off,
            ctx_nsgt->memory_requirements[idx[i]].alignment,
            ctx_nsgt->capacity[idx[i]]);
        VkDeviceSize aligned_size = war_vulkan_align_size_up(
            sz + (off - aligned_offset),
            ctx_nsgt->memory_requirements[idx[i]].alignment,
            ctx_nsgt->capacity[idx[i]] - aligned_offset);
        ctx_nsgt->mapped_memory_range[i] = (VkMappedMemoryRange){
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .pNext = NULL,
            .memory = ctx_nsgt->device_memory[idx[i]],
            .offset = aligned_offset,
            .size = aligned_size,
        };
    }
    VkResult result = vkInvalidateMappedMemoryRanges(
        device, idx_count, ctx_nsgt->mapped_memory_range);
    assert(result == VK_SUCCESS);
}

static inline void war_nsgt_copy(uint32_t idx_count,
                                 uint32_t* idx_src,
                                 uint32_t* idx_dst,
                                 VkDeviceSize* src_offset,
                                 VkDeviceSize* dst_offset,
                                 VkDeviceSize* size,
                                 VkCommandBuffer cmd,
                                 war_nsgt_context* ctx_nsgt) {
    for (uint32_t i = 0; i < idx_count; i++) {
        if (ctx_nsgt->image[idx_src[i]]) { goto war_label_copy_image; }
        VkDeviceSize src_offset_temp = src_offset ? src_offset[i] : 0;
        VkDeviceSize dst_offset_temp = dst_offset ? dst_offset[i] : 0;
        VkDeviceSize size_temp = size[i];
        if (!size_temp) {
            src_offset_temp = 0;
            dst_offset_temp = 0;
            VkDeviceSize dst_capacity = ctx_nsgt->capacity[idx_dst[i]];
            size_temp = ctx_nsgt->capacity[idx_src[i]];
            if (size_temp > dst_capacity) { size_temp = dst_capacity; }
        }
        if (dst_offset_temp + size_temp > ctx_nsgt->capacity[idx_dst[i]]) {
            src_offset_temp = 0;
            dst_offset_temp = 0;
            VkDeviceSize dst_capacity = ctx_nsgt->capacity[idx_dst[i]];
            size_temp = ctx_nsgt->capacity[idx_src[i]];
            if (size_temp > dst_capacity) { size_temp = dst_capacity; }
        }
        VkBufferCopy copy = {
            .srcOffset = src_offset_temp,
            .dstOffset = dst_offset_temp,
            .size = size_temp,
        };
        vkCmdCopyBuffer(cmd,
                        ctx_nsgt->buffer[idx_src[i]],
                        ctx_nsgt->buffer[idx_dst[i]],
                        1,
                        &copy);
        continue;
    war_label_copy_image:
        VkImage src_img = ctx_nsgt->image[idx_src[i]];
        VkImage dst_img = ctx_nsgt->image[idx_dst[i]];

        VkImageSubresourceLayers subresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        // Get size in bytes and convert to number of pixels
        VkDeviceSize num_bytes =
            (size && size[i]) ? size[i] : ctx_nsgt->size[idx_src[i]];
        if (num_bytes == 0) continue;

        uint32_t num_pixels = (uint32_t)(num_bytes / sizeof(float));
        if (num_pixels == 0) continue;

        // Compute linear pixel offset in destination
        uint32_t linear_offset =
            (dst_offset ? dst_offset[i] / sizeof(float) : 0);

        // Image dimensions
        uint32_t image_width = ctx_nsgt->frame_capacity;
        uint32_t image_height = ctx_nsgt->bin_capacity;

        if (linear_offset >= image_width * image_height) continue;

        // Clip to image capacity
        uint32_t max_pixels = image_width * image_height - linear_offset;
        if (num_pixels > max_pixels) num_pixels = max_pixels;

        // Convert linear offset → 2D coordinates
        uint32_t dst_x = linear_offset % image_width;
        uint32_t dst_y = linear_offset / image_width;

        // Copy row by row
        while (num_pixels > 0 && dst_y < image_height) {
            uint32_t row_remaining = image_width - dst_x;
            uint32_t copy_width =
                (num_pixels < row_remaining) ? num_pixels : row_remaining;
            uint32_t copy_height = 1; // one row per iteration

            VkOffset3D dst_offset_2d = {(int32_t)dst_x, (int32_t)dst_y, 0};
            VkExtent3D extent = {copy_width, copy_height, 1};

            VkImageCopy copy_region = {
                .srcSubresource = subresource,
                .srcOffset = {0, 0, 0}, // full row copy from src
                .dstSubresource = subresource,
                .dstOffset = dst_offset_2d,
                .extent = extent,
            };

            vkCmdCopyImage(cmd,
                           src_img,
                           ctx_nsgt->image_layout[idx_src[i]],
                           dst_img,
                           ctx_nsgt->image_layout[idx_dst[i]],
                           1,
                           &copy_region);

            num_pixels -= copy_width;
            dst_x = 0;  // next row starts at x=0
            dst_y += 1; // move to next row
        }
    }
}

static inline void war_nsgt_barrier(uint32_t idx_count,
                                    uint32_t* idx,
                                    VkDeviceSize* dst_offset,
                                    VkDeviceSize* dst_size,
                                    VkPipelineStageFlags* dst_stage,
                                    VkAccessFlags* dst_access,
                                    VkImageLayout* dst_image_layout,
                                    VkCommandBuffer cmd,
                                    war_nsgt_context* ctx_nsgt) {
    for (uint32_t i = 0; i < idx_count; i++) {
        if (ctx_nsgt->image[idx[i]]) { goto war_label_image_barrier; }
        ctx_nsgt->buffer_memory_barrier[i] = (VkBufferMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = ctx_nsgt->access_flags[idx[i]],
            .dstAccessMask = dst_access[i],
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = ctx_nsgt->buffer[idx[i]],
            .offset = dst_offset ? dst_offset[i] : 0,
            .size = dst_size ? dst_size[i] : VK_WHOLE_SIZE,
        };
        vkCmdPipelineBarrier(cmd,
                             ctx_nsgt->pipeline_stage_flags[idx[i]],
                             dst_stage[i],
                             0, // dependencyFlags
                             0,
                             NULL, // memory barriers
                             1,
                             &ctx_nsgt->buffer_memory_barrier[i],
                             0,
                             NULL); // image barriers
        ctx_nsgt->access_flags[idx[i]] = dst_access[i];
        ctx_nsgt->pipeline_stage_flags[idx[i]] = dst_stage[i];
        continue;
    war_label_image_barrier:
        ctx_nsgt->image_memory_barrier[i] = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = ctx_nsgt->access_flags[idx[i]],
            .dstAccessMask = dst_access[i],
            .oldLayout = ctx_nsgt->image_layout[idx[i]],
            .newLayout = dst_image_layout[i],
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = ctx_nsgt->image[idx[i]],
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
                             ctx_nsgt->pipeline_stage_flags[idx[i]],
                             dst_stage[i],
                             0,
                             0,
                             NULL,
                             0,
                             NULL,
                             1,
                             &ctx_nsgt->image_memory_barrier[i]);
        ctx_nsgt->image_layout[idx[i]] = dst_image_layout[i];
        ctx_nsgt->access_flags[idx[i]] = dst_access[i];
        ctx_nsgt->pipeline_stage_flags[idx[i]] = dst_stage[i];
    }
}

static inline void war_nsgt_clear(uint32_t idx_count,
                                  uint32_t* idx,
                                  VkDeviceSize* dst_offset,
                                  VkDeviceSize* dst_size,
                                  VkCommandBuffer cmd,
                                  war_nsgt_context* ctx_nsgt) {
    for (uint32_t i = 0; i < idx_count; i++) {
        if (ctx_nsgt->image[idx[i]]) { goto war_label_clear_image; }
        VkDeviceSize dst_offset_temp = dst_offset ? dst_offset[i] : 0;
        VkDeviceSize dst_size_temp =
            dst_size ? dst_size[i] : ctx_nsgt->capacity[idx[i]];
        vkCmdFillBuffer(
            cmd, ctx_nsgt->buffer[idx[i]], dst_offset_temp, dst_size_temp, 0);
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
                             ctx_nsgt->image[idx[i]],
                             ctx_nsgt->image_layout[idx[i]],
                             &clear_color_value,
                             1,
                             &image_subresource_range);
    }
}

static inline void war_nsgt_compute_pipeline_bind_set_dispatch(
    uint32_t idx_pipeline, VkCommandBuffer cmd, war_nsgt_context* ctx_nsgt) {
    uint32_t descriptor_set_count = 1;
    VkPipelineBindPoint pipeline_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    vkCmdBindPipeline(
        cmd, pipeline_bind_point, ctx_nsgt->pipeline[idx_pipeline]);
    vkCmdBindDescriptorSets(cmd,
                            pipeline_bind_point,
                            ctx_nsgt->pipeline_layout[idx_pipeline],
                            0,
                            descriptor_set_count,
                            &ctx_nsgt->descriptor_set[idx_pipeline],
                            0,
                            NULL);
    vkCmdPushConstants(cmd,
                       ctx_nsgt->pipeline_layout[idx_pipeline],
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       ctx_nsgt->push_constant_size[idx_pipeline],
                       &ctx_nsgt->compute_push_constant);
    vkCmdDispatch(
        cmd,
        ctx_nsgt->pipeline_dispatch_group[idx_pipeline * ctx_nsgt->groups + 0],
        ctx_nsgt->pipeline_dispatch_group[idx_pipeline * ctx_nsgt->groups + 1],
        ctx_nsgt->pipeline_dispatch_group[idx_pipeline * ctx_nsgt->groups + 2]);
}

static inline void war_nsgt_compute_pipeline_dispatch(
    uint32_t idx_pipeline, VkCommandBuffer cmd, war_nsgt_context* ctx_nsgt) {
    VkPipelineBindPoint pipeline_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    vkCmdBindPipeline(
        cmd, pipeline_bind_point, ctx_nsgt->pipeline[idx_pipeline]);
    vkCmdPushConstants(cmd,
                       ctx_nsgt->pipeline_layout[idx_pipeline],
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       ctx_nsgt->push_constant_size[idx_pipeline],
                       &ctx_nsgt->compute_push_constant);
    vkCmdDispatch(
        cmd,
        ctx_nsgt->pipeline_dispatch_group[idx_pipeline * ctx_nsgt->groups + 0],
        ctx_nsgt->pipeline_dispatch_group[idx_pipeline * ctx_nsgt->groups + 1],
        ctx_nsgt->pipeline_dispatch_group[idx_pipeline * ctx_nsgt->groups + 2]);
}

static inline void war_nsgt_draw(VkCommandBuffer cmd,
                                 war_nsgt_context* ctx_nsgt) {
    vkCmdBindPipeline(
        cmd,
        ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_graphics],
        ctx_nsgt->pipeline[ctx_nsgt->pipeline_idx_graphics]);
    vkCmdBindDescriptorSets(
        cmd,
        ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_graphics],
        ctx_nsgt->pipeline_layout[ctx_nsgt->pipeline_idx_graphics],
        0,
        1,
        &ctx_nsgt->descriptor_set[ctx_nsgt->set_idx_graphics],
        0,
        NULL);
    vkCmdSetViewport(cmd, 0, 1, &ctx_nsgt->graphics_viewport);
    vkCmdSetScissor(cmd, 0, 1, &ctx_nsgt->graphics_rect_2d);
    vkCmdPushConstants(
        cmd,
        ctx_nsgt->pipeline_layout[ctx_nsgt->pipeline_idx_graphics],
        ctx_nsgt
            ->push_constant_shader_stage_flags[ctx_nsgt->pipeline_idx_graphics],
        0,
        ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_graphics],
        &ctx_nsgt->graphics_push_constant);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

static inline void war_nsgt_compute(VkDevice device,
                                    VkQueue queue,
                                    VkCommandBuffer cmd,
                                    VkFence fence,
                                    uint8_t* wav,
                                    uint32_t samples_size) {
    VkResult result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    assert(result == VK_SUCCESS);
    result = vkResetFences(device, 1, &fence);
    assert(result == VK_SUCCESS);
    result = vkResetCommandBuffer(cmd, 0);
    assert(result == VK_SUCCESS);
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(cmd, &cmd_buffer_begin_info);
    assert(result == VK_SUCCESS);

    result = vkEndCommandBuffer(cmd);
    assert(result == VK_SUCCESS);
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    result = vkResetFences(device, 1, &fence);
    assert(result == VK_SUCCESS);
    result = vkQueueSubmit(queue, 1, &submit_info, fence);
    assert(result == VK_SUCCESS);
}

static inline void war_nsgt_init(war_nsgt_context* ctx_nsgt,
                                 war_pool* pool_wr,
                                 war_lua_context* ctx_lua,
                                 VkDevice device,
                                 VkPhysicalDevice physical_device,
                                 VkRenderPass render_pass,
                                 VkCommandBuffer cmd_buffer,
                                 VkQueue queue,
                                 VkFence fence,
                                 float physical_width,
                                 float physical_height) {
    header("war_nsgt_init");
    vkGetPhysicalDeviceProperties(physical_device,
                                  &ctx_nsgt->physical_device_properties);
    ctx_nsgt->descriptor_count = 0;
    ctx_nsgt->descriptor_image_count = 0;
    ctx_nsgt->resource_count = atomic_load(&ctx_lua->NSGT_RESOURCE_COUNT);
    ctx_nsgt->descriptor_set_count =
        atomic_load(&ctx_lua->NSGT_DESCRIPTOR_SET_COUNT);
    ctx_nsgt->shader_count = atomic_load(&ctx_lua->NSGT_SHADER_COUNT);
    ctx_nsgt->pipeline_count = atomic_load(&ctx_lua->NSGT_PIPELINE_COUNT);
    ctx_nsgt->path_limit = atomic_load(&ctx_lua->A_PATH_LIMIT);
    ctx_nsgt->bin_capacity = atomic_load(&ctx_lua->NSGT_BIN_CAPACITY);
    ctx_nsgt->frame_capacity = atomic_load(&ctx_lua->NSGT_FRAME_CAPACITY);
    ctx_nsgt->sample_rate = atomic_load(&ctx_lua->A_SAMPLE_RATE);
    ctx_nsgt->sample_duration = atomic_load(&ctx_lua->A_SAMPLE_DURATION);
    ctx_nsgt->channel_count = atomic_load(&ctx_lua->A_CHANNEL_COUNT);
    ctx_nsgt->frequency_min = atomic_load(&ctx_lua->NSGT_FREQUENCY_MIN);
    ctx_nsgt->frequency_max = atomic_load(&ctx_lua->NSGT_FREQUENCY_MAX);
    ctx_nsgt->alpha = atomic_load(&ctx_lua->NSGT_ALPHA);
    ctx_nsgt->window_length_min = atomic_load(&ctx_lua->NSGT_WINDOW_LENGTH_MIN);
    ctx_nsgt->shape_factor = atomic_load(&ctx_lua->NSGT_SHAPE_FACTOR);
    ctx_nsgt->groups = atomic_load(&ctx_lua->NSGT_GROUPS);
    ctx_nsgt->wav_capacity = ctx_nsgt->sample_rate * ctx_nsgt->sample_duration *
                             ctx_nsgt->channel_count * sizeof(float);
    ctx_nsgt->nsgt_capacity =
        ctx_nsgt->bin_capacity * ctx_nsgt->frame_capacity * sizeof(float);
    ctx_nsgt->magnitude_capacity =
        ctx_nsgt->bin_capacity * ctx_nsgt->frame_capacity * sizeof(float);
    ctx_nsgt->offset_capacity = ctx_nsgt->bin_capacity * sizeof(uint32_t);
    ctx_nsgt->length_capacity = ctx_nsgt->bin_capacity * sizeof(uint32_t);
    ctx_nsgt->frequency_capacity = ctx_nsgt->bin_capacity * sizeof(float);
    ctx_nsgt->hop_capacity = ctx_nsgt->bin_capacity * sizeof(uint32_t);
    ctx_nsgt->transient_capacity =
        ctx_nsgt->bin_capacity * ctx_nsgt->frame_capacity * sizeof(float);
    //-------------------------------------------------------------------------
    // NSGT FORMULA (GAUSSIAN + DUAL WINDOW, EXACT INVERTIBLE, INLINE MIN/MAX)
    //-------------------------------------------------------------------------
    float* frequency = malloc(ctx_nsgt->frequency_capacity);
    uint32_t* length = malloc(ctx_nsgt->length_capacity);
    uint32_t* hop = malloc(ctx_nsgt->hop_capacity);
    uint32_t* offset = malloc(ctx_nsgt->offset_capacity);
    float* cis = NULL;
    float* window = NULL;
    float* dual_window = NULL;
    float* scale = NULL;
    float* sum_squares = NULL;
    uint32_t window_cursor = 0;
    float ERB_min =
        21.4f * log10f(4.37f * ctx_nsgt->frequency_min / 1000.0f + 1.0f);
    float ERB_max =
        21.4f * log10f(4.37f * ctx_nsgt->frequency_max / 1000.0f + 1.0f);
    float ERB_scale = (ERB_max - ERB_min) / (ctx_nsgt->bin_capacity - 1);
    for (uint32_t i = 0; i < ctx_nsgt->bin_capacity; i++) {
        float erb_i = ERB_min + i * ERB_scale;
        frequency[i] = (1000.0f / 4.37f) * (powf(10.0f, erb_i / 21.4f) - 1.0f);
        float erb_bandwidth = 24.7f * (4.37f * frequency[i] / 1000.0f + 1.0f);
        uint32_t length_temp = (uint32_t)roundf(
            ctx_nsgt->alpha * ctx_nsgt->sample_rate / erb_bandwidth);
        if (length_temp < ctx_nsgt->window_length_min)
            length_temp = ctx_nsgt->window_length_min;
        if (length_temp & 1) length_temp++; // ensure even length
        length[i] = length_temp;
        hop[i] = length[i] / 2;
        window_cursor += length[i];
    }
    offset[0] = 0;
    for (uint32_t i = 1; i < ctx_nsgt->bin_capacity; i++) {
        offset[i] = offset[i - 1] + hop[i - 1];
    }
    ctx_nsgt->window_capacity = window_cursor * sizeof(float);
    window = malloc(ctx_nsgt->window_capacity);
    dual_window = malloc(ctx_nsgt->window_capacity);
    memset(window, 0, ctx_nsgt->window_capacity);
    memset(dual_window, 0, ctx_nsgt->window_capacity);
    for (uint32_t i = 0; i < ctx_nsgt->bin_capacity; i++) {
        uint32_t length_i = length[i];
        uint32_t offset_i = offset[i];
        float sigma = length_i / ctx_nsgt->shape_factor;
        for (uint32_t n = 0; n < length_i; n++) {
            float x = (float)n - ((float)length_i / 2.0f);
            window[offset_i + n] = expf(-M_PI * x * x / (sigma * sigma));
        }
    }
    sum_squares = malloc(window_cursor * sizeof(float));
    memset(sum_squares, 0, window_cursor * sizeof(float));
    for (uint32_t b = 0; b < ctx_nsgt->bin_capacity; b++) {
        for (uint32_t n = 0; n < length[b]; n++) {
            sum_squares[offset[b] + n] +=
                window[offset[b] + n] * window[offset[b] + n];
        }
    }
    scale = malloc(ctx_nsgt->bin_capacity * sizeof(float));
    for (uint32_t b = 0; b < ctx_nsgt->bin_capacity; b++) {
        uint32_t off = offset[b];
        uint32_t L = length[b];
        float max_ss = 0.0f;
        for (uint32_t n = 0; n < L; n++) {
            if (sum_squares[off + n] > max_ss) max_ss = sum_squares[off + n];
        }
        scale[b] = 1.0f / max_ss;
        for (uint32_t n = 0; n < L; n++) {
            dual_window[off + n] = window[off + n] * scale[b];
        }
    }
    //-------------------------------------------------------------------------
    // Tight frame check (dual window), ignoring near-zero edges
    memset(sum_squares, 0, window_cursor * sizeof(float));
    for (uint32_t b = 0; b < ctx_nsgt->bin_capacity; b++) {
        uint32_t off = offset[b];
        uint32_t L = length[b];
        for (uint32_t n = 0; n < L; n++) {
            sum_squares[off + n] += window[off + n] * dual_window[off + n];
        }
    }
    float min_val_dual = FLT_MAX, max_val_dual = -FLT_MAX;
    float ignore_threshold = 1e-6f;
    for (uint32_t n = 0; n < window_cursor; n++) {
        if (sum_squares[n] < ignore_threshold) continue; // ignore zero edges
        if (sum_squares[n] < min_val_dual) min_val_dual = sum_squares[n];
        if (sum_squares[n] > max_val_dual) max_val_dual = sum_squares[n];
    }
    call_king_terry("TIGHT FRAME CHECK (gaussian, dual): min=%.8f max=%.8f",
                    min_val_dual,
                    max_val_dual);
    // Tight frame check (raw Gaussian, non-dual), ignoring near-zero edges
    memset(sum_squares, 0, window_cursor * sizeof(float));
    for (uint32_t b = 0; b < ctx_nsgt->bin_capacity; b++) {
        uint32_t off = offset[b];
        uint32_t L = length[b];
        for (uint32_t n = 0; n < L; n++) {
            sum_squares[off + n] += window[off + n] * window[off + n];
        }
    }
    float min_val_raw = FLT_MAX, max_val_raw = -FLT_MAX;
    for (uint32_t n = 0; n < window_cursor; n++) {
        if (sum_squares[n] < ignore_threshold) continue; // ignore zero edges
        if (sum_squares[n] < min_val_raw) min_val_raw = sum_squares[n];
        if (sum_squares[n] > max_val_raw) max_val_raw = sum_squares[n];
    }
    call_king_terry("TIGHT FRAME CHECK (gaussian, non-dual): min=%.8f max=%.8f",
                    min_val_raw,
                    max_val_raw);
    ctx_nsgt->cis_capacity = ctx_nsgt->window_capacity * 2;
    cis = malloc(ctx_nsgt->window_capacity * 2);
    for (uint32_t b = 0; b < ctx_nsgt->bin_capacity; b++) {
        uint32_t L = length[b];
        uint32_t off = offset[b];
        float freq = frequency[b];
        for (uint32_t t = 0; t < L; t++) {
            float phase = -6.28318530718f * freq * t / (float)L;
            cis[(off + t) * 2 + 0] = cosf(phase);
            cis[(off + t) * 2 + 1] = sinf(phase);
        }
    }
    for (uint32_t i = 0; i < ctx_nsgt->bin_capacity; i++) {
        if (length[i] > ctx_nsgt->window_length_max) {
            ctx_nsgt->window_length_max = length[i];
        }
        if (hop[i] < ctx_nsgt->hop_min) { ctx_nsgt->hop_min = hop[i]; }
    }
    call_king_terry("window length max: %u", ctx_nsgt->window_length_max);
    if (ctx_nsgt->hop_min == 0) { ctx_nsgt->hop_min = 1; }
    call_king_terry("hop min: %u", ctx_nsgt->hop_min);
    ctx_nsgt->compute_push_constant.window_length_max =
        ctx_nsgt->window_length_max;
    ctx_nsgt->compute_push_constant.bin_capacity = ctx_nsgt->bin_capacity;
    ctx_nsgt->compute_push_constant.frame_capacity = ctx_nsgt->frame_capacity;
    //-------------------------------------------------------------------------
    // Cleanup
    free(sum_squares);
    free(scale);
    //-------------------------------------------------------------------------
    // END NSGT FORMULA (GAUSSIAN + DUAL WINDOW, INLINE MIN/MAX)
    //-------------------------------------------------------------------------
    // pool alloc
    ctx_nsgt->memory_property_flags = war_pool_alloc(
        pool_wr, sizeof(VkMemoryPropertyFlags) * ctx_nsgt->resource_count);
    ctx_nsgt->usage_flags = war_pool_alloc(
        pool_wr, sizeof(VkBufferUsageFlags) * ctx_nsgt->resource_count);
    ctx_nsgt->buffer =
        war_pool_alloc(pool_wr, sizeof(VkBuffer) * ctx_nsgt->resource_count);
    ctx_nsgt->memory_requirements = war_pool_alloc(
        pool_wr, sizeof(VkMemoryRequirements) * ctx_nsgt->resource_count);
    ctx_nsgt->device_memory = war_pool_alloc(
        pool_wr, sizeof(VkDeviceMemory) * ctx_nsgt->resource_count);
    ctx_nsgt->map =
        war_pool_alloc(pool_wr, sizeof(void*) * ctx_nsgt->resource_count);
    ctx_nsgt->capacity = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_nsgt->resource_count);
    ctx_nsgt->descriptor_buffer_info = war_pool_alloc(
        pool_wr, sizeof(VkDescriptorBufferInfo) * ctx_nsgt->resource_count);
    ctx_nsgt->descriptor_image_info = war_pool_alloc(
        pool_wr, sizeof(VkDescriptorImageInfo) * ctx_nsgt->resource_count);
    ctx_nsgt->image =
        war_pool_alloc(pool_wr, sizeof(VkImage) * ctx_nsgt->resource_count);
    ctx_nsgt->image_view =
        war_pool_alloc(pool_wr, sizeof(VkImageView) * ctx_nsgt->resource_count);
    ctx_nsgt->format =
        war_pool_alloc(pool_wr, sizeof(VkFormat) * ctx_nsgt->resource_count);
    ctx_nsgt->extent_3d =
        war_pool_alloc(pool_wr, sizeof(VkExtent3D) * ctx_nsgt->resource_count);
    ctx_nsgt->mapped_memory_range = war_pool_alloc(
        pool_wr, sizeof(VkMappedMemoryRange) * ctx_nsgt->resource_count);
    ctx_nsgt->buffer_memory_barrier = war_pool_alloc(
        pool_wr, sizeof(VkBufferMemoryBarrier) * ctx_nsgt->resource_count);
    ctx_nsgt->image_memory_barrier = war_pool_alloc(
        pool_wr, sizeof(VkImageMemoryBarrier) * ctx_nsgt->resource_count);
    ctx_nsgt->image_usage_flags = war_pool_alloc(
        pool_wr, sizeof(VkImageUsageFlags) * ctx_nsgt->resource_count);
    ctx_nsgt->shader_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkShaderStageFlags) * ctx_nsgt->resource_count);
    ctx_nsgt->descriptor_set_layout_binding = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorSetLayoutBinding) * ctx_nsgt->resource_count);
    ctx_nsgt->write_descriptor_set = war_pool_alloc(
        pool_wr, sizeof(VkWriteDescriptorSet) * ctx_nsgt->resource_count);
    ctx_nsgt->image_layout = war_pool_alloc(
        pool_wr, sizeof(VkImageLayout) * ctx_nsgt->resource_count);
    ctx_nsgt->access_flags = war_pool_alloc(
        pool_wr, sizeof(VkAccessFlags) * ctx_nsgt->resource_count);
    ctx_nsgt->pipeline_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkPipelineStageFlags) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_src_idx =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_dst_idx =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_size = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_src_offset = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_dst_offset = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_nsgt->resource_count);
    ctx_nsgt->descriptor_set = war_pool_alloc(
        pool_wr, sizeof(VkDescriptorSet) * ctx_nsgt->descriptor_set_count);
    ctx_nsgt->descriptor_set_layout = war_pool_alloc(
        pool_wr,
        sizeof(VkDescriptorSetLayout) * ctx_nsgt->descriptor_set_count);
    ctx_nsgt->image_descriptor_type = war_pool_alloc(
        pool_wr, sizeof(VkDescriptorType) * ctx_nsgt->descriptor_set_count);
    ctx_nsgt->descriptor_pool = war_pool_alloc(
        pool_wr, sizeof(VkDescriptorPool) * ctx_nsgt->descriptor_set_count);
    ctx_nsgt->descriptor_image_layout = war_pool_alloc(
        pool_wr, sizeof(VkImageLayout) * ctx_nsgt->descriptor_set_count);
    ctx_nsgt->shader_module = war_pool_alloc(
        pool_wr, sizeof(VkShaderModule) * ctx_nsgt->shader_count);
    ctx_nsgt->pipeline =
        war_pool_alloc(pool_wr, sizeof(VkPipeline) * ctx_nsgt->pipeline_count);
    ctx_nsgt->pipeline_layout = war_pool_alloc(
        pool_wr, sizeof(VkPipelineLayout) * ctx_nsgt->pipeline_count);
    ctx_nsgt->pipeline_set_idx =
        war_pool_alloc(pool_wr,
                       sizeof(uint32_t) * ctx_nsgt->pipeline_count *
                           ctx_nsgt->descriptor_set_count);
    memset(ctx_nsgt->pipeline_set_idx,
           UINT32_MAX,
           ctx_nsgt->pipeline_count * ctx_nsgt->descriptor_set_count *
               sizeof(uint32_t));
    ctx_nsgt->pipeline_shader_idx = war_pool_alloc(
        pool_wr,
        sizeof(uint32_t) * ctx_nsgt->pipeline_count * ctx_nsgt->shader_count);
    memset(ctx_nsgt->pipeline_shader_idx,
           UINT32_MAX,
           ctx_nsgt->pipeline_count * ctx_nsgt->shader_count *
               sizeof(uint32_t));
    ctx_nsgt->pipeline_shader_stage_create_info = war_pool_alloc(
        pool_wr,
        sizeof(VkPipelineShaderStageCreateInfo) * ctx_nsgt->shader_count);
    ctx_nsgt->shader_stage_flag_bits = war_pool_alloc(
        pool_wr, sizeof(VkShaderStageFlagBits) * ctx_nsgt->shader_count);
    ctx_nsgt->shader_path = war_pool_alloc(
        pool_wr, sizeof(char) * ctx_nsgt->shader_count * ctx_nsgt->path_limit);
    ctx_nsgt->structure_type = war_pool_alloc(
        pool_wr, sizeof(VkStructureType) * ctx_nsgt->pipeline_count);
    ctx_nsgt->push_constant_shader_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkShaderStageFlags) * ctx_nsgt->pipeline_count);
    ctx_nsgt->push_constant_size =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->pipeline_count);
    ctx_nsgt->pipeline_bind_point = war_pool_alloc(
        pool_wr, sizeof(VkPipelineBindPoint) * ctx_nsgt->pipeline_count);
    ctx_nsgt->size =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_data =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_data_2 =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->resource_count);
    ctx_nsgt->pipeline_dispatch_group = war_pool_alloc(
        pool_wr,
        sizeof(uint32_t) * ctx_nsgt->pipeline_count * ctx_nsgt->groups);
    ctx_nsgt->pipeline_local_size = war_pool_alloc(
        pool_wr,
        sizeof(uint32_t) * ctx_nsgt->pipeline_count * ctx_nsgt->groups);
    for (uint32_t i = 0; i < ctx_nsgt->pipeline_count * ctx_nsgt->groups; i++) {
        ctx_nsgt->pipeline_dispatch_group[i] = 1;
        ctx_nsgt->pipeline_local_size[i] = 1;
    }
    ctx_nsgt->fn_image_layout = war_pool_alloc(
        pool_wr, sizeof(VkImageLayout) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_access_flags = war_pool_alloc(
        pool_wr, sizeof(VkAccessFlags) * ctx_nsgt->resource_count);
    ctx_nsgt->fn_pipeline_stage_flags = war_pool_alloc(
        pool_wr, sizeof(VkPipelineStageFlags) * ctx_nsgt->resource_count);
    //-------------------------------------------------------------------------
    // NSGT IDX
    //-------------------------------------------------------------------------
    // resource
    ctx_nsgt->idx_offset = 0;
    ctx_nsgt->idx_hop = 1;
    ctx_nsgt->idx_length = 2;
    ctx_nsgt->idx_window = 3;
    ctx_nsgt->idx_dual_window = 4;
    ctx_nsgt->idx_frequency = 5;
    ctx_nsgt->idx_cis = 6;
    ctx_nsgt->idx_wav_temp = 7;
    ctx_nsgt->idx_wav = 8;
    ctx_nsgt->idx_nsgt_temp = 9;
    ctx_nsgt->idx_nsgt = 10;
    ctx_nsgt->idx_magnitude_temp = 11;
    ctx_nsgt->idx_magnitude = 12;
    ctx_nsgt->idx_transient_temp = 13;
    ctx_nsgt->idx_transient = 14;
    ctx_nsgt->idx_image_temp = 15;
    ctx_nsgt->idx_image = 16;
    ctx_nsgt->idx_wav_stage = 17;
    ctx_nsgt->idx_offset_stage = 18;
    ctx_nsgt->idx_hop_stage = 19;
    ctx_nsgt->idx_length_stage = 20;
    ctx_nsgt->idx_window_stage = 21;
    ctx_nsgt->idx_dual_window_stage = 22;
    ctx_nsgt->idx_frequency_stage = 23;
    ctx_nsgt->idx_cis_stage = 24;
    // descriptor set
    ctx_nsgt->set_idx_compute = 0;
    ctx_nsgt->set_idx_graphics = 1;
    // compute
    ctx_nsgt->image_descriptor_type[ctx_nsgt->set_idx_compute] =
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    ctx_nsgt->descriptor_image_layout[ctx_nsgt->set_idx_compute] =
        VK_IMAGE_LAYOUT_GENERAL;
    // graphics
    ctx_nsgt->image_descriptor_type[ctx_nsgt->set_idx_graphics] =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    ctx_nsgt->descriptor_image_layout[ctx_nsgt->set_idx_graphics] =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // shader
    ctx_nsgt->shader_idx_compute_nsgt = 0;
    ctx_nsgt->shader_idx_compute_transient = 1;
    ctx_nsgt->shader_idx_compute_magnitude = 2;
    ctx_nsgt->shader_idx_compute_image = 3;
    ctx_nsgt->shader_idx_compute_wav = 4;
    ctx_nsgt->shader_idx_vertex = 5;
    ctx_nsgt->shader_idx_fragment = 6;
    // war_nsgt_compute_nsgt
    char* path_compute_nsgt = "build/spv/war_nsgt_compute_nsgt.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_compute_nsgt *
                                  ctx_nsgt->path_limit],
           path_compute_nsgt,
           strlen(path_compute_nsgt));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_compute_nsgt] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    // war_nsgt_compute_magnitude
    char* path_compute_magnitude = "build/spv/war_nsgt_compute_magnitude.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_compute_magnitude *
                                  ctx_nsgt->path_limit],
           path_compute_magnitude,
           strlen(path_compute_magnitude));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_compute_magnitude] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    // war_nsgt_compute_transient
    char* path_compute_transient = "build/spv/war_nsgt_compute_transient.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_compute_transient *
                                  ctx_nsgt->path_limit],
           path_compute_transient,
           strlen(path_compute_transient));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_compute_transient] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    // war_nsgt_compute_image
    char* path_compute_image = "build/spv/war_nsgt_compute_image.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_compute_image *
                                  ctx_nsgt->path_limit],
           path_compute_image,
           strlen(path_compute_image));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_compute_image] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    // war_nsgt_compute_wav
    char* path_compute_wav = "build/spv/war_nsgt_compute_wav.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_compute_wav *
                                  ctx_nsgt->path_limit],
           path_compute_wav,
           strlen(path_compute_wav));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_compute_wav] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    // war_nsgt_vertex
    char* path_vertex = "build/spv/war_nsgt_vertex.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_vertex *
                                  ctx_nsgt->path_limit],
           path_vertex,
           strlen(path_vertex));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_vertex] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // war_nsgt_fragment
    char* path_fragment = "build/spv/war_nsgt_fragment.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_fragment *
                                  ctx_nsgt->path_limit],
           path_fragment,
           strlen(path_fragment));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_fragment] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // pipeline idx
    ctx_nsgt->pipeline_idx_compute_nsgt = 0;
    ctx_nsgt->pipeline_idx_compute_transient = 1;
    ctx_nsgt->pipeline_idx_compute_magnitude = 2;
    ctx_nsgt->pipeline_idx_compute_image = 3;
    ctx_nsgt->pipeline_idx_compute_wav = 4;
    ctx_nsgt->pipeline_idx_graphics = 5;
    // compute_nsgt pipeline
    ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_compute_nsgt] =
        VK_PIPELINE_BIND_POINT_COMPUTE;
    ctx_nsgt->pipeline_set_idx[ctx_nsgt->pipeline_idx_compute_nsgt *
                                   ctx_nsgt->descriptor_set_count +
                               0] = ctx_nsgt->set_idx_compute;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_compute_nsgt *
                                      ctx_nsgt->shader_count +
                                  0] = ctx_nsgt->shader_idx_compute_nsgt;
    ctx_nsgt->structure_type[ctx_nsgt->pipeline_idx_compute_nsgt] =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ctx_nsgt->push_constant_shader_stage_flags
        [ctx_nsgt->pipeline_idx_compute_nsgt] = VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_compute_nsgt] =
        sizeof(war_nsgt_compute_push_constant);
    ctx_nsgt->pipeline_local_size[ctx_nsgt->pipeline_idx_compute_nsgt *
                                      ctx_nsgt->groups +
                                  0] = 64;
    // compute_transient pipeline
    ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_compute_transient] =
        VK_PIPELINE_BIND_POINT_COMPUTE;
    ctx_nsgt->pipeline_set_idx[ctx_nsgt->pipeline_idx_compute_transient *
                                   ctx_nsgt->descriptor_set_count +
                               0] = ctx_nsgt->set_idx_compute;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_compute_transient *
                                      ctx_nsgt->shader_count +
                                  0] = ctx_nsgt->shader_idx_compute_transient;
    ctx_nsgt->structure_type[ctx_nsgt->pipeline_idx_compute_transient] =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ctx_nsgt->push_constant_shader_stage_flags
        [ctx_nsgt->pipeline_idx_compute_transient] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_compute_transient] =
        sizeof(war_nsgt_compute_push_constant);
    ctx_nsgt->pipeline_local_size[ctx_nsgt->pipeline_idx_compute_transient *
                                      ctx_nsgt->groups +
                                  0] = 64;
    // compute_magnitude pipeline
    ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_compute_magnitude] =
        VK_PIPELINE_BIND_POINT_COMPUTE;
    ctx_nsgt->pipeline_set_idx[ctx_nsgt->pipeline_idx_compute_magnitude *
                                   ctx_nsgt->descriptor_set_count +
                               0] = ctx_nsgt->set_idx_compute;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_compute_magnitude *
                                      ctx_nsgt->shader_count +
                                  0] = ctx_nsgt->shader_idx_compute_magnitude;
    ctx_nsgt->structure_type[ctx_nsgt->pipeline_idx_compute_magnitude] =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ctx_nsgt->push_constant_shader_stage_flags
        [ctx_nsgt->pipeline_idx_compute_magnitude] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_compute_magnitude] =
        sizeof(war_nsgt_compute_push_constant);
    ctx_nsgt->pipeline_local_size[ctx_nsgt->pipeline_idx_compute_magnitude *
                                      ctx_nsgt->groups +
                                  0] = 64;
    // compute_image pipeline
    ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_compute_image] =
        VK_PIPELINE_BIND_POINT_COMPUTE;
    ctx_nsgt->pipeline_set_idx[ctx_nsgt->pipeline_idx_compute_image *
                                   ctx_nsgt->descriptor_set_count +
                               0] = ctx_nsgt->set_idx_compute;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_compute_image *
                                      ctx_nsgt->shader_count +
                                  0] = ctx_nsgt->shader_idx_compute_image;
    ctx_nsgt->structure_type[ctx_nsgt->pipeline_idx_compute_image] =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ctx_nsgt->push_constant_shader_stage_flags
        [ctx_nsgt->pipeline_idx_compute_image] = VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_compute_image] =
        sizeof(war_nsgt_compute_push_constant);
    ctx_nsgt->pipeline_local_size[ctx_nsgt->pipeline_idx_compute_image *
                                      ctx_nsgt->groups +
                                  0] = 32;
    ctx_nsgt->pipeline_local_size[ctx_nsgt->pipeline_idx_compute_image *
                                      ctx_nsgt->groups +
                                  1] = 32;
    // compute_wav pipeline
    ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_compute_wav] =
        VK_PIPELINE_BIND_POINT_COMPUTE;
    ctx_nsgt->pipeline_set_idx[ctx_nsgt->pipeline_idx_compute_wav *
                                   ctx_nsgt->descriptor_set_count +
                               0] = ctx_nsgt->set_idx_compute;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_compute_wav *
                                      ctx_nsgt->shader_count +
                                  0] = ctx_nsgt->shader_idx_compute_wav;
    ctx_nsgt->structure_type[ctx_nsgt->pipeline_idx_compute_wav] =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ctx_nsgt
        ->push_constant_shader_stage_flags[ctx_nsgt->pipeline_idx_compute_wav] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_compute_wav] =
        sizeof(war_nsgt_compute_push_constant);
    ctx_nsgt->pipeline_local_size[ctx_nsgt->pipeline_idx_compute_wav *
                                      ctx_nsgt->groups +
                                  0] = 64;
    // graphics pipeline
    ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_graphics] =
        VK_PIPELINE_BIND_POINT_GRAPHICS;
    ctx_nsgt->pipeline_set_idx[ctx_nsgt->pipeline_idx_graphics *
                                   ctx_nsgt->descriptor_set_count +
                               0] = ctx_nsgt->set_idx_graphics;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_graphics *
                                      ctx_nsgt->shader_count +
                                  0] = ctx_nsgt->shader_idx_vertex;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_graphics *
                                      ctx_nsgt->shader_count +
                                  1] = ctx_nsgt->shader_idx_fragment;
    ctx_nsgt->structure_type[ctx_nsgt->pipeline_idx_graphics] =
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ctx_nsgt
        ->push_constant_shader_stage_flags[ctx_nsgt->pipeline_idx_graphics] =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_graphics] =
        sizeof(war_nsgt_graphics_push_constant);
    // offset
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_offset] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_offset] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_offset] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_offset] = ctx_nsgt->offset_capacity;
    // hop
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_hop] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_hop] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_hop] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_hop] = ctx_nsgt->hop_capacity;
    // length
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_length] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_length] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_length] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_length] = ctx_nsgt->length_capacity;
    // window
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_window] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_window] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_window] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_window] = ctx_nsgt->window_capacity;
    // dual_window
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_dual_window] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_dual_window] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_dual_window] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_dual_window] = ctx_nsgt->window_capacity;
    // frequency
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_frequency] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_frequency] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_frequency] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_frequency] = ctx_nsgt->frequency_capacity;
    // cis
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_cis] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_cis] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_cis] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_cis] = ctx_nsgt->cis_capacity;
    // offset_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_offset_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_offset_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_offset_stage] = ctx_nsgt->offset_capacity;
    // hop_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_hop_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_hop_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_hop_stage] = ctx_nsgt->hop_capacity;
    // length_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_length_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_length_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_length_stage] = ctx_nsgt->length_capacity;
    // window_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_window_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_window_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_window_stage] = ctx_nsgt->window_capacity;
    // dual_window_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_dual_window_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_dual_window_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_dual_window_stage] =
        ctx_nsgt->window_capacity;
    // frequency_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_frequency_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_frequency_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_frequency_stage] =
        ctx_nsgt->frequency_capacity;
    // cis stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_cis_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_cis_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_cis_stage] = ctx_nsgt->cis_capacity;
    // wav_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_wav_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_wav_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_wav_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_wav_temp] = ctx_nsgt->wav_capacity;
    // wav
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_wav] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_wav] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_wav] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_wav] = ctx_nsgt->wav_capacity;
    // nsgt_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_nsgt_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_nsgt_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_nsgt_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_nsgt_temp] = ctx_nsgt->nsgt_capacity;
    // nsgt
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_nsgt] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_nsgt] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_nsgt] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_nsgt] = ctx_nsgt->nsgt_capacity;
    // magnitude_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_magnitude_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_magnitude_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_magnitude_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_magnitude_temp] =
        ctx_nsgt->magnitude_capacity;
    // magnitude
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_magnitude] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_magnitude] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_magnitude] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_magnitude] = ctx_nsgt->magnitude_capacity;
    // transient_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_transient_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_transient_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_transient_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_transient_temp] =
        ctx_nsgt->transient_capacity;
    // transient
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_transient] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_transient] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_transient] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_transient] = ctx_nsgt->transient_capacity;
    // image_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_image_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_image_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_nsgt->format[ctx_nsgt->idx_image_temp] = VK_FORMAT_R32_SFLOAT;
    ctx_nsgt->extent_3d[ctx_nsgt->idx_image_temp] =
        (VkExtent3D){.width = ctx_nsgt->frame_capacity,
                     .height = ctx_nsgt->bin_capacity,
                     .depth = 1};
    ctx_nsgt->image_usage_flags[ctx_nsgt->idx_image_temp] =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // image
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_image] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_image] =
        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_nsgt->format[ctx_nsgt->idx_image] = VK_FORMAT_R32_SFLOAT;
    ctx_nsgt->extent_3d[ctx_nsgt->idx_image] =
        (VkExtent3D){.width = ctx_nsgt->frame_capacity,
                     .height = ctx_nsgt->bin_capacity,
                     .depth = 1};
    ctx_nsgt->image_usage_flags[ctx_nsgt->idx_image] =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // wav_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_wav_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_wav_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_wav_stage] = ctx_nsgt->wav_capacity;
    // sampler
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
    VkResult result = vkCreateSampler(
        device, &graphics_sampler_info, NULL, &ctx_nsgt->sampler);
    assert(result == VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device,
                                        &physical_device_memory_properties);
    for (VkDeviceSize i = 0; i < ctx_nsgt->resource_count; i++) {
        ctx_nsgt->pipeline_stage_flags[i] = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkImageUsageFlags image_usage_flags = ctx_nsgt->image_usage_flags[i];
        if ((image_usage_flags &
             (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) != 0) {
            goto image;
        }
        VkMemoryPropertyFlags memory_property_flags =
            ctx_nsgt->memory_property_flags[i];
        uint8_t device_local =
            (memory_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
        VkBufferCreateInfo buffer_create_info = {0};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = ctx_nsgt->capacity[i];
        buffer_create_info.usage = ctx_nsgt->usage_flags[i];
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        result = vkCreateBuffer(
            device, &buffer_create_info, NULL, &ctx_nsgt->buffer[i]);
        assert(result == VK_SUCCESS);
        vkGetBufferMemoryRequirements(
            device, ctx_nsgt->buffer[i], &ctx_nsgt->memory_requirements[i]);
        uint32_t memory_type_index = UINT32_MAX;
        VkMemoryRequirements memory_requirements =
            ctx_nsgt->memory_requirements[i];
        for (uint32_t j = 0;
             j < physical_device_memory_properties.memoryTypeCount;
             j++) {
            if ((memory_requirements.memoryTypeBits & (1 << j)) &&
                (physical_device_memory_properties.memoryTypes[j]
                     .propertyFlags &
                 ctx_nsgt->memory_property_flags[i]) ==
                    ctx_nsgt->memory_property_flags[i]) {
                memory_type_index = j;
                break;
            }
        }
        assert(memory_type_index != UINT32_MAX);
        VkMemoryAllocateInfo memory_allocate_info = {0};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type_index;
        result = vkAllocateMemory(
            device, &memory_allocate_info, NULL, &ctx_nsgt->device_memory[i]);
        assert(result == VK_SUCCESS);
        result = vkBindBufferMemory(
            device, ctx_nsgt->buffer[i], ctx_nsgt->device_memory[i], 0);
        assert(result == VK_SUCCESS);
        if (!device_local) {
            result = vkMapMemory(device,
                                 ctx_nsgt->device_memory[i],
                                 0,
                                 ctx_nsgt->capacity[i],
                                 0,
                                 &ctx_nsgt->map[i]);
            assert(result == VK_SUCCESS);
            continue;
        }
        // descriptor_set_layout_binding
        ctx_nsgt->descriptor_set_layout_binding[i].binding = i;
        ctx_nsgt->descriptor_set_layout_binding[i].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ctx_nsgt->descriptor_set_layout_binding[i].descriptorCount = 1;
        ctx_nsgt->descriptor_set_layout_binding[i].stageFlags =
            ctx_nsgt->shader_stage_flags[i];
        ctx_nsgt->descriptor_count++;
        continue;
    image:
        VkImageCreateInfo image_create_info = {0};
        image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_create_info.imageType = VK_IMAGE_TYPE_2D;
        image_create_info.format = ctx_nsgt->format[i];
        image_create_info.extent = ctx_nsgt->extent_3d[i];
        image_create_info.mipLevels = 1;
        image_create_info.arrayLayers = 1;
        image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_create_info.usage = ctx_nsgt->image_usage_flags[i];
        image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        result = vkCreateImage(
            device, &image_create_info, NULL, &ctx_nsgt->image[i]);
        assert(result == VK_SUCCESS);
        vkGetImageMemoryRequirements(
            device, ctx_nsgt->image[i], &ctx_nsgt->memory_requirements[i]);
        memory_requirements = ctx_nsgt->memory_requirements[i];
        memory_type_index = UINT32_MAX;
        for (uint32_t j = 0;
             j < physical_device_memory_properties.memoryTypeCount;
             j++) {
            if ((memory_requirements.memoryTypeBits & (1 << j)) &&
                (physical_device_memory_properties.memoryTypes[j]
                     .propertyFlags &
                 ctx_nsgt->memory_property_flags[i]) ==
                    ctx_nsgt->memory_property_flags[i]) {
                memory_type_index = j;
                break;
            }
        }
        assert(memory_type_index != UINT32_MAX);
        memory_allocate_info = (VkMemoryAllocateInfo){0};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = memory_type_index;
        result = vkAllocateMemory(
            device, &memory_allocate_info, NULL, &ctx_nsgt->device_memory[i]);
        assert(result == VK_SUCCESS);
        result = vkBindImageMemory(
            device, ctx_nsgt->image[i], ctx_nsgt->device_memory[i], 0);
        assert(result == VK_SUCCESS);
        VkImageViewCreateInfo view_info = {0};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = ctx_nsgt->image[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = ctx_nsgt->format[i];
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        result = vkCreateImageView(
            device, &view_info, NULL, &ctx_nsgt->image_view[i]);
        assert(result == VK_SUCCESS);
        // descriptor_set_layout_binding
        ctx_nsgt->descriptor_set_layout_binding[i].binding = i;
        ctx_nsgt->descriptor_set_layout_binding[i].descriptorCount = 1;
        ctx_nsgt->descriptor_set_layout_binding[i].stageFlags =
            ctx_nsgt->shader_stage_flags[i];
        ctx_nsgt->descriptor_count++;
        ctx_nsgt->descriptor_image_count++;
    }
    for (uint32_t i = 0; i < ctx_nsgt->descriptor_set_count; i++) {
        for (uint32_t k = 0; k < ctx_nsgt->descriptor_count; k++) {
            VkImageUsageFlags image_usage_flags =
                ctx_nsgt->image_usage_flags[k];
            if ((image_usage_flags & (VK_IMAGE_USAGE_STORAGE_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT)) != 0) {
                goto image_descriptor_type;
            }
            continue;
        image_descriptor_type:
            ctx_nsgt->descriptor_set_layout_binding[k].descriptorType =
                ctx_nsgt->image_descriptor_type[i];
        }
        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = ctx_nsgt->descriptor_count,
            .pBindings = ctx_nsgt->descriptor_set_layout_binding,
        };
        result =
            vkCreateDescriptorSetLayout(device,
                                        &descriptor_set_layout_create_info,
                                        NULL,
                                        &ctx_nsgt->descriptor_set_layout[i]);
        assert(result == VK_SUCCESS);
        VkDescriptorPoolSize descriptor_pool_size[2] = {
            (VkDescriptorPoolSize){
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = ctx_nsgt->descriptor_count -
                                   ctx_nsgt->descriptor_image_count,
            },
            (VkDescriptorPoolSize){
                .type = ctx_nsgt->image_descriptor_type[i],
                .descriptorCount = ctx_nsgt->descriptor_image_count,
            },
        };
        VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = 2,
            .pPoolSizes = descriptor_pool_size,
            .maxSets = 1,
        };
        result = vkCreateDescriptorPool(device,
                                        &descriptor_pool_create_info,
                                        NULL,
                                        &ctx_nsgt->descriptor_pool[i]);
        assert(result == VK_SUCCESS);
        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = ctx_nsgt->descriptor_pool[i],
            .descriptorSetCount = 1,
            .pSetLayouts = &ctx_nsgt->descriptor_set_layout[i],
        };
        result = vkAllocateDescriptorSets(device,
                                          &descriptor_set_allocate_info,
                                          &ctx_nsgt->descriptor_set[i]);
        assert(result == VK_SUCCESS);
        for (uint32_t k = 0; k < ctx_nsgt->descriptor_count; k++) {
            VkImageUsageFlags image_usage_flags =
                ctx_nsgt->image_usage_flags[k];
            if ((image_usage_flags & (VK_IMAGE_USAGE_STORAGE_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT)) != 0) {
                goto descriptor_image;
            }
            // descriptor buffer info
            ctx_nsgt->descriptor_buffer_info[k].buffer = ctx_nsgt->buffer[k];
            ctx_nsgt->descriptor_buffer_info[k].offset = 0;
            ctx_nsgt->descriptor_buffer_info[k].range = ctx_nsgt->capacity[k];
            /// write descriptor set
            ctx_nsgt->write_descriptor_set[k].sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ctx_nsgt->write_descriptor_set[k].dstBinding = k;
            ctx_nsgt->write_descriptor_set[k].descriptorCount = 1;
            ctx_nsgt->write_descriptor_set[k].descriptorType =
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            ctx_nsgt->write_descriptor_set[k].pBufferInfo =
                &ctx_nsgt->descriptor_buffer_info[k];
            ctx_nsgt->write_descriptor_set[k].dstSet =
                ctx_nsgt->descriptor_set[i];
            // pipeline stage flags
            ctx_nsgt->pipeline_stage_flags[k] =
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            continue;
        descriptor_image:
            // descriptor_image_info
            ctx_nsgt->descriptor_image_info[k].imageView =
                ctx_nsgt->image_view[k];
            ctx_nsgt->descriptor_image_info[k].sampler = ctx_nsgt->sampler;
            ctx_nsgt->descriptor_image_info[k].imageLayout =
                ctx_nsgt->descriptor_image_layout[i];
            // write_descriptor_set
            ctx_nsgt->write_descriptor_set[k].sType =
                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ctx_nsgt->write_descriptor_set[k].dstBinding = k;
            ctx_nsgt->write_descriptor_set[k].descriptorCount = 1;
            ctx_nsgt->write_descriptor_set[k].pImageInfo =
                &ctx_nsgt->descriptor_image_info[k];
            ctx_nsgt->write_descriptor_set[k].descriptorType =
                ctx_nsgt->image_descriptor_type[i];
            ctx_nsgt->write_descriptor_set[k].dstSet =
                ctx_nsgt->descriptor_set[i];
            // image layout
            ctx_nsgt->image_layout[k] = VK_IMAGE_LAYOUT_UNDEFINED;
            // pipeline stage flags
            ctx_nsgt->pipeline_stage_flags[k] =
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }
        vkUpdateDescriptorSets(device,
                               ctx_nsgt->descriptor_count,
                               ctx_nsgt->write_descriptor_set,
                               0,
                               NULL);
    }
    for (uint32_t i = 0; i < ctx_nsgt->shader_count; i++) {
        uint8_t fn_result = war_vulkan_get_shader_module(
            device,
            &ctx_nsgt->shader_module[i],
            &ctx_nsgt->shader_path[i * ctx_nsgt->path_limit]);
        assert(fn_result);
    }
    for (uint32_t i = 0; i < ctx_nsgt->pipeline_count; i++) {
        VkPushConstantRange push_constant_range;
        push_constant_range.stageFlags =
            ctx_nsgt->push_constant_shader_stage_flags[i];
        push_constant_range.offset = 0;
        push_constant_range.size = ctx_nsgt->push_constant_size[i];
        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {0};
        pipeline_layout_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts =
            &ctx_nsgt->descriptor_set_layout
                 [ctx_nsgt
                      ->pipeline_set_idx[i * ctx_nsgt->descriptor_set_count]];
        pipeline_layout_create_info.pushConstantRangeCount = 1;
        pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
        result = vkCreatePipelineLayout(device,
                                        &pipeline_layout_create_info,
                                        NULL,
                                        &ctx_nsgt->pipeline_layout[i]);
        assert(result == VK_SUCCESS);
        uint32_t pipeline_shader_count = 0;
        uint32_t pipeline_shader_idx = 0;
        uint8_t graphics_pipeline = 0;
        for (uint32_t k = 0; k < ctx_nsgt->shader_count; k++) {
            uint32_t tmp_pipeline_shader_idx =
                ctx_nsgt->pipeline_shader_idx[i * ctx_nsgt->shader_count + k];
            if (tmp_pipeline_shader_idx == UINT32_MAX) { continue; }
            pipeline_shader_idx = tmp_pipeline_shader_idx;
            ctx_nsgt->pipeline_shader_stage_create_info[pipeline_shader_count]
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            ctx_nsgt->pipeline_shader_stage_create_info[pipeline_shader_count]
                .stage = ctx_nsgt->shader_stage_flag_bits[pipeline_shader_idx];
            ctx_nsgt->pipeline_shader_stage_create_info[pipeline_shader_count]
                .module = ctx_nsgt->shader_module[pipeline_shader_idx];
            ctx_nsgt->pipeline_shader_stage_create_info[pipeline_shader_count]
                .pName = "main";
            pipeline_shader_count++;
            if (ctx_nsgt->shader_stage_flag_bits[pipeline_shader_idx] ==
                VK_SHADER_STAGE_VERTEX_BIT) {
                graphics_pipeline = 1;
            }
        }
        if (graphics_pipeline) { goto graphics_pipeline; }
        // compute pipeline
        assert(pipeline_shader_count == 1 &&
               ctx_nsgt->shader_stage_flag_bits[pipeline_shader_idx] ==
                   VK_SHADER_STAGE_COMPUTE_BIT);
        VkComputePipelineCreateInfo compute_pipeline_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage =
                (VkPipelineShaderStageCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = ctx_nsgt->shader_module[pipeline_shader_idx],
                    .pName = "main",
                },
            .layout = ctx_nsgt->pipeline_layout[i],
        };
        result = vkCreateComputePipelines(device,
                                          VK_NULL_HANDLE,
                                          1,
                                          &compute_pipeline_create_info,
                                          NULL,
                                          &ctx_nsgt->pipeline[i]);
        assert(result == VK_SUCCESS);
        continue;
    graphics_pipeline:
        VkPipelineVertexInputStateCreateInfo
            graphics_pipeline_vertex_input_state_create_info = {0};
        graphics_pipeline_vertex_input_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo
            graphics_pipeline_input_assembly_state_create_info = {0};
        graphics_pipeline_input_assembly_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        graphics_pipeline_input_assembly_state_create_info.topology =
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
        VkPipelineDepthStencilStateCreateInfo
            graphics_pipeline_depth_stencil_state_create_info = {0};
        graphics_pipeline_depth_stencil_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        graphics_pipeline_depth_stencil_state_create_info.depthTestEnable =
            VK_TRUE;
        graphics_pipeline_depth_stencil_state_create_info.depthWriteEnable =
            VK_TRUE;
        graphics_pipeline_depth_stencil_state_create_info.depthCompareOp =
            VK_COMPARE_OP_GREATER_OR_EQUAL;
        VkPipelineColorBlendStateCreateInfo
            graphics_pipeline_color_blend_state_create_info = {0};
        VkPipelineColorBlendAttachmentState
            graphics_pipeline_color_blend_attachment_state = {
                .colorWriteMask =
                    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                .blendEnable = VK_FALSE,
            };
        graphics_pipeline_color_blend_state_create_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        graphics_pipeline_color_blend_state_create_info.attachmentCount = 1;
        graphics_pipeline_color_blend_state_create_info.pAttachments =
            &graphics_pipeline_color_blend_attachment_state;
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
            .pStages = ctx_nsgt->pipeline_shader_stage_create_info,
            .layout = ctx_nsgt->pipeline_layout[i],
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
                &graphics_pipeline_depth_stencil_state_create_info,
            .pColorBlendState =
                &graphics_pipeline_color_blend_state_create_info,
            .pDynamicState = &graphics_pipeline_dynamic_state_create_info,
            .renderPass = render_pass,
            .subpass = 0,
        };
        result = vkCreateGraphicsPipelines(device,
                                           VK_NULL_HANDLE,
                                           1,
                                           &graphics_pipeline_create_info,
                                           NULL,
                                           &ctx_nsgt->pipeline[i]);
        assert(result == VK_SUCCESS);
    }
    //-------------------------------------------------------------------------
    // FLUSH
    //-------------------------------------------------------------------------
    memcpy(ctx_nsgt->map[ctx_nsgt->idx_offset_stage],
           offset,
           ctx_nsgt->offset_capacity);
    memcpy(ctx_nsgt->map[ctx_nsgt->idx_hop_stage], hop, ctx_nsgt->hop_capacity);
    memcpy(ctx_nsgt->map[ctx_nsgt->idx_length_stage],
           length,
           ctx_nsgt->length_capacity);
    memcpy(ctx_nsgt->map[ctx_nsgt->idx_window_stage],
           window,
           ctx_nsgt->window_capacity);
    memcpy(ctx_nsgt->map[ctx_nsgt->idx_dual_window_stage],
           dual_window,
           ctx_nsgt->window_capacity);
    memcpy(ctx_nsgt->map[ctx_nsgt->idx_frequency_stage],
           frequency,
           ctx_nsgt->frequency_capacity);
    memcpy(ctx_nsgt->map[ctx_nsgt->idx_cis_stage], cis, ctx_nsgt->cis_capacity);
    ctx_nsgt->fn_src_idx[0] = ctx_nsgt->idx_offset_stage;
    ctx_nsgt->fn_src_idx[1] = ctx_nsgt->idx_hop_stage;
    ctx_nsgt->fn_src_idx[2] = ctx_nsgt->idx_length_stage;
    ctx_nsgt->fn_src_idx[3] = ctx_nsgt->idx_window_stage;
    ctx_nsgt->fn_src_idx[4] = ctx_nsgt->idx_dual_window_stage;
    ctx_nsgt->fn_src_idx[5] = ctx_nsgt->idx_frequency_stage;
    ctx_nsgt->fn_src_idx[6] = ctx_nsgt->idx_cis_stage;
    ctx_nsgt->fn_idx_count = 7;
    war_nsgt_flush(ctx_nsgt->fn_idx_count,
                   ctx_nsgt->fn_src_idx,
                   NULL,
                   NULL,
                   device,
                   ctx_nsgt);
    // cleanup
    free(offset);
    free(hop);
    free(length);
    free(window);
    free(dual_window);
    free(frequency);
    free(cis);
    //-------------------------------------------------------------------------
    // RIGHT AFTER INITIALIZATION (ONE TIME) UNDEFINED -> GENERAL
    //-------------------------------------------------------------------------
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(cmd_buffer, &cmd_buffer_begin_info);
    assert(result == VK_SUCCESS);
    //   src
    ctx_nsgt->fn_src_idx[0] = ctx_nsgt->idx_offset_stage;
    ctx_nsgt->fn_src_idx[1] = ctx_nsgt->idx_hop_stage;
    ctx_nsgt->fn_src_idx[2] = ctx_nsgt->idx_length_stage;
    ctx_nsgt->fn_src_idx[3] = ctx_nsgt->idx_window_stage;
    ctx_nsgt->fn_src_idx[4] = ctx_nsgt->idx_dual_window_stage;
    ctx_nsgt->fn_src_idx[5] = ctx_nsgt->idx_frequency_stage;
    ctx_nsgt->fn_src_idx[6] = ctx_nsgt->idx_cis_stage;
    // dst
    ctx_nsgt->fn_dst_idx[0] = ctx_nsgt->idx_offset;
    ctx_nsgt->fn_dst_idx[1] = ctx_nsgt->idx_hop;
    ctx_nsgt->fn_dst_idx[2] = ctx_nsgt->idx_length;
    ctx_nsgt->fn_dst_idx[3] = ctx_nsgt->idx_window;
    ctx_nsgt->fn_dst_idx[4] = ctx_nsgt->idx_dual_window;
    ctx_nsgt->fn_dst_idx[5] = ctx_nsgt->idx_frequency;
    ctx_nsgt->fn_dst_idx[6] = ctx_nsgt->idx_cis;
    ctx_nsgt->fn_idx_count = 7;
    for (uint32_t i = 0; i < ctx_nsgt->fn_idx_count; i++) {
        ctx_nsgt->fn_pipeline_stage_flags[i] = VK_PIPELINE_STAGE_TRANSFER_BIT;
        ctx_nsgt->fn_access_flags[i] = VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    war_nsgt_barrier(ctx_nsgt->fn_idx_count,
                     ctx_nsgt->fn_dst_idx,
                     0,
                     0,
                     ctx_nsgt->fn_pipeline_stage_flags,
                     ctx_nsgt->fn_access_flags,
                     0,
                     cmd_buffer,
                     ctx_nsgt);
    for (uint32_t i = 0; i < ctx_nsgt->fn_idx_count; i++) {
        ctx_nsgt->fn_pipeline_stage_flags[i] = VK_PIPELINE_STAGE_TRANSFER_BIT;
        ctx_nsgt->fn_access_flags[i] = VK_ACCESS_TRANSFER_READ_BIT;
    }
    war_nsgt_barrier(ctx_nsgt->fn_idx_count,
                     ctx_nsgt->fn_src_idx,
                     0,
                     0,
                     ctx_nsgt->fn_pipeline_stage_flags,
                     ctx_nsgt->fn_access_flags,
                     0,
                     cmd_buffer,
                     ctx_nsgt);
    war_nsgt_copy(ctx_nsgt->fn_idx_count,
                  ctx_nsgt->fn_src_idx,
                  ctx_nsgt->fn_dst_idx,
                  0,
                  0,
                  ctx_nsgt->fn_size,
                  cmd_buffer,
                  ctx_nsgt);
    ctx_nsgt->fn_src_idx[0] = ctx_nsgt->idx_image;
    ctx_nsgt->fn_pipeline_stage_flags[0] = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    ctx_nsgt->fn_access_flags[0] = 0;
    ctx_nsgt->fn_image_layout[0] = VK_IMAGE_LAYOUT_GENERAL;
    ctx_nsgt->fn_idx_count = 1;
    war_nsgt_barrier(ctx_nsgt->fn_idx_count,
                     ctx_nsgt->fn_src_idx,
                     0,
                     0,
                     ctx_nsgt->fn_pipeline_stage_flags,
                     ctx_nsgt->fn_access_flags,
                     ctx_nsgt->fn_image_layout,
                     cmd_buffer,
                     ctx_nsgt);
    result = vkEndCommandBuffer(cmd_buffer);
    assert(result == VK_SUCCESS);
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buffer,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };
    result = vkResetFences(device, 1, &fence);
    assert(result == VK_SUCCESS);
    result = vkQueueSubmit(queue, 1, &submit_info, fence);
    assert(result == VK_SUCCESS);
    result = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
    assert(result == VK_SUCCESS);
    ctx_nsgt->graphics_viewport.x = 0.0f;
    ctx_nsgt->graphics_viewport.y = 0.0f;
    ctx_nsgt->graphics_viewport.width = (float)physical_width;
    ctx_nsgt->graphics_viewport.height = (float)physical_height;
    ctx_nsgt->graphics_viewport.minDepth = 0.0f,
    ctx_nsgt->graphics_viewport.maxDepth = 1.0f,
    ctx_nsgt->graphics_rect_2d.offset.x = 0;
    ctx_nsgt->graphics_rect_2d.extent.width = (uint32_t)physical_width;
    ctx_nsgt->graphics_rect_2d.extent.height = (uint32_t)physical_height;
    //-------------------------------------------------------------------------
    // FLAGS
    //-------------------------------------------------------------------------
    end("war_nsgt_init");
}

#endif // WAR_NSGT_H
