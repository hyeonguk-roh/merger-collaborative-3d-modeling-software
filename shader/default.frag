#version 410 core

in vec3 f_pos;
in vec3 f_norm;
out vec4 final_color;

uniform bool selected;
uniform vec3 light_pos;

vec3 light_color = vec3(1.0, 1.0, 1.0);
vec3 object_color = vec3(0.5, 0.5, 0.5);
vec3 select_color = vec3(1.0, 0.0, 1.0);

void main()
{
    vec3 norm = normalize(f_norm);
    vec3 light_dir = normalize(light_pos - f_pos);
    float diffuse = max(dot(light_dir, norm), 0.0);
    float ambient = 0.2;

    float d = length(light_pos - f_pos);

    float attenuation = 1.0 / (1.0 + (0.027 * d) + (0.0028 * d * d));
    vec3 lighting = (ambient * light_color) + (diffuse * attenuation * light_color);

    if (selected)
        final_color = vec4(select_color, 1.0);
    else
        final_color = vec4(object_color * lighting, 1.0);
}