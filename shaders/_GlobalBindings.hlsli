#ifndef UTAH_GLOBAL_BINDINGS_HLSLI
#define UTAH_GLOBAL_BINDINGS_HLSLI
#pragma once

#include "_SharedTypes.hlsli"

[[vk::binding(0, 0)]] ConstantBuffer<CameraUBO> cam;
[[vk::binding(1, 0)]] ConstantBuffer<LightUBO> light;
[[vk::binding(2, 0)]] StructuredBuffer<ObjectData> objBuf;
[[vk::binding(3, 0)]] StructuredBuffer<MatData> matBuf;

// Whatever this trick is called, callsite decides which one to call
[[vk::binding(4, 0)]] Texture2D textures[];
[[vk::binding(4, 0)]] TextureCube textureCubes[];
[[vk::binding(5, 0)]] SamplerState textureSamplers[];

[[vk::binding(6, 0)]] ConstantBuffer<ShadowMapUBO> shadowMap;
[[vk::binding(7, 0)]] Texture2D shadowMaps[];
[[vk::binding(8, 0)]] SamplerComparisonState shadowMapCmpSampler;
[[vk::binding(9, 0)]] TextureCube shadowCubeMaps[];

[[vk::binding(10, 0)]] Texture2D hdrTexture;
[[vk::binding(11, 0)]] TextureCube skyboxCubemap;

[[vk::binding(12, 0)]] ConstantBuffer<SceneIBLUBO> sceneIBL;

[[vk::binding(13, 0)]] Texture2D gBufferColorTargets[];
[[vk::binding(14, 0)]] Texture2D depthTarget;

// PushConstant, for now, is defined in individual shaders based on usage

// Stay in sync with TextureManager::SamplerType
static const uint SAMPLER_REPEAT_ANISO = 0;
static const uint SAMPLER_CLAMP_EDGE = 1;
static const uint SAMPLER_REPEAT_U_CLAMP_V = 2;

// Stay in sync with VulkanRenderer::GBufferColorTargetType
static const uint G_BUFFER_COLOR_TARGET_POSITION    = 0;
static const uint G_BUFFER_COLOR_TARGET_NORMAL      = 1;
static const uint G_BUFFER_COLOR_TARGET_ALBEDO      = 2;
static const uint G_BUFFER_COLOR_TARGET_ORM         = 3;
static const uint G_BUFFER_COLOR_TARGET_EMISSIVE    = 4;

#endif