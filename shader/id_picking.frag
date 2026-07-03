#version 410 core

flat in uint f_face_id;
flat in uint f_vertex_id;
uniform uint model_id;

out uvec3 final_out;
void main()
{
    final_out = uvec3(model_id, f_face_id, f_vertex_id);
}