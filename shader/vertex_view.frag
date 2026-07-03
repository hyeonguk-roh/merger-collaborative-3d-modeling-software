#version 410 core

out vec4 final_color;
uniform uint selected;

void main()
{
    if (selected > 0) final_color = vec4(1.0, 0.0, 1.0, 1.0); else final_color = vec4(1.0, 1.0, 1.0, 1.0);
}