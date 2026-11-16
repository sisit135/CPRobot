// Vulkan setup/frame helpers adapted from Dear ImGui v1.91.9b SDL2 example.
/*
The MIT License (MIT)

Copyright (c) 2014-2025 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
// Dear ImGui: standalone example application for SDL2 + Vulkan

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/
// folder).
// - Introduction, links and more at the top of imgui.cpp

// Important note to the reader who wish to integrate imgui_impl_vulkan.cpp/.h
// in their own engine/app.
// - Common ImGui_ImplVulkan_XXX functions and structures are used to interface
// with imgui_impl_vulkan.cpp/.h.
//   You will use those if you want to use this rendering backend in your
//   engine/app.
// - Helper ImGui_ImplVulkanH_XXX functions and structures are only used by this
// example (main.cpp) and by
//   the backend itself (imgui_impl_vulkan.cpp), but should PROBABLY NOT be used
//   by your own engine/app code.
// Read comments in imgui_impl_vulkan.h.

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include <SDL.h>
#include <SDL_vulkan.h>
#include <stdio.h>  // printf, fprintf
#include <stdlib.h> // abort

// Volk headers
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

// #define APP_USE_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif

// Data
static VkAllocationCallbacks *g_Allocator = nullptr;
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice g_Device = VK_NULL_HANDLE;
static uint32_t g_QueueFamily = (uint32_t)-1;
static VkQueue g_Queue = VK_NULL_HANDLE;
#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
#endif
static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t g_MinImageCount = 2;
static bool g_SwapChainRebuild = false;

static void check_vk_result(VkResult err) {
  if (err == VK_SUCCESS)
    return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0)
    abort();
}

#ifdef APP_USE_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType,
             uint64_t object, size_t location, int32_t messageCode,
             const char *pLayerPrefix, const char *pMessage, void *pUserData) {
  (void)flags;
  (void)object;
  (void)location;
  (void)messageCode;
  (void)pUserData;
  (void)pLayerPrefix; // Unused arguments
  fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n",
          objectType, pMessage);
  return VK_FALSE;
}
#endif // APP_USE_VULKAN_DEBUG_REPORT

static bool
IsExtensionAvailable(const ImVector<VkExtensionProperties> &properties,
                     const char *extension) {
  for (const VkExtensionProperties &p : properties)
    if (strcmp(p.extensionName, extension) == 0)
      return true;
  return false;
}

static void SetupVulkan(ImVector<const char *> instance_extensions) {
  VkResult err;
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
  volkInitialize();
#endif

  // Create Vulkan Instance
  {
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // Enumerate available extensions
    uint32_t properties_count;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    properties.resize(properties_count);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count,
                                                 properties.Data);
    check_vk_result(err);

    // Enable required extensions
    if (IsExtensionAvailable(
            properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
      instance_extensions.push_back(
          VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (IsExtensionAvailable(properties,
                             VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
      instance_extensions.push_back(
          VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
      create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    // Enabling validation layers
#ifdef APP_USE_VULKAN_DEBUG_REPORT
    const char *layers[] = {"VK_LAYER_KHRONOS_validation"};
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;
    instance_extensions.push_back("VK_EXT_debug_report");
#endif

    // Create Vulkan Instance
    create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
    create_info.ppEnabledExtensionNames = instance_extensions.Data;
    err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
    check_vk_result(err);
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
    volkLoadInstance(g_Instance);
#endif

    // Setup the debug report callback
#ifdef APP_USE_VULKAN_DEBUG_REPORT
    auto f_vkCreateDebugReportCallbackEXT =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
            g_Instance, "vkCreateDebugReportCallbackEXT");
    IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType =
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                            VK_DEBUG_REPORT_WARNING_BIT_EXT |
                            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = debug_report;
    debug_report_ci.pUserData = nullptr;
    err = f_vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci,
                                           g_Allocator, &g_DebugReport);
    check_vk_result(err);
#endif
  }

  // Select Physical Device (GPU)
  g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
  IM_ASSERT(g_PhysicalDevice != VK_NULL_HANDLE);

  // Select graphics queue family
  g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
  IM_ASSERT(g_QueueFamily != (uint32_t)-1);

  // Create Logical Device (with 1 queue)
  {
    ImVector<const char *> device_extensions;
    device_extensions.push_back("VK_KHR_swapchain");

    // Enumerate physical device extension
    uint32_t properties_count;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr,
                                         &properties_count, nullptr);
    properties.resize(properties_count);
    vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr,
                                         &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (IsExtensionAvailable(properties,
                             VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
      device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    const float queue_priority[] = {1.0f};
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = g_QueueFamily;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount =
        sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
    create_info.ppEnabledExtensionNames = device_extensions.Data;
    err =
        vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
    check_vk_result(err);
    vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
  }

  // Create Descriptor Pool
  // If you wish to load e.g. additional textures you may need to alter pools
  // sizes and maxSets.
  {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 0;
    for (VkDescriptorPoolSize &pool_size : pool_sizes)
      pool_info.maxSets += pool_size.descriptorCount;
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator,
                                 &g_DescriptorPool);
    check_vk_result(err);
  }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used
// by the demo. Your real engine/app may not use them.
static void SetupVulkanWindow(ImGui_ImplVulkanH_Window *wd,
                              VkSurfaceKHR surface, int width, int height) {
  wd->Surface = surface;

  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily,
                                       wd->Surface, &res);
  if (res != VK_TRUE) {
    fprintf(stderr, "Error no WSI support on physical device 0\n");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = {
      VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
      VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
  const VkColorSpaceKHR requestSurfaceColorSpace =
      VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
      g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat,
      (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
      requestSurfaceColorSpace);

  // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
  VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_MAILBOX_KHR,
                                      VK_PRESENT_MODE_IMMEDIATE_KHR,
                                      VK_PRESENT_MODE_FIFO_KHR};
#else
  VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
#endif
  wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
      g_PhysicalDevice, wd->Surface, &present_modes[0],
      IM_ARRAYSIZE(present_modes));
  // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(g_MinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device,
                                         wd, g_QueueFamily, g_Allocator, width,
                                         height, g_MinImageCount);
}

static void CleanupVulkan() {
  vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

#ifdef APP_USE_VULKAN_DEBUG_REPORT
  // Remove the debug report callback
  auto f_vkDestroyDebugReportCallbackEXT =
      (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
          g_Instance, "vkDestroyDebugReportCallbackEXT");
  f_vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif // APP_USE_VULKAN_DEBUG_REPORT

  vkDestroyDevice(g_Device, g_Allocator);
  vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow() {
  ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData,
                                  g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window *wd, ImDrawData *draw_data) {
  VkSemaphore image_acquired_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
  VkSemaphore render_complete_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX,
                                       image_acquired_semaphore, VK_NULL_HANDLE,
                                       &wd->FrameIndex);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    g_SwapChainRebuild = true;
  if (err == VK_ERROR_OUT_OF_DATE_KHR)
    return;
  if (err != VK_SUBOPTIMAL_KHR)
    check_vk_result(err);

  ImGui_ImplVulkanH_Frame *fd = &wd->Frames[wd->FrameIndex];
  {
    err = vkWaitForFences(
        g_Device, 1, &fd->Fence, VK_TRUE,
        UINT64_MAX); // wait indefinitely instead of periodically checking
    check_vk_result(err);

    err = vkResetFences(g_Device, 1, &fd->Fence);
    check_vk_result(err);
  }
  {
    err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    check_vk_result(err);
  }
  {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = wd->RenderPass;
    info.framebuffer = fd->Framebuffer;
    info.renderArea.extent.width = wd->Width;
    info.renderArea.extent.height = wd->Height;
    info.clearValueCount = 1;
    info.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // Record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(fd->CommandBuffer);
  {
    VkPipelineStageFlags wait_stage =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &image_acquired_semaphore;
    info.pWaitDstStageMask = &wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &fd->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &render_complete_semaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);
    err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
    check_vk_result(err);
  }
}

static void FramePresent(ImGui_ImplVulkanH_Window *wd) {
  if (g_SwapChainRebuild)
    return;
  VkSemaphore render_complete_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &render_complete_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &wd->Swapchain;
  info.pImageIndices = &wd->FrameIndex;
  VkResult err = vkQueuePresentKHR(g_Queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    g_SwapChainRebuild = true;
  if (err == VK_ERROR_OUT_OF_DATE_KHR)
    return;
  if (err != VK_SUBOPTIMAL_KHR)
    check_vk_result(err);
  wd->SemaphoreIndex =
      (wd->SemaphoreIndex + 1) %
      wd->SemaphoreCount; // Now we can use the next set of semaphores
}

#include "bup/bup_tasks.hpp"
#include "lifecycle.hpp"
#include "localization/config.hpp"
#include "logging/log.hpp"
#include "motor_control/config.hpp"
#include "navigation/path_executor.hpp"
#include "scheduler.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
void draw_scene(lifecycle::Session &s, bool rays, bool path, bool diagnostics) {
  auto &p = *s.robot;
  const auto &map = s.maps.variants[s.options.variant];
  auto size = ImGui::GetIO().DisplaySize;
  auto draw = ImGui::GetBackgroundDrawList();
  const float scale = std::max(150.f, std::min(size.y - 135, size.x - 420));
  const ImVec2 origin(40, 95 + scale);
  auto at = [&](float x, float y) {
    return ImVec2(origin.x + x * scale, origin.y - y * scale);
  };
  auto cell = [&](loc::Cell c) {
    return at((c.x + .5f) * .2f, (c.y + .5f) * .2f);
  };
  draw->AddRectFilled(at(0, 1), at(1, 0), IM_COL32(30, 39, 51, 255));
  for (int y = 0; y < 5; ++y)
    for (int x = 0; x < 5; ++x) {
      auto a = at(x * .2f, (y + 1) * .2f), b = at((x + 1) * .2f, y * .2f);
      const loc::Cell c{x, y};
      if (c == s.goal)
        draw->AddRectFilled(a, b, IM_COL32(48, 91, 73, 255));
      if (c == s.start)
        draw->AddRectFilled(a, b, IM_COL32(38, 72, 110, 255));
      draw->AddRect(a, b, IM_COL32(72, 85, 101, 150));
      char label[16];
      std::snprintf(label, sizeof label, "%d,%d", x, y);
      draw->AddText(ImVec2(a.x + 6, a.y + 5), IM_COL32(120, 138, 158, 255),
                    label);
      auto wall = [&](uint8_t bit, ImVec2 from, ImVec2 to) {
        if (map.edge(c, bit) == loc::EdgeState::CLOSED)
          draw->AddLine(from, to, IM_COL32(218, 227, 235, 255), 4);
      };
      wall(loc::NORTH, a, ImVec2(b.x, a.y));
      wall(loc::WEST, a, ImVec2(a.x, b.y));
      wall(loc::SOUTH, ImVec2(a.x, b.y), b);
      wall(loc::EAST, ImVec2(b.x, a.y), b);
    }
  // Physical tape across every known-open end-cell entrance, 5..25 mm before
  // edge.
  for (auto d :
       {loc::Cell{1, 0}, loc::Cell{-1, 0}, loc::Cell{0, 1}, loc::Cell{0, -1}}) {
    if (!map.can_step({s.goal.x - d.x, s.goal.y - d.y}, s.goal) ||
        s.options.no_tape)
      continue;
    float x = (s.goal.x + .5f) * .2f - d.x * .115f,
          y = (s.goal.y + .5f) * .2f - d.y * .115f;
    draw->AddLine(at(x - d.y * .08f, y + d.x * .08f),
                  at(x + d.y * .08f, y - d.x * .08f), IM_COL32(0, 0, 0, 255),
                  scale * .02f);
  }
  if (path) {
    for (unsigned i = 1; i < s.route.count; ++i)
      draw->AddLine(cell(s.route[i - 1]), cell(s.route[i]),
                    IM_COL32(100, 165, 255, 130), 5);
    for (size_t i = 1; i < s.trail.size(); ++i)
      draw->AddLine(at(s.trail[i - 1].x, s.trail[i - 1].y),
                    at(s.trail[i].x, s.trail[i].y), IM_COL32(86, 231, 177, 255),
                    2);
  }
  if (rays)
    for (int sensor = 0; sensor < 3; ++sensor) {
      auto m = loc::cfg::lidar_mounts[sensor];
      float ox =
          p.x + std::cos(p.yaw) * m.x_body_m - std::sin(p.yaw) * m.y_body_m;
      float oy =
          p.y + std::sin(p.yaw) * m.x_body_m + std::cos(p.yaw) * m.y_body_m;
      const ImU32 colors[] = {IM_COL32(117, 196, 255, 170),
                              IM_COL32(255, 211, 106, 170),
                              IM_COL32(234, 139, 255, 170)};
      for (int beam = 0; beam < 9; ++beam) {
        auto ray = m;
        ray.yaw_body_rad += loc::deg2rad(15) * (float(beam) / 8 - .5f);
        float distance = p.ray(ray);
        if (!std::isfinite(distance))
          continue;
        float a = p.yaw + ray.yaw_body_rad;
        auto hit = at(ox + std::cos(a) * distance, oy + std::sin(a) * distance);
        draw->AddLine(at(ox, oy), hit, colors[sensor], beam == 4 ? 2 : 1);
        draw->AddCircleFilled(hit, 2, colors[sensor]);
      }
    }
  auto body = [&](float x, float y) {
    return at(p.x + std::cos(p.yaw) * x - std::sin(p.yaw) * y,
              p.y + std::sin(p.yaw) * x + std::cos(p.yaw) * y);
  };
  ImVec2 corners[robot::geometry::platform_outline.size()];
  for (std::size_t i = 0; i < robot::geometry::platform_outline.size(); ++i) {
    const auto point =
        robot::geometry::to_body(robot::geometry::platform_outline[i]);
    corners[i] = body(point.x_body_m, point.y_body_m);
  }
  const int corner_count = int(robot::geometry::platform_outline.size());
  draw->AddConcavePolyFilled(corners, corner_count,
                             IM_COL32(92, 156, 205, 150));
  for (const auto mount : robot::geometry::wheel_mounts) {
    const auto wheel = robot::geometry::to_body(mount);
    const float x = wheel.x_body_m;
    const float y = wheel.y_body_m;
    const float r = motor::cfg::wheel_radius_m;
    const float w = robot::geometry::wheel_width_m / 2;
    ImVec2 tire[] = {body(x - r, y - w), body(x + r, y - w), body(x + r, y + w),
                     body(x - r, y + w)};
    draw->AddConvexPolyFilled(tire, 4, IM_COL32(40, 40, 40, 255));
    draw->AddPolyline(tire, 4, IM_COL32(245, 205, 60, 255), ImDrawFlags_Closed,
                      2);
  }
  draw->AddPolyline(corners, corner_count, IM_COL32(152, 206, 255, 255),
                    ImDrawFlags_Closed, 2);
  draw->AddLine(body(0, 0), body(robot::geometry::footprint_front_m, 0),
                IM_COL32(255, 255, 255, 255), 3);
  const auto &f = s.navigation.feature;
  if (f.pose.tracking)
    draw->AddCircle(at(f.pose.x_m, f.pose.y_m), 7, IM_COL32(255, 210, 90, 255),
                    16, 2);
  ImGui::SetNextWindowPos(ImVec2(25, 15), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(.9f);
  ImGui::Begin("Lifecycle", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
  ImGui::Text("FEATURE | Q%u | seed %u | entrance %u | elapsed %.2fs",
              s.options.variant + 1, s.options.seed, s.entrance, s.seconds());
  double remaining =
      s.navigation.run.deadline_us
          ? std::max(0.,
                     (s.navigation.run.deadline_us - lifecycle::now_us()) / 1e6)
          : nav::RunDeadline::kBudgetUs / 1e6;
  ImGui::Text("Run: %s | remaining %.2fs | loc_start: %s",
              lifecycle::run_name(s.navigation.run.state), remaining,
              s.key_sent ? "pressed" : "automatic after READY + 5s");
  if (s.finished())
    ImGui::TextColored(s.passed ? ImVec4(.3f, 1, .6f, 1)
                                : ImVec4(1, .4f, .3f, 1),
                       "%s", s.result.c_str());
  ImGui::End();
  if (!diagnostics)
    return;
  ImGui::SetNextWindowPos(ImVec2(origin.x + scale + 20, 95), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(345, std::max(280.f, size.y - 120)),
                           ImGuiCond_Always);
  ImGui::Begin("Subsystem diagnostics", nullptr, ImGuiWindowFlags_NoMove);
  ImGui::Text("Bring-up: %u | config: RAM ready rev %u",
              unsigned(bup::bup_task_get_status()),
              s.config.store.snapshot().revision);
  ImGui::Text("Board: %s", s.ready_us ? "READY" : "BOOT");
  if (!s.key_sent && s.ready_us)
    ImGui::Text("Key countdown: %.2fs",
                std::max(0., 5 - (lifecycle::now_us() - s.ready_us) / 1e6));
  ImGui::Text("Navigation: %s",
              s.key_sent ? lifecycle::startup_name(s.navigation.startup)
                         : "not started");
  ImGui::Text("Path: %s", lifecycle::execution_name(s.navigation.execution));
  ImGui::Text("Path fault: %s",
              nav::execute_fault_name(static_cast<nav::ExecuteFault>(
                  s.navigation.execution_fault)));
  ImGui::Text("FEATURE: %s | resolve=%u candidates=%u",
              f.pose.tracking ? "tracking" : "unresolved",
              unsigned(f.resolution.status), f.candidates);
  ImGui::Text("RHR mode: %s | reason=%u",
              s.options.force_rhr ? "forced host test" : "automatic",
              unsigned(s.navigation.fallback_reason));
  static const char *rhr_phases[] = {"IDLE",   "DECIDE", "TURN",
                                     "COAST",  "CLEAR",  "FOLLOW",
                                     "PAUSED", "FAULT",  "CENTER"};
  const unsigned rhr_phase = unsigned(s.navigation.fallback_phase);
  ImGui::Text("Contradictions: %u | RHR: %s phase=%s", f.contradiction_frames,
              s.navigation.fallback ? "active" : "inactive",
              rhr_phase < 9 ? rhr_phases[rhr_phase] : "UNKNOWN");
  ImGui::Text("EKF observer: health=%u tracking=%d",
              unsigned(s.observer.health), s.observer.tracking);
  ImGui::Text("Motor: state=%u PWM L %.3f R %.3f", unsigned(s.motor.status),
              s.duty_left, s.duty_right);
  ImGui::Text("Tape: clear=%u event=%u seen=%d", s.sensors.color.clear,
              s.sensors.end_tape.id, s.navigation.end_tape_seen);
  ImGui::Text("Tape unresolved=%d collision=%d",
              s.navigation.end_tape_unresolved, p.collided);
  ImGui::Separator();
  ImGui::Text("Truth: %.3f %.3f yaw %.1f deg", p.x, p.y,
              p.yaw * 180 / loc::kPi);
  ImGui::Text("Surface: %s", s.options.surface.enabled
                                 ? "rubber / smooth board (assumed)"
                                 : "legacy kinematic plant");
  if (s.options.surface.enabled) {
    const auto &surface = s.options.surface;
    ImGui::Text("Friction static %.2f sliding %.2f | rolling %.3f",
                surface.mu_static, surface.mu_kinetic,
                surface.rolling_resistance_coefficient);
    ImGui::Text("Contact load L/R %.0f%% / %.0f%% | mass %.0fg",
                surface.contact_left_fraction * 100,
                surface.contact_right_fraction * 100, surface.mass_kg * 1000);
    ImGui::Text("Body vx/vy %.3f / %.3f m/s | yaw %.3f rad/s", p.vx_m_s,
                p.vy_m_s, p.body_omega_rps);
    ImGui::Text("Wheel slip L/R %.3f / %.3f m/s", p.slip_left_m_s,
                p.slip_right_m_s);
    ImGui::Text("Tire forces L (%.3f,%.3f) R (%.3f,%.3f) N", p.force_left_n,
                p.force_left_lat_n, p.force_right_n, p.force_right_lat_n);
  }
  ImGui::Text("Feature: %.3f %.3f yaw %.1f deg", f.pose.x_m, f.pose.y_m,
              f.pose.yaw_rad * 180 / loc::kPi);
  ImGui::Text("Gyro: %.4f rad/s", s.sensors.imu.gz_rps);
  ImGui::Text("Accel: %.2f %.2f %.2f m/s2", s.sensors.imu.ax_mps2,
              s.sensors.imu.ay_mps2, s.sensors.imu.az_mps2);
  ImGui::Text("MAG: %.2f %.2f %.2f uT", s.sensors.mag.mx_uT,
              s.sensors.mag.my_uT, s.sensors.mag.mz_uT);
  ImGui::Text("MAG clean=%d", s.sensors.magnetic_clean);
  for (int i = 0; i < 3; ++i)
    ImGui::Text("TOF %s: %7.1f mm | age %.0f ms",
                i == 0   ? "L"
                : i == 1 ? "F"
                         : "R",
                s.sensors.range_m[i] * 1000,
                (lifecycle::now_us() - s.sensors.tof_us[i]) / 1000.);
  ImGui::Text("TOF: MT19937 Gaussian sigma %.1f mm",
              s.options.noise_sigma * 1000);
  ImGui::Text("FOV overlay: 15 deg / 9 physical rays");
  ImGui::Text("TOF return model: %s",
              s.options.nearest_return ? "nearest ray" : "center ray");
  ImGui::Text("TCS RGB: %u %u %u seq=%u", s.sensors.color.red,
              s.sensors.color.green, s.sensors.color.blue,
              s.sensors.color.sequence);
  ImGui::Separator();
  for (const auto &t : lifecycle::task_states())
    ImGui::TextUnformatted(t.c_str());
  ImGui::Text("Logs: stdout adapter | dropped=%u", robot_log::dropped());
  ImGui::TextUnformatted("Telemetry: excluded");
  ImGui::Separator();
  ImGui::TextWrapped(
      "R: rays | P: path | D: diagnostics\nSpace: pause | N: step 10ms | Esc: "
      "quit\nBlue: reference A* route\nGreen: actual trail | Gold: FEATURE "
      "pose\nRestart executable for a fresh lifecycle.");
  ImGui::End();
}
} // namespace

int run_gui(lifecycle::Session &session, bool exit_on_result, float speed) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL: %s\n", SDL_GetError());
    return 2;
  }
  SDL_Window *window = SDL_CreateWindow(
      "THS FEATURE lifecycle | Vulkan", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1200, 850,
      SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    std::fprintf(stderr, "SDL Vulkan: %s\n", SDL_GetError());
    SDL_Quit();
    return 2;
  }
  unsigned count = 0;
  SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
  ImVector<const char *> extensions;
  extensions.resize(count);
  SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.Data);
  SetupVulkan(extensions);
  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(window, g_Instance, &surface)) {
    std::fprintf(stderr, "surface: %s\n", SDL_GetError());
    CleanupVulkan();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 2;
  }
  int w, h;
  SDL_Vulkan_GetDrawableSize(window, &w, &h);
  auto wd = &g_MainWindowData;
  SetupVulkanWindow(wd, surface, w, h);
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  ImGui::StyleColorsDark();
  ImGui_ImplSDL2_InitForVulkan(window);
  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = g_Instance;
  info.PhysicalDevice = g_PhysicalDevice;
  info.Device = g_Device;
  info.QueueFamily = g_QueueFamily;
  info.Queue = g_Queue;
  info.DescriptorPool = g_DescriptorPool;
  info.RenderPass = wd->RenderPass;
  info.MinImageCount = g_MinImageCount;
  info.ImageCount = wd->ImageCount;
  info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  info.CheckVkResultFn = check_vk_result;
  ImGui_ImplVulkan_Init(&info);
  bool done = false, paused = false, rays = true, path = true,
       diagnostics = true;
  double accumulator = 0;
  auto previous = std::chrono::steady_clock::now();
  while (!done) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      ImGui_ImplSDL2_ProcessEvent(&e);
      if (e.type == SDL_QUIT || (e.type == SDL_WINDOWEVENT &&
                                 e.window.event == SDL_WINDOWEVENT_CLOSE))
        done = true;
      if (e.type == SDL_KEYDOWN && !e.key.repeat &&
          !ImGui::GetIO().WantTextInput) {
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE:
          done = true;
          break;
        case SDLK_r:
          rays = !rays;
          break;
        case SDLK_p:
          path = !path;
          break;
        case SDLK_d:
          diagnostics = !diagnostics;
          break;
        case SDLK_SPACE:
          paused = !paused;
          accumulator = 0;
          break;
        case SDLK_n:
          if (paused)
            for (int i = 0; i < 10; ++i)
              session.tick();
          break;
        }
      }
    }
    auto now = std::chrono::steady_clock::now();
    if (!paused && !session.finished())
      accumulator +=
          std::min(.1, std::chrono::duration<double>(now - previous).count()) *
          speed;
    previous = now;
    while (accumulator >= .001 && !session.finished()) {
      session.tick();
      accumulator -= .001;
    }
    if (exit_on_result && session.finished())
      done = true;
    SDL_Vulkan_GetDrawableSize(window, &w, &h);
    if (w <= 0 || h <= 0 ||
        (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)) {
      SDL_Delay(10);
      continue;
    }
    if (g_SwapChainRebuild || wd->Width != w || wd->Height != h) {
      ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
      ImGui_ImplVulkanH_CreateOrResizeWindow(
          g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily,
          g_Allocator, w, h, g_MinImageCount);
      wd->FrameIndex = 0;
      g_SwapChainRebuild = false;
    }
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    draw_scene(session, rays, path, diagnostics);
    if (paused) {
      ImGui::SetNextWindowPos(ImVec2(40, ImGui::GetIO().DisplaySize.y - 35));
      ImGui::Begin("pause", nullptr,
                   ImGuiWindowFlags_NoDecoration |
                       ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::TextUnformatted("PAUSED - Space resumes / N steps");
      ImGui::End();
    }
    ImGui::Render();
    wd->ClearValue.color = {{.035f, .047f, .065f, 1}};
    FrameRender(wd, ImGui::GetDrawData());
    FramePresent(wd);
  }
  check_vk_result(vkDeviceWaitIdle(g_Device));
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  CleanupVulkanWindow();
  CleanupVulkan();
  SDL_DestroyWindow(window);
  SDL_Quit();
  if (!session.finished())
    session.halt();
  return 0;
}
