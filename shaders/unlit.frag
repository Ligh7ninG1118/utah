#version 450
#extension GL_EXT_nonuniform_qualifier : require


struct MatData 
{ 
    uint texIndices[4];
    vec4 color;
    float shininess;
};

layout(set = 0, binding = 3) readonly buffer MaterialBuffer {
    MatData materials[];
} matBuf;


// --------------- Fragment Input ---------------
layout(location = 0) flat in uint inMatIndex;

// --------------- Fragment Output ---------------
layout(location = 0) out vec4 outColor;


void main()
{
    vec4 color = matBuf.materials[inMatIndex].color;
    outColor = color;
}
