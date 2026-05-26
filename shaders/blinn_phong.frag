#version 450
#extension GL_EXT_nonuniform_qualifier : require

// --------------- Point Light ---------------
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

const uint MAX_POINT_LIGHTS = 32;

// --------------- Light UBO ---------------
layout(set = 0, binding = 1) uniform LightUBO
{
    vec3 eyePos;
    uint lightNum;
    PointLight pointLights[MAX_POINT_LIGHTS];
} lightUBO;

struct MatData 
{ 
    uint texIndices[4];
    vec4 color;
};

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    MatData materials[];
} matBuf;

// --------------- Texture ---------------
layout(set = 0, binding = 4) uniform sampler2D textures[];

// --------------- Fragment Input ---------------
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) flat in uint inMatIndex;   //Need for this? PC can also reach frag shader(?)

// --------------- Fragment Output ---------------
layout(location = 0) out vec4 outColor;

vec3 CalculatePointLightTex(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objColor)
{
    float dist     = length(light.position - fragPos);
    vec3  lightDir = normalize(light.position - fragPos);
    float atten    = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    // Ambient
    vec3 ambient = light.ambient * objColor * atten;

    // Diffuse
    float diffDot = max(dot(normal, lightDir), 0.0);
    vec3  diffuse = light.diffuse * objColor * diffDot * atten;

    // Specular (Blinn-Phong)
    vec3  H       = normalize(lightDir + viewDir);
    float specDot = pow(max(dot(normal, H), 0.0), 64.0);
    vec3  specular = light.specular * objColor * specDot * atten;

    return ambient + diffuse + specular;
}

void main()
{
    uint texIndex = matBuf.materials[inMatIndex].texIndices[0];
    vec4 tex     = texture(textures[nonuniformEXT(texIndex)], inTexCoord);
    vec3 viewDir = normalize(lightUBO.eyePos - inWorldPos);

    vec3 result = vec3(0.0);
    for (uint i = 0; i < lightUBO.lightNum; i++)
    {
        result += CalculatePointLightTex(lightUBO.pointLights[i], inNormal, inWorldPos, viewDir, tex.rgb);
    }

    outColor = vec4(result, tex.a);
}
