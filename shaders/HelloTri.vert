#version 450

// --------------- Camera UBO (set 0, binding 0) ---------------
layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
} camUBO;

// --------------- Object UBO (set 1, binding 0) ---------------
layout(set = 1, binding = 0) uniform ObjectUBO
{
    mat4 model;
} objUBO;

// --------------- Vertex Input ---------------
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// --------------- Vertex Output ---------------
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec2 outTexCoord;

void main()
{
    outWorldPos = (objUBO.model * vec4(inPosition, 1.0)).xyz;
    gl_Position = camUBO.proj * camUBO.view * vec4(outWorldPos, 1.0);

    // For non-uniform scale, pass inverse-transpose separately
    outNormal   = normalize(mat3(objUBO.model) * inNormal);
    outTexCoord = inTexCoord;
}
