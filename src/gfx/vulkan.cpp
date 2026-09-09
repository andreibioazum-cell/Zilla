// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Zilla contributors.

#include "gfx/vulkan.h"

#include "core/log.h"
#include "gfx/embedded_shaders.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <vector>

namespace zilla {

namespace {

constexpr uint32_t FRAMES_IN_FLIGHT = 2;
constexpr const char *VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT p_severity,
		VkDebugUtilsMessageTypeFlagsEXT,
		const VkDebugUtilsMessengerCallbackDataEXT *p_data,
		void *) {
	if (p_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		ZILLA_LOG_ERROR("Vulkan: %s", p_data->pMessage);
	} else if (p_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		ZILLA_LOG_WARN("Vulkan: %s", p_data->pMessage);
	}
	return VK_FALSE;
}

} // namespace

VulkanRenderer::~VulkanRenderer() {
	shutdown();
}

// --- setup ------------------------------------------------------------------

bool VulkanRenderer::init(GLFWwindow *p_window, bool p_validation) {
	window_ = p_window;
	validation_ = p_validation;

	if (!create_instance(p_validation)) {
		return false;
	}
	if (!create_surface(p_window)) {
		return false;
	}
	if (!pick_physical_device()) {
		return false;
	}
	if (!create_device(p_validation)) {
		return false;
	}
	if (!create_swapchain()) {
		return false;
	}
	if (!create_render_pass()) {
		return false;
	}
	if (!create_descriptor_resources()) {
		return false;
	}
	if (!create_pipeline()) {
		return false;
	}
	if (!create_framebuffers()) {
		return false;
	}
	if (!create_command_buffers()) {
		return false;
	}
	if (!create_sync_objects()) {
		return false;
	}
	return true;
}

bool VulkanRenderer::check_validation_layer() const {
	uint32_t count = 0;
	vkEnumerateInstanceLayerProperties(&count, nullptr);
	std::vector<VkLayerProperties> layers(count);
	vkEnumerateInstanceLayerProperties(&count, layers.data());
	for (const VkLayerProperties &layer : layers) {
		if (std::strcmp(layer.layerName, VALIDATION_LAYER) == 0) {
			return true;
		}
	}
	return false;
}

bool VulkanRenderer::create_instance(bool p_validation) {
	if (volkInitialize() != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: no loader found (install the Vulkan runtime / libvulkan).");
		return false;
	}

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Zilla";
	app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.pEngineName = "Zilla";
	app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.apiVersion = VK_API_VERSION_1_1;

	uint32_t glfw_extension_count = 0;
	const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	std::vector<const char *> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);
	if (p_validation) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = uint32_t(extensions.size());
	create_info.ppEnabledExtensionNames = extensions.data();

	const bool layer_available = check_validation_layer();
	if (p_validation && !layer_available) {
		ZILLA_LOG_WARN("Vulkan: %s is not installed, running without validation.", VALIDATION_LAYER);
	}
	if (p_validation && layer_available) {
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = &VALIDATION_LAYER;
	}

	if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateInstance() failed.");
		return false;
	}
	volkLoadInstance(instance_);

	if (p_validation && layer_available) {
		VkDebugUtilsMessengerCreateInfoEXT messenger_info{};
		messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		messenger_info.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		messenger_info.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		messenger_info.pfnUserCallback = debug_callback;
		if (vkCreateDebugUtilsMessengerEXT(instance_, &messenger_info, nullptr, &debug_messenger_) != VK_SUCCESS) {
			ZILLA_LOG_WARN("Vulkan: could not install the debug messenger.");
		}
	}
	return true;
}

bool VulkanRenderer::create_surface(GLFWwindow *p_window) {
	if (glfwCreateWindowSurface(instance_, p_window, nullptr, &surface_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: glfwCreateWindowSurface() failed.");
		return false;
	}
	return true;
}

VulkanRenderer::QueueFamilies VulkanRenderer::find_queue_families(VkPhysicalDevice p_device) const {
	QueueFamilies families;
	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &count, nullptr);
	std::vector<VkQueueFamilyProperties> properties(count);
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &count, properties.data());

	for (uint32_t i = 0; i < count; ++i) {
		if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			families.graphics = i;
		}
		VkBool32 present_support = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface_, &present_support);
		if (present_support) {
			families.present = i;
		}
		if (families.complete()) {
			break;
		}
	}
	return families;
}

bool VulkanRenderer::pick_physical_device() {
	uint32_t count = 0;
	vkEnumeratePhysicalDevices(instance_, &count, nullptr);
	if (count == 0) {
		ZILLA_LOG_ERROR("Vulkan: no GPU with Vulkan support was found.");
		return false;
	}
	std::vector<VkPhysicalDevice> devices(count);
	vkEnumeratePhysicalDevices(instance_, &count, devices.data());

	int best_score = -1;
	for (VkPhysicalDevice device : devices) {
		QueueFamilies families = find_queue_families(device);
		if (!families.complete()) {
			continue;
		}
		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
		std::vector<VkExtensionProperties> extensions(extension_count);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, extensions.data());
		bool has_swapchain = false;
		for (const VkExtensionProperties &extension : extensions) {
			if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
				has_swapchain = true;
				break;
			}
		}
		if (!has_swapchain) {
			continue;
		}

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);
		int score = 0;
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score = 2;
		} else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			score = 1;
		}
		if (score > best_score) {
			best_score = score;
			physical_device_ = device;
			families_ = families;
			device_name_ = properties.deviceName;
		}
	}

	if (best_score < 0) {
		ZILLA_LOG_ERROR("Vulkan: no suitable GPU found.");
		return false;
	}
	ZILLA_LOG_INFO("Vulkan: using %s.", device_name_.c_str());
	return true;
}

bool VulkanRenderer::create_device(bool p_validation) {
	const std::set<uint32_t> unique_families = { families_.graphics, families_.present };
	std::vector<VkDeviceQueueCreateInfo> queue_infos;
	const float priority = 1.0f;
	for (uint32_t family : unique_families) {
		VkDeviceQueueCreateInfo queue_info{};
		queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info.queueFamilyIndex = family;
		queue_info.queueCount = 1;
		queue_info.pQueuePriorities = &priority;
		queue_infos.push_back(queue_info);
	}

	VkPhysicalDeviceFeatures features{};
	const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = uint32_t(queue_infos.size());
	create_info.pQueueCreateInfos = queue_infos.data();
	create_info.enabledExtensionCount = 1;
	create_info.ppEnabledExtensionNames = device_extensions;
	create_info.pEnabledFeatures = &features;
	if (p_validation && check_validation_layer()) {
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = &VALIDATION_LAYER;
	}

	if (vkCreateDevice(physical_device_, &create_info, nullptr, &device_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateDevice() failed.");
		return false;
	}
	volkLoadDevice(device_);

	vkGetDeviceQueue(device_, families_.graphics, 0, &graphics_queue_);
	vkGetDeviceQueue(device_, families_.present, 0, &present_queue_);
	return true;
}

bool VulkanRenderer::create_swapchain() {
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &capabilities);

	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &format_count, formats.data());

	VkSurfaceFormatKHR format = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	if (!formats.empty()) {
		format = formats[0];
		for (const VkSurfaceFormatKHR &candidate : formats) {
			if (candidate.format == VK_FORMAT_B8G8R8A8_UNORM &&
					candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				format = candidate;
				break;
			}
		}
	}

	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, nullptr);
	std::vector<VkPresentModeKHR> present_modes(present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &present_mode_count, present_modes.data());
	for (VkPresentModeKHR candidate : present_modes) {
		if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) {
			present_mode = candidate;
			break;
		}
	}

	int window_width = 0;
	int window_height = 0;
	glfwGetFramebufferSize(window_, &window_width, &window_height);
	VkExtent2D extent = capabilities.currentExtent;
	if (extent.width == UINT32_MAX || extent.width == 0 || extent.height == 0) {
		extent.width = uint32_t(clamp(float(window_width),
				float(capabilities.minImageExtent.width), float(capabilities.maxImageExtent.width)));
		extent.height = uint32_t(clamp(float(window_height),
				float(capabilities.minImageExtent.height), float(capabilities.maxImageExtent.height)));
	}
	if (extent.width == 0 || extent.height == 0) {
		return false;
	}
	extent_ = extent;
	swapchain_format_ = format.format;

	uint32_t image_count = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
		image_count = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface_;
	create_info.minImageCount = image_count;
	create_info.imageFormat = format.format;
	create_info.imageColorSpace = format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.preTransform = capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = swapchain_;

	const std::vector<uint32_t> queue_indices = unique_family_indices();
	if (queue_indices.size() > 1) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = uint32_t(queue_indices.size());
		create_info.pQueueFamilyIndices = queue_indices.data();
	} else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(device_, &create_info, nullptr, &new_swapchain) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateSwapchainKHR() failed.");
		return false;
	}

	destroy_swapchain_resources();
	swapchain_ = new_swapchain;

	uint32_t created_count = 0;
	vkGetSwapchainImagesKHR(device_, swapchain_, &created_count, nullptr);
	swapchain_images_.resize(created_count);
	vkGetSwapchainImagesKHR(device_, swapchain_, &created_count, swapchain_images_.data());

	swapchain_views_.resize(created_count);
	for (uint32_t i = 0; i < created_count; ++i) {
		VkImageViewCreateInfo view_info{};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = swapchain_images_[i];
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = swapchain_format_;
		view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.layerCount = 1;
		if (vkCreateImageView(device_, &view_info, nullptr, &swapchain_views_[i]) != VK_SUCCESS) {
			ZILLA_LOG_ERROR("Vulkan: vkCreateImageView() failed.");
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::create_render_pass() {
	VkAttachmentDescription attachment{};
	attachment.format = swapchain_format_;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_reference{};
	color_reference.attachment = 0;
	color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_reference;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.attachmentCount = 1;
	create_info.pAttachments = &attachment;
	create_info.subpassCount = 1;
	create_info.pSubpasses = &subpass;
	create_info.dependencyCount = 1;
	create_info.pDependencies = &dependency;

	return vkCreateRenderPass(device_, &create_info, nullptr, &render_pass_) == VK_SUCCESS;
}

bool VulkanRenderer::create_descriptor_resources() {
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.bindingCount = 1;
	layout_info.pBindings = &binding;
	if (vkCreateDescriptorSetLayout(device_, &layout_info, nullptr, &descriptor_layout_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateDescriptorSetLayout() failed.");
		return false;
	}

	VkDescriptorPoolSize pool_size{};
	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = 1;

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	pool_info.maxSets = 1;
	if (vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateDescriptorPool() failed.");
		return false;
	}

	VkDescriptorSetAllocateInfo allocate_info{};
	allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocate_info.descriptorPool = descriptor_pool_;
	allocate_info.descriptorSetCount = 1;
	allocate_info.pSetLayouts = &descriptor_layout_;
	if (vkAllocateDescriptorSets(device_, &allocate_info, &descriptor_set_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkAllocateDescriptorSets() failed.");
		return false;
	}
	return true;
}

bool VulkanRenderer::create_pipeline() {
	VkShaderModule vertex_module = VK_NULL_HANDLE;
	VkShaderModule fragment_module = VK_NULL_HANDLE;

	auto create_module = [&](const uint32_t *p_code, size_t p_size, VkShaderModule &p_out) {
		VkShaderModuleCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		info.codeSize = p_size;
		info.pCode = p_code;
		return vkCreateShaderModule(device_, &info, nullptr, &p_out) == VK_SUCCESS;
	};

	if (!create_module(quad_vert, quad_vert_size, vertex_module)) {
		ZILLA_LOG_ERROR("Vulkan: could not create the vertex shader module.");
		return false;
	}
	if (!create_module(quad_frag, quad_frag_size, fragment_module)) {
		ZILLA_LOG_ERROR("Vulkan: could not create the fragment shader module.");
		return false;
	}

	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertex_module;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragment_module;
	stages[1].pName = "main";

	VkVertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(DrawVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributes[7]{};
	attributes[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(DrawVertex, x) };
	attributes[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(DrawVertex, u) };
	attributes[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(DrawVertex, lx) };
	attributes[3] = { 3, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(DrawVertex, fill) };
	attributes[4] = { 4, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(DrawVertex, border) };
	attributes[5] = { 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(DrawVertex, half_w) };
	attributes[6] = { 6, 0, VK_FORMAT_R32_SFLOAT, offsetof(DrawVertex, tex) };

	VkPipelineVertexInputStateCreateInfo vertex_input{};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 7;
	vertex_input.pVertexAttributeDescriptions = attributes;

	VkPipelineInputAssemblyStateCreateInfo input_assembly{};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample{};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState blend{};
	blend.blendEnable = VK_TRUE;
	blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.colorBlendOp = VK_BLEND_OP_ADD;
	blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.alphaBlendOp = VK_BLEND_OP_ADD;
	blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blending{};
	blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blending.attachmentCount = 1;
	blending.pAttachments = &blend;

	const VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state{};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;

	VkPushConstantRange push_constant{};
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	push_constant.offset = 0;
	push_constant.size = sizeof(float) * 2;

	VkPipelineLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &descriptor_layout_;
	layout_info.pushConstantRangeCount = 1;
	layout_info.pPushConstantRanges = &push_constant;
	if (vkCreatePipelineLayout(device_, &layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreatePipelineLayout() failed.");
		return false;
	}

	VkGraphicsPipelineCreateInfo pipeline_info{};
	pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = stages;
	pipeline_info.pVertexInputState = &vertex_input;
	pipeline_info.pInputAssemblyState = &input_assembly;
	pipeline_info.pViewportState = &viewport_state;
	pipeline_info.pRasterizationState = &rasterizer;
	pipeline_info.pMultisampleState = &multisample;
	pipeline_info.pColorBlendState = &blending;
	pipeline_info.pDynamicState = &dynamic_state;
	pipeline_info.layout = pipeline_layout_;
	pipeline_info.renderPass = render_pass_;
	pipeline_info.subpass = 0;

	const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
			nullptr, &pipeline_) == VK_SUCCESS;

	vkDestroyShaderModule(device_, vertex_module, nullptr);
	vkDestroyShaderModule(device_, fragment_module, nullptr);

	if (!created) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateGraphicsPipelines() failed.");
	}
	return created;
}

bool VulkanRenderer::create_framebuffers() {
	framebuffers_.resize(swapchain_views_.size());
	for (size_t i = 0; i < swapchain_views_.size(); ++i) {
		VkFramebufferCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		create_info.renderPass = render_pass_;
		create_info.attachmentCount = 1;
		create_info.pAttachments = &swapchain_views_[i];
		create_info.width = extent_.width;
		create_info.height = extent_.height;
		create_info.layers = 1;
		if (vkCreateFramebuffer(device_, &create_info, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
			ZILLA_LOG_ERROR("Vulkan: vkCreateFramebuffer() failed.");
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::create_command_buffers() {
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = families_.graphics;
	if (vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateCommandPool() failed.");
		return false;
	}

	command_buffers_.resize(FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocate_info{};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = FRAMES_IN_FLIGHT;
	return vkAllocateCommandBuffers(device_, &allocate_info, command_buffers_.data()) == VK_SUCCESS;
}

bool VulkanRenderer::create_sync_objects() {
	image_available_.resize(FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	render_finished_.resize(FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	in_flight_.resize(FRAMES_IN_FLIGHT, VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphore_info{};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fence_info{};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
		if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_[i]) != VK_SUCCESS ||
				vkCreateFence(device_, &fence_info, nullptr, &in_flight_[i]) != VK_SUCCESS) {
			ZILLA_LOG_ERROR("Vulkan: could not create the sync objects.");
			return false;
		}
	}
	return true;
}

// --- memory -----------------------------------------------------------------

uint32_t VulkanRenderer::find_memory_type(uint32_t p_type_filter, VkMemoryPropertyFlags p_properties) const {
	VkPhysicalDeviceMemoryProperties memory_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
		if ((p_type_filter & (1u << i)) &&
				(memory_properties.memoryTypes[i].propertyFlags & p_properties) == p_properties) {
			return i;
		}
	}
	ZILLA_LOG_ERROR("Vulkan: no suitable memory type found.");
	return UINT32_MAX;
}

bool VulkanRenderer::ensure_buffer(Buffer &p_buffer, VkBufferUsageFlags p_usage, VkDeviceSize p_capacity) {
	if (p_buffer.buffer != VK_NULL_HANDLE && p_buffer.capacity >= p_capacity) {
		return true;
	}

	if (p_buffer.buffer != VK_NULL_HANDLE) {
		// The GPU may still be using the old buffer.
		vkDeviceWaitIdle(device_);
		if (p_buffer.mapped != nullptr) {
			vkUnmapMemory(device_, p_buffer.memory);
			p_buffer.mapped = nullptr;
		}
		vkDestroyBuffer(device_, p_buffer.buffer, nullptr);
		vkFreeMemory(device_, p_buffer.memory, nullptr);
		p_buffer.buffer = VK_NULL_HANDLE;
		p_buffer.memory = VK_NULL_HANDLE;
	}

	const VkDeviceSize capacity = std::max(p_capacity, std::max<VkDeviceSize>(p_buffer.capacity * 2, 65536));

	VkBufferCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	create_info.size = capacity;
	create_info.usage = p_usage;
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(device_, &create_info, nullptr, &p_buffer.buffer) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateBuffer() failed.");
		return false;
	}

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(device_, p_buffer.buffer, &requirements);
	VkMemoryAllocateInfo allocate_info{};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize = requirements.size;
	allocate_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (allocate_info.memoryTypeIndex == UINT32_MAX) {
		return false;
	}
	if (vkAllocateMemory(device_, &allocate_info, nullptr, &p_buffer.memory) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkAllocateMemory() failed.");
		return false;
	}
	vkBindBufferMemory(device_, p_buffer.buffer, p_buffer.memory, 0);
	if (vkMapMemory(device_, p_buffer.memory, 0, capacity, 0, &p_buffer.mapped) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkMapMemory() failed.");
		return false;
	}
	p_buffer.capacity = capacity;
	return true;
}

std::vector<uint32_t> VulkanRenderer::unique_family_indices() const {
	std::vector<uint32_t> indices;
	indices.push_back(families_.graphics);
	if (families_.present != families_.graphics) {
		indices.push_back(families_.present);
	}
	return indices;
}

VkCommandBuffer VulkanRenderer::begin_single_time_commands() {
	VkCommandBufferAllocateInfo allocate_info{};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandPool = command_pool_;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(device_, &allocate_info, &command_buffer);

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);
	return command_buffer;
}

void VulkanRenderer::end_single_time_commands(VkCommandBuffer p_command_buffer) {
	vkEndCommandBuffer(p_command_buffer);
	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &p_command_buffer;
	vkQueueSubmit(graphics_queue_, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphics_queue_);
	vkFreeCommandBuffers(device_, command_pool_, 1, &p_command_buffer);
}

bool VulkanRenderer::upload_font_atlas(const uint8_t *p_pixels, int p_width, int p_height) {
	const VkDeviceSize size = VkDeviceSize(p_width) * VkDeviceSize(p_height);

	Buffer staging{};
	if (!ensure_buffer(staging, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size)) {
		return false;
	}
	std::memcpy(staging.mapped, p_pixels, size_t(size));

	VkImageCreateInfo image_info{};
	image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = VK_FORMAT_R8_UNORM;
	image_info.extent = { uint32_t(p_width), uint32_t(p_height), 1 };
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (vkCreateImage(device_, &image_info, nullptr, &font_image_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateImage() failed.");
		return false;
	}

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(device_, font_image_, &requirements);
	VkMemoryAllocateInfo allocate_info{};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize = requirements.size;
	allocate_info.memoryTypeIndex = find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (allocate_info.memoryTypeIndex == UINT32_MAX) {
		return false;
	}
	if (vkAllocateMemory(device_, &allocate_info, nullptr, &font_memory_) != VK_SUCCESS) {
		return false;
	}
	vkBindImageMemory(device_, font_image_, font_memory_, 0);

	auto transition = [&](VkCommandBuffer p_cmd, VkImageLayout p_old, VkImageLayout p_new,
							  VkAccessFlags p_src_access, VkAccessFlags p_dst_access,
							  VkPipelineStageFlags p_src_stage, VkPipelineStageFlags p_dst_stage) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = p_old;
		barrier.newLayout = p_new;
		barrier.srcAccessMask = p_src_access;
		barrier.dstAccessMask = p_dst_access;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = font_image_;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(p_cmd, p_src_stage, p_dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	};

	VkCommandBuffer command_buffer = begin_single_time_commands();
	transition(command_buffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { uint32_t(p_width), uint32_t(p_height), 1 };
	vkCmdCopyBufferToImage(command_buffer, staging.buffer, font_image_,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	transition(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	end_single_time_commands(command_buffer);

	VkImageViewCreateInfo view_info{};
	view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_info.image = font_image_;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = VK_FORMAT_R8_UNORM;
	view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = 1;
	if (vkCreateImageView(device_, &view_info, nullptr, &font_view_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: could not create the font image view.");
		return false;
	}

	VkSamplerCreateInfo sampler_info{};
	sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler_info.magFilter = VK_FILTER_LINEAR;
	sampler_info.minFilter = VK_FILTER_LINEAR;
	sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.maxAnisotropy = 1.0f;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;
	sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.0f;
	if (vkCreateSampler(device_, &sampler_info, nullptr, &font_sampler_) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkCreateSampler() failed.");
		return false;
	}

	VkDescriptorImageInfo image_descriptor{};
	image_descriptor.imageView = font_view_;
	image_descriptor.sampler = font_sampler_;
	image_descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set_;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &image_descriptor;
	vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

	if (staging.mapped != nullptr) {
		vkUnmapMemory(device_, staging.memory);
	}
	vkDestroyBuffer(device_, staging.buffer, nullptr);
	vkFreeMemory(device_, staging.memory, nullptr);

	ZILLA_LOG_INFO("Vulkan: uploaded a %dx%d font atlas.", p_width, p_height);
	return true;
}

// --- frame ------------------------------------------------------------------

void VulkanRenderer::destroy_swapchain_resources() {
	for (VkFramebuffer framebuffer : framebuffers_) {
		if (framebuffer != VK_NULL_HANDLE) {
			vkDestroyFramebuffer(device_, framebuffer, nullptr);
		}
	}
	framebuffers_.clear();
	for (VkImageView view : swapchain_views_) {
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(device_, view, nullptr);
		}
	}
	swapchain_views_.clear();
	swapchain_images_.clear();
	if (swapchain_ != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(device_, swapchain_, nullptr);
		swapchain_ = VK_NULL_HANDLE;
	}
}

void VulkanRenderer::recreate_swapchain() {
	vkDeviceWaitIdle(device_);
	if (!create_swapchain()) {
		ZILLA_LOG_ERROR("Vulkan: swapchain recreation failed.");
		return;
	}
	create_framebuffers();
}

bool VulkanRenderer::frame(const DrawList &p_list, const Color &p_clear, float p_content_scale) {
	int framebuffer_width = 0;
	int framebuffer_height = 0;
	glfwGetFramebufferSize(window_, &framebuffer_width, &framebuffer_height);
	if (framebuffer_width <= 0 || framebuffer_height <= 0) {
		return false; // window is minimised
	}

	if (rebuild_swapchain_ || uint32_t(framebuffer_width) != extent_.width ||
			uint32_t(framebuffer_height) != extent_.height) {
		recreate_swapchain();
		rebuild_swapchain_ = false;
		return false;
	}

	vkWaitForFences(device_, 1, &in_flight_[current_frame_], VK_TRUE, UINT64_MAX);

	uint32_t image_index = 0;
	VkResult result = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
			image_available_[current_frame_], VK_NULL_HANDLE, &image_index);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreate_swapchain();
		return false;
	}
	if (result != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkAcquireNextImageKHR() failed (%d).", int(result));
		return false;
	}
	vkResetFences(device_, 1, &in_flight_[current_frame_]);

	// Upload the geometry for this frame.
	const size_t vertex_bytes = p_list.vertices().size() * sizeof(DrawVertex);
	const size_t index_bytes = p_list.indices().size() * sizeof(uint32_t);
	if (!p_list.indices().empty()) {
		if (!ensure_buffer(vertex_buffer_, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VkDeviceSize(vertex_bytes)) ||
				!ensure_buffer(index_buffer_, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VkDeviceSize(index_bytes))) {
			return false;
		}
		std::memcpy(vertex_buffer_.mapped, p_list.vertices().data(), vertex_bytes);
		std::memcpy(index_buffer_.mapped, p_list.indices().data(), index_bytes);
	}

	VkCommandBuffer command_buffer = command_buffers_[current_frame_];
	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkClearValue clear_value{};
	clear_value.color = { { p_clear.r, p_clear.g, p_clear.b, 1.0f } };

	VkRenderPassBeginInfo render_pass_info{};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass_;
	render_pass_info.framebuffer = framebuffers_[image_index];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = extent_;
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_value;
	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

	if (!p_list.indices().empty()) {
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = float(extent_.width);
		viewport.height = float(extent_.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(command_buffer, 0, 1, &viewport);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
		const VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_.buffer, offsets);
		vkCmdBindIndexBuffer(command_buffer, index_buffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
				0, 1, &descriptor_set_, 0, nullptr);

		const float scale[2] = {
			2.0f * p_content_scale / float(extent_.width),
			2.0f * p_content_scale / float(extent_.height),
		};
		vkCmdPushConstants(command_buffer, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
				sizeof(scale), scale);

		for (const DrawCommand &command : p_list.commands()) {
			VkRect2D scissor{};
			if (command.has_clip) {
				const float x = command.clip.x * p_content_scale;
				const float y = command.clip.y * p_content_scale;
				const float w = command.clip.w * p_content_scale;
				const float h = command.clip.h * p_content_scale;
				scissor.offset.x = int32_t(clamp(x, 0.0f, float(extent_.width)));
				scissor.offset.y = int32_t(clamp(y, 0.0f, float(extent_.height)));
				const float right = clamp(x + w, float(scissor.offset.x), float(extent_.width));
				const float bottom = clamp(y + h, float(scissor.offset.y), float(extent_.height));
				scissor.extent.width = uint32_t(right - float(scissor.offset.x));
				scissor.extent.height = uint32_t(bottom - float(scissor.offset.y));
			} else {
				scissor.offset = { 0, 0 };
				scissor.extent = extent_;
			}
			if (scissor.extent.width == 0 || scissor.extent.height == 0) {
				continue;
			}
			vkCmdSetScissor(command_buffer, 0, 1, &scissor);
			vkCmdDrawIndexed(command_buffer, command.index_count, 1, command.first_index, 0, 0);
		}
	}

	vkCmdEndRenderPass(command_buffer);
	if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkEndCommandBuffer() failed.");
		return false;
	}

	VkSemaphore wait_semaphores[] = { image_available_[current_frame_] };
	VkSemaphore signal_semaphores[] = { render_finished_[current_frame_] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	if (vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_[current_frame_]) != VK_SUCCESS) {
		ZILLA_LOG_ERROR("Vulkan: vkQueueSubmit() failed.");
		return false;
	}

	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain_;
	present_info.pImageIndices = &image_index;

	result = vkQueuePresentKHR(present_queue_, &present_info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		rebuild_swapchain_ = true;
	}

	current_frame_ = (current_frame_ + 1) % FRAMES_IN_FLIGHT;
	return true;
}

// --- teardown ---------------------------------------------------------------

void VulkanRenderer::shutdown() {
	if (device_ == VK_NULL_HANDLE) {
		return;
	}
	vkDeviceWaitIdle(device_);

	auto destroy_buffer = [&](Buffer &p_buffer) {
		if (p_buffer.mapped != nullptr) {
			vkUnmapMemory(device_, p_buffer.memory);
		}
		if (p_buffer.buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(device_, p_buffer.buffer, nullptr);
		}
		if (p_buffer.memory != VK_NULL_HANDLE) {
			vkFreeMemory(device_, p_buffer.memory, nullptr);
		}
		p_buffer = Buffer();
	};
	destroy_buffer(vertex_buffer_);
	destroy_buffer(index_buffer_);

	for (VkFence fence : in_flight_) {
		if (fence != VK_NULL_HANDLE) {
			vkDestroyFence(device_, fence, nullptr);
		}
	}
	for (VkSemaphore semaphore : image_available_) {
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(device_, semaphore, nullptr);
		}
	}
	for (VkSemaphore semaphore : render_finished_) {
		if (semaphore != VK_NULL_HANDLE) {
			vkDestroySemaphore(device_, semaphore, nullptr);
		}
	}
	in_flight_.clear();
	image_available_.clear();
	render_finished_.clear();

	if (command_pool_ != VK_NULL_HANDLE) {
		vkDestroyCommandPool(device_, command_pool_, nullptr);
		command_pool_ = VK_NULL_HANDLE;
	}
	if (pipeline_ != VK_NULL_HANDLE) {
		vkDestroyPipeline(device_, pipeline_, nullptr);
	}
	if (pipeline_layout_ != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
	}
	if (font_sampler_ != VK_NULL_HANDLE) {
		vkDestroySampler(device_, font_sampler_, nullptr);
	}
	if (font_view_ != VK_NULL_HANDLE) {
		vkDestroyImageView(device_, font_view_, nullptr);
	}
	if (font_image_ != VK_NULL_HANDLE) {
		vkDestroyImage(device_, font_image_, nullptr);
	}
	if (font_memory_ != VK_NULL_HANDLE) {
		vkFreeMemory(device_, font_memory_, nullptr);
	}
	if (descriptor_pool_ != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
	}
	if (descriptor_layout_ != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
	}
	if (render_pass_ != VK_NULL_HANDLE) {
		vkDestroyRenderPass(device_, render_pass_, nullptr);
	}
	destroy_swapchain_resources();
	if (surface_ != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance_, surface_, nullptr);
	}
	vkDestroyDevice(device_, nullptr);
	device_ = VK_NULL_HANDLE;

	if (debug_messenger_ != VK_NULL_HANDLE) {
		vkDestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
	}
	vkDestroyInstance(instance_, nullptr);
	instance_ = VK_NULL_HANDLE;
}

} // namespace zilla
