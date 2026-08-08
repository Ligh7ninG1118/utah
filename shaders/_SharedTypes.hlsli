#ifndef UTAH_SHARED_TYPES_HLSLI
#define UTAH_SHARED_TYPES_HLSLI
#pragma once

// Constants
static const uint MAX_POINT_LIGHTS = 32;
static const uint MAX_DIR_LIGHTS = 4;
static const uint MAX_SPOT_LIGHTS = 32;
static const uint MAX_SHADOW_CASTER_LIGHTS = 64;
static const uint MAX_TEX_SLOTS = 8;

static const float PI = 3.1415926535f;


struct CameraUBO
{
    float4x4 view;
    float4x4 proj;
    float4x4 invView;
    
    float3 eyePos;
    float nearPlane;
    float farPlane;
};

struct ObjectData
{
    float4x4 model;
};

// Mirrors MaterialManager::MaterialGPU
struct MatData
{
    uint texIndices[MAX_TEX_SLOTS]; // 0 base, 1 MR, 2 normal, 3 emissive, 4 occlusion, 5 to 7 reserved
    uint samplerIndices[MAX_TEX_SLOTS];
    float4 baseColorFactor;
    float4 ormFactor; // r = ao strength, g = roughness, b = metallic
    float4 emissiveFactor;
    float4 params; // r = normal scale, g = alpha cutoff, b/a = reserved
};

// Lighting, shadow maps

struct PointLight
{
    float3 position;
    float intensity;
    float3 color;
    float range;
};

struct DirectionalLight
{
    float3 direction;
    float intensity;
    float3 color;
    float range;
};

struct SpotLight
{
    float3 position;
    float cutoff;
    float3 direction;
    float outerCutoff;
    float3 color;
    float intensity;
    float range;
};

struct LightUBO
{
    uint pointLightNum;
    uint dirLightNum;
    uint spotLightNum;

    PointLight pointLights[MAX_POINT_LIGHTS];
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
};

struct ShadowMapUBO
{
    float4x4 lightViewProj[MAX_SHADOW_CASTER_LIGHTS];
};

struct SceneIBLUBO
{
    float3 ambientColor;
    float intensity;
    uint irradianceIndex;
    uint prefilteredIndex;
    uint brdfLUTIndex;
    uint prefilteredMaxMip;
};

struct SSAOKernelUBO
{
    float3 samples[64];
};

#endif