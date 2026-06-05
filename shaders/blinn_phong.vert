#version 450

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

struct ObjectData 
{ 
    mat4 model; 
};
layout(set = 0, binding = 2) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objBuf;

layout(set = 0, binding = 5) uniform ShadowMapUBO
{
    mat4 lightViewProj;
} shadowMapUBO;

layout(push_constant) uniform PushConstants {
    uint objIndex;
    uint matIndex;
} pc;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec4 outWorldPosLightSpace;
layout(location = 4) flat out uint outMatIndex;

void main() {
    mat4 model      = objBuf.objects[pc.objIndex].model;
    vec4 worldPos   = model * vec4(inPos, 1.0);
    outWorldPosLightSpace = shadowMapUBO.lightViewProj * worldPos;
    gl_Position     = cam.proj * cam.view * worldPos;
    outWorldPos     = worldPos.xyz;
    outNormal       = normalize(mat3(model) * inNormal);
    outUV           = inUV;
    outMatIndex = pc.matIndex;
}
