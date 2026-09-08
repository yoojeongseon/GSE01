#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;
out vec4 v_Color;
void main() {
    gl_Position = vec4(a_Position.x / 640.0 - 1.0, 1.0 - a_Position.y / 400.0, 0.0, 1.0);
    v_Color = a_Color;
}
