// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.
//
// A small Vulkan 2D renderer: one render pass, one pipeline, one dynamic vertex
// buffer. It consumes the `DrawList` produced by the UI layer, so no other part
// of the game touches Vulkan.
#ifndef ZILLA_GFX_VULKAN_H
#define ZILLA_GFX_VULKAN_H

#include "core/types.h"
#include "gfx/draw_list.h"

#include "volk/volk.h"

#include <cstdint>
#include <string>
#include <vector>

struct GLFWwindow;

namespace zilla {

class VulkanRenderer {
public:
	VulkanRenderer() = default;
	~VulkanRenderer();

	VulkanRenderer(const VulkanRenderer &) = delete;
	VulkanRenderer &operator=(const VulkanRenderer &) = delete;

	bool init(GLFWwindow *p_window, bool p_validation);
	void shutdown();

	// Uploads the font atlas (single channel coverage data) once.
	bool upload_font_atlas(const uint8_t *p_pixels, int p_width, int p_height);

	// Renders one frame. Returns false when the frame was dropped because the
	// swapchain had to be rebuilt (the caller simply draws the next one).
	bool frame(const DrawList &p_list, const Color &p_clear, float p_content_scale);

	void request_swapchain_rebuild() { rebuild_swapchain_ = true; }

	const std::string &device_name() const { return device_name_; }
	uint32_t framebuffer_width() const { return extent_.width; }
	uint32_t framebuffer_height() const { return extent_.height; }

private:
	struct Buffer {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		void *mapped = nullptr;
		VkDeviceSize capacity = 0;
	};

	struct QueueFamilies {
		uint32_t graphics = UINT32_MAX;
		uint32_t present = UINT32_MAX;
		bool complete() const { return graphics != UINT32_MAX && present != UINT32_MAX; }
	};

	bool create_instance(bool p_validation);
	bool create_surface(GLFWwindow *p_window);
	bool pick_physical_device();
	bool create_device(bool p_validation);
	bool create_swapchain();
	bool create_render_pass();
	bool create_pipeline();
	bool create_framebuffers();
	bool create_command_buffers();
	bool create_sync_objects();
	bool create_descriptor_resources();
	std::vector<uint32_t> unique_family_indices() const;
	bool ensure_buffer(Buffer &p_buffer, VkBufferUsageFlags p_usage, VkDeviceSize p_capacity);
	void destroy_swapchain_resources();
	void recreate_swapchain();

	// Helpers.
	QueueFamilies find_queue_families(VkPhysicalDevice p_device) const;
	uint32_t find_memory_type(uint32_t p_type_filter, VkMemoryPropertyFlags p_properties) const;
	VkCommandBuffer begin_single_time_commands();
	void end_single_time_commands(VkCommandBuffer p_command_buffer);
	bool check_validation_layer() const;

	VkInstance instance_ = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
	VkDevice device_ = VK_NULL_HANDLE;
	VkQueue graphics_queue_ = VK_NULL_HANDLE;
	VkQueue present_queue_ = VK_NULL_HANDLE;
	QueueFamilies families_;

	GLFWwindow *window_ = nullptr;
	VkSurfaceKHR surface_ = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
	VkFormat swapchain_format_ = VK_FORMAT_B8G8R8A8_UNORM;
	VkExtent2D extent_ = { 0, 0 };
	std::vector<VkImage> swapchain_images_;
	std::vector<VkImageView> swapchain_views_;
	std::vector<VkFramebuffer> framebuffers_;

	VkRenderPass render_pass_ = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
	VkPipeline pipeline_ = VK_NULL_HANDLE;

	VkCommandPool command_pool_ = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> command_buffers_;
	std::vector<VkSemaphore> image_available_;
	std::vector<VkSemaphore> render_finished_;
	std::vector<VkFence> in_flight_;
	uint32_t current_frame_ = 0;

	Buffer vertex_buffer_;
	Buffer index_buffer_;

	VkImage font_image_ = VK_NULL_HANDLE;
	VkDeviceMemory font_memory_ = VK_NULL_HANDLE;
	VkImageView font_view_ = VK_NULL_HANDLE;
	VkSampler font_sampler_ = VK_NULL_HANDLE;
	VkDescriptorPool descriptor_pool_ = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_layout_ = VK_NULL_HANDLE;
	VkDescriptorSet descriptor_set_ = VK_NULL_HANDLE;

	bool validation_ = false;
	bool rebuild_swapchain_ = false;
	std::string device_name_ = "неизвестное устройство";
};

} // namespace zilla

#endif // ZILLA_GFX_VULKAN_H
