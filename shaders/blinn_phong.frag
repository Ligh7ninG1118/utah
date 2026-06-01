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
    float cutOff;
    vec3 specular;
    float outerCutOff;
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

vec3 CalculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objColor)
{
    float dist     = length(light.position - fragPos);
    vec3  lightDir = normalize(light.position - fragPos);
    float atten    = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);

    // Ambient
    vec3 ambient = light.ambient * objColor * atten;

    // Diffuse
    float diffDot = max(dot(normal, lightDir), 0.0);
    vec3  diffuse = light.diffuse * objColor * diffDot * atten;

    // Specular
    vec3  H       = normalize(lightDir + viewDir);
    float specDot = pow(max(dot(normal, H), 0.0), 64.0);
    vec3  specular = light.specular * objColor * specDot * atten;

    return ambient + diffuse + specular;
}

vec3 CalculateDirectionalLight(DirectionalLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objColor)
{
    // Ambient
    vec3 ambient = light.ambient * objColor;

    // Diffuse
    float diffDot = max(dot(normal, light.direction), 0.0);
    vec3  diffuse = light.diffuse * objColor * diffDot;

    // Specular
    vec3  H       = normalize(light.direction + viewDir);
    float specDot = pow(max(dot(normal, H), 0.0), 64.0);
    vec3  specular = light.specular * objColor * specDot;

    return ambient + diffuse + specular;
}

vec3 CalculateSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objColor)
{
    float distance = length(light.position - fragPos);
	float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

	vec3 ambient = light.ambient * objColor;
	ambient *= attenuation;

	vec3 lightDir = normalize(light.position - fragPos);
	vec3 norm = normalize(normal);
	float diffDot = max(dot(norm, lightDir), 0.0f);
	vec3 diffuse = light.diffuse * objColor * diffDot;
	diffuse *= attenuation;

	vec3 reflectDir = reflect(-lightDir, norm);
	float specDot = pow(max(dot(viewDir, reflectDir), 0.0f), 64.0);
	vec3 specular = light.specular * objColor * specDot;
	specular *= attenuation;

	float theta = dot(lightDir, normalize(-light.direction));
	vec3 result;
	float intensity = clamp((theta - light.outerCutOff) / (light.cutOff - light.outerCutOff), 0.0, 1.0);
	diffuse *= intensity;
	specular *= intensity;

	return ambient + diffuse + specular;
}

void main()
{
    uint texIndex = matBuf.materials[inMatIndex].texIndices[0];
    vec4 baseColor = matBuf.materials[inMatIndex].color;
    vec4 tex     = texture(textures[nonuniformEXT(texIndex)], inTexCoord);

    vec3 albedo = tex.rgb * baseColor.rgb;

    vec3 viewDir = normalize(lightUBO.eyePos - inWorldPos);
    vec3 result = vec3(0.0);
    for (uint i = 0; i < lightUBO.pointLightNum; i++)
        result += CalculatePointLight(lightUBO.pointLights[i], inNormal, inWorldPos, viewDir, albedo);

    for (uint i = 0; i < lightUBO.dirLightNum; i++)
        result += CalculateDirectionalLight(lightUBO.dirLights[i], inNormal, inWorldPos, viewDir, albedo);

    for (uint i = 0; i < lightUBO.spotLightNum; i++)
        result += CalculateSpotLight(lightUBO.spotLights[i], inNormal, inWorldPos, viewDir, albedo);

    outColor = vec4(result, tex.a);
}
