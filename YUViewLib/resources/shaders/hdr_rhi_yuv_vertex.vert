#version 440

// HDR10 YUV->RGB Vertex Shader
// 
// This shader transforms vertex positions and texture coordinates for the fullscreen quad
// used in HDR10 video rendering. It shares the same uniform block as the fragment shader
// but only uses the projection and texture matrices.

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 0) out vec2 TexCoord;

// Uniform block layout must match the fragment shader exactly for binding compatibility
// Note: colorMatrix, offsetVec, hdrParams are included for layout matching but not used here
layout(std140, binding = 0) uniform UniformBlock {
    mat4 projectionMatrix;
    mat4 textureMatrix;
    mat4 colorMatrix;  // Unused in vertex shader, included for layout matching
    vec4 offsetVec;    // Unused in vertex shader, included for layout matching
    vec4 hdrParams;    // Unused in vertex shader, included for layout matching
};

void main() {
    // Transform vertex position by projection matrix
    gl_Position = projectionMatrix * vec4(aPos, 0.0, 1.0);
    
    // Transform texture coordinates by texture matrix and flip Y
    // The Y flip is needed because OpenGL/Vulkan have different coordinate conventions
    TexCoord = (textureMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;
    TexCoord.y = 1.0 - TexCoord.y;
}
