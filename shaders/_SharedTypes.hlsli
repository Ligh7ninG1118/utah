#ifndef SHAREDTYPES_HLSLI
#define SHAREDTYPES_HLSLI

// Constants
static const uint MAX_POINT_LIGHTS = 32;
static const uint MAX_DIR_LIGHTS = 4;
static const uint MAX_SPOT_LIGHTS = 32;
static const uint MAX_SHADOW_CASTER_LIGHTS = 64;

static const float PI = 3.1415926535f;


struct CameraUBO
{
    float4x4 view;
    float4x4 proj;
    
    float3 eyePos;
    float nearPlane;
    float farPlane;
};

struct ObjectData
{
    float4x4 model;
};

struct MatData
{
    uint texIndices[4]; // [0] Albedo, [1] ORM, [2] Normal, [3] Emissive
    uint samplerIndices[4];
    float4 baseColorFactor;
    float3 ormFactor;
    float3 emissiveFactor;
    float normalScale;
};

// Lighting, shadow maps

struct PointLight
{
    float3 position;
    float3 color;
    float intensity;
    float range;
};

struct DirectionalLight
{
    float3 direction;
    float3 color;
    float intensity;
    float range;
};

struct SpotLight
{
    float3 position;
    float3 direction;
    float cutoff;
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
    uint irradianceIndex;
    uint prefilteredIndex;
    uint brdfLUTIndex;
    uint samplerIndex;
    float intensity;
    uint prefilteredMaxMip;
    float3 ambientColor;
};

#endif