//-----------------------------------------------------------------------------
//
// WAR - make music with vim motions
// Copyright (C) 2025 Nick Monaco
//
// This file is part of WAR 1.0 software.
// WAR 1.0 software is licensed under the GNU Affero General Public License
// version 3, with the following modification: attribution to the original
// author is waived.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// For the full license text, see LICENSE-AGPL and LICENSE-CC-BY-SA and LICENSE.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// src/h/war_vulkan.h
//-----------------------------------------------------------------------------

#ifndef WAR_VULKAN_H
#define WAR_VULKAN_H

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

static inline uint8_t war_vulkan_get_shader_module(
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

static inline VkDeviceSize war_vulkan_align_size_up(VkDeviceSize size,
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

static inline VkDeviceSize war_vulkan_align_offset_down(VkDeviceSize offset,
                                                        VkDeviceSize alignment,
                                                        VkDeviceSize capacity) {
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

static inline void war_nsgt_flush(uint32_t idx_count,
                                  uint32_t* idx,
                                  VkDeviceSize* offset,
                                  VkDeviceSize* size,
                                  VkDevice device,
                                  war_nsgt_context* ctx_nsgt) {
    assert(idx_count <= ctx_nsgt->resource_count);
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
            ctx_nsgt->capacity[idx[i]]);
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
    assert(idx_count <= ctx_nsgt->resource_count);
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
            ctx_nsgt->capacity[idx[i]]);
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

static inline void war_nsgt_copy(VkCommandBuffer cmd,
                                 uint32_t idx_src,
                                 uint32_t idx_dst,
                                 VkDeviceSize src_offset,
                                 VkDeviceSize dst_offset,
                                 VkDeviceSize size,
                                 war_nsgt_context* ctx_nsgt) {
    VkBufferCopy copy = {
        .srcOffset = src_offset,
        .dstOffset = dst_offset,
        .size = size,
    };
    vkCmdCopyBuffer(
        cmd, ctx_nsgt->buffer[idx_src], ctx_nsgt->buffer[idx_dst], 1, &copy);
}

static inline void war_nsgt_buffer_barrier(uint32_t idx_count,
                                           uint32_t* idx,
                                           VkPipelineStageFlags dst_stage,
                                           VkAccessFlags dst_access,
                                           VkCommandBuffer cmd,
                                           war_nsgt_context* ctx_nsgt) {
    assert(idx_count <= ctx_nsgt->resource_count);
    VkPipelineStageFlags pipeline_stage_flags_mask = 0;
    for (uint32_t i = 0; i < idx_count; i++) {
        ctx_nsgt->buffer_memory_barrier[i] = (VkBufferMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = ctx_nsgt->access_flags[idx[i]],
            .dstAccessMask = dst_access,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = ctx_nsgt->buffer[idx[i]],
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        pipeline_stage_flags_mask |= ctx_nsgt->pipeline_stage_flags[idx[i]];
        ctx_nsgt->access_flags[idx[i]] = dst_access;
        ctx_nsgt->pipeline_stage_flags[idx[i]] = dst_stage;
    }
    vkCmdPipelineBarrier(cmd,
                         pipeline_stage_flags_mask,
                         dst_stage,
                         0, // dependencyFlags
                         0,
                         NULL, // memory barriers
                         idx_count,
                         ctx_nsgt->buffer_memory_barrier,
                         0,
                         NULL); // image barriers
}

static inline void war_nsgt_image_barrier(uint32_t idx_count,
                                          uint32_t* idx,
                                          VkPipelineStageFlags dst_stage,
                                          VkAccessFlags dst_access,
                                          VkImageLayout new_layout,
                                          VkCommandBuffer cmd,
                                          war_nsgt_context* ctx_nsgt) {
    assert(idx_count <= ctx_nsgt->resource_count);
    VkPipelineStageFlags pipeline_stage_flags_mask = 0;
    for (uint32_t i = 0; i < idx_count; i++) {
        ctx_nsgt->image_memory_barrier[i] = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = NULL,
            .srcAccessMask = ctx_nsgt->access_flags[idx[i]],
            .dstAccessMask = dst_access,
            .oldLayout = ctx_nsgt->image_layout[idx[i]],
            .newLayout = new_layout,
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
        pipeline_stage_flags_mask |= ctx_nsgt->pipeline_stage_flags[idx[i]];
        ctx_nsgt->image_layout[idx[i]] = new_layout;
        ctx_nsgt->access_flags[idx[i]] = dst_access;
        ctx_nsgt->pipeline_stage_flags[idx[i]] = dst_stage;
    }
    vkCmdPipelineBarrier(cmd,
                         pipeline_stage_flags_mask,
                         dst_stage,
                         0, // dependencyFlags
                         0,
                         NULL, // memory barriers
                         0,
                         NULL, // buffer barriers
                         idx_count,
                         ctx_nsgt->image_memory_barrier);
}

static inline void war_vulkan_init(war_vulkan_context* ctx_vk,
                                   war_lua_context* ctx_lua,
                                   war_pool* pool_wr,
                                   uint32_t width,
                                   uint32_t height) {
    header("war_vulkan_init");
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
    VkResult result = vkCreateInstance(&instance_info, NULL, &ctx_vk->instance);
    assert(result == VK_SUCCESS);
    enum {
        max_gpu_count = 10,
    };
    uint32_t gpu_count = 0;
    VkPhysicalDevice physical_devices[max_gpu_count];
    vkEnumeratePhysicalDevices(ctx_vk->instance, &gpu_count, NULL);
    assert(gpu_count != 0 && gpu_count <= max_gpu_count);
    vkEnumeratePhysicalDevices(ctx_vk->instance, &gpu_count, physical_devices);
    ctx_vk->physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties device_props;
    for (uint32_t i = 0; i < gpu_count; i++) {
        vkGetPhysicalDeviceProperties(physical_devices[i], &device_props);
        call_king_terry("Found GPU %u: %s (vendorID=0x%x, deviceID=0x%x)",
                        i,
                        device_props.deviceName,
                        device_props.vendorID,
                        device_props.deviceID);
        if (device_props.vendorID == 0x8086) {
            ctx_vk->physical_device = physical_devices[i];
            call_king_terry("Selected Intel GPU: %s", device_props.deviceName);
            break;
        }
    }
    if (ctx_vk->physical_device == VK_NULL_HANDLE) {
        ctx_vk->physical_device = physical_devices[0];
        vkGetPhysicalDeviceProperties(ctx_vk->physical_device, &device_props);
        call_king_terry("Fallback GPU selected: %s (vendorID=0x%x)",
                        device_props.deviceName,
                        device_props.vendorID);
    }
    assert(ctx_vk->physical_device != VK_NULL_HANDLE);
    ctx_vk->non_coherent_atom_size = device_props.limits.nonCoherentAtomSize;
    ctx_vk->min_memory_map_alignment =
        device_props.limits.minMemoryMapAlignment;
    ctx_vk->min_uniform_buffer_offset_alignment =
        device_props.limits.minUniformBufferOffsetAlignment;
    ctx_vk->min_storage_buffer_offset_alignment =
        device_props.limits.minStorageBufferOffsetAlignment;
    uint32_t device_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(
        ctx_vk->physical_device, NULL, &device_extension_count, NULL);
    VkExtensionProperties* device_extensions_properties =
        malloc(sizeof(VkExtensionProperties) * device_extension_count);
    vkEnumerateDeviceExtensionProperties(ctx_vk->physical_device,
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
        ctx_vk->physical_device, NULL, &extension_count, NULL);
    VkExtensionProperties* available_extensions =
        malloc(sizeof(VkExtensionProperties) * extension_count);
    vkEnumerateDeviceExtensionProperties(
        ctx_vk->physical_device, NULL, &extension_count, available_extensions);
#if DEBUG
    for (uint32_t i = 0; i < extension_count; i++) {
        call_king_terry("%s", available_extensions[i].extensionName);
    }
#endif
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
        ctx_vk->physical_device, &queue_family_count, NULL);
    if (queue_family_count > max_family_count) {
        queue_family_count = max_family_count;
    }
    VkQueueFamilyProperties queue_families[max_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(
        ctx_vk->physical_device, &queue_family_count, queue_families);
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
            .queueFamilyIndex = ctx_vk->queue_family_index,
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
    result = vkCreateDevice(
        ctx_vk->physical_device, &device_info, NULL, &ctx_vk->device);
    assert(result == VK_SUCCESS);
    ctx_vk->vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(
        ctx_vk->device, "vkGetSemaphoreFdKHR");
    assert(ctx_vk->vkGetSemaphoreFdKHR && "failed to load vkGetSemaphoreFdKHR");
    ctx_vk->vkImportSemaphoreFdKHR =
        (PFN_vkImportSemaphoreFdKHR)vkGetDeviceProcAddr(
            ctx_vk->device, "vkImportSemaphoreFdKHR");
    assert(ctx_vk->vkGetSemaphoreFdKHR &&
           "failed to load vkImportSemaphoreFdKHR");
    VkFormat quad_depth_format = VK_FORMAT_D32_SFLOAT;
    VkImageCreateInfo quad_depth_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = quad_depth_format,
        .extent = {width, height, 1},
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
    VkResult r = vkCreateImage(
        ctx_vk->device, &quad_depth_image_info, NULL, &quad_depth_image);
    assert(r == VK_SUCCESS);
    VkMemoryRequirements quad_depth_mem_reqs;
    vkGetImageMemoryRequirements(
        ctx_vk->device, quad_depth_image, &quad_depth_mem_reqs);
    VkPhysicalDeviceMemoryProperties quad_depth_mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
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
    r = vkAllocateMemory(
        ctx_vk->device, &quad_depth_alloc_info, NULL, &quad_depth_image_memory);
    assert(r == VK_SUCCESS);
    r = vkBindImageMemory(
        ctx_vk->device, quad_depth_image, quad_depth_image_memory, 0);
    assert(r == VK_SUCCESS);
    // --- create ctx_vk->image view ---
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
    r = vkCreateImageView(
        ctx_vk->device, &quad_depth_view_info, NULL, &quad_depth_image_view);
    assert(r == VK_SUCCESS);
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
    vkGetDeviceQueue(
        ctx_vk->device, ctx_vk->queue_family_index, 0, &ctx_vk->queue);
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ctx_vk->queue_family_index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    result = vkCreateCommandPool(
        ctx_vk->device, &pool_info, NULL, &ctx_vk->cmd_pool);
    assert(result == VK_SUCCESS);
    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx_vk->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    result = vkAllocateCommandBuffers(
        ctx_vk->device, &cmd_buf_info, &ctx_vk->cmd_buffer);
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
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    result =
        vkCreateImage(ctx_vk->device, &image_create_info, NULL, &ctx_vk->image);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(ctx_vk->device, ctx_vk->image, &mem_reqs);
    VkExportMemoryAllocateInfo export_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &mem_properties);
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    uint32_t memory_type = 0;
    uint8_t found_memory_type = 0;
    call_king_terry("Looking for ctx_vk->memory type with properties: 0x%x",
                    properties);
    call_king_terry("Available ctx_vk->memory types:");
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        VkMemoryPropertyFlags flags =
            mem_properties.memoryTypes[i].propertyFlags;
        call_king_terry("Type %u: flags=0x%x", i, flags);

        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (flags & properties) == properties) {
            call_king_terry("-> Selected ctx_vk->memory type %u", i);
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
    result = vkAllocateMemory(
        ctx_vk->device, &mem_alloc_info, NULL, &ctx_vk->memory);
    assert(result == VK_SUCCESS);
    result =
        vkBindImageMemory(ctx_vk->device, ctx_vk->image, ctx_vk->memory, 0);
    assert(result == VK_SUCCESS);
    VkMemoryGetFdInfoKHR get_fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = ctx_vk->memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    int dmabuf_fd = -1;
    PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR =
        (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(ctx_vk->device,
                                                  "vkGetMemoryFdKHR");
    result = vkGetMemoryFdKHR(ctx_vk->device, &get_fd_info, &ctx_vk->dmabuf_fd);
    assert(result == VK_SUCCESS);
    assert(ctx_vk->dmabuf_fd > 0);
    int flags = fcntl(ctx_vk->dmabuf_fd, F_GETFD);
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
    result = vkCreateRenderPass(
        ctx_vk->device, &render_pass_info, NULL, &ctx_vk->render_pass);
    assert(result == VK_SUCCESS);
    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx_vk->image,
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
    result = vkCreateImageView(
        ctx_vk->device, &image_view_info, NULL, &ctx_vk->image_view);
    assert(result == VK_SUCCESS);
    VkImageView quad_fb_attachments[2] = {ctx_vk->image_view,
                                          quad_depth_image_view};
    VkFramebufferCreateInfo frame_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = ctx_vk->render_pass,
        .attachmentCount = 2,
        .pAttachments = quad_fb_attachments,
        .width = width,
        .height = height,
        .layers = 1,
    };
    result = vkCreateFramebuffer(
        ctx_vk->device, &frame_buffer_info, NULL, &ctx_vk->frame_buffer);
    assert(result == VK_SUCCESS);

    uint32_t* vertex_code;
    const char* vertex_path = "build/spv/war_quad_vertex.spv";
    FILE* vertex_spv = fopen(vertex_path, "rb");
    assert(vertex_spv);
    fseek(vertex_spv, 0, SEEK_END);
    long vertex_size = ftell(vertex_spv);
    fseek(vertex_spv, 0, SEEK_SET);
    assert(vertex_size > 0 &&
           (vertex_size % 4 == 0)); // SPIR-V is 4-byte aligned
    vertex_code = malloc(vertex_size);
    assert(vertex_code);
    size_t vertex_spv_read = fread(vertex_code, 1, vertex_size, vertex_spv);
    assert(vertex_spv_read == (size_t)vertex_size);
    fclose(vertex_spv);
    VkShaderModuleCreateInfo vertex_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertex_size,
        .pCode = vertex_code};
    VkShaderModule vertex_shader;
    result = vkCreateShaderModule(
        ctx_vk->device, &vertex_shader_info, NULL, &vertex_shader);
    assert(result == VK_SUCCESS);
    free(vertex_code);
    uint32_t* fragment_code;
    const char* fragment_path = "build/spv/war_quad_fragment.spv";
    FILE* fragment_spv = fopen(fragment_path, "rb");
    assert(fragment_spv);
    fseek(fragment_spv, 0, SEEK_END);
    long fragment_size = ftell(fragment_spv);
    fseek(fragment_spv, 0, SEEK_SET);
    assert(fragment_size > 0 &&
           (fragment_size % 4 == 0)); // SPIR-V is 4-byte aligned
    fragment_code = malloc(fragment_size);
    assert(fragment_code);
    size_t fragment_spv_read =
        fread(fragment_code, 1, fragment_size, fragment_spv);
    assert(fragment_spv_read == (size_t)fragment_size);
    fclose(fragment_spv);
    VkShaderModuleCreateInfo fragment_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragment_size,
        .pCode = fragment_code};
    VkShaderModule fragment_shader;
    result = vkCreateShaderModule(
        ctx_vk->device, &fragment_shader_info, NULL, &fragment_shader);
    assert(result == VK_SUCCESS);
    free(fragment_code);
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL,
    };
    VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_binding,
    };
    VkDescriptorSetLayout descriptor_set_layout;
    vkCreateDescriptorSetLayout(
        ctx_vk->device, &descriptor_layout_info, NULL, &descriptor_set_layout);
    VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader,
            .pName = "main",
        }};
    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(war_quad_push_constant),
    };
    VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    result = vkCreatePipelineLayout(
        ctx_vk->device, &layout_info, NULL, &ctx_vk->pipeline_layout);
    assert(result == VK_SUCCESS);
    VkVertexInputBindingDescription quad_vertex_binding = {
        .binding = 0,
        .stride = sizeof(war_quad_vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputBindingDescription quad_instance_binding = {
        .binding = 1,
        .stride = sizeof(war_quad_instance),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    };
    VkVertexInputAttributeDescription quad_vertex_attrs[] = {
        (VkVertexInputAttributeDescription){
            .location = 0,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, corner),
            .format = VK_FORMAT_R32G32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 1,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, pos),
            .format = VK_FORMAT_R32G32B32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 2,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, color),
            .format = VK_FORMAT_R8G8B8A8_UNORM,
        },
        (VkVertexInputAttributeDescription){
            .location = 3,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, outline_thickness),
            .format = VK_FORMAT_R32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 4,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, outline_color),
            .format = VK_FORMAT_R8G8B8A8_UNORM,
        },
        (VkVertexInputAttributeDescription){
            .location = 5,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, line_thickness),
            .format = VK_FORMAT_R32G32_SFLOAT,
        },
        (VkVertexInputAttributeDescription){
            .location = 6,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, flags),
            .format = VK_FORMAT_R32_UINT,
        },
        (VkVertexInputAttributeDescription){
            .location = 7,
            .binding = 0,
            .offset = offsetof(war_quad_vertex, span),
            .format = VK_FORMAT_R32G32_SFLOAT,
        },
    };
    uint32_t num_quad_vertex_attrs = 8;
    VkVertexInputAttributeDescription quad_instance_attrs[] = {
        (VkVertexInputAttributeDescription){.location = num_quad_vertex_attrs,
                                            .binding = 1,
                                            .format = VK_FORMAT_R32_UINT,
                                            .offset =
                                                offsetof(war_quad_instance, x)},
        (VkVertexInputAttributeDescription){
            .location = num_quad_vertex_attrs + 1,
            .binding = 1,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(war_quad_instance, y)},
        (VkVertexInputAttributeDescription){
            .location = num_quad_vertex_attrs + 2,
            .binding = 1,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(war_quad_instance, color)},
        (VkVertexInputAttributeDescription){
            .location = num_quad_vertex_attrs + 3,
            .binding = 1,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(war_quad_instance, flags)},
    };
    uint32_t num_quad_instance_attrs = 4;
    VkVertexInputAttributeDescription* all_attrs =
        malloc(sizeof(VkVertexInputAttributeDescription) *
               (num_quad_vertex_attrs + num_quad_instance_attrs));
    memcpy(all_attrs,
           quad_vertex_attrs,
           sizeof(VkVertexInputAttributeDescription) * num_quad_vertex_attrs);
    memcpy(all_attrs + num_quad_vertex_attrs,
           quad_instance_attrs,
           sizeof(VkVertexInputAttributeDescription) * num_quad_instance_attrs);
    VkVertexInputBindingDescription all_bindings[] = {quad_vertex_binding,
                                                      quad_instance_binding};
    VkPipelineVertexInputStateCreateInfo quad_vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = all_bindings,
        .vertexAttributeDescriptionCount =
            num_quad_instance_attrs + num_quad_vertex_attrs,
        .pVertexAttributeDescriptions = all_attrs,
    };
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {width, height},
    };
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,  // enable depth testing
        .depthWriteEnable = VK_TRUE, // enable writing to depth buffer
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .stencilTestEnable = VK_FALSE,
    };
    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &quad_vertex_input,
        .pInputAssemblyState =
            &(VkPipelineInputAssemblyStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            },
        .pViewportState =
            &(VkPipelineViewportStateCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .pViewports = &viewport,
                .scissorCount = 1,
                .pScissors = &scissor,
            },
        .pDepthStencilState = &depth_stencil,
        .pRasterizationState =
            &(VkPipelineRasterizationStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f,
            },
        .pMultisampleState =
            &(VkPipelineMultisampleStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            },
        .pColorBlendState =
            &(VkPipelineColorBlendStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments =
                    (VkPipelineColorBlendAttachmentState[]){
                        {
                            .blendEnable = VK_TRUE, // enable blending
                            .srcColorBlendFactor =
                                VK_BLEND_FACTOR_SRC_ALPHA, // source color
                                                           // weight
                            .dstColorBlendFactor =
                                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // dest
                                                                     // color
                                                                     // weight
                            .colorBlendOp = VK_BLEND_OP_ADD, // combine src+dst
                            .srcAlphaBlendFactor =
                                VK_BLEND_FACTOR_ONE, // source alpha
                            .dstAlphaBlendFactor =
                                VK_BLEND_FACTOR_ZERO,        // dest alpha
                            .alphaBlendOp = VK_BLEND_OP_ADD, // combine alpha
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,
                        },
                    },
            },
        .layout = ctx_vk->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
        .subpass = 0,
        .pDynamicState = NULL,
    };
    result = vkCreateGraphicsPipelines(ctx_vk->device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &pipeline_info,
                                       NULL,
                                       &ctx_vk->quad_pipeline);
    assert(result == VK_SUCCESS);
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx_vk->image,
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
        ctx_vk->cmd_buffer,
        &(VkCommandBufferBeginInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        });
    vkCmdPipelineBarrier(ctx_vk->cmd_buffer,
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
    vkEndCommandBuffer(ctx_vk->cmd_buffer);
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &ctx_vk->cmd_buffer,
    };
    vkQueueSubmit(ctx_vk->queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_vk->queue);
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    uint32_t max_frames = atomic_load(&ctx_lua->VK_MAX_FRAMES);
    ctx_vk->in_flight_fences =
        war_pool_alloc(pool_wr, sizeof(VkFence) * max_frames);
    for (size_t i = 0; i < max_frames; i++) {
        result = vkCreateFence(
            ctx_vk->device, &fence_info, NULL, &ctx_vk->in_flight_fences[i]);
        assert(result == VK_SUCCESS);
    }
    uint32_t quads_max = atomic_load(&ctx_lua->WR_QUADS_MAX);
    uint32_t quads_vertices_max = quads_max * 4;
    uint32_t quads_indices_max = quads_max * 6;
    VkBufferCreateInfo quads_vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = quads_vertices_max * sizeof(war_quad_vertex) * max_frames,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    result = vkCreateBuffer(ctx_vk->device,
                            &quads_vertex_buffer_info,
                            NULL,
                            &ctx_vk->quads_vertex_buffer);
    assert(result == VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device,
        ctx_vk->quads_vertex_buffer,
        &ctx_vk->quads_vertex_buffer_memory_requirements);
    VkBufferCreateInfo quads_index_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = quads_indices_max * sizeof(uint32_t) * max_frames,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    result = vkCreateBuffer(ctx_vk->device,
                            &quads_index_buffer_info,
                            NULL,
                            &ctx_vk->quads_index_buffer);
    assert(result == VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device,
        ctx_vk->quads_index_buffer,
        &ctx_vk->quads_index_buffer_memory_requirements);
    VkBufferCreateInfo quads_instance_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 1,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    result = vkCreateBuffer(ctx_vk->device,
                            &quads_instance_buffer_info,
                            NULL,
                            &ctx_vk->quads_instance_buffer);
    assert(result == VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device,
        ctx_vk->quads_instance_buffer,
        &ctx_vk->quads_instance_buffer_memory_requirements);
    VkMemoryRequirements quads_vertex_mem_reqs;
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_vk->quads_vertex_buffer, &quads_vertex_mem_reqs);
    uint32_t quads_vertex_memory_type_index = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties quads_vertex_mem_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &quads_vertex_mem_properties);
    for (uint32_t i = 0; i < quads_vertex_mem_properties.memoryTypeCount; i++) {
        if ((quads_vertex_mem_reqs.memoryTypeBits & (1 << i)) &&
            (quads_vertex_mem_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            quads_vertex_memory_type_index = i;
            break;
        }
    }
    assert(quads_vertex_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo quads_vertex_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quads_vertex_mem_reqs.size,
        .memoryTypeIndex = quads_vertex_memory_type_index,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &quads_vertex_alloc_info,
                              NULL,
                              &ctx_vk->quads_vertex_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(ctx_vk->device,
                                ctx_vk->quads_vertex_buffer,
                                ctx_vk->quads_vertex_buffer_memory,
                                0);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements quads_index_mem_reqs;
    vkGetBufferMemoryRequirements(
        ctx_vk->device, ctx_vk->quads_index_buffer, &quads_index_mem_reqs);
    uint32_t quads_index_memory_type_index = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties quads_index_mem_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &quads_index_mem_properties);
    for (uint32_t i = 0; i < quads_index_mem_properties.memoryTypeCount; i++) {
        if ((quads_index_mem_reqs.memoryTypeBits & (1 << i)) &&
            (quads_index_mem_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            quads_index_memory_type_index = i;
            break;
        }
    }
    assert(quads_index_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo quads_index_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quads_index_mem_reqs.size,
        .memoryTypeIndex = quads_index_memory_type_index,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &quads_index_alloc_info,
                              NULL,
                              &ctx_vk->quads_index_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(ctx_vk->device,
                                ctx_vk->quads_index_buffer,
                                ctx_vk->quads_index_buffer_memory,
                                0);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements quads_instance_mem_reqs;
    vkGetBufferMemoryRequirements(ctx_vk->device,
                                  ctx_vk->quads_instance_buffer,
                                  &quads_instance_mem_reqs);
    uint32_t quads_instance_memory_type_index = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties quads_instance_mem_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &quads_instance_mem_properties);
    for (uint32_t i = 0; i < quads_instance_mem_properties.memoryTypeCount;
         i++) {
        if ((quads_instance_mem_reqs.memoryTypeBits & (1 << i)) &&
            (quads_instance_mem_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            quads_instance_memory_type_index = i;
            break;
        }
    }
    assert(quads_instance_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo quads_instance_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = quads_instance_mem_reqs.size,
        .memoryTypeIndex = quads_instance_memory_type_index,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &quads_instance_alloc_info,
                              NULL,
                              &ctx_vk->quads_instance_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(ctx_vk->device,
                                ctx_vk->quads_instance_buffer,
                                ctx_vk->quads_instance_buffer_memory,
                                0);
    assert(result == VK_SUCCESS);
    VkImageCreateInfo texture_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    vkCreateImage(
        ctx_vk->device, &texture_image_info, NULL, &ctx_vk->texture_image);
    VkMemoryRequirements texture_image_mem_reqs;
    vkGetImageMemoryRequirements(
        ctx_vk->device, ctx_vk->texture_image, &texture_image_mem_reqs);
    VkPhysicalDeviceMemoryProperties texture_image_mem_props;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &texture_image_mem_props);
    uint32_t texture_image_memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < texture_image_mem_props.memoryTypeCount; i++) {
        if ((texture_image_mem_reqs.memoryTypeBits & (1 << i)) &&
            (texture_image_mem_props.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            texture_image_memory_type_index = i;
            break;
        }
    }
    assert(texture_image_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo texture_image_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = texture_image_mem_reqs.size,
        .memoryTypeIndex = texture_image_memory_type_index,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &texture_image_alloc_info,
                              NULL,
                              &ctx_vk->texture_memory);
    assert(result == VK_SUCCESS);
    result = vkBindImageMemory(
        ctx_vk->device, ctx_vk->texture_image, ctx_vk->texture_memory, 0);
    assert(result == VK_SUCCESS);
    VkSamplerCreateInfo sampler_info = {0};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    result = vkCreateSampler(
        ctx_vk->device, &sampler_info, NULL, &ctx_vk->texture_sampler);
    assert(result == VK_SUCCESS);
    VkImageViewCreateInfo view_info = {0};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = ctx_vk->texture_image; // Your VkImage handle
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    result = vkCreateImageView(
        ctx_vk->device, &view_info, NULL, &ctx_vk->texture_image_view);
    assert(result == VK_SUCCESS);
    VkDescriptorPoolSize descriptor_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &descriptor_pool_size,
        .maxSets = 1,
    };
    vkCreateDescriptorPool(ctx_vk->device,
                           &descriptor_pool_info,
                           NULL,
                           &ctx_vk->font_descriptor_pool);
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx_vk->font_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_set_layout,
    };
    vkAllocateDescriptorSets(
        ctx_vk->device, &alloc_info, &ctx_vk->font_descriptor_set);
    VkDescriptorImageInfo descriptor_image_info = {
        .sampler = ctx_vk->texture_sampler,
        .imageView = ctx_vk->texture_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ctx_vk->font_descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &descriptor_image_info,
    };
    vkUpdateDescriptorSets(ctx_vk->device, 1, &descriptor_write, 0, NULL);
    ctx_vk->quads_vertex_buffer_mapped = war_pool_alloc(pool_wr, sizeof(void*));
    vkMapMemory(ctx_vk->device,
                ctx_vk->quads_vertex_buffer_memory,
                0,
                sizeof(war_quad_vertex) * quads_vertices_max * max_frames,
                0,
                &ctx_vk->quads_vertex_buffer_mapped);
    ctx_vk->quads_index_buffer_mapped = war_pool_alloc(pool_wr, sizeof(void*));
    vkMapMemory(ctx_vk->device,
                ctx_vk->quads_index_buffer_memory,
                0,
                sizeof(uint32_t) * quads_indices_max * max_frames,
                0,
                &ctx_vk->quads_index_buffer_mapped);
    ctx_vk->quads_instance_buffer_mapped =
        war_pool_alloc(pool_wr, sizeof(void*));
    vkMapMemory(ctx_vk->device,
                ctx_vk->quads_instance_buffer_memory,
                0,
                1,
                0,
                &ctx_vk->quads_instance_buffer_mapped);
    //-------------------------------------------------------------------------
    // TEXT PIPELINE
    //-------------------------------------------------------------------------
    FT_Init_FreeType(&ctx_vk->ft_library);
    FT_New_Face(ctx_vk->ft_library,
                "assets/fonts/FreeMono.otf",
                0,
                &ctx_vk->ft_regular);
    // font config
    float font_pixel_height = atomic_load(&ctx_lua->VK_FONT_PIXEL_HEIGHT);
    FT_Set_Pixel_Sizes(ctx_vk->ft_regular,
                       0,
                       (int)atomic_load(&ctx_lua->VK_FONT_PIXEL_HEIGHT));
    ctx_vk->ascent = ctx_vk->ft_regular->size->metrics.ascender / 64.0f;
    ctx_vk->descent = ctx_vk->ft_regular->size->metrics.descender / 64.0f;
    ctx_vk->cell_height = ctx_vk->ft_regular->size->metrics.height / 64.0f;
    ctx_vk->font_height = ctx_vk->ascent - ctx_vk->descent;
    ctx_vk->line_gap = ctx_vk->cell_height - ctx_vk->font_height;
    ctx_vk->baseline = ctx_vk->ascent + ctx_vk->line_gap / 2.0f;
    ctx_vk->cell_width = 0;
    int atlas_width = atomic_load(&ctx_lua->VK_ATLAS_WIDTH);
    int atlas_height = atomic_load(&ctx_lua->VK_ATLAS_HEIGHT);
    ctx_vk->glyph_count = atomic_load(&ctx_lua->VK_GLYPH_COUNT);
    uint8_t* atlas_pixels = malloc(atlas_width * atlas_height);
    assert(atlas_pixels);
    ctx_vk->glyphs =
        war_pool_alloc(pool_wr, sizeof(war_glyph_info) * ctx_vk->glyph_count);
    int pen_x = 0, pen_y = 0, row_height = 0;
    for (int c = 0; c < ctx_vk->glyph_count; c++) {
        FT_Load_Char(ctx_vk->ft_regular, c, FT_LOAD_RENDER);
        if (c == 'M') {
            call_king_terry("for monospaced fonts");
            ctx_vk->cell_width = ctx_vk->ft_regular->glyph->advance.x / 64.0f;
        }
        FT_Bitmap* bmp = &ctx_vk->ft_regular->glyph->bitmap;
        int w = bmp->width;
        int h = bmp->rows;
        if (pen_x + w >= atlas_width) {
            pen_x = 0;
            pen_y += row_height + 1;
            row_height = 0;
        }
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                atlas_pixels[(pen_x + x) + (pen_y + y) * atlas_width] =
                    bmp->buffer[x + y * bmp->width];
            }
        }
        ctx_vk->glyphs[c].advance_x =
            ctx_vk->ft_regular->glyph->advance.x / 64.0f;
        ctx_vk->glyphs[c].advance_y =
            ctx_vk->ft_regular->glyph->advance.y / 64.0f;
        ctx_vk->glyphs[c].bearing_x = ctx_vk->ft_regular->glyph->bitmap_left;
        ctx_vk->glyphs[c].bearing_y = ctx_vk->ft_regular->glyph->bitmap_top;
        ctx_vk->glyphs[c].width = w;
        ctx_vk->glyphs[c].height = h;
        ctx_vk->glyphs[c].uv_x0 = (float)pen_x / atlas_width;
        ctx_vk->glyphs[c].uv_y0 = (float)pen_y / atlas_height;
        ctx_vk->glyphs[c].uv_x1 = (float)(pen_x + w) / atlas_width;
        ctx_vk->glyphs[c].uv_y1 = (float)(pen_y + h) / atlas_height;
        ctx_vk->glyphs[c].ascent =
            ctx_vk->ft_regular->glyph->metrics.horiBearingY / 64.0f;
        ctx_vk->glyphs[c].descent =
            (ctx_vk->ft_regular->glyph->metrics.height / 64.0f) -
            ctx_vk->glyphs[c].ascent;
        pen_x += w + 1;
        if (h > row_height) { row_height = h; }
    }
    assert(ctx_vk->cell_width != 0);
    VkImageCreateInfo sdf_image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM, // single-channel grayscale
        .extent = {atlas_width, atlas_height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    result = vkCreateImage(
        ctx_vk->device, &sdf_image_info, NULL, &ctx_vk->text_image);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements sdf_memory_requirements;
    vkGetImageMemoryRequirements(
        ctx_vk->device, ctx_vk->text_image, &sdf_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_image_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &sdf_image_memory_properties);
    uint32_t sdf_image_memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < sdf_image_memory_properties.memoryTypeCount; i++) {
        if ((sdf_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_image_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            sdf_image_memory_type_index = i;
            break;
        }
    }
    assert(sdf_image_memory_type_index != UINT32_MAX);
    VkMemoryAllocateInfo sdf_image_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_memory_requirements.size,
        .memoryTypeIndex = sdf_image_memory_type_index,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &sdf_image_allocate_info,
                              NULL,
                              &ctx_vk->text_image_memory);
    assert(result == VK_SUCCESS);
    result = vkBindImageMemory(
        ctx_vk->device, ctx_vk->text_image, ctx_vk->text_image_memory, 0);
    assert(result == VK_SUCCESS);
    VkImageViewCreateInfo sdf_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = ctx_vk->text_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8_UNORM,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    result = vkCreateImageView(
        ctx_vk->device, &sdf_view_info, NULL, &ctx_vk->text_image_view);
    assert(result == VK_SUCCESS);
    VkSamplerCreateInfo sdf_sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    vkCreateSampler(
        ctx_vk->device, &sdf_sampler_info, NULL, &ctx_vk->text_sampler);
    VkDeviceSize sdf_image_size = atlas_width * atlas_height;
    VkBuffer sdf_staging_buffer;
    VkDeviceMemory sdf_staging_buffer_memory;
    VkBufferCreateInfo sdf_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_image_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    result = vkCreateBuffer(
        ctx_vk->device, &sdf_buffer_info, NULL, &sdf_staging_buffer);
    assert(result == VK_SUCCESS);
    VkMemoryRequirements sdf_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(
        ctx_vk->device, sdf_staging_buffer, &sdf_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &sdf_buffer_memory_properties);
    uint32_t sdf_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0; i < sdf_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_buffer_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_buffer_memory_type,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &sdf_buffer_allocate_info,
                              NULL,
                              &sdf_staging_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(
        ctx_vk->device, sdf_staging_buffer, sdf_staging_buffer_memory, 0);
    assert(result == VK_SUCCESS);
    void* sdf_buffer_data;
    vkMapMemory(ctx_vk->device,
                sdf_staging_buffer_memory,
                0,
                sdf_image_size,
                0,
                &sdf_buffer_data);
    memcpy(sdf_buffer_data, atlas_pixels, (size_t)sdf_image_size);
    vkUnmapMemory(ctx_vk->device, sdf_staging_buffer_memory);
    VkCommandBufferAllocateInfo sdf_command_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool =
            ctx_vk->cmd_pool, // reuse your existing command pool here
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer sdf_copy_command_buffer;
    vkAllocateCommandBuffers(ctx_vk->device,
                             &sdf_command_buffer_allocate_info,
                             &sdf_copy_command_buffer);
    VkCommandBufferBeginInfo sdf_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(sdf_copy_command_buffer, &sdf_begin_info);
    VkImageMemoryBarrier barrier_to_transfer = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx_vk->text_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    vkCmdPipelineBarrier(sdf_copy_command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier_to_transfer);
    VkBufferImageCopy sdf_region = {
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
                .width = atlas_width,
                .height = atlas_height,
                .depth = 1,
            },
    };
    vkCmdCopyBufferToImage(sdf_copy_command_buffer,
                           sdf_staging_buffer,
                           ctx_vk->text_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &sdf_region);
    VkImageMemoryBarrier barrier_to_shader_read = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = ctx_vk->text_image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    vkCmdPipelineBarrier(sdf_copy_command_buffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         NULL,
                         0,
                         NULL,
                         1,
                         &barrier_to_shader_read);
    vkEndCommandBuffer(sdf_copy_command_buffer);
    VkSubmitInfo sdf_submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &sdf_copy_command_buffer,
    };
    vkQueueSubmit(ctx_vk->queue, 1, &sdf_submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_vk->queue);
    vkFreeCommandBuffers(
        ctx_vk->device, ctx_vk->cmd_pool, 1, &sdf_copy_command_buffer);
    vkDestroyBuffer(ctx_vk->device, sdf_staging_buffer, NULL);
    vkFreeMemory(ctx_vk->device, sdf_staging_buffer_memory, NULL);
    VkDescriptorSetLayoutBinding sdf_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    VkDescriptorSetLayoutCreateInfo sdf_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sdf_binding,
    };
    vkCreateDescriptorSetLayout(ctx_vk->device,
                                &sdf_layout_info,
                                NULL,
                                &ctx_vk->font_descriptor_set_layout);
    VkDescriptorPoolSize sdf_pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
    };
    VkDescriptorPoolCreateInfo sdf_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &sdf_pool_size,
        .maxSets = 1,
    };
    vkCreateDescriptorPool(
        ctx_vk->device, &sdf_pool_info, NULL, &ctx_vk->font_descriptor_pool);
    VkDescriptorSetAllocateInfo sdf_descriptor_set_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = ctx_vk->font_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &ctx_vk->font_descriptor_set_layout,
    };
    vkAllocateDescriptorSets(ctx_vk->device,
                             &sdf_descriptor_set_allocate_info,
                             &ctx_vk->font_descriptor_set);
    VkDescriptorImageInfo sdf_descriptor_info = {
        .sampler = ctx_vk->text_sampler,
        .imageView = ctx_vk->text_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkWriteDescriptorSet write_descriptor_sets = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = ctx_vk->font_descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &sdf_descriptor_info,
    };
    vkUpdateDescriptorSets(ctx_vk->device, 1, &write_descriptor_sets, 0, NULL);
    uint32_t* sdf_vertex_code;
    const char* sdf_vertex_path = "build/spv/war_text_vertex.spv";
    FILE* sdf_vertex_spv = fopen(sdf_vertex_path, "rb");
    assert(sdf_vertex_spv);
    fseek(sdf_vertex_spv, 0, SEEK_END);
    long sdf_vertex_size = ftell(sdf_vertex_spv);
    fseek(sdf_vertex_spv, 0, SEEK_SET);
    assert(sdf_vertex_size > 0 &&
           (sdf_vertex_size % 4 == 0)); // SPIR-V is 4-byte aligned
    sdf_vertex_code = malloc(sdf_vertex_size);
    assert(sdf_vertex_code);
    size_t sdf_vertex_spv_read =
        fread(sdf_vertex_code, 1, sdf_vertex_size, sdf_vertex_spv);
    assert(sdf_vertex_spv_read == (size_t)sdf_vertex_size);
    fclose(sdf_vertex_spv);
    VkShaderModuleCreateInfo sdf_vertex_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sdf_vertex_size,
        .pCode = sdf_vertex_code};
    result = vkCreateShaderModule(ctx_vk->device,
                                  &sdf_vertex_shader_info,
                                  NULL,
                                  &ctx_vk->text_vertex_shader);
    assert(result == VK_SUCCESS);
    free(sdf_vertex_code);
    uint32_t* sdf_fragment_code;
    const char* sdf_fragment_path = "build/spv/war_text_fragment.spv";
    FILE* sdf_fragment_spv = fopen(sdf_fragment_path, "rb");
    assert(sdf_fragment_spv);
    fseek(sdf_fragment_spv, 0, SEEK_END);
    long sdf_fragment_size = ftell(sdf_fragment_spv);
    fseek(sdf_fragment_spv, 0, SEEK_SET);
    assert(sdf_fragment_size > 0 &&
           (sdf_fragment_size % 4 == 0)); // SPIR-V is 4-byte aligned
    sdf_fragment_code = malloc(sdf_fragment_size);
    assert(sdf_fragment_code);
    size_t sdf_fragment_spv_read =
        fread(sdf_fragment_code, 1, sdf_fragment_size, sdf_fragment_spv);
    assert(sdf_fragment_spv_read == (size_t)sdf_fragment_size);
    fclose(sdf_fragment_spv);
    VkShaderModuleCreateInfo sdf_fragment_shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sdf_fragment_size,
        .pCode = sdf_fragment_code};
    result = vkCreateShaderModule(ctx_vk->device,
                                  &sdf_fragment_shader_info,
                                  NULL,
                                  &ctx_vk->text_fragment_shader);
    assert(result == VK_SUCCESS);
    free(sdf_fragment_code);
    ctx_vk->text_push_constant_range = (VkPushConstantRange){
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(war_text_push_constant),
    };
    VkPipelineLayoutCreateInfo sdf_pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ctx_vk->font_descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &ctx_vk->text_push_constant_range,
    };
    result = vkCreatePipelineLayout(ctx_vk->device,
                                    &sdf_pipeline_layout_info,
                                    NULL,
                                    &ctx_vk->text_pipeline_layout);
    assert(result == VK_SUCCESS);
    VkPipelineShaderStageCreateInfo sdf_shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = ctx_vk->text_vertex_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = ctx_vk->text_fragment_shader,
            .pName = "main",
        },
    };
    uint32_t text_quads_max = atomic_load(&ctx_lua->WR_TEXT_QUADS_MAX);
    uint32_t text_quads_vertices_max = text_quads_max * 4;
    uint32_t text_quads_indices_max = text_quads_max * 6;
    VkDeviceSize sdf_vertex_buffer_size =
        sizeof(war_text_vertex) * text_quads_vertices_max * max_frames;
    VkBufferCreateInfo sdf_vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_vertex_buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    result = vkCreateBuffer(ctx_vk->device,
                            &sdf_vertex_buffer_info,
                            NULL,
                            &ctx_vk->text_vertex_buffer);
    assert(result == VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device,
        ctx_vk->text_vertex_buffer,
        &ctx_vk->text_vertex_buffer_memory_requirements);
    VkMemoryRequirements sdf_vertex_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(ctx_vk->device,
                                  ctx_vk->text_vertex_buffer,
                                  &sdf_vertex_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_vertex_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &sdf_vertex_buffer_memory_properties);
    uint32_t sdf_vertex_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0;
         i < sdf_vertex_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_vertex_buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_vertex_buffer_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_vertex_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_vertex_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_vertex_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_vertex_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_vertex_buffer_memory_type,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &sdf_vertex_buffer_allocate_info,
                              NULL,
                              &ctx_vk->text_vertex_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(ctx_vk->device,
                                ctx_vk->text_vertex_buffer,
                                ctx_vk->text_vertex_buffer_memory,
                                0);
    assert(result == VK_SUCCESS);
    VkDeviceSize sdf_index_buffer_size =
        sizeof(uint32_t) * text_quads_indices_max * max_frames;
    VkBufferCreateInfo sdf_index_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_index_buffer_size,
        .usage =
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    result = vkCreateBuffer(ctx_vk->device,
                            &sdf_index_buffer_info,
                            NULL,
                            &ctx_vk->text_index_buffer);
    assert(result == VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device,
        ctx_vk->text_index_buffer,
        &ctx_vk->text_index_buffer_memory_requirements);
    VkMemoryRequirements sdf_index_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(ctx_vk->device,
                                  ctx_vk->text_index_buffer,
                                  &sdf_index_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_index_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &sdf_index_buffer_memory_properties);
    uint32_t sdf_index_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0; i < sdf_index_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_index_buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
            (sdf_index_buffer_memory_properties.memoryTypes[i].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_index_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_index_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_index_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_index_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_index_buffer_memory_type,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &sdf_index_buffer_allocate_info,
                              NULL,
                              &ctx_vk->text_index_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(ctx_vk->device,
                                ctx_vk->text_index_buffer,
                                ctx_vk->text_index_buffer_memory,
                                0);
    assert(result == VK_SUCCESS);
    VkDeviceSize sdf_instance_buffer_size = 1;
    VkBufferCreateInfo sdf_instance_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sdf_instance_buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    result = vkCreateBuffer(ctx_vk->device,
                            &sdf_instance_buffer_info,
                            NULL,
                            &ctx_vk->text_instance_buffer);
    assert(result == VK_SUCCESS);
    vkGetBufferMemoryRequirements(
        ctx_vk->device,
        ctx_vk->text_instance_buffer,
        &ctx_vk->text_instance_buffer_memory_requirements);
    VkMemoryRequirements sdf_instance_buffer_memory_requirements;
    vkGetBufferMemoryRequirements(ctx_vk->device,
                                  ctx_vk->text_instance_buffer,
                                  &sdf_instance_buffer_memory_requirements);
    VkPhysicalDeviceMemoryProperties sdf_instance_buffer_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(ctx_vk->physical_device,
                                        &sdf_instance_buffer_memory_properties);
    uint32_t sdf_instance_buffer_memory_type = UINT32_MAX;
    for (uint32_t i = 0;
         i < sdf_instance_buffer_memory_properties.memoryTypeCount;
         i++) {
        if ((sdf_instance_buffer_memory_requirements.memoryTypeBits &
             (1 << i)) &&
            (sdf_instance_buffer_memory_properties.memoryTypes[i]
                 .propertyFlags &
             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            sdf_instance_buffer_memory_type = i;
            break;
        }
    }
    assert(sdf_instance_buffer_memory_type != UINT32_MAX);
    VkMemoryAllocateInfo sdf_instance_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sdf_instance_buffer_memory_requirements.size,
        .memoryTypeIndex = sdf_instance_buffer_memory_type,
    };
    result = vkAllocateMemory(ctx_vk->device,
                              &sdf_instance_buffer_allocate_info,
                              NULL,
                              &ctx_vk->text_instance_buffer_memory);
    assert(result == VK_SUCCESS);
    result = vkBindBufferMemory(ctx_vk->device,
                                ctx_vk->text_instance_buffer,
                                ctx_vk->text_instance_buffer_memory,
                                0);
    assert(result == VK_SUCCESS);
    VkVertexInputBindingDescription sdf_vertex_binding_desc = {
        .binding = 0,
        .stride = sizeof(war_text_vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputBindingDescription sdf_instance_binding_desc = {
        .binding = 1,
        .stride = sizeof(war_text_instance),
        .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
    };
    VkVertexInputAttributeDescription sdf_vertex_attribute_descs[] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(war_text_vertex, corner)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(war_text_vertex, pos)},
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(war_text_vertex, color)},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(war_text_vertex, uv)},
        {4,
         0,
         VK_FORMAT_R32G32_SFLOAT,
         offsetof(war_text_vertex, glyph_bearing)},
        {5, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(war_text_vertex, glyph_size)},
        {6, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, ascent)},
        {7, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, descent)},
        {8, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, thickness)},
        {9, 0, VK_FORMAT_R32_SFLOAT, offsetof(war_text_vertex, feather)},
        {10, 0, VK_FORMAT_R32_UINT, offsetof(war_text_vertex, flags)},
    };
    uint32_t num_sdf_vertex_attrs = 11;
    VkVertexInputAttributeDescription sdf_instance_attribute_descs[] = {
        {num_sdf_vertex_attrs,
         1,
         VK_FORMAT_R32G32_UINT,
         offsetof(war_text_instance, x)},
        {num_sdf_vertex_attrs + 1,
         1,
         VK_FORMAT_R32G32_UINT,
         offsetof(war_text_instance, y)},
        {num_sdf_vertex_attrs + 2,
         1,
         VK_FORMAT_R8G8B8A8_UINT,
         offsetof(war_text_instance, color)},
        {num_sdf_vertex_attrs + 3,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, uv_x)},
        {num_sdf_vertex_attrs + 4,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, uv_y)},
        {num_sdf_vertex_attrs + 5,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, thickness)},
        {num_sdf_vertex_attrs + 6,
         1,
         VK_FORMAT_R32_SFLOAT,
         offsetof(war_text_instance, feather)},
        {num_sdf_vertex_attrs + 7,
         1,
         VK_FORMAT_R32G32_UINT,
         offsetof(war_text_instance, flags)},
    };
    uint32_t num_sdf_instance_attrs = 8;
    VkVertexInputAttributeDescription* sdf_all_attrs =
        malloc((num_sdf_vertex_attrs + num_sdf_instance_attrs) *
               sizeof(VkVertexInputAttributeDescription));
    memcpy(sdf_all_attrs,
           sdf_vertex_attribute_descs,
           sizeof(sdf_vertex_attribute_descs));
    memcpy(sdf_all_attrs + num_sdf_vertex_attrs,
           sdf_instance_attribute_descs,
           sizeof(sdf_instance_attribute_descs));
    VkVertexInputBindingDescription sdf_all_bindings[] = {
        sdf_vertex_binding_desc, sdf_instance_binding_desc};
    VkPipelineVertexInputStateCreateInfo sdf_vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 2,
        .pVertexBindingDescriptions = sdf_all_bindings,
        .vertexAttributeDescriptionCount =
            num_sdf_vertex_attrs + num_sdf_instance_attrs,
        .pVertexAttributeDescriptions = sdf_all_attrs,
    };
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };
    VkViewport sdf_viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D sdf_scissor = {
        .offset = {0, 0},
        .extent = {width, height},
    };
    VkPipelineDepthStencilStateCreateInfo sdf_depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,  // enable depth testing
        .depthWriteEnable = VK_TRUE, // enable writing to depth buffer
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .stencilTestEnable = VK_FALSE,
    };
    VkPipelineViewportStateCreateInfo sdf_viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &sdf_viewport,
        .scissorCount = 1,
        .pScissors = &sdf_scissor,
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_TRUE,                           // enable blending
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA, // source color weight
        .dstColorBlendFactor =
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,     // dest color weight
        .colorBlendOp = VK_BLEND_OP_ADD,             // combine src+dst
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,  // source alpha
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // dest alpha
        .alphaBlendOp = VK_BLEND_OP_ADD,             // combine alpha
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };
    VkAttachmentDescription sdf_color_attachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM, // e.g., VK_FORMAT_B8G8R8A8_UNORM
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, // Keep existing content
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference sdf_color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription sdf_subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &sdf_color_attachment_ref,
        .pDepthStencilAttachment = &quad_depth_ref,
    };
    VkAttachmentDescription sdf_attachments[2] = {sdf_color_attachment,
                                                  quad_depth_attachment};
    VkGraphicsPipelineCreateInfo sdf_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = sdf_shader_stages,
        .pVertexInputState = &sdf_vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState =
            &(VkPipelineViewportStateCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .pViewports = &viewport,
                .scissorCount = 1,
                .pScissors = &scissor,
            },
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = NULL,
        .layout = ctx_vk->text_pipeline_layout,
        .renderPass = ctx_vk->render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };
    result = vkCreateGraphicsPipelines(ctx_vk->device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &sdf_pipeline_info,
                                       NULL,
                                       &ctx_vk->text_pipeline);
    assert(result == VK_SUCCESS);
    ctx_vk->text_vertex_buffer_mapped = war_pool_alloc(pool_wr, sizeof(void*));
    vkMapMemory(ctx_vk->device,
                ctx_vk->text_vertex_buffer_memory,
                0,
                sizeof(war_text_vertex) * text_quads_vertices_max * max_frames,
                0,
                &ctx_vk->text_vertex_buffer_mapped);
    ctx_vk->text_index_buffer_mapped = war_pool_alloc(pool_wr, sizeof(void*));
    vkMapMemory(ctx_vk->device,
                ctx_vk->text_index_buffer_memory,
                0,
                sizeof(uint32_t) * text_quads_indices_max * max_frames,
                0,
                &ctx_vk->text_index_buffer_mapped);
    ctx_vk->text_instance_buffer_mapped =
        war_pool_alloc(pool_wr, sizeof(void*));
    vkMapMemory(ctx_vk->device,
                ctx_vk->text_instance_buffer_memory,
                0,
                1,
                0,
                &ctx_vk->text_instance_buffer_mapped);
    VkPipelineDepthStencilStateCreateInfo transparent_depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,  // enable depth testing
        .depthWriteEnable = VK_TRUE, // enable writing to depth buffer
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .stencilTestEnable = VK_FALSE,
    };
    VkGraphicsPipelineCreateInfo transparent_quad_pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &quad_vertex_input,
        .pInputAssemblyState =
            &(VkPipelineInputAssemblyStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            },
        .pViewportState =
            &(VkPipelineViewportStateCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .viewportCount = 1,
                .pViewports = &viewport,
                .scissorCount = 1,
                .pScissors = &scissor,
            },
        .pDepthStencilState = &transparent_depth_stencil,
        .pRasterizationState =
            &(VkPipelineRasterizationStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .polygonMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .lineWidth = 1.0f,
            },
        .pMultisampleState =
            &(VkPipelineMultisampleStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            },
        .pColorBlendState =
            &(VkPipelineColorBlendStateCreateInfo){
                .sType =
                    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .attachmentCount = 1,
                .pAttachments =
                    (VkPipelineColorBlendAttachmentState[]){
                        {
                            .blendEnable = VK_TRUE, // enable blending
                            .srcColorBlendFactor =
                                VK_BLEND_FACTOR_SRC_ALPHA, // source color
                                                           // weight
                            .dstColorBlendFactor =
                                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, // dest
                                                                     // color
                                                                     // weight
                            .colorBlendOp = VK_BLEND_OP_ADD, // combine src+dst
                            .srcAlphaBlendFactor =
                                VK_BLEND_FACTOR_ONE, // source alpha
                            .dstAlphaBlendFactor =
                                VK_BLEND_FACTOR_ZERO,        // dest alpha
                            .alphaBlendOp = VK_BLEND_OP_ADD, // combine alpha
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,
                        },
                    },
            },
        .layout = ctx_vk->pipeline_layout,
        .renderPass = ctx_vk->render_pass,
        .subpass = 0,
        .pDynamicState = NULL,
    };
    result = vkCreateGraphicsPipelines(ctx_vk->device,
                                       VK_NULL_HANDLE,
                                       1,
                                       &transparent_quad_pipeline_info,
                                       NULL,
                                       &ctx_vk->transparent_quad_pipeline);
    assert(result == VK_SUCCESS);
    //-------------------------------------------------------------------------
    // WP_LINUX_DRM_SYNCOBJ
    //-------------------------------------------------------------------------
    // maybe another time    
    //-------------------------------------------------------------------------
    // END WP_LINUX_DRM_SYNCOBJ
    //-------------------------------------------------------------------------
    free(instance_extensions_properties);
    free(device_extensions_properties);
    free(all_attrs);
    free(sdf_all_attrs);
    end("war_vulkan_init");
}

static inline void war_nsgt_init(war_nsgt_context* ctx_nsgt,
                                 war_pool* pool_wr,
                                 war_lua_context* ctx_lua,
                                 VkDevice device,
                                 VkPhysicalDevice physical_device,
                                 VkRenderPass render_pass,
                                 VkCommandBuffer cmd_buffer,
                                 VkQueue queue,
                                 VkFence fence) {
    header("war_nsgt_init");
    // counts
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
    ctx_nsgt->wav_channel_capacity =
        ctx_nsgt->sample_rate * ctx_nsgt->sample_duration * sizeof(float);
    ctx_nsgt->nsgt_channel_capacity =
        ctx_nsgt->bin_capacity * ctx_nsgt->frame_capacity * sizeof(float) * 2;
    ctx_nsgt->magnitude_channel_capacity =
        ctx_nsgt->bin_capacity * ctx_nsgt->frame_capacity * sizeof(float);
    ctx_nsgt->offset_capacity = ctx_nsgt->bin_capacity * sizeof(uint32_t);
    ctx_nsgt->length_capacity = ctx_nsgt->bin_capacity * sizeof(uint32_t);
    ctx_nsgt->frequency_capacity = ctx_nsgt->bin_capacity * sizeof(float);
    ctx_nsgt->hop_capacity = ctx_nsgt->bin_capacity * sizeof(uint32_t);
    //-------------------------------------------------------------------------
    // NSGT FORMULA (GAUSSIAN + DUAL WINDOW, EXACT INVERTIBLE, INLINE MIN/MAX)
    //-------------------------------------------------------------------------
    float* frequency = malloc(ctx_nsgt->frequency_capacity);
    uint32_t* length = malloc(ctx_nsgt->length_capacity);
    uint32_t* hop = malloc(ctx_nsgt->hop_capacity);
    uint32_t* offset = malloc(ctx_nsgt->offset_capacity);
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
    ctx_nsgt->src_idx =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->resource_count);
    ctx_nsgt->dst_idx =
        war_pool_alloc(pool_wr, sizeof(uint32_t) * ctx_nsgt->resource_count);
    ctx_nsgt->size = war_pool_alloc(
        pool_wr, sizeof(VkDeviceSize) * ctx_nsgt->resource_count);
    ctx_nsgt->offset = war_pool_alloc(
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
    //-------------------------------------------------------------------------
    // CONFIG
    //-------------------------------------------------------------------------
    // resource
    ctx_nsgt->idx_offset = 0;
    ctx_nsgt->idx_hop = 1;
    ctx_nsgt->idx_length = 2;
    ctx_nsgt->idx_window = 3;
    ctx_nsgt->idx_dual_window = 4;
    ctx_nsgt->idx_frequency = 5;
    ctx_nsgt->idx_l = 6;
    ctx_nsgt->idx_r = 7;
    ctx_nsgt->idx_l_nsgt_temp = 8;
    ctx_nsgt->idx_r_nsgt_temp = 9;
    ctx_nsgt->idx_l_nsgt = 10;
    ctx_nsgt->idx_r_nsgt = 11;
    ctx_nsgt->idx_l_magnitude_temp = 12;
    ctx_nsgt->idx_r_magnitude_temp = 13;
    ctx_nsgt->idx_l_magnitude = 14;
    ctx_nsgt->idx_r_magnitude = 15;
    ctx_nsgt->idx_l_image = 16;
    ctx_nsgt->idx_r_image = 17;
    ctx_nsgt->idx_l_stage = 18;
    ctx_nsgt->idx_r_stage = 19;
    ctx_nsgt->idx_offset_stage = 20;
    ctx_nsgt->idx_hop_stage = 21;
    ctx_nsgt->idx_length_stage = 22;
    ctx_nsgt->idx_window_stage = 23;
    ctx_nsgt->idx_dual_window_stage = 24;
    ctx_nsgt->idx_frequency_stage = 25;
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
    ctx_nsgt->shader_idx_compute = 0;
    ctx_nsgt->shader_idx_vertex = 1;
    ctx_nsgt->shader_idx_fragment = 2;
    // shader compute
    char* path_compute = "build/spv/war_nsgt_compute.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_compute *
                                  ctx_nsgt->path_limit],
           path_compute,
           strlen(path_compute));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_compute] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    // shader vertex
    char* path_vertex = "build/spv/war_nsgt_vertex.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_vertex *
                                  ctx_nsgt->path_limit],
           path_vertex,
           strlen(path_vertex));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_vertex] =
        VK_SHADER_STAGE_VERTEX_BIT;
    // shader fragment
    char* path_fragment = "build/spv/war_nsgt_fragment.spv";
    memcpy(&ctx_nsgt->shader_path[ctx_nsgt->shader_idx_fragment *
                                  ctx_nsgt->path_limit],
           path_fragment,
           strlen(path_fragment));
    ctx_nsgt->shader_stage_flag_bits[ctx_nsgt->shader_idx_fragment] =
        VK_SHADER_STAGE_FRAGMENT_BIT;
    // pipeline idx
    ctx_nsgt->pipeline_idx_compute = 0;
    ctx_nsgt->pipeline_idx_graphics = 1;
    // compute pipeline
    ctx_nsgt->pipeline_bind_point[ctx_nsgt->pipeline_idx_compute] =
        VK_PIPELINE_BIND_POINT_COMPUTE;
    ctx_nsgt->pipeline_set_idx[ctx_nsgt->pipeline_idx_compute *
                                   ctx_nsgt->descriptor_set_count +
                               0] = ctx_nsgt->set_idx_compute;
    ctx_nsgt->pipeline_shader_idx[ctx_nsgt->pipeline_idx_compute *
                                      ctx_nsgt->shader_count +
                                  0] = ctx_nsgt->shader_idx_compute;
    ctx_nsgt->structure_type[ctx_nsgt->pipeline_idx_compute] =
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ctx_nsgt->push_constant_shader_stage_flags[ctx_nsgt->pipeline_idx_compute] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->push_constant_size[ctx_nsgt->pipeline_idx_compute] =
        sizeof(war_nsgt_compute_push_constant);
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
    // l
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_l] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_l] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_l] = VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_l] = ctx_nsgt->wav_channel_capacity;
    // r
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_r] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_r] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_r] = VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_r] = ctx_nsgt->wav_channel_capacity;
    // l_nsgt_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_l_nsgt_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_l_nsgt_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_l_nsgt_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_l_nsgt_temp] =
        ctx_nsgt->nsgt_channel_capacity;
    // r_nsgt_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_r_nsgt_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_r_nsgt_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_r_nsgt_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_r_nsgt_temp] =
        ctx_nsgt->nsgt_channel_capacity;
    // l_nsgt
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_l_nsgt] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_l_nsgt] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_l_nsgt] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_l_nsgt] = ctx_nsgt->nsgt_channel_capacity;
    // r_nsgt
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_r_nsgt] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_r_nsgt] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_r_nsgt] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_r_nsgt] = ctx_nsgt->nsgt_channel_capacity;
    // l_magnitude_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_l_magnitude_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_l_magnitude_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_l_magnitude_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_l_magnitude_temp] =
        ctx_nsgt->magnitude_channel_capacity;
    // r_magnitude_temp
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_r_magnitude_temp] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_r_magnitude_temp] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_r_magnitude_temp] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_r_magnitude_temp] =
        ctx_nsgt->magnitude_channel_capacity;
    // l_magnitude
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_l_magnitude] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_l_magnitude] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_l_magnitude] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_l_magnitude] =
        ctx_nsgt->magnitude_channel_capacity;
    // r_magnitude
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_r_magnitude] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_r_magnitude] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_r_magnitude] =
        VK_SHADER_STAGE_COMPUTE_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_r_magnitude] =
        ctx_nsgt->magnitude_channel_capacity;
    // l_image
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_l_image] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_l_image] =
        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_nsgt->format[ctx_nsgt->idx_l_image] = VK_FORMAT_R32_SFLOAT;
    ctx_nsgt->extent_3d[ctx_nsgt->idx_l_image] =
        (VkExtent3D){.width = ctx_nsgt->frame_capacity,
                     .height = ctx_nsgt->bin_capacity,
                     .depth = 1};
    ctx_nsgt->image_usage_flags[ctx_nsgt->idx_l_image] =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // r_image
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_r_image] =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    ctx_nsgt->shader_stage_flags[ctx_nsgt->idx_r_image] =
        VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    ctx_nsgt->format[ctx_nsgt->idx_r_image] = VK_FORMAT_R32_SFLOAT;
    ctx_nsgt->extent_3d[ctx_nsgt->idx_r_image] =
        (VkExtent3D){.width = ctx_nsgt->frame_capacity,
                     .height = ctx_nsgt->bin_capacity,
                     .depth = 1};
    ctx_nsgt->image_usage_flags[ctx_nsgt->idx_r_image] =
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // l_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_l_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_l_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_l_stage] = ctx_nsgt->wav_channel_capacity;
    // r_stage
    ctx_nsgt->memory_property_flags[ctx_nsgt->idx_r_stage] =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    ctx_nsgt->usage_flags[ctx_nsgt->idx_r_stage] =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ctx_nsgt->capacity[ctx_nsgt->idx_r_stage] = ctx_nsgt->wav_channel_capacity;
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
        }
        if (pipeline_shader_count > 1 ||
            (pipeline_shader_count == 1 &&
             ctx_nsgt->shader_stage_flag_bits[pipeline_shader_idx] ==
                 VK_SHADER_STAGE_VERTEX_BIT)) {
            goto graphics_pipeline;
        }
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
    ctx_nsgt->src_idx[0] = ctx_nsgt->idx_offset_stage;
    ctx_nsgt->src_idx[1] = ctx_nsgt->idx_hop_stage;
    ctx_nsgt->src_idx[2] = ctx_nsgt->idx_length_stage;
    ctx_nsgt->src_idx[3] = ctx_nsgt->idx_window_stage;
    ctx_nsgt->src_idx[4] = ctx_nsgt->idx_dual_window_stage;
    ctx_nsgt->src_idx[5] = ctx_nsgt->idx_frequency_stage;
    ctx_nsgt->fn_idx_count = 6;
    war_nsgt_flush(ctx_nsgt->fn_idx_count,
                   ctx_nsgt->src_idx,
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
    //-------------------------------------------------------------------------
    // RIGHT AFTER INITIALIZATION (ONE TIME) UNDEFINED -> GENERAL
    //-------------------------------------------------------------------------
    VkCommandBufferBeginInfo cmd_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    result = vkBeginCommandBuffer(cmd_buffer, &cmd_buffer_begin_info);
    assert(result == VK_SUCCESS);
    // images
    ctx_nsgt->src_idx[0] = ctx_nsgt->idx_l_image;
    ctx_nsgt->src_idx[1] = ctx_nsgt->idx_r_image;
    ctx_nsgt->fn_idx_count = 2;
    war_nsgt_image_barrier(ctx_nsgt->fn_idx_count,
                           ctx_nsgt->src_idx,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_ACCESS_SHADER_WRITE_BIT |
                               VK_ACCESS_SHADER_READ_BIT,
                           VK_IMAGE_LAYOUT_GENERAL,
                           cmd_buffer,
                           ctx_nsgt);
    // buffers
    //  src
    ctx_nsgt->src_idx[0] = ctx_nsgt->idx_offset_stage;
    ctx_nsgt->src_idx[1] = ctx_nsgt->idx_hop_stage;
    ctx_nsgt->src_idx[2] = ctx_nsgt->idx_length_stage;
    ctx_nsgt->src_idx[3] = ctx_nsgt->idx_window_stage;
    ctx_nsgt->src_idx[4] = ctx_nsgt->idx_dual_window_stage;
    ctx_nsgt->src_idx[5] = ctx_nsgt->idx_frequency_stage;
    // dst
    ctx_nsgt->dst_idx[0] = ctx_nsgt->idx_offset;
    ctx_nsgt->dst_idx[1] = ctx_nsgt->idx_hop;
    ctx_nsgt->dst_idx[2] = ctx_nsgt->idx_length;
    ctx_nsgt->dst_idx[3] = ctx_nsgt->idx_window;
    ctx_nsgt->dst_idx[4] = ctx_nsgt->idx_dual_window;
    ctx_nsgt->dst_idx[5] = ctx_nsgt->idx_frequency;
    // size
    ctx_nsgt->fn_idx_count = 6;
    for (uint32_t i = 0; i < ctx_nsgt->fn_idx_count; i++) {
        ctx_nsgt->size[i] = ctx_nsgt->capacity[ctx_nsgt->src_idx[i]];
    }
    for (uint32_t i = 0; i < ctx_nsgt->fn_idx_count; i++) {
        war_nsgt_copy(cmd_buffer,
                      ctx_nsgt->src_idx[i],
                      ctx_nsgt->dst_idx[i],
                      0,
                      0,
                      ctx_nsgt->size[i],
                      ctx_nsgt);
    }
    war_nsgt_buffer_barrier(ctx_nsgt->fn_idx_count,
                            ctx_nsgt->dst_idx,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_ACCESS_SHADER_READ_BIT |
                                VK_ACCESS_SHADER_WRITE_BIT,
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
    //-------------------------------------------------------------------------
    // FLAGS
    //-------------------------------------------------------------------------
    ctx_nsgt->dirty_compute = 1;
    end("war_nsgt_init");
}

#endif // WAR_VULKAN_H
