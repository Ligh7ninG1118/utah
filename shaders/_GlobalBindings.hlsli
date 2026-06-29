#ifndef BINDINGS_HLSLI
#define BINDINGS_HLSLI

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
[[vk::binding(11, 0)]] SamplerState hdrSampler;
[[vk::binding(12, 0)]] TextureCube skyboxCubemap;

[[vk::binding(13, 0)]] ConstantBuffer<SceneIBLUBO> sceneIBL;

// PushConstant, for now, is defined in individual shaders based on usage

#endif