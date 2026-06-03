#version 450

layout(push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;
    mat4 model;
} pc;

layout(location = 0) in vec3 inPos;

void main() {
    gl_Position     = pc.lightSpaceMatrix * pc.model * vec4(inPos, 1.0f);
}
