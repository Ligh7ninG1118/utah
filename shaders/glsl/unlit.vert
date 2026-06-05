#version 450

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
} cam;

struct ObjectData { mat4 model; };
layout(set = 0, binding = 2) readonly buffer ObjectBuffer {
    ObjectData objects[];
} objBuf;

layout(push_constant) uniform PushConstants {
    uint objIndex;
    uint matIndex;
} pc;

layout(location = 0) in vec3 inPos;

layout(location = 0) flat out uint outMatIndex;

void main() {
    mat4 model      = objBuf.objects[pc.objIndex].model;
    vec4 worldPos   = model * vec4(inPos, 1.0);
    gl_Position     = cam.proj * cam.view * worldPos;
    outMatIndex = pc.matIndex;
}
