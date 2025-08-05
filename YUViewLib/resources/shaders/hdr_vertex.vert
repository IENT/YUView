#version 330 core

// Vertex attributes
layout (location = 0) in vec2 aPos;        // Vertex position
layout (location = 1) in vec2 aTexCoord;   // Texture coordinates

// Output to fragment shader
out vec2 TexCoord;

// Transformation matrices
uniform mat4 textureMatrix;      // Texture coordinate transformation
uniform mat4 projectionMatrix;   // Projection transformation (for aspect ratio)

void main()
{
    // Transform vertex position to normalized device coordinates
    gl_Position = projectionMatrix * vec4(aPos, 0.0, 1.0);
    
    // Transform texture coordinates 
    // This allows for texture transformations like scaling, rotation, etc.
    TexCoord = (textureMatrix * vec4(aTexCoord, 0.0, 1.0)).xy;
}