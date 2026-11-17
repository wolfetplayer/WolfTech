// vk_pipelines.c -- render pass, framebuffers, descriptor/pipeline layout, and the two 2D pipeline variants.

#include <string.h>
#include <stddef.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_shaders.h"
#include "vk_pipelines.h"
#include "tr_image.h"

void vk_createRenderPass( void ) {
	VkAttachmentDescription attachments[2];
	attachments[0].flags = 0;
	attachments[0].format = vk.surface_format.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	attachments[1].flags = 0;
	attachments[1].format = vk.depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference color_ref;
	color_ref.attachment = 0;
	color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depth_ref;
	depth_ref.attachment = 1;
	depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass;
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_ref;
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = &depth_ref;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkSubpassDependency dependency;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dependencyFlags = 0;

	VkRenderPassCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.attachmentCount = 2;
	desc.pAttachments = attachments;
	desc.subpassCount = 1;
	desc.pSubpasses = &subpass;
	desc.dependencyCount = 1;
	desc.pDependencies = &dependency;

	VK_CHECK( qvkCreateRenderPass( vk.device, &desc, NULL, &vk.render_pass ) );
}

void vk_destroyRenderPass( void ) {
	qvkDestroyRenderPass( vk.device, vk.render_pass, NULL );
}

void vk_createFrameBuffers( uint32_t width, uint32_t height ) {
	uint32_t i;
	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		VkImageView attachments[2];
		attachments[0] = vk.swapchain_image_views[i];
		attachments[1] = vk.depth_view;

		VkFramebufferCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.renderPass = vk.render_pass;
		desc.attachmentCount = 2;
		desc.pAttachments = attachments;
		desc.width = width;
		desc.height = height;
		desc.layers = 1;
		VK_CHECK( qvkCreateFramebuffer( vk.device, &desc, NULL, &vk.framebuffers[i] ) );
	}
}

void vk_destroyFrameBuffers( void ) {
	uint32_t i;
	for ( i = 0; i < vk.swapchain_image_count; i++ ) {
		qvkDestroyFramebuffer( vk.device, vk.framebuffers[i], NULL );
	}
}

void vk_createPipelineLayout( void ) {
	{
		VkDescriptorPoolSize pool_size;
		pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_size.descriptorCount = MAX_DRAWIMAGES;

		VkDescriptorPoolCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		desc.maxSets = MAX_DRAWIMAGES;
		desc.poolSizeCount = 1;
		desc.pPoolSizes = &pool_size;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &desc, NULL, &vk.descriptor_pool ) );
	}

	{
		VkDescriptorSetLayoutBinding binding;
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		binding.pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.bindingCount = 1;
		desc.pBindings = &binding;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.set_layout ) );
	}

	{
		// Matches single_texture.vert's push_constant size (128 bytes) even though only the first 64 are written.
		VkPushConstantRange push_range;
		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		push_range.offset = 0;
		push_range.size = 128;

		VkPipelineLayoutCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.setLayoutCount = 1;
		desc.pSetLayouts = &vk.set_layout;
		desc.pushConstantRangeCount = 1;
		desc.pPushConstantRanges = &push_range;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.pipeline_layout ) );
	}
}

void vk_destroyPipelineLayout( void ) {
	qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout, NULL );
	qvkDestroyDescriptorSetLayout( vk.device, vk.set_layout, NULL );
	qvkDestroyDescriptorPool( vk.device, vk.descriptor_pool, NULL );
}

typedef struct {
	int32_t alpha_test_func;
	int32_t color_op;
	int32_t clip_plane;
} vk2d_spec_data_t;

static VkPipeline create_pipeline( VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
									VkBool32 depthTestEnable, VkCullModeFlags cullMode ) {
	VkPipeline pipeline;

	vk2d_spec_data_t spec_data = { 0, 0, 0 };

	VkSpecializationMapEntry spec_entries[3];
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( vk2d_spec_data_t, alpha_test_func );
	spec_entries[0].size = sizeof( int32_t );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( vk2d_spec_data_t, color_op );
	spec_entries[1].size = sizeof( int32_t );
	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( vk2d_spec_data_t, clip_plane );
	spec_entries[2].size = sizeof( int32_t );

	VkSpecializationInfo spec_info;
	spec_info.mapEntryCount = 3;
	spec_info.pMapEntries = spec_entries;
	spec_info.dataSize = sizeof( spec_data );
	spec_info.pData = &spec_data;

	VkPipelineShaderStageCreateInfo stages[2];
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].pNext = NULL;
	stages[0].flags = 0;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk_single_texture_vs;
	stages[0].pName = "main";
	stages[0].pSpecializationInfo = &spec_info;

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].pNext = NULL;
	stages[1].flags = 0;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk_single_texture_fs;
	stages[1].pName = "main";
	stages[1].pSpecializationInfo = &spec_info;

	VkVertexInputBindingDescription binding;
	binding.binding = 0;
	binding.stride = sizeof( vk2d_vertex_t );
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attribs[3];
	attribs[0].location = 0;
	attribs[0].binding = 0;
	attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribs[0].offset = offsetof( vk2d_vertex_t, pos );
	attribs[1].location = 1;
	attribs[1].binding = 0;
	attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	attribs[1].offset = offsetof( vk2d_vertex_t, color );
	attribs[2].location = 2;
	attribs[2].binding = 0;
	attribs[2].format = VK_FORMAT_R32G32_SFLOAT;
	attribs[2].offset = offsetof( vk2d_vertex_t, uv );

	VkPipelineVertexInputStateCreateInfo vertex_input;
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.pNext = NULL;
	vertex_input.flags = 0;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &binding;
	vertex_input.vertexAttributeDescriptionCount = 3;
	vertex_input.pVertexAttributeDescriptions = attribs;

	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.pNext = NULL;
	input_assembly.flags = 0;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewport_state;
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = NULL;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = NULL;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = NULL;

	VkPipelineRasterizationStateCreateInfo rasterization;
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.pNext = NULL;
	rasterization.flags = 0;
	rasterization.depthClampEnable = VK_FALSE;
	rasterization.rasterizerDiscardEnable = VK_FALSE;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = cullMode;
	rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization.depthBiasEnable = VK_FALSE;
	rasterization.depthBiasConstantFactor = 0.0f;
	rasterization.depthBiasClamp = 0.0f;
	rasterization.depthBiasSlopeFactor = 0.0f;
	rasterization.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample;
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.pNext = NULL;
	multisample.flags = 0;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.sampleShadingEnable = VK_FALSE;
	multisample.minSampleShading = 1.0f;
	multisample.pSampleMask = NULL;
	multisample.alphaToCoverageEnable = VK_FALSE;
	multisample.alphaToOneEnable = VK_FALSE;

	// 2D UI shares the render pass's depth attachment but never tests/writes it.
	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.pNext = NULL;
	depth_stencil.flags = 0;
	depth_stencil.depthTestEnable = depthTestEnable;
	depth_stencil.depthWriteEnable = depthTestEnable;
	depth_stencil.depthCompareOp = depthTestEnable ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS;
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = VK_FALSE;
	memset( &depth_stencil.front, 0, sizeof( depth_stencil.front ) );
	memset( &depth_stencil.back, 0, sizeof( depth_stencil.back ) );
	depth_stencil.minDepthBounds = 0.0f;
	depth_stencil.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState blend_attachment;
	blend_attachment.blendEnable = blendEnable;
	blend_attachment.srcColorBlendFactor = srcBlend;
	blend_attachment.dstColorBlendFactor = dstBlend;
	blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
	blend_attachment.srcAlphaBlendFactor = srcBlend;
	blend_attachment.dstAlphaBlendFactor = dstBlend;
	blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
									   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blend_state;
	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = NULL;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &blend_attachment;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state;
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = NULL;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo desc;
	desc.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.stageCount = 2;
	desc.pStages = stages;
	desc.pVertexInputState = &vertex_input;
	desc.pInputAssemblyState = &input_assembly;
	desc.pTessellationState = NULL;
	desc.pViewportState = &viewport_state;
	desc.pRasterizationState = &rasterization;
	desc.pMultisampleState = &multisample;
	desc.pDepthStencilState = &depth_stencil;
	desc.pColorBlendState = &blend_state;
	desc.pDynamicState = &dynamic_state;
	desc.layout = vk.pipeline_layout;
	desc.renderPass = vk.render_pass;
	desc.subpass = 0;
	desc.basePipelineHandle = VK_NULL_HANDLE;
	desc.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &pipeline ) );
	return pipeline;
}

void vk_create2DPipelines( void ) {
	vk.pipeline_2d_opaque = create_pipeline( VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_FALSE, VK_CULL_MODE_NONE );
	vk.pipeline_2d_blend = create_pipeline( VK_TRUE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_FALSE, VK_CULL_MODE_NONE );
}

void vk_destroy2DPipelines( void ) {
	qvkDestroyPipeline( vk.device, vk.pipeline_2d_opaque, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_2d_blend, NULL );
}

// No cull mode yet -- tr_shader.c's parsed cullNone isn't wired to any pipeline choice yet.
void vk_create3DPipelines( void ) {
	vk.pipeline_3d_opaque = create_pipeline( VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_TRUE, VK_CULL_MODE_NONE );
}

void vk_destroy3DPipelines( void ) {
	qvkDestroyPipeline( vk.device, vk.pipeline_3d_opaque, NULL );
}
