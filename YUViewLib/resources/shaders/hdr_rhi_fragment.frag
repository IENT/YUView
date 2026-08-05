#version 440

// HDR RGB Fragment Shader
// 
// This shader samples from a pre-converted RGB texture.
// Used as fallback for SDR content or when RGB texture path is needed.
// For HDR10, the texture should already contain PQ-encoded values.

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;

// Texture sampler for RGB video texture
layout(binding = 1) uniform sampler2D videoTexture;

void main() {
    // Sample the texture and output directly
    // For HDR10 swap chain, values are PQ-encoded
    // For SDR, values are in sRGB color space
    FragColor = texture(videoTexture, TexCoord);
}
