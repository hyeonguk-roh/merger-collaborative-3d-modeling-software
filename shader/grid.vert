#version 410 core

/*
    GLSL Version of grid tutorial found at: https://dev.to/javiersalcedopuyo/simple-infinite-grid-shader-5fah
*/


const vec3 vertices[4] = vec3[4](
    vec3(-1.0, 0.0, 1.0),
    vec3(1.0, 0.0, 1.0),
    vec3(1.0, 0.0, -1.0),
    vec3(-1.0, 0.0, -1.0)
);

const uint indices[6] = uint[6](0, 1, 2, 2, 3, 0);

uniform mat4 view;
uniform mat4 projection;
uniform vec3 cam_world_pos;

out vec3 f_cam_world_pos;
out vec2 coords;

float grid_size = 100.0;

void main()
{
    vec3 vert_pos = vertices[indices[gl_VertexID]] * grid_size;
    vert_pos.xz += cam_world_pos.xz;
    gl_Position = projection * view * vec4(vert_pos, 1.0);
    coords = vert_pos.xz;
    f_cam_world_pos = cam_world_pos;
}