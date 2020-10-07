#version 330 core

// The Vertex Shader is the programmable Shader stage in the rendering pipeline that
// handles the processing of individual vertices. Vertex shaders are fed
// Vertex Attribute data, as specified from a vertex array object by a drawing command.
// A vertex shader receives a single vertex from the vertex stream and generates
// a single vertex to the output vertex stream. There must be a 1:1 mapping from
// input vertices to output vertices.

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexLuma;

// Output data ; will be interpolated for each fragment.
out vec2 fragmentUVLuma; // texture coordinates

// Note: We need no model view projection (MVP) matrix
// the vertices are setup to be aligned with openGL clip coordinates.
// Thus no further coordinate transform will be necessary.
// Further, textrue coordinates can be obtained by dropping the z-axis coordinate.
// https://learnopengl.com/Getting-started/Coordinate-Systems

void main(){
    // we simply keep the location of the vertex
    gl_Position = vec4(vertexPosition_modelspace, 1.0f);
    // for texture coordinates we simply discard the z axis, and flip the y axis
    fragmentUVLuma = vec2(vertexLuma.x, 1.0 - vertexLuma.y);
}
