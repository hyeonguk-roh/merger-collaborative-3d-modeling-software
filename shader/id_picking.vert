#version 410 core

layout(location = 0) in vec3 v_pos;
layout(location = 1) in uint v_face_id;
layout(location = 2) in uint v_vertex_id;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

flat out uint f_face_id;
flat out uint f_vertex_id;

void main()
{
    gl_Position = projection * view * model * vec4(v_pos, 1.0);
    f_face_id = v_face_id;
    f_vertex_id = v_vertex_id;
}