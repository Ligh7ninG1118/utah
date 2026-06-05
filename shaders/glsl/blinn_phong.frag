#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct PointLight
{
    vec3  position;
    float constant;
    vec3  ambient;
    float linear;
    vec3  diffuse;
    float quadratic;
    vec3  specular;
    float padding;
};

struct DirectionalLight
{
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight
{
    vec3 position;
    float constant;
    vec3 direction;
    float linear;
    vec3 ambient;
    float quadratic;
    vec3 diffuse;
    float cutoff;
    vec3 specular;
    float outerCutoff;
};

const uint MAX_POINT_LIGHTS = 32;
const uint MAX_DIR_LIGHTS = 4;
const uint MAX_SPOT_LIGHTS = 32;

// --------------- Light UBO ---------------
layout(set = 0, binding = 1) uniform LightUBO
{
    vec3 eyePos;
    uint pointLightNum;
    uint dirLightNum;
    uint spotLightNum;

    PointLight pointLights[MAX_POINT_LIGHTS];
    DirectionalLight dirLights[MAX_DIR_LIGHTS];
    SpotLight spotLights[MAX_SPOT_LIGHTS];
} lightUBO;

struct MatData 
{ 
    uint texIndices[4]; //[0] Albedo, [1] Specular
    vec4 color;
    float shininess;
};

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    MatData materials[];
} matBuf;

// --------------- Texture ---------------
layout(set = 0, binding = 4) uniform sampler2D textures[];

layout(set = 0, binding = 6) uniform sampler2D shadowMap;


// --------------- Fragment Input ---------------
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inWorldPosLightSpace;
layout(location = 4) flat in uint inMatIndex;   //Need for this? PC can also reach frag shader(?)

// --------------- Fragment Output ---------------
layout(location = 0) out vec4 outColor;

vec3 CalculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, float specStrength, float shininess)
{
    float dist     = length(light.position - fragPos);
    vec3  lightDir = normalize(light.position - fragPos);
    float atten    = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3 ambient = light.ambient * albedo;

    float diffDot = max(dot(normal, lightDir), 0.0);
    vec3  diffuse = light.diffuse * albedo * diffDot;

    vec3  H        = normalize(lightDir + viewDir);
    float specDot  = pow(max(dot(normal, H), 0.0), shininess);
    vec3  specular = light.specular * specStrength * specDot;

    return (ambient + diffuse + specular) * atten;
}

vec3 CalculateDirectionalLight(DirectionalLight light, vec3 normal, vec3 viewDir, vec3 albedo, float specStrength, float shininess)
{
    vec3 lightDir = normalize(-light.direction);

    vec3 ambient = light.ambient * albedo;

    float diffDot = max(dot(normal, lightDir), 0.0);
    vec3  diffuse = light.diffuse * albedo * diffDot;

    vec3  H        = normalize(lightDir + viewDir);
    float specDot  = pow(max(dot(normal, H), 0.0), shininess);
    vec3  specular = light.specular * specStrength * specDot;

    return ambient + diffuse + specular;
}

vec3 CalculateSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 albedo, float specStrength, float shininess)
{
    float dist     = length(light.position - fragPos);
    float atten    = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    vec3  lightDir = normalize(light.position - fragPos);

    vec3 ambient = light.ambient * albedo;

    float diffDot = max(dot(normal, lightDir), 0.0);
    vec3  diffuse = light.diffuse * albedo * diffDot;

    // Blinn-Phong half-vector, consistent with the other two
    vec3  H        = normalize(lightDir + viewDir);
    float specDot  = pow(max(dot(normal, H), 0.0), shininess);
    vec3  specular = light.specular * specStrength * specDot;

    // Spotlight cone: cutOff/outerCutOff are cosines, so inner > outer
    float theta     = dot(lightDir, normalize(-light.direction));
    float intensity = clamp((theta - light.outerCutoff) / (light.cutoff - light.outerCutoff), 0.0, 1.0);

    diffuse  *= intensity;
    specular *= intensity;

    return (ambient + diffuse + specular) * atten;
}

float ShadowCalculation(vec4 worldPosLightSpace)
{
    vec3 projCoords = inWorldPosLightSpace.xyz / inWorldPosLightSpace.w;
    vec2 uv = projCoords.xy * 0.5f + 0.5f;
    float currentDepth = projCoords.z;

    float closestDepth = texture(shadowMap, uv).r;
    float bias = 0.0f;
    float shadow = currentDepth < (closestDepth - bias) ? 0.5f : 0.0f; //reverse z
    return shadow;
}

void main()
{
    uint albedoIdx = matBuf.materials[inMatIndex].texIndices[0];
    uint specIdx   = matBuf.materials[inMatIndex].texIndices[1];
    vec4 baseColor = matBuf.materials[inMatIndex].color;
    float shininess = matBuf.materials[inMatIndex].shininess;

    vec4 texSample   = texture(textures[nonuniformEXT(albedoIdx)], inTexCoord);
    vec3 albedo      = texSample.rgb * baseColor.rgb;
    float specStrength = texture(textures[nonuniformEXT(specIdx)], inTexCoord).r;

    vec3 normal  = normalize(inNormal);
    vec3 viewDir = normalize(lightUBO.eyePos - inWorldPos);
    vec3 result  = vec3(0.0);

    for (uint i = 0; i < lightUBO.pointLightNum; i++)
        result += CalculatePointLight(lightUBO.pointLights[i], normal, inWorldPos, viewDir, albedo, specStrength, shininess);

    for (uint i = 0; i < lightUBO.dirLightNum; i++)
        result += CalculateDirectionalLight(lightUBO.dirLights[i], normal, viewDir, albedo, specStrength, shininess);

    for (uint i = 0; i < lightUBO.spotLightNum; i++)
        result += CalculateSpotLight(lightUBO.spotLights[i], normal, inWorldPos, viewDir, albedo, specStrength, shininess);

    float shadow = ShadowCalculation(inWorldPosLightSpace);

    result *= (1.0f - shadow);

    outColor = vec4(result, texSample.a);
}
