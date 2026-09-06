#ifndef UTAH_SHARED_TYPES_HLSLI
#define UTAH_SHARED_TYPES_HLSLI
#pragma once

// Constants
static const uint MAX_POINT_LIGHTS = 32;
static const uint MAX_DIR_LIGHTS = 4;
static const uint MAX_SPOT_LIGHTS = 32;
static const uint MAX_TEX_SLOTS = 8;

static const uint SHADOW_2D_SLOT_COUNT = 4;   // dir + spot
static const uint SHADOW_CUBE_SLOT_COUNT = 4;   // point
static const uint CUBE_FACE_COUNT = 6;
static const uint CUBE_FACE_VIEWMASK = 0x3F; // 0b00111111
static const int SHADOW_INDEX_NONE = -1;
static const uint SHADOW_CUBE_MATRIX_BASE = SHADOW_2D_SLOT_COUNT;
static const uint SHADOW_MATRIX_COUNT = SHADOW_2D_SLOT_COUNT + SHADOW_CUBE_SLOT_COUNT * CUBE_FACE_COUNT;

static const uint CLUSTER_GRID_X = 16; // independent of resolution
static const uint CLUSTER_GRID_Y = 9;
static const uint CLUSTER_GRID_Z = 24;
static const uint CLUSTER_COUNT = CLUSTER_GRID_X * CLUSTER_GRID_Y * CLUSTER_GRID_Z; // 3456
static const uint INVALID_CLUSTER_KEY = 0xFFFFFFFF;

static const uint MAX_CLUSTER_LIGHTS = 4096;
static const uint BVH_BRANCH = 32;
static const uint MAX_BVH_NODES = 133;
static const uint MAX_LIGHTS_PER_CLUSTER = 128;
static const uint MAX_CLUSTER_LIGHT_INDICES = 262144;
static const uint BVH_STACK_SIZE = 64;
static const uint BVH_LEAF_BIT = 0x80000000;

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
    int shadowIndex;
};

struct DirectionalLight
{
    float3 direction;
    float intensity;
    float3 color;
    float range;
    int shadowIndex;
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
    int shadowIndex;
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
    float4x4 lightViewProj[SHADOW_MATRIX_COUNT];
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

struct ClusterList
{
    uint uniqueCount; uint pad0; uint pad1; uint pad2;
    uint list[CLUSTER_COUNT];
};

struct BVHNodeGPU
{
    float3 minAABB;
    uint firstIndex;
    float3 maxAABB;
    uint count;
};

struct ClusterLightListGPU
{
    uint count;
    uint pad[3];
    uint list[MAX_CLUSTER_LIGHT_INDICES];
};

#endif