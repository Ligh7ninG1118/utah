#version 450

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

void main() 
{
    mat4 model = objBuf.objects[pc.objIndex].model;
    gl_Position = shadowMapUBO.lightViewProj * model * vec4(inPos, 1.0f);
}
