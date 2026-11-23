// vk_pipelines.c -- render pass, framebuffers, descriptor/pipeline layouts, and the 2D/3D pipeline variants.

#include <string.h>
#include <stddef.h>

#include "VKimpl.h"
#include "vk_instance.h"
#include "vk_shaders.h"
#include "vk_pipelines.h"
#include "tr_image.h"
#include "tr_portal.h"

// Decal shaders set shader_t.polygonOffset to avoid z-fighting; defaults match code/renderer's.
static cvar_t *r_offsetFactor;
static cvar_t *r_offsetUnits;

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
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // stencil shadow volumes (vk_shadows.c) need it cleared every frame
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

#define MAX_MULTITEX_DESCRIPTORS 256
static VkDescriptorPool s_multiTexDescriptorPool;
static void vk_resetMultiTexDescriptorCache( void );

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
		// binding 0 = diffuse, binding 1 = lightmap; sets are allocated from vk_world.c's own pool, not vk.descriptor_pool.
		VkDescriptorSetLayoutBinding bindings[2];
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[0].pImmutableSamplers = NULL;
		bindings[1] = bindings[0];
		bindings[1].binding = 1;

		VkDescriptorSetLayoutCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.bindingCount = 2;
		desc.pBindings = bindings;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.lightmap_set_layout ) );
	}

	{
		// One UNIFORM_BUFFER_DYNAMIC binding for the MDS/MDM bone-matrix UBO -- see vk.skin_set_layout's comment; the buffer/pool/descriptor set are vk_entity.c's own.
		VkDescriptorSetLayoutBinding binding;
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		binding.pImmutableSamplers = NULL;

		VkDescriptorSetLayoutCreateInfo desc;
		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.bindingCount = 1;
		desc.pBindings = &binding;
		VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.skin_set_layout ) );
	}

	{
		// Matches single_texture.vert's push_constant size (128 bytes): mvp, then fog's camPos/fogColor/fogParams.
		VkPushConstantRange push_range;
		VkDescriptorSetLayout skinSets[2];
		push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
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

		desc.pSetLayouts = &vk.lightmap_set_layout;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.lightmap_pipeline_layout ) );

		skinSets[0] = vk.set_layout;
		skinSets[1] = vk.skin_set_layout;
		desc.setLayoutCount = 2;
		desc.pSetLayouts = skinSets;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.skin_pipeline_layout ) );

		skinSets[0] = vk.lightmap_set_layout;
		VK_CHECK( qvkCreatePipelineLayout( vk.device, &desc, NULL, &vk.skin_lightmap_pipeline_layout ) );
	}

	{
		// vk_getMultiTextureDescriptor's cache allocates from this -- same 2-binding layout as the world lightmap pass.
		VkDescriptorPoolSize poolSize;
		VkDescriptorPoolCreateInfo poolDesc;

		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = MAX_MULTITEX_DESCRIPTORS * 2;

		poolDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolDesc.pNext = NULL;
		poolDesc.flags = 0;
		poolDesc.maxSets = MAX_MULTITEX_DESCRIPTORS;
		poolDesc.poolSizeCount = 1;
		poolDesc.pPoolSizes = &poolSize;
		VK_CHECK( qvkCreateDescriptorPool( vk.device, &poolDesc, NULL, &s_multiTexDescriptorPool ) );
	}

	vk_resetMultiTexDescriptorCache(); // drop any sets cached against a previous (now-destroyed) pool, e.g. on vid_restart
}

void vk_destroyPipelineLayout( void ) {
	qvkDestroyDescriptorPool( vk.device, s_multiTexDescriptorPool, NULL );
	qvkDestroyPipelineLayout( vk.device, vk.skin_lightmap_pipeline_layout, NULL );
	qvkDestroyPipelineLayout( vk.device, vk.skin_pipeline_layout, NULL );
	qvkDestroyDescriptorSetLayout( vk.device, vk.skin_set_layout, NULL );
	qvkDestroyPipelineLayout( vk.device, vk.lightmap_pipeline_layout, NULL );
	qvkDestroyDescriptorSetLayout( vk.device, vk.lightmap_set_layout, NULL );
	qvkDestroyPipelineLayout( vk.device, vk.pipeline_layout, NULL );
	qvkDestroyDescriptorSetLayout( vk.device, vk.set_layout, NULL );
	qvkDestroyDescriptorPool( vk.device, vk.descriptor_pool, NULL );
}

typedef struct {
	int32_t alpha_test_func;
	int32_t color_op;
	int32_t clip_plane;
	int32_t fog_enabled;
} vk2d_spec_data_t;

static VkPipeline create_pipeline_ex( VkShaderModule vs, VkShaderModule fs, VkPipelineLayout layout,
									const VkSpecializationInfo *specInfo,
									const VkVertexInputBindingDescription *binding, uint32_t numBindings,
									const VkVertexInputAttributeDescription *attribs, uint32_t numAttribs,
									VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
									VkBool32 depthTestEnable, VkCullModeFlags cullMode, VkPolygonMode polygonMode,
									VkBool32 polygonOffset, VkBool32 depthWriteEnable, VkColorComponentFlags colorWriteMask,
									VkBool32 stencilTestEnable, VkCompareOp stencilCompareOp, VkStencilOp stencilPassOp ) {
	VkPipeline pipeline;

	VkPipelineShaderStageCreateInfo stages[2];
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].pNext = NULL;
	stages[0].flags = 0;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vs;
	stages[0].pName = "main";
	stages[0].pSpecializationInfo = specInfo;

	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].pNext = NULL;
	stages[1].flags = 0;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fs;
	stages[1].pName = "main";
	stages[1].pSpecializationInfo = specInfo;

	VkPipelineVertexInputStateCreateInfo vertex_input;
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.pNext = NULL;
	vertex_input.flags = 0;
	vertex_input.vertexBindingDescriptionCount = numBindings;
	vertex_input.pVertexBindingDescriptions = binding;
	vertex_input.vertexAttributeDescriptionCount = numAttribs;
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
	rasterization.polygonMode = polygonMode;
	rasterization.cullMode = cullMode;
	rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization.depthBiasEnable = polygonOffset;
	rasterization.depthBiasConstantFactor = polygonOffset ? r_offsetUnits->value : 0.0f;
	rasterization.depthBiasClamp = 0.0f;
	rasterization.depthBiasSlopeFactor = polygonOffset ? r_offsetFactor->value : 0.0f;
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
	depth_stencil.depthWriteEnable = depthWriteEnable;
	depth_stencil.depthCompareOp = depthTestEnable ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_ALWAYS; // matches GL1's GL_LEQUAL default; needed so a later stage redrawing the same depth still passes
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.stencilTestEnable = stencilTestEnable;
	depth_stencil.front.failOp = VK_STENCIL_OP_KEEP;
	depth_stencil.front.passOp = stencilPassOp;
	depth_stencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
	depth_stencil.front.compareOp = stencilCompareOp;
	depth_stencil.front.compareMask = 0xFF;
	depth_stencil.front.writeMask = 0xFF;
	depth_stencil.front.reference = 0;
	depth_stencil.back = depth_stencil.front;
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
	blend_attachment.colorWriteMask = colorWriteMask;

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
	desc.layout = layout;
	desc.renderPass = vk.render_pass;
	desc.subpass = 0;
	desc.basePipelineHandle = VK_NULL_HANDLE;
	desc.basePipelineIndex = -1;

	VK_CHECK( qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &desc, NULL, &pipeline ) );
	return pipeline;
}

static VkPipeline create_pipeline( VkShaderModule vs, VkShaderModule fs, VkPipelineLayout layout,
									const VkSpecializationInfo *specInfo,
									const VkVertexInputBindingDescription *binding,
									const VkVertexInputAttributeDescription *attribs, uint32_t numAttribs,
									VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
									VkBool32 depthTestEnable, VkCullModeFlags cullMode ) {
	VkColorComponentFlags fullMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
									   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	return create_pipeline_ex( vs, fs, layout, specInfo, binding, 1, attribs, numAttribs,
		blendEnable, srcBlend, dstBlend, depthTestEnable, cullMode, VK_POLYGON_MODE_FILL, VK_FALSE,
		depthTestEnable, fullMask, VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );
}

// vk2d_vertex_t attribs, shared by the 2D pipelines and the (unused-for-world-now) opaque 3D one.
static void Get2DAttribs( VkVertexInputBindingDescription *binding, VkVertexInputAttributeDescription attribs[3] ) {
	binding->binding = 0;
	binding->stride = sizeof( vk2d_vertex_t );
	binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

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
}

// fogEnabled selects single_texture.vert's compile-time fog branch -- on for the 3D pipeline, off for the 2D UI ones (which share the shader and must never be fogged).
// clipPlaneEnabled selects the fragment-discard fallback; hardware clip-distance pipelines bind vk_single_texture_clip_vs instead and pass qfalse here.
static void GetSpecInfo( vk2d_spec_data_t *spec_data, VkSpecializationMapEntry spec_entries[4], VkSpecializationInfo *specInfo, qboolean fogEnabled, int32_t alphaTestFunc, qboolean clipPlaneEnabled ) {
	spec_data->alpha_test_func = alphaTestFunc;
	spec_data->color_op = 0;
	spec_data->clip_plane = clipPlaneEnabled ? 1 : 0;
	spec_data->fog_enabled = fogEnabled ? 1 : 0;

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( vk2d_spec_data_t, alpha_test_func );
	spec_entries[0].size = sizeof( int32_t );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( vk2d_spec_data_t, color_op );
	spec_entries[1].size = sizeof( int32_t );
	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( vk2d_spec_data_t, clip_plane );
	spec_entries[2].size = sizeof( int32_t );
	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( vk2d_spec_data_t, fog_enabled );
	spec_entries[3].size = sizeof( int32_t );

	specInfo->mapEntryCount = 4;
	specInfo->pMapEntries = spec_entries;
	specInfo->dataSize = sizeof( *spec_data );
	specInfo->pData = spec_data;
}

void vk_create2DPipelines( void ) {
	VkVertexInputBindingDescription binding;
	VkVertexInputAttributeDescription attribs[3];
	vk2d_spec_data_t spec_data;
	VkSpecializationMapEntry spec_entries[4];
	VkSpecializationInfo specInfo;

	Get2DAttribs( &binding, attribs );
	GetSpecInfo( &spec_data, spec_entries, &specInfo, qfalse, 0, qfalse );

	vk.pipeline_2d_opaque = create_pipeline( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding, attribs, 3, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_FALSE, VK_CULL_MODE_NONE );
	vk.pipeline_2d_blend = create_pipeline( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding, attribs, 3, VK_TRUE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_FALSE, VK_CULL_MODE_NONE );
}

void vk_destroy2DPipelines( void ) {
	qvkDestroyPipeline( vk.device, vk.pipeline_2d_opaque, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_2d_blend, NULL );
}

// Matches code/renderer's GL_Cull given this renderer's Y-flip + CLOCKWISE frontFace (no mirror flip: no mirrors yet).
VkCullModeFlags vk_cullModeForShader( cullType_t cullType ) {
	if ( cullType == CT_TWO_SIDED ) {
		return VK_CULL_MODE_NONE;
	}
	return ( cullType == CT_BACK_SIDED ) ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
}

// NONE (two-sided) has no front/back to flip -- used for mirror pipeline variants, see tr_portal.h.
static VkCullModeFlags vk_flipCullMode( VkCullModeFlags cullMode ) {
	if ( cullMode == VK_CULL_MODE_FRONT_BIT ) {
		return VK_CULL_MODE_BACK_BIT;
	}
	if ( cullMode == VK_CULL_MODE_BACK_BIT ) {
		return VK_CULL_MODE_FRONT_BIT;
	}
	return cullMode;
}

// Cache of pipelines by (blend, cull, alphaTest, clipPlane, mirror, polygonOffset) state, linear-scanned: vkQuake3-scale content alone hits 60-100+ live pipelines on a smaller key (no clipPlane/mirror/polygonOffset axis), so 64 was already tight -- raised for headroom; hash-based lookup deliberately skipped with no profiling evidence the scan is ever hot.
#define MAX_CACHED_3D_PIPELINES 128

typedef struct {
	VkDeviceSize vertexStride;
	VkBool32 blendEnable;
	VkBlendFactor srcBlend;
	VkBlendFactor dstBlend;
	VkCullModeFlags cullMode;
	int32_t alphaTestFunc;
	VkBool32 clipPlane;
	VkBool32 mirror;
	VkBool32 polygonOffset;
} cachedPipelineKey_t;

static cachedPipelineKey_t s_cachedPipelineKeys[MAX_CACHED_3D_PIPELINES];
static VkPipeline s_cachedPipelines[MAX_CACHED_3D_PIPELINES];
static int s_numCachedPipelines;

static void vk_resetPipelineCache3D( void ) {
	s_numCachedPipelines = 0;
}

static void vk_destroyPipelineCache3D( void ) {
	int i;
	for ( i = 0; i < s_numCachedPipelines; i++ ) {
		qvkDestroyPipeline( vk.device, s_cachedPipelines[i], NULL );
	}
	s_numCachedPipelines = 0;
}

VkPipeline vk_getPipeline3D( VkDeviceSize vertexStride, VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
							  VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 clipPlane, VkBool32 mirror,
							  VkBool32 polygonOffset ) {
	VkVertexInputBindingDescription binding2d;
	VkVertexInputAttributeDescription attribs2d[3];
	vk2d_spec_data_t spec_data;
	VkSpecializationMapEntry spec_entries[4];
	VkSpecializationInfo specInfo;
	VkShaderModule vs;
	int i;

	for ( i = 0; i < s_numCachedPipelines; i++ ) {
		const cachedPipelineKey_t *key = &s_cachedPipelineKeys[i];
		if ( key->vertexStride == vertexStride && key->blendEnable == blendEnable && key->srcBlend == srcBlend &&
			 key->dstBlend == dstBlend && key->cullMode == cullMode && key->alphaTestFunc == alphaTestFunc &&
			 key->clipPlane == clipPlane && key->mirror == mirror && key->polygonOffset == polygonOffset ) {
			return s_cachedPipelines[i];
		}
	}

	if ( s_numCachedPipelines >= MAX_CACHED_3D_PIPELINES ) {
		ri.Printf( PRINT_WARNING, "MAX_CACHED_3D_PIPELINES hit, falling back to the default pipeline\n" );
		return blendEnable ? vk.pipeline_3d_blend : vk.pipeline_3d_opaque;
	}

	Get2DAttribs( &binding2d, attribs2d );
	binding2d.stride = vertexStride; // attribute offsets are stride-independent, so this alone retargets the binding

	// Hardware clip-distance needs its own vertex shader module; see single_texture_clip.vert.
	vs = ( clipPlane && vk.features.shaderClipDistance ) ? vk_single_texture_clip_vs : vk_single_texture_vs;
	GetSpecInfo( &spec_data, spec_entries, &specInfo, qtrue, alphaTestFunc, clipPlane && !vk.features.shaderClipDistance );

	s_cachedPipelineKeys[s_numCachedPipelines].vertexStride = vertexStride;
	s_cachedPipelineKeys[s_numCachedPipelines].blendEnable = blendEnable;
	s_cachedPipelineKeys[s_numCachedPipelines].srcBlend = srcBlend;
	s_cachedPipelineKeys[s_numCachedPipelines].dstBlend = dstBlend;
	s_cachedPipelineKeys[s_numCachedPipelines].cullMode = cullMode;
	s_cachedPipelineKeys[s_numCachedPipelines].alphaTestFunc = alphaTestFunc;
	s_cachedPipelineKeys[s_numCachedPipelines].clipPlane = clipPlane;
	s_cachedPipelineKeys[s_numCachedPipelines].mirror = mirror;
	s_cachedPipelineKeys[s_numCachedPipelines].polygonOffset = polygonOffset;
	s_cachedPipelines[s_numCachedPipelines] = create_pipeline_ex( vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, 1, attribs2d, 3, blendEnable, srcBlend, dstBlend, VK_TRUE, mirror ? vk_flipCullMode( cullMode ) : cullMode,
		VK_POLYGON_MODE_FILL, polygonOffset, VK_TRUE,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );

	return s_cachedPipelines[s_numCachedPipelines++];
}

// Key space is tiny in practice (almost always one blend mode per sky shader) -- a linear-scanned cache like vk_getPipeline3D's, just much smaller.
#define MAX_CACHED_SKY_CLOUD_PIPELINES 8
static VkBlendFactor s_skyCloudSrc[MAX_CACHED_SKY_CLOUD_PIPELINES];
static VkBlendFactor s_skyCloudDst[MAX_CACHED_SKY_CLOUD_PIPELINES];
static VkPipeline s_skyCloudPipelines[MAX_CACHED_SKY_CLOUD_PIPELINES];
static int s_numSkyCloudPipelines;

static void vk_resetPipelineCacheSkyCloud( void ) {
	s_numSkyCloudPipelines = 0;
}

static void vk_destroyPipelineCacheSkyCloud( void ) {
	int i;
	for ( i = 0; i < s_numSkyCloudPipelines; i++ ) {
		qvkDestroyPipeline( vk.device, s_skyCloudPipelines[i], NULL );
	}
	s_numSkyCloudPipelines = 0;
}

VkPipeline vk_getPipelineSkyCloud( VkBlendFactor srcBlend, VkBlendFactor dstBlend ) {
	VkVertexInputBindingDescription binding2d;
	VkVertexInputAttributeDescription attribs2d[3];
	vk2d_spec_data_t spec_data;
	VkSpecializationMapEntry spec_entries[4];
	VkSpecializationInfo specInfo;
	int i;

	for ( i = 0; i < s_numSkyCloudPipelines; i++ ) {
		if ( s_skyCloudSrc[i] == srcBlend && s_skyCloudDst[i] == dstBlend ) {
			return s_skyCloudPipelines[i];
		}
	}

	if ( s_numSkyCloudPipelines >= MAX_CACHED_SKY_CLOUD_PIPELINES ) {
		ri.Printf( PRINT_WARNING, "MAX_CACHED_SKY_CLOUD_PIPELINES hit, falling back to vk.pipeline_sky\n" );
		return vk.pipeline_sky;
	}

	Get2DAttribs( &binding2d, attribs2d );
	GetSpecInfo( &spec_data, spec_entries, &specInfo, qfalse, 0, qfalse ); // fog off, like vk.pipeline_sky

	s_skyCloudSrc[s_numSkyCloudPipelines] = srcBlend;
	s_skyCloudDst[s_numSkyCloudPipelines] = dstBlend;
	s_skyCloudPipelines[s_numSkyCloudPipelines] = create_pipeline_ex( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, 1, attribs2d, 3, VK_TRUE, srcBlend, dstBlend, VK_FALSE, VK_CULL_MODE_NONE,
		VK_POLYGON_MODE_FILL, VK_FALSE, VK_FALSE,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );

	return s_skyCloudPipelines[s_numSkyCloudPipelines++];
}

static void vk_destroyMultiTexPipelineCache( void );
static void vk_resetEntityLerpPipelineCache( void );
static void vk_destroyEntityLerpPipelineCache( void );
static void vk_resetEntityLerpMTPipelineCache( void );
static void vk_destroyEntityLerpMTPipelineCache( void );
static void vk_resetEntitySkinPipelineCache( void );
static void vk_destroyEntitySkinPipelineCache( void );
static void vk_resetEntitySkinMTPipelineCache( void );
static void vk_destroyEntitySkinMTPipelineCache( void );

// world_lightmap.vert/.frag's spec constants: clip_plane is constant_id 0 (not single_texture's 2), alpha_test_func is constant_id 1.
typedef struct {
	int32_t clip_plane;
	int32_t alpha_test_func;
} worldLightmapSpecData_t;

static void GetWorldLightmapSpecInfo( worldLightmapSpecData_t *spec_data, VkSpecializationMapEntry spec_entries[2],
									   VkSpecializationInfo *specInfo, qboolean clipPlaneEnabled, int32_t alphaTestFunc ) {
	spec_data->clip_plane = clipPlaneEnabled ? 1 : 0;
	spec_data->alpha_test_func = alphaTestFunc;
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( worldLightmapSpecData_t, clip_plane );
	spec_entries[0].size = sizeof( int32_t );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( worldLightmapSpecData_t, alpha_test_func );
	spec_entries[1].size = sizeof( int32_t );
	specInfo->mapEntryCount = 2;
	specInfo->pMapEntries = spec_entries;
	specInfo->dataSize = sizeof( *spec_data );
	specInfo->pData = spec_data;
}

// pipeline_3d_opaque/blend are vk_draw3d.c's debug pipeline + the cache's fallback; real entity/poly draws use vk_getPipeline3D.
void vk_create3DPipelines( void ) {
	VkVertexInputBindingDescription binding2d;
	VkVertexInputAttributeDescription attribs2d[3];
	VkVertexInputBindingDescription worldBinding;
	VkVertexInputAttributeDescription lightmapAttribs[7];
	vk2d_spec_data_t spec_data, sky_spec_data;
	VkSpecializationMapEntry spec_entries[4], sky_spec_entries[4];
	VkSpecializationInfo specInfo, skySpecInfo;
	VkBool32 hwClip = vk.features.shaderClipDistance;
	VkShaderModule worldPortalVs = hwClip ? vk_world_lightmap_clip_vs : vk_world_lightmap_vs;
	int ct, at;

	r_offsetFactor = ri.Cvar_Get( "r_offsetfactor", "-1", CVAR_CHEAT );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-2", CVAR_CHEAT );

	Get2DAttribs( &binding2d, attribs2d );

	worldBinding.binding = 0;
	worldBinding.stride = sizeof( vkWorldVertex_t );
	worldBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	lightmapAttribs[0].location = 0;
	lightmapAttribs[0].binding = 0;
	lightmapAttribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	lightmapAttribs[0].offset = offsetof( vkWorldVertex_t, pos );
	lightmapAttribs[1].location = 1;
	lightmapAttribs[1].binding = 0;
	lightmapAttribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	lightmapAttribs[1].offset = offsetof( vkWorldVertex_t, color );
	lightmapAttribs[2].location = 2;
	lightmapAttribs[2].binding = 0;
	lightmapAttribs[2].format = VK_FORMAT_R32G32_SFLOAT;
	lightmapAttribs[2].offset = offsetof( vkWorldVertex_t, uv );
	lightmapAttribs[3].location = 3;
	lightmapAttribs[3].binding = 0;
	lightmapAttribs[3].format = VK_FORMAT_R32G32_SFLOAT;
	lightmapAttribs[3].offset = offsetof( vkWorldVertex_t, lmUv );
	lightmapAttribs[4].location = 4;
	lightmapAttribs[4].binding = 0;
	lightmapAttribs[4].format = VK_FORMAT_R16G16B16A16_SNORM;
	lightmapAttribs[4].offset = offsetof( vkWorldVertex_t, normal );
	lightmapAttribs[5].location = 5;
	lightmapAttribs[5].binding = 0;
	lightmapAttribs[5].format = VK_FORMAT_R16G16B16A16_SNORM;
	lightmapAttribs[5].offset = offsetof( vkWorldVertex_t, tangent );
	lightmapAttribs[6].location = 6;
	lightmapAttribs[6].binding = 0;
	lightmapAttribs[6].format = VK_FORMAT_R16G16B16A16_SNORM;
	lightmapAttribs[6].offset = offsetof( vkWorldVertex_t, lightdir );

	GetSpecInfo( &spec_data, spec_entries, &specInfo, qtrue, 0, qfalse );
	GetSpecInfo( &sky_spec_data, sky_spec_entries, &skySpecInfo, qfalse, 0, qfalse );

	vk.pipeline_3d_opaque = create_pipeline( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, attribs2d, 3, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_TRUE, VK_CULL_MODE_NONE );
	vk.pipeline_3d_blend = create_pipeline( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, attribs2d, 3, VK_TRUE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_TRUE, VK_CULL_MODE_NONE );
	for ( ct = CT_FRONT_SIDED; ct <= CT_TWO_SIDED; ct++ ) {
		VkCullModeFlags cull = vk_cullModeForShader( (cullType_t)ct );

		for ( at = 0; at < 4; at++ ) {
			worldLightmapSpecData_t lm_spec_data, lm_portal_spec_data;
			VkSpecializationMapEntry lm_spec_entries[2], lm_portal_spec_entries[2];
			VkSpecializationInfo lmSpecInfo, lmPortalSpecInfo;

			GetWorldLightmapSpecInfo( &lm_spec_data, lm_spec_entries, &lmSpecInfo, qfalse, at );
			GetWorldLightmapSpecInfo( &lm_portal_spec_data, lm_portal_spec_entries, &lmPortalSpecInfo, !hwClip, at ); // clip off when hwClip's vk_world_lightmap_clip_vs handles it instead

			vk.pipeline_3d_lightmap[ct][at] = create_pipeline( vk_world_lightmap_vs, vk_world_lightmap_fs, vk.lightmap_pipeline_layout, &lmSpecInfo,
				&worldBinding, lightmapAttribs, 7, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_TRUE, cull );
			// Portal/mirror variants: clip plane active, mirror also flips cull mode (tr_portal.h).
			vk.pipeline_3d_lightmap_portal[ct][at] = create_pipeline( worldPortalVs, vk_world_lightmap_fs, vk.lightmap_pipeline_layout, &lmPortalSpecInfo,
				&worldBinding, lightmapAttribs, 7, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_TRUE, cull );
			vk.pipeline_3d_lightmap_mirror[ct][at] = create_pipeline( worldPortalVs, vk_world_lightmap_fs, vk.lightmap_pipeline_layout, &lmPortalSpecInfo,
				&worldBinding, lightmapAttribs, 7, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_TRUE, vk_flipCullMode( cull ) );
		}
	}
	// Depth test/write off so draw order alone keeps the sky behind everything -- see vk_sky.c.
	vk.pipeline_sky = create_pipeline( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &skySpecInfo,
		&binding2d, attribs2d, 3, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_FALSE, VK_CULL_MODE_NONE );

	// r_showtris: VK_POLYGON_MODE_LINE needs fillModeNonSolid, which not every device supports.
	vk.pipeline_world_wireframe = vk.features.fillModeNonSolid ?
		create_pipeline_ex( vk_world_lightmap_vs, vk_world_lightmap_fs, vk.lightmap_pipeline_layout, NULL,
			&worldBinding, 1, lightmapAttribs, 7, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
			VK_TRUE, VK_CULL_MODE_NONE, VK_POLYGON_MODE_LINE, VK_FALSE, VK_TRUE,
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP ) : VK_NULL_HANDLE;

	// Coronas' occlusion-visibility probe (vk_flares.c): depth-tested like real geometry, writes neither depth nor color.
	vk.pipeline_occlusion_probe = create_pipeline_ex( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, 1, attribs2d, 3, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
		VK_TRUE, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_FALSE, VK_FALSE, 0,
		VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );

	// Stencil shadow volumes (vk_shadows.c): two cull-flipped stamp passes plus one full-screen darken pass.
	vk.pipeline_shadow_volume_incr = create_pipeline_ex( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, 1, attribs2d, 3, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
		VK_TRUE, VK_CULL_MODE_FRONT_BIT, VK_POLYGON_MODE_FILL, VK_FALSE, VK_FALSE, 0,
		VK_TRUE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_INCREMENT_AND_CLAMP );
	vk.pipeline_shadow_volume_decr = create_pipeline_ex( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, 1, attribs2d, 3, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
		VK_TRUE, VK_CULL_MODE_BACK_BIT, VK_POLYGON_MODE_FILL, VK_FALSE, VK_FALSE, 0,
		VK_TRUE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_DECREMENT_AND_CLAMP );
	vk.pipeline_shadow_darken = create_pipeline_ex( vk_single_texture_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		&binding2d, 1, attribs2d, 3, VK_TRUE, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ZERO,
		VK_FALSE, VK_CULL_MODE_NONE, VK_POLYGON_MODE_FILL, VK_FALSE, VK_FALSE,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_TRUE, VK_COMPARE_OP_NOT_EQUAL, VK_STENCIL_OP_KEEP );

	vk_resetPipelineCache3D();
	vk_resetPipelineCacheSkyCloud();
	vk_resetEntityLerpPipelineCache();
	vk_resetEntityLerpMTPipelineCache();
	vk_resetEntitySkinPipelineCache();
	vk_resetEntitySkinMTPipelineCache();
}

void vk_destroy3DPipelines( void ) {
	int ct, at;

	vk_destroyPipelineCache3D();
	vk_destroyPipelineCacheSkyCloud();
	vk_destroyEntityLerpPipelineCache();
	vk_destroyEntityLerpMTPipelineCache();
	vk_destroyEntitySkinPipelineCache();
	vk_destroyEntitySkinMTPipelineCache();

	qvkDestroyPipeline( vk.device, vk.pipeline_3d_opaque, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_3d_blend, NULL );
	for ( ct = CT_FRONT_SIDED; ct <= CT_TWO_SIDED; ct++ ) {
		for ( at = 0; at < 4; at++ ) {
			qvkDestroyPipeline( vk.device, vk.pipeline_3d_lightmap[ct][at], NULL );
			qvkDestroyPipeline( vk.device, vk.pipeline_3d_lightmap_portal[ct][at], NULL );
			qvkDestroyPipeline( vk.device, vk.pipeline_3d_lightmap_mirror[ct][at], NULL );
		}
	}
	qvkDestroyPipeline( vk.device, vk.pipeline_sky, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_world_wireframe, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_occlusion_probe, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_shadow_volume_incr, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_shadow_volume_decr, NULL );
	qvkDestroyPipeline( vk.device, vk.pipeline_shadow_darken, NULL );

	vk_destroyMultiTexPipelineCache();
}

// Stage-0 world+lightmap pipeline for (cullType, alphaTestFunc), auto-switched to the clip-plane/mirror variant while tr_portal.c is recursing.
VkPipeline vk_getWorldLightmapPipeline( cullType_t cullType, int32_t alphaTestFunc ) {
	if ( R_InMirrorView() ) {
		return vk.pipeline_3d_lightmap_mirror[cullType][alphaTestFunc];
	}
	if ( R_InPortalView() ) {
		return vk.pipeline_3d_lightmap_portal[cullType][alphaTestFunc];
	}
	return vk.pipeline_3d_lightmap[cullType][alphaTestFunc];
}

// vkMultiTexVertex_t attribs -- byte-identical layout to vkWorldVertex_t, described independently since the types aren't otherwise related.
static void GetMultiTexAttribs( VkVertexInputBindingDescription *binding, VkVertexInputAttributeDescription attribs[4] ) {
	binding->binding = 0;
	binding->stride = sizeof( vkMultiTexVertex_t );
	binding->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	attribs[0].location = 0;
	attribs[0].binding = 0;
	attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribs[0].offset = offsetof( vkMultiTexVertex_t, pos );
	attribs[1].location = 1;
	attribs[1].binding = 0;
	attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	attribs[1].offset = offsetof( vkMultiTexVertex_t, color );
	attribs[2].location = 2;
	attribs[2].binding = 0;
	attribs[2].format = VK_FORMAT_R32G32_SFLOAT;
	attribs[2].offset = offsetof( vkMultiTexVertex_t, uv0 );
	attribs[3].location = 3;
	attribs[3].binding = 0;
	attribs[3].format = VK_FORMAT_R32G32_SFLOAT;
	attribs[3].offset = offsetof( vkMultiTexVertex_t, uv1 );
}

// Bisection aid for the shattered-geometry report; ruled out (no effect) but left in for now.
static cvar_t *r_vk_disableMultiTex;

qboolean vk_canCollapseMultiTex( const shader_t *shader, int *outCombineMode ) {
	const shaderStage_t *s0, *s1;

	if ( !r_vk_disableMultiTex ) {
		r_vk_disableMultiTex = ri.Cvar_Get( "r_vk_disableMultiTex", "0", 0 ); // not CVAR_CHEAT, needs no sv_cheats
	}
	if ( r_vk_disableMultiTex->integer ) {
		return qfalse;
	}

	if ( shader->numStages != 2 ) {
		return qfalse;
	}
	s0 = &shader->stages[0];
	s1 = &shader->stages[1];
	if ( s0->blendEnabled || !s1->blendEnabled || s1->rgbGen != CGEN_IDENTITY ) {
		return qfalse;
	}
	if ( s1->srcBlend == VK_BLEND_FACTOR_DST_COLOR && s1->dstBlend == VK_BLEND_FACTOR_ZERO ) {
		*outCombineMode = 0;
		return qtrue;
	}
	if ( s1->srcBlend == VK_BLEND_FACTOR_ONE && s1->dstBlend == VK_BLEND_FACTOR_ONE ) {
		*outCombineMode = 1;
		return qtrue;
	}
	return qfalse;
}

typedef struct {
	int32_t combine_mode;
	int32_t alpha_test_func;
} multiTexSpecData_t;

// Cache of multi_texture pipelines by (combine, cull, alphaTest); linear-scanned like vk_getPipeline3D's. Key space here is bounded (combine x cull x alphaTest = 2x3x4 = 24), comfortably under this cap -- no change needed when vk_getPipeline3D's cap was raised.
#define MAX_CACHED_MULTITEX_PIPELINES 32

typedef struct {
	int32_t combineMode;
	VkCullModeFlags cullMode;
	int32_t alphaTestFunc;
} cachedMultiTexPipelineKey_t;

static cachedMultiTexPipelineKey_t s_cachedMultiTexKeys[MAX_CACHED_MULTITEX_PIPELINES];
static VkPipeline s_cachedMultiTexPipelines[MAX_CACHED_MULTITEX_PIPELINES];
static int s_numCachedMultiTexPipelines;

static void vk_destroyMultiTexPipelineCache( void ) {
	int i;
	for ( i = 0; i < s_numCachedMultiTexPipelines; i++ ) {
		qvkDestroyPipeline( vk.device, s_cachedMultiTexPipelines[i], NULL );
	}
	s_numCachedMultiTexPipelines = 0;
}

VkPipeline vk_getPipelineMultiTexture( int combineMode, VkCullModeFlags cullMode, int32_t alphaTestFunc ) {
	VkVertexInputBindingDescription binding;
	VkVertexInputAttributeDescription attribs[4];
	multiTexSpecData_t spec_data;
	VkSpecializationMapEntry spec_entries[2];
	VkSpecializationInfo specInfo;
	int i;

	for ( i = 0; i < s_numCachedMultiTexPipelines; i++ ) {
		const cachedMultiTexPipelineKey_t *key = &s_cachedMultiTexKeys[i];
		if ( key->combineMode == combineMode && key->cullMode == cullMode && key->alphaTestFunc == alphaTestFunc ) {
			return s_cachedMultiTexPipelines[i];
		}
	}

	if ( s_numCachedMultiTexPipelines >= MAX_CACHED_MULTITEX_PIPELINES ) {
		ri.Printf( PRINT_WARNING, "MAX_CACHED_MULTITEX_PIPELINES hit, falling back to the opaque 3D pipeline\n" );
		return vk.pipeline_3d_opaque;
	}

	GetMultiTexAttribs( &binding, attribs );

	spec_data.combine_mode = combineMode;
	spec_data.alpha_test_func = alphaTestFunc;
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( multiTexSpecData_t, combine_mode );
	spec_entries[0].size = sizeof( int32_t );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( multiTexSpecData_t, alpha_test_func );
	spec_entries[1].size = sizeof( int32_t );
	specInfo.mapEntryCount = 2;
	specInfo.pMapEntries = spec_entries;
	specInfo.dataSize = sizeof( spec_data );
	specInfo.pData = &spec_data;

	s_cachedMultiTexKeys[s_numCachedMultiTexPipelines].combineMode = combineMode;
	s_cachedMultiTexKeys[s_numCachedMultiTexPipelines].cullMode = cullMode;
	s_cachedMultiTexKeys[s_numCachedMultiTexPipelines].alphaTestFunc = alphaTestFunc;
	// Always opaque (no blend) -- see vk_getPipelineMultiTexture's header comment for why.
	s_cachedMultiTexPipelines[s_numCachedMultiTexPipelines] = create_pipeline( vk_multi_texture_vs, vk_multi_texture_fs,
		vk.lightmap_pipeline_layout, &specInfo, &binding, attribs, 4, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
		VK_TRUE, cullMode );

	return s_cachedMultiTexPipelines[s_numCachedMultiTexPipelines++];
}

// Entity GPU vertex-lerp pipelines (MD3/MDC, vk_entity.c): 3 bindings -- the model's static per-surface buffer bound twice (current frame, old frame, same VkBuffer at two byte offsets) plus this frame's dynamic color/uv. Cached like vk_getPipeline3D's, minus the clipPlane/mirror axis (entities never use either) since vertex input layout, not just shader state, depends on this key.
typedef struct {
	VkBool32 blendEnable;
	VkBlendFactor srcBlend;
	VkBlendFactor dstBlend;
	VkCullModeFlags cullMode;
	int32_t alphaTestFunc;
	VkBool32 polygonOffset;
	VkBool32 lightingDiffuse;
} cachedEntityLerpKey_t;

#define MAX_CACHED_ENTITY_LERP_PIPELINES 32
static cachedEntityLerpKey_t s_cachedEntityLerpKeys[MAX_CACHED_ENTITY_LERP_PIPELINES];
static VkPipeline s_cachedEntityLerpPipelines[MAX_CACHED_ENTITY_LERP_PIPELINES];
static int s_numCachedEntityLerpPipelines;

static void vk_resetEntityLerpPipelineCache( void ) {
	s_numCachedEntityLerpPipelines = 0;
}

static void vk_destroyEntityLerpPipelineCache( void ) {
	int i;
	for ( i = 0; i < s_numCachedEntityLerpPipelines; i++ ) {
		qvkDestroyPipeline( vk.device, s_cachedEntityLerpPipelines[i], NULL );
	}
	s_numCachedEntityLerpPipelines = 0;
}

// Entity-lerp/entity-skin single-tex pipelines' own spec constants -- the same 4 single_texture.frag already has (alpha_test_func/color_op/clip_plane/fog_enabled) plus lighting_diffuse (constant_id 4), read only by entity_lerp.vert/entity_skin.vert; kept separate from vk2d_spec_data_t/GetSpecInfo so this field doesn't ripple into every other consumer of that shared struct (2D, sky, world debug 3D, etc.).
typedef struct {
	int32_t alpha_test_func;
	int32_t color_op;
	int32_t clip_plane;
	int32_t fog_enabled;
	int32_t lighting_diffuse;
} entityLitSpecData_t;

static void GetEntityLitSpecInfo( entityLitSpecData_t *spec_data, VkSpecializationMapEntry spec_entries[5],
								   VkSpecializationInfo *specInfo, int32_t alphaTestFunc, VkBool32 lightingDiffuse ) {
	spec_data->alpha_test_func = alphaTestFunc;
	spec_data->color_op = 0;
	spec_data->clip_plane = 0; // entities never clip
	spec_data->fog_enabled = 1; // entities always want fog support
	spec_data->lighting_diffuse = lightingDiffuse ? 1 : 0;

	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( entityLitSpecData_t, alpha_test_func );
	spec_entries[0].size = sizeof( int32_t );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( entityLitSpecData_t, color_op );
	spec_entries[1].size = sizeof( int32_t );
	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( entityLitSpecData_t, clip_plane );
	spec_entries[2].size = sizeof( int32_t );
	spec_entries[3].constantID = 3;
	spec_entries[3].offset = offsetof( entityLitSpecData_t, fog_enabled );
	spec_entries[3].size = sizeof( int32_t );
	spec_entries[4].constantID = 4;
	spec_entries[4].offset = offsetof( entityLitSpecData_t, lighting_diffuse );
	spec_entries[4].size = sizeof( int32_t );

	specInfo->mapEntryCount = 5;
	specInfo->pMapEntries = spec_entries;
	specInfo->dataSize = sizeof( *spec_data );
	specInfo->pData = spec_data;
}

static void GetEntityLerpAttribs( VkVertexInputBindingDescription bindings[3], VkVertexInputAttributeDescription attribs[5] ) {
	bindings[0].binding = 0;
	bindings[0].stride = sizeof( vkEntityFrameVert_t );
	bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindings[1] = bindings[0];
	bindings[1].binding = 1;
	bindings[2].binding = 2;
	bindings[2].stride = sizeof( vkEntityDynamicVert_t );
	bindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	attribs[0].location = 0; attribs[0].binding = 0; attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[0].offset = offsetof( vkEntityFrameVert_t, pos );
	attribs[1].location = 1; attribs[1].binding = 1; attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[1].offset = offsetof( vkEntityFrameVert_t, pos );
	attribs[2].location = 2; attribs[2].binding = 2; attribs[2].format = VK_FORMAT_R8G8B8A8_UNORM; attribs[2].offset = offsetof( vkEntityDynamicVert_t, color );
	attribs[3].location = 3; attribs[3].binding = 2; attribs[3].format = VK_FORMAT_R32G32_SFLOAT; attribs[3].offset = offsetof( vkEntityDynamicVert_t, uv );
	// Current frame's normal only (binding 0) -- not blended with oldframe's, a deliberate simplification for real-but-not-frame-blended lighting; see entity_lerp.vert.
	attribs[4].location = 4; attribs[4].binding = 0; attribs[4].format = VK_FORMAT_R16G16B16A16_SNORM; attribs[4].offset = offsetof( vkEntityFrameVert_t, normal );
}

VkPipeline vk_getPipelineEntityLerp( VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
									   VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 polygonOffset,
									   VkBool32 lightingDiffuse ) {
	VkVertexInputBindingDescription bindings[3];
	VkVertexInputAttributeDescription attribs[5];
	entityLitSpecData_t spec_data;
	VkSpecializationMapEntry spec_entries[5];
	VkSpecializationInfo specInfo;
	int i;

	for ( i = 0; i < s_numCachedEntityLerpPipelines; i++ ) {
		const cachedEntityLerpKey_t *key = &s_cachedEntityLerpKeys[i];
		if ( key->blendEnable == blendEnable && key->srcBlend == srcBlend && key->dstBlend == dstBlend &&
			 key->cullMode == cullMode && key->alphaTestFunc == alphaTestFunc && key->polygonOffset == polygonOffset &&
			 key->lightingDiffuse == lightingDiffuse ) {
			return s_cachedEntityLerpPipelines[i];
		}
	}

	if ( s_numCachedEntityLerpPipelines >= MAX_CACHED_ENTITY_LERP_PIPELINES ) {
		ri.Printf( PRINT_WARNING, "MAX_CACHED_ENTITY_LERP_PIPELINES hit, falling back to the default pipeline\n" );
		return blendEnable ? vk.pipeline_3d_blend : vk.pipeline_3d_opaque;
	}

	GetEntityLerpAttribs( bindings, attribs );
	GetEntityLitSpecInfo( &spec_data, spec_entries, &specInfo, alphaTestFunc, lightingDiffuse );

	s_cachedEntityLerpKeys[s_numCachedEntityLerpPipelines].blendEnable = blendEnable;
	s_cachedEntityLerpKeys[s_numCachedEntityLerpPipelines].srcBlend = srcBlend;
	s_cachedEntityLerpKeys[s_numCachedEntityLerpPipelines].dstBlend = dstBlend;
	s_cachedEntityLerpKeys[s_numCachedEntityLerpPipelines].cullMode = cullMode;
	s_cachedEntityLerpKeys[s_numCachedEntityLerpPipelines].lightingDiffuse = lightingDiffuse;
	s_cachedEntityLerpKeys[s_numCachedEntityLerpPipelines].alphaTestFunc = alphaTestFunc;
	s_cachedEntityLerpKeys[s_numCachedEntityLerpPipelines].polygonOffset = polygonOffset;
	s_cachedEntityLerpPipelines[s_numCachedEntityLerpPipelines] = create_pipeline_ex( vk_entity_lerp_vs, vk_single_texture_fs, vk.pipeline_layout, &specInfo,
		bindings, 3, attribs, 5, blendEnable, srcBlend, dstBlend, VK_TRUE, cullMode,
		VK_POLYGON_MODE_FILL, polygonOffset, VK_TRUE,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );

	return s_cachedEntityLerpPipelines[s_numCachedEntityLerpPipelines++];
}

// Multitex twin of the above, mirroring vk_getPipelineMultiTexture's (combine, cull, alphaTest) key -- always opaque, same reasoning as that function's header comment.
typedef struct {
	int32_t combineMode;
	VkCullModeFlags cullMode;
	int32_t alphaTestFunc;
	VkBool32 lightingDiffuse;
} cachedEntityLerpMTKey_t;

// Entity multitex pipelines' own (combine, alphaTest, lighting_diffuse) spec constants -- kept separate from the shared multiTexSpecData_t (world/poly multitex) for the same reason entityLitSpecData_t is separate from vk2d_spec_data_t.
typedef struct {
	int32_t combine_mode;
	int32_t alpha_test_func;
	int32_t lighting_diffuse;
} entityLitMultiTexSpecData_t;

#define MAX_CACHED_ENTITY_LERP_MT_PIPELINES 16
static cachedEntityLerpMTKey_t s_cachedEntityLerpMTKeys[MAX_CACHED_ENTITY_LERP_MT_PIPELINES];
static VkPipeline s_cachedEntityLerpMTPipelines[MAX_CACHED_ENTITY_LERP_MT_PIPELINES];
static int s_numCachedEntityLerpMTPipelines;

static void vk_resetEntityLerpMTPipelineCache( void ) {
	s_numCachedEntityLerpMTPipelines = 0;
}

static void vk_destroyEntityLerpMTPipelineCache( void ) {
	int i;
	for ( i = 0; i < s_numCachedEntityLerpMTPipelines; i++ ) {
		qvkDestroyPipeline( vk.device, s_cachedEntityLerpMTPipelines[i], NULL );
	}
	s_numCachedEntityLerpMTPipelines = 0;
}

static void GetEntityLerpMultiTexAttribs( VkVertexInputBindingDescription bindings[3], VkVertexInputAttributeDescription attribs[6] ) {
	bindings[0].binding = 0;
	bindings[0].stride = sizeof( vkEntityFrameVert_t );
	bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindings[1] = bindings[0];
	bindings[1].binding = 1;
	bindings[2].binding = 2;
	bindings[2].stride = sizeof( vkEntityDynamicMTVert_t );
	bindings[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	attribs[0].location = 0; attribs[0].binding = 0; attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[0].offset = offsetof( vkEntityFrameVert_t, pos );
	attribs[1].location = 1; attribs[1].binding = 1; attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[1].offset = offsetof( vkEntityFrameVert_t, pos );
	attribs[2].location = 2; attribs[2].binding = 2; attribs[2].format = VK_FORMAT_R8G8B8A8_UNORM; attribs[2].offset = offsetof( vkEntityDynamicMTVert_t, color );
	attribs[3].location = 3; attribs[3].binding = 2; attribs[3].format = VK_FORMAT_R32G32_SFLOAT; attribs[3].offset = offsetof( vkEntityDynamicMTVert_t, uv0 );
	attribs[4].location = 4; attribs[4].binding = 2; attribs[4].format = VK_FORMAT_R32G32_SFLOAT; attribs[4].offset = offsetof( vkEntityDynamicMTVert_t, uv1 );
	attribs[5].location = 5; attribs[5].binding = 0; attribs[5].format = VK_FORMAT_R16G16B16A16_SNORM; attribs[5].offset = offsetof( vkEntityFrameVert_t, normal );
}

VkPipeline vk_getPipelineEntityLerpMultiTex( int combineMode, VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 lightingDiffuse ) {
	VkVertexInputBindingDescription bindings[3];
	VkVertexInputAttributeDescription attribs[6];
	entityLitMultiTexSpecData_t spec_data;
	VkSpecializationMapEntry spec_entries[3];
	VkSpecializationInfo specInfo;
	int i;

	for ( i = 0; i < s_numCachedEntityLerpMTPipelines; i++ ) {
		const cachedEntityLerpMTKey_t *key = &s_cachedEntityLerpMTKeys[i];
		if ( key->combineMode == combineMode && key->cullMode == cullMode && key->alphaTestFunc == alphaTestFunc &&
			 key->lightingDiffuse == lightingDiffuse ) {
			return s_cachedEntityLerpMTPipelines[i];
		}
	}

	if ( s_numCachedEntityLerpMTPipelines >= MAX_CACHED_ENTITY_LERP_MT_PIPELINES ) {
		ri.Printf( PRINT_WARNING, "MAX_CACHED_ENTITY_LERP_MT_PIPELINES hit, falling back to the opaque 3D pipeline\n" );
		return vk.pipeline_3d_opaque;
	}

	GetEntityLerpMultiTexAttribs( bindings, attribs );

	spec_data.combine_mode = combineMode;
	spec_data.alpha_test_func = alphaTestFunc;
	spec_data.lighting_diffuse = lightingDiffuse ? 1 : 0;
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( entityLitMultiTexSpecData_t, combine_mode );
	spec_entries[0].size = sizeof( int32_t );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( entityLitMultiTexSpecData_t, alpha_test_func );
	spec_entries[1].size = sizeof( int32_t );
	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( entityLitMultiTexSpecData_t, lighting_diffuse );
	spec_entries[2].size = sizeof( int32_t );
	specInfo.mapEntryCount = 3;
	specInfo.pMapEntries = spec_entries;
	specInfo.dataSize = sizeof( spec_data );
	specInfo.pData = &spec_data;

	s_cachedEntityLerpMTKeys[s_numCachedEntityLerpMTPipelines].combineMode = combineMode;
	s_cachedEntityLerpMTKeys[s_numCachedEntityLerpMTPipelines].cullMode = cullMode;
	s_cachedEntityLerpMTKeys[s_numCachedEntityLerpMTPipelines].alphaTestFunc = alphaTestFunc;
	s_cachedEntityLerpMTKeys[s_numCachedEntityLerpMTPipelines].lightingDiffuse = lightingDiffuse;
	s_cachedEntityLerpMTPipelines[s_numCachedEntityLerpMTPipelines] = create_pipeline_ex( vk_entity_lerp_multitex_vs, vk_multi_texture_fs,
		vk.lightmap_pipeline_layout, &specInfo, bindings, 3, attribs, 6, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
		VK_TRUE, cullMode, VK_POLYGON_MODE_FILL, VK_FALSE, VK_TRUE,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );

	return s_cachedEntityLerpMTPipelines[s_numCachedEntityLerpMTPipelines++];
}

// Entity GPU bone-skinning pipelines (MDS/MDM, vk_entity.c): static bind-pose buffer + dynamic color/uv, using vk.skin_pipeline_layout/skin_lightmap_pipeline_layout (set 1 = the bone-matrix UBO) instead of vk.pipeline_layout/lightmap_pipeline_layout.
typedef struct {
	VkBool32 blendEnable;
	VkBlendFactor srcBlend;
	VkBlendFactor dstBlend;
	VkCullModeFlags cullMode;
	int32_t alphaTestFunc;
	VkBool32 polygonOffset;
	VkBool32 lightingDiffuse;
} cachedEntitySkinKey_t;

#define MAX_CACHED_ENTITY_SKIN_PIPELINES 32
static cachedEntitySkinKey_t s_cachedEntitySkinKeys[MAX_CACHED_ENTITY_SKIN_PIPELINES];
static VkPipeline s_cachedEntitySkinPipelines[MAX_CACHED_ENTITY_SKIN_PIPELINES];
static int s_numCachedEntitySkinPipelines;

static void vk_resetEntitySkinPipelineCache( void ) {
	s_numCachedEntitySkinPipelines = 0;
}

static void vk_destroyEntitySkinPipelineCache( void ) {
	int i;
	for ( i = 0; i < s_numCachedEntitySkinPipelines; i++ ) {
		qvkDestroyPipeline( vk.device, s_cachedEntitySkinPipelines[i], NULL );
	}
	s_numCachedEntitySkinPipelines = 0;
}

static void GetEntitySkinAttribs( VkVertexInputBindingDescription bindings[2], VkVertexInputAttributeDescription attribs[7] ) {
	bindings[0].binding = 0;
	bindings[0].stride = sizeof( vkEntitySkinnedVert_t );
	bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindings[1].binding = 1;
	bindings[1].stride = sizeof( vkEntityDynamicVert_t );
	bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	attribs[0].location = 0; attribs[0].binding = 0; attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[0].offset = offsetof( vkEntitySkinnedVert_t, offset[0] );
	attribs[1].location = 1; attribs[1].binding = 0; attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[1].offset = offsetof( vkEntitySkinnedVert_t, offset[1] );
	attribs[2].location = 2; attribs[2].binding = 0; attribs[2].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[2].offset = offsetof( vkEntitySkinnedVert_t, offset[2] );
	attribs[3].location = 3; attribs[3].binding = 0; attribs[3].format = VK_FORMAT_R32G32B32_SFLOAT; attribs[3].offset = offsetof( vkEntitySkinnedVert_t, offset[3] );
	attribs[4].location = 4; attribs[4].binding = 0; attribs[4].format = VK_FORMAT_R8G8B8A8_UINT; attribs[4].offset = offsetof( vkEntitySkinnedVert_t, boneIndex );
	attribs[5].location = 5; attribs[5].binding = 0; attribs[5].format = VK_FORMAT_R8G8B8A8_UNORM; attribs[5].offset = offsetof( vkEntitySkinnedVert_t, boneWeight );
	// Bind-pose normal -- skinned the same weighted-sum way as offset[0..3] in the shader so it rotates with the animated pose; see entity_skin.vert.
	attribs[6].location = 6; attribs[6].binding = 0; attribs[6].format = VK_FORMAT_R16G16B16A16_SNORM; attribs[6].offset = offsetof( vkEntitySkinnedVert_t, normal );
}

VkPipeline vk_getPipelineEntitySkin( VkBool32 blendEnable, VkBlendFactor srcBlend, VkBlendFactor dstBlend,
									   VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 polygonOffset,
									   VkBool32 lightingDiffuse ) {
	VkVertexInputBindingDescription bindings[2];
	VkVertexInputAttributeDescription skinAttribs[7];
	VkVertexInputAttributeDescription attribs[9];
	entityLitSpecData_t spec_data;
	VkSpecializationMapEntry spec_entries[5];
	VkSpecializationInfo specInfo;
	int i;

	for ( i = 0; i < s_numCachedEntitySkinPipelines; i++ ) {
		const cachedEntitySkinKey_t *key = &s_cachedEntitySkinKeys[i];
		if ( key->blendEnable == blendEnable && key->srcBlend == srcBlend && key->dstBlend == dstBlend &&
			 key->cullMode == cullMode && key->alphaTestFunc == alphaTestFunc && key->polygonOffset == polygonOffset &&
			 key->lightingDiffuse == lightingDiffuse ) {
			return s_cachedEntitySkinPipelines[i];
		}
	}

	if ( s_numCachedEntitySkinPipelines >= MAX_CACHED_ENTITY_SKIN_PIPELINES ) {
		ri.Printf( PRINT_WARNING, "MAX_CACHED_ENTITY_SKIN_PIPELINES hit, falling back to the default pipeline\n" );
		return blendEnable ? vk.pipeline_3d_blend : vk.pipeline_3d_opaque;
	}

	GetEntitySkinAttribs( bindings, skinAttribs );
	memcpy( attribs, skinAttribs, sizeof( skinAttribs ) );
	attribs[7].location = 7; attribs[7].binding = 1; attribs[7].format = VK_FORMAT_R8G8B8A8_UNORM; attribs[7].offset = offsetof( vkEntityDynamicVert_t, color );
	attribs[8].location = 8; attribs[8].binding = 1; attribs[8].format = VK_FORMAT_R32G32_SFLOAT; attribs[8].offset = offsetof( vkEntityDynamicVert_t, uv );

	GetEntityLitSpecInfo( &spec_data, spec_entries, &specInfo, alphaTestFunc, lightingDiffuse );

	s_cachedEntitySkinKeys[s_numCachedEntitySkinPipelines].blendEnable = blendEnable;
	s_cachedEntitySkinKeys[s_numCachedEntitySkinPipelines].srcBlend = srcBlend;
	s_cachedEntitySkinKeys[s_numCachedEntitySkinPipelines].dstBlend = dstBlend;
	s_cachedEntitySkinKeys[s_numCachedEntitySkinPipelines].cullMode = cullMode;
	s_cachedEntitySkinKeys[s_numCachedEntitySkinPipelines].alphaTestFunc = alphaTestFunc;
	s_cachedEntitySkinKeys[s_numCachedEntitySkinPipelines].polygonOffset = polygonOffset;
	s_cachedEntitySkinKeys[s_numCachedEntitySkinPipelines].lightingDiffuse = lightingDiffuse;
	s_cachedEntitySkinPipelines[s_numCachedEntitySkinPipelines] = create_pipeline_ex( vk_entity_skin_vs, vk_single_texture_fs, vk.skin_pipeline_layout, &specInfo,
		bindings, 2, attribs, 9, blendEnable, srcBlend, dstBlend, VK_TRUE, cullMode,
		VK_POLYGON_MODE_FILL, polygonOffset, VK_TRUE,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );

	return s_cachedEntitySkinPipelines[s_numCachedEntitySkinPipelines++];
}

// Multitex twin of the above, mirroring vk_getPipelineMultiTexture's (combine, cull, alphaTest) key.
typedef struct {
	int32_t combineMode;
	VkCullModeFlags cullMode;
	int32_t alphaTestFunc;
	VkBool32 lightingDiffuse;
} cachedEntitySkinMTKey_t;

#define MAX_CACHED_ENTITY_SKIN_MT_PIPELINES 16
static cachedEntitySkinMTKey_t s_cachedEntitySkinMTKeys[MAX_CACHED_ENTITY_SKIN_MT_PIPELINES];
static VkPipeline s_cachedEntitySkinMTPipelines[MAX_CACHED_ENTITY_SKIN_MT_PIPELINES];
static int s_numCachedEntitySkinMTPipelines;

static void vk_resetEntitySkinMTPipelineCache( void ) {
	s_numCachedEntitySkinMTPipelines = 0;
}

static void vk_destroyEntitySkinMTPipelineCache( void ) {
	int i;
	for ( i = 0; i < s_numCachedEntitySkinMTPipelines; i++ ) {
		qvkDestroyPipeline( vk.device, s_cachedEntitySkinMTPipelines[i], NULL );
	}
	s_numCachedEntitySkinMTPipelines = 0;
}

VkPipeline vk_getPipelineEntitySkinMultiTex( int combineMode, VkCullModeFlags cullMode, int32_t alphaTestFunc, VkBool32 lightingDiffuse ) {
	VkVertexInputBindingDescription bindings[2];
	VkVertexInputAttributeDescription skinAttribs[7];
	VkVertexInputAttributeDescription attribs[10];
	entityLitMultiTexSpecData_t spec_data;
	VkSpecializationMapEntry spec_entries[3];
	VkSpecializationInfo specInfo;
	int i;

	for ( i = 0; i < s_numCachedEntitySkinMTPipelines; i++ ) {
		const cachedEntitySkinMTKey_t *key = &s_cachedEntitySkinMTKeys[i];
		if ( key->combineMode == combineMode && key->cullMode == cullMode && key->alphaTestFunc == alphaTestFunc &&
			 key->lightingDiffuse == lightingDiffuse ) {
			return s_cachedEntitySkinMTPipelines[i];
		}
	}

	if ( s_numCachedEntitySkinMTPipelines >= MAX_CACHED_ENTITY_SKIN_MT_PIPELINES ) {
		ri.Printf( PRINT_WARNING, "MAX_CACHED_ENTITY_SKIN_MT_PIPELINES hit, falling back to the opaque 3D pipeline\n" );
		return vk.pipeline_3d_opaque;
	}

	GetEntitySkinAttribs( bindings, skinAttribs );
	bindings[1].stride = sizeof( vkEntityDynamicMTVert_t );
	memcpy( attribs, skinAttribs, sizeof( skinAttribs ) );
	attribs[7].location = 7; attribs[7].binding = 1; attribs[7].format = VK_FORMAT_R8G8B8A8_UNORM; attribs[7].offset = offsetof( vkEntityDynamicMTVert_t, color );
	attribs[8].location = 8; attribs[8].binding = 1; attribs[8].format = VK_FORMAT_R32G32_SFLOAT; attribs[8].offset = offsetof( vkEntityDynamicMTVert_t, uv0 );
	attribs[9].location = 9; attribs[9].binding = 1; attribs[9].format = VK_FORMAT_R32G32_SFLOAT; attribs[9].offset = offsetof( vkEntityDynamicMTVert_t, uv1 );

	spec_data.combine_mode = combineMode;
	spec_data.alpha_test_func = alphaTestFunc;
	spec_data.lighting_diffuse = lightingDiffuse ? 1 : 0;
	spec_entries[0].constantID = 0;
	spec_entries[0].offset = offsetof( entityLitMultiTexSpecData_t, combine_mode );
	spec_entries[0].size = sizeof( int32_t );
	spec_entries[1].constantID = 1;
	spec_entries[1].offset = offsetof( entityLitMultiTexSpecData_t, alpha_test_func );
	spec_entries[1].size = sizeof( int32_t );
	spec_entries[2].constantID = 2;
	spec_entries[2].offset = offsetof( entityLitMultiTexSpecData_t, lighting_diffuse );
	spec_entries[2].size = sizeof( int32_t );
	specInfo.mapEntryCount = 3;
	specInfo.pMapEntries = spec_entries;
	specInfo.dataSize = sizeof( spec_data );
	specInfo.pData = &spec_data;

	s_cachedEntitySkinMTKeys[s_numCachedEntitySkinMTPipelines].combineMode = combineMode;
	s_cachedEntitySkinMTKeys[s_numCachedEntitySkinMTPipelines].cullMode = cullMode;
	s_cachedEntitySkinMTKeys[s_numCachedEntitySkinMTPipelines].alphaTestFunc = alphaTestFunc;
	s_cachedEntitySkinMTKeys[s_numCachedEntitySkinMTPipelines].lightingDiffuse = lightingDiffuse;
	s_cachedEntitySkinMTPipelines[s_numCachedEntitySkinMTPipelines] = create_pipeline_ex( vk_entity_skin_multitex_vs, vk_multi_texture_fs,
		vk.skin_lightmap_pipeline_layout, &specInfo, bindings, 2, attribs, 10, VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO,
		VK_TRUE, cullMode, VK_POLYGON_MODE_FILL, VK_FALSE, VK_TRUE,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE, VK_COMPARE_OP_ALWAYS, VK_STENCIL_OP_KEEP );

	return s_cachedEntitySkinMTPipelines[s_numCachedEntitySkinMTPipelines++];
}

// img0/img1 pointer-pair cache for vk.lightmap_set_layout-shaped sets; never freed individually, just dropped when s_multiTexDescriptorPool is destroyed.
static struct {
	const struct image_s *img0;
	const struct image_s *img1;
} s_cachedMultiTexDescKeys[MAX_MULTITEX_DESCRIPTORS];
static VkDescriptorSet s_cachedMultiTexDescSets[MAX_MULTITEX_DESCRIPTORS];
static int s_numCachedMultiTexDescSets;

static void vk_resetMultiTexDescriptorCache( void ) {
	s_numCachedMultiTexDescSets = 0;
}

VkDescriptorSet vk_getMultiTextureDescriptor( struct image_s *img0, struct image_s *img1 ) {
	VkDescriptorSetAllocateInfo allocInfo;
	VkDescriptorImageInfo imageInfo[2];
	VkWriteDescriptorSet writes[2];
	VkDescriptorSet set;
	int i;

	for ( i = 0; i < s_numCachedMultiTexDescSets; i++ ) {
		if ( s_cachedMultiTexDescKeys[i].img0 == img0 && s_cachedMultiTexDescKeys[i].img1 == img1 ) {
			return s_cachedMultiTexDescSets[i];
		}
	}

	if ( s_numCachedMultiTexDescSets >= MAX_MULTITEX_DESCRIPTORS ) {
		ri.Printf( PRINT_WARNING, "MAX_MULTITEX_DESCRIPTORS hit, reusing the last cached descriptor set\n" );
		return s_cachedMultiTexDescSets[MAX_MULTITEX_DESCRIPTORS - 1];
	}

	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.descriptorPool = s_multiTexDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.lightmap_set_layout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &allocInfo, &set ) );

	imageInfo[0].sampler = img0->vk_sampler;
	imageInfo[0].imageView = img0->vk_view;
	imageInfo[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo[1].sampler = img1->vk_sampler;
	imageInfo[1].imageView = img1->vk_view;
	imageInfo[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].pNext = NULL;
	writes[0].dstSet = set;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imageInfo[0];
	writes[0].pBufferInfo = NULL;
	writes[0].pTexelBufferView = NULL;
	writes[1] = writes[0];
	writes[1].dstBinding = 1;
	writes[1].pImageInfo = &imageInfo[1];

	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );

	s_cachedMultiTexDescKeys[s_numCachedMultiTexDescSets].img0 = img0;
	s_cachedMultiTexDescKeys[s_numCachedMultiTexDescSets].img1 = img1;
	s_cachedMultiTexDescSets[s_numCachedMultiTexDescSets] = set;

	return s_cachedMultiTexDescSets[s_numCachedMultiTexDescSets++];
}
