#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertexPosition_modelspace;
layout(location = 1) in vec2 vertexLuma;
layout(location = 2) in vec2 vertexChroma;

// Output data ; will be interpolated for each fragment.
out vec2 fragmentUVLuma;
//out vec2 fragmentUVChroma;

// Values that stay constant for the whole mesh.
uniform mat4 MVP;

void main(){

    gl_Position = vec4(vertexPosition_modelspace, 1.0f);

    // Projected position in 3D homogeneous coordinates
    /*vec4 ref_position_world = vec4(vertexPosition_modelspace[0],vertexPosition_modelspace[1],1,1.0);



    gl_Position =  MVP * ref_position_world;
    /// putting -1.0 there mirrors the image so it is correct. should not be necessary. how to fix?
    gl_Position[0] = gl_Position[0]/gl_Position[3];
    gl_Position[1] = gl_Position[1]/gl_Position[3];
    gl_Position[2] = gl_Position[2]/gl_Position[3];
    gl_Position[3] = 1.0;*/
    fragmentUVLuma = vec2(vertexLuma.x, 1.0 - vertexLuma.y);
    //fragmentUVChroma = vec2(vertexChroma.x, 1.0 - vertexChroma.y);
}
