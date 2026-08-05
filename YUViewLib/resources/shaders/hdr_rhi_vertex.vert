#version 440

// HDR RGB Vertex Shader
// 
// This shader transforms vertex positions and texture coordinates for the fullscreen quad
// used in video rendering. Used for pre-converted RGB textures (SDR fallback path).

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 0) out vec2 TexCoord;

// Uniform block for RGB pipeline (simpler than YUV pipeline)
layout(std140, binding = 0) uniform UniformBlock {
    mat4 projectionMatrix;
    mat4 textureMatrix;
};

void main() {
    // Transform vertex position by projection matrix
    gl_Position = projectionMatrix * vec4(aPos, 0.0, 1.0);
    
    // Transform texture coordinates by texture matrix and flip Y
    TexCoord = (textureMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;
    TexCoord.y = 1.0 - TexCoord.y;
}
