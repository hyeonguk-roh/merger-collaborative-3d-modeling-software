#version 410 core

/*
    GLSL Version of grid tutorial found at: https://dev.to/javiersalcedopuyo/simple-infinite-grid-shader-5fah
*/

in vec3 f_cam_world_pos;
in vec2 coords;
out vec4 final_color;

float cell_size = 1.0;
float half_cell_size = 0.5f * cell_size;
float subcell_size = 0.1;
float half_subcell_size = 0.5 * subcell_size;

vec3 cell_line_color = vec3(0.75);
vec3 subcell_line_color = vec3(0.5);
float cell_line_thickness = 0.01;
float subcell_line_thickness = 0.001;

float max_fade_distance = 25.0f;

void main()
{
    vec2 cell_coords = mod(coords + half_cell_size, cell_size);
    vec2 distance_to_cell = abs(cell_coords - half_cell_size);
    vec2 subcell_coords = mod(coords + half_subcell_size, subcell_size);
    vec2 distance_to_subcell = abs(subcell_coords - half_subcell_size);

    vec2 d = fwidth(coords);
    vec2 adjusted_cell_line_thickness = 0.5 * (cell_line_thickness + d);
    vec2 adjusted_subcell_line_thickness = 0.5 * (subcell_line_thickness + d);

    final_color = vec4(0.0);
    if ( any(lessThan(distance_to_subcell, adjusted_subcell_line_thickness)) ) final_color = vec4(subcell_line_color, 1.0);
    if ( any(lessThan(distance_to_cell, adjusted_cell_line_thickness)) ) final_color = vec4(cell_line_color, 1.0);

    float opacity_falloff = smoothstep(1.0, 0.0, length(coords - f_cam_world_pos.xz) / max_fade_distance);
    final_color *= opacity_falloff;
}