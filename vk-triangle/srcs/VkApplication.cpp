//
//  VkApplication.cpp
//  vk-triangle
//
//  Created by Kai Chen on 10/22/22.
//

#include "VkApplication.hpp"

#include <iostream>
#include <string>
#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const int MAX_FRAMES_IN_FLIGHT = 2;

void VkApplication::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void VkApplication::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    window = glfwCreateWindow(800, 600, "Vulkan Triangle", nullptr, nullptr);
}

void VkApplication::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        drawFrame();
    }
    vkDeviceWaitIdle(vkbDevice.device);
}

void VkApplication::cleanup() {
    auto device = vkbDevice.device;
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, data.finishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, data.availableSemaphores[i], nullptr);
        vkDestroyFence(device, data.inflightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, data.commandPool, nullptr);
    
    for (auto framebuffer: data.framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    
    vkDestroyPipeline(device, data.graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, data.pipelineLayout, nullptr);
    vkDestroyRenderPass(device, data.renderPass, nullptr);
    
    for (auto imageView: data.imageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    
    vkb::destroy_swapchain(vkbSwapchain);
    vkb::destroy_device(vkbDevice);
    vkb::destroy_surface(vkbInstance, vkSurface);
    vkb::destroy_instance(vkbInstance);
    
    glfwDestroyWindow(window);
    glfwTerminate();
}

void VkApplication::initVulkan() {
    createDevice();
    createSwapchain();
    initQueues();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

void VkApplication::createDevice() {
    vkb::InstanceBuilder builder;
    auto instance = builder
        .set_app_name("Vulkan Triangle")
//        .use_default_debug_messenger()
        .request_validation_layers()
        .set_debug_callback([] (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                VkDebugUtilsMessageTypeFlagsEXT messageType,
                                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                void* pUserData) -> VkBool32 {
            auto severity = vkb::to_string_message_severity(messageSeverity);
            auto type = vkb::to_string_message_type(messageType);
            std::cout << "[" << severity << ": " << type << "] "<< pCallbackData->pMessage << std::endl;
            return VK_FALSE;
        })
        .build();
    if (!instance) {
        std::cout << instance.error().message() << std::endl;
        throw std::runtime_error(instance.error().message());
    }
    
    vkbInstance = instance.value();
    
    // Create surface
    if (glfwCreateWindowSurface(vkbInstance.instance, window, nullptr, &vkSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
    
    // Physical device
    vkb::PhysicalDeviceSelector seletor(vkbInstance);
    auto physDevice = seletor
        .set_surface(vkSurface)
        .set_minimum_version(1, 1)
        .select();
    if (!physDevice) {
        std::cout << physDevice.error().message() << std::endl;
        throw std::runtime_error(physDevice.error().message());
    }
    
    // Device
    vkb::DeviceBuilder deviceBuilder { physDevice.value() };
    auto device = deviceBuilder.build();
    if (!device) {
        std::cout << device.error().message() << std::endl;
        throw std::runtime_error(device.error().message());
    }
    
    vkbDevice = device.value();
    
}

void VkApplication::createSwapchain() {
    vkb::SwapchainBuilder builder { vkbDevice };
    auto swapchain = builder
        .set_old_swapchain(vkbSwapchain)
        .build();
    if (!swapchain) {
        std::cout << swapchain.error().message() << " " << swapchain.vk_result() << std::endl;
        throw std::runtime_error(swapchain.error().message() + " " + std::to_string(swapchain.vk_result()));
    }
        
    vkb::destroy_swapchain(vkbSwapchain);
    vkbSwapchain = swapchain.value();
}

void VkApplication::initQueues() {
    auto graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics);
    if (!graphicsQueue.has_value()) {
        std::cout << "failed to get graphics queue: " << graphicsQueue.error().message() << std::endl;
        throw std::runtime_error("failed to get graphics queue: " +  graphicsQueue.error().message());
    }
    data.graphicsQueue = graphicsQueue.value();
    
    auto presentQueue = vkbDevice.get_queue(vkb::QueueType::present);
    if (!presentQueue.has_value()) {
        std::cout << "failed to get present queue: " << presentQueue.error().message() << std::endl;
        throw std::runtime_error("failed to get present queue: " + presentQueue.error().message());
    }
    data.presentQueue = presentQueue.value();
}

void VkApplication::createRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = vkbSwapchain.image_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;
    
    if (vkCreateRenderPass(vkbDevice.device, &renderPassInfo, nullptr, &data.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass");
    }
}

std::vector<char> VkApplication::readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename) ;
    }
    
    size_t filesize = (size_t)file.tellg();
    std::vector<char> buffer(filesize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(filesize));
    
    file.close();
    
    return buffer;
}

VkShaderModule VkApplication::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(vkbDevice.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    
    return shaderModule;
}

void VkApplication::createGraphicsPipeline() {
    auto vertCode = readFile("shaders/vert.spv");
    auto fragCode = readFile("shaders/frag.spv");
    
    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);
    
    if (VK_NULL_HANDLE == vertModule || VK_NULL_HANDLE == fragModule) {
        std::cout << "failed to create shader module" << std::endl;
        throw std::runtime_error("failed to create shader module");
    }
    
    VkPipelineShaderStageCreateInfo vert_stage_info = {};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vertModule;
    vert_stage_info.pName = "main";
    
    VkPipelineShaderStageCreateInfo frag_stage_info = {};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = fragModule;
    frag_stage_info.pName = "main";
    
    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage_info, frag_stage_info };
    
    VkPipelineVertexInputStateCreateInfo vertex_input_info = {};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vkbSwapchain.extent.width;
    viewport.height = (float)vkbSwapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = vkbSwapchain.extent;
    
    VkPipelineViewportStateCreateInfo viewport_state = {};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo color_blending = {};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &colorBlendAttachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pushConstantRangeCount = 0;
    
    if (vkCreatePipelineLayout(vkbDevice.device, &pipeline_layout_info, nullptr, &data.pipelineLayout) != VK_SUCCESS) {
        std::cout << "failed to create pipeline layout" << std::endl;
        throw std::runtime_error("failed to create pipeline layout");
    }
    
    std::vector<VkDynamicState> dynamic_states = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    
    VkPipelineDynamicStateCreateInfo dynamic_info = {};
    dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_info.dynamicStateCount = static_cast<uint32_t> (dynamic_states.size ());
    dynamic_info.pDynamicStates = dynamic_states.data ();
    
    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_info;
    pipeline_info.layout = data.pipelineLayout;
    pipeline_info.renderPass = data.renderPass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    
    if (vkCreateGraphicsPipelines(vkbDevice.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &data.graphicsPipeline) != VK_SUCCESS) {
        std::cout << "failed to create pipline" << std::endl;
        throw std::runtime_error("failed to create pipline");
    }
    
    vkDestroyShaderModule(vkbDevice.device, fragModule, nullptr);
    vkDestroyShaderModule(vkbDevice.device, vertModule, nullptr);
}

void VkApplication::createFramebuffers() {
    data.images = vkbSwapchain.get_images().value();
    data.imageViews = vkbSwapchain.get_image_views().value();
    
    data.framebuffers.resize(data.imageViews.size());
    for (size_t i = 0; i < data.imageViews.size(); i++) {
        VkImageView attachments[] = { data.imageViews[i] };
        
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = data.renderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = vkbSwapchain.extent.width;
        info.height = vkbSwapchain.extent.height;
        info.layers = 1;
        
        if (vkCreateFramebuffer(vkbDevice.device, &info, nullptr, &data.framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer at index: " + std::to_string(i));
        }
    }
}

void VkApplication::createCommandPool() {
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    if (vkCreateCommandPool(vkbDevice.device, &info, nullptr, &data.commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool");
    }
}

void VkApplication::createCommandBuffers() {
    data.commandBuffers.resize(data.framebuffers.size());
    
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = data.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)data.commandBuffers.size();
    
    if (vkAllocateCommandBuffers(vkbDevice.device, &allocInfo, data.commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command buffers");
    }
    
    for (size_t i = 0; i < data.commandBuffers.size(); i++) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(data.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin command buffer at [" + std::to_string(i) + "]");
        }
        
        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = data.renderPass;
        renderPassInfo.framebuffer = data.framebuffers[i];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = vkbSwapchain.extent;
        
        VkClearValue clearColor { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        
        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)vkbSwapchain.extent.width;
        viewport.height = (float)vkbSwapchain.extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor = {};
        scissor.offset = { 0, 0 };
        scissor.extent = vkbSwapchain.extent;
        
        vkCmdSetViewport(data.commandBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(data.commandBuffers[i], 0, 1, &scissor);
        vkCmdBeginRenderPass(data.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(data.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, data.graphicsPipeline);
        vkCmdDraw(data.commandBuffers[i], 3, 1, 0, 0);
        vkCmdEndRenderPass(data.commandBuffers[i]);
        
        if (vkEndCommandBuffer(data.commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to end command buffer at [" + std::to_string(i) + "]");
        }
    }
}

void VkApplication::createSyncObjects() {
    data.availableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    data.finishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    data.inflightFences.resize(MAX_FRAMES_IN_FLIGHT);
    data.imageInflight.resize(vkbSwapchain.image_count, VK_NULL_HANDLE);
    
    VkSemaphoreCreateInfo semaphore = {};
    semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fence = {};
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
    auto device = vkbDevice.device;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphore, nullptr, &data.availableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphore, nullptr, &data.finishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fence, nullptr, &data.inflightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create sync objects");
        }
    }
}

void VkApplication::recreateSwapchain() {
    auto device = vkbDevice.device;
    vkDeviceWaitIdle(device);
    vkDestroyCommandPool(device, data.commandPool, nullptr);
    
    for (auto framebuffer: data.framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    
    vkbSwapchain.destroy_image_views(data.imageViews);
    
    createSwapchain();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
}

void VkApplication::drawFrame() {
    auto device = vkbDevice.device;
    
    vkWaitForFences(device, 1, &data.inflightFences[data.currentFrame], VK_TRUE, UINT64_MAX);
    
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device, vkbSwapchain.swapchain, UINT64_MAX, data.availableSemaphores[data.currentFrame], VK_NULL_HANDLE, &imageIndex);
    
    if (VK_ERROR_OUT_OF_DATE_KHR == result) {
        recreateSwapchain();
        return;
    } else if (VK_SUCCESS != result && VK_SUBOPTIMAL_KHR != result) {
        throw std::runtime_error("failed to acquire swapchain image. Error " + std::to_string(result));
    }
    
    if (data.imageInflight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &data.imageInflight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    data.imageInflight[imageIndex] = data.inflightFences[data.currentFrame];
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkSemaphore waitSemaphores[] = { data.availableSemaphores[data.currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &data.commandBuffers[imageIndex];
    
    VkSemaphore signalSemaphores[] = { data.finishedSemaphores[data.currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    
    vkResetFences(device, 1, &data.inflightFences[data.currentFrame]);
    
    if (vkQueueSubmit(data.graphicsQueue, 1, &submitInfo, data.inflightFences[data.currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer");
    }
    
    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signalSemaphores;
    
    VkSwapchainKHR swapchains[] = { vkbSwapchain };
    present.swapchainCount = 1;
    present.pSwapchains = swapchains;
    
    present.pImageIndices = &imageIndex;
    
    result = vkQueuePresentKHR(data.presentQueue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swapchain image");
    }
    
    data.currentFrame = (data.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
