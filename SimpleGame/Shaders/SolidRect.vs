#version 330 core
layout(location = 0) in vec2 a_Local;
layout(location = 1) in vec4 a_AB;
layout(location = 2) in vec4 a_CD;
layout(location = 3) in vec4 a_Color;
out vec4 v_Color;

void main()
{
    // Canonical mesh coordinates become screen-space corners per instance.
    vec2 top = mix(a_AB.xy, a_AB.zw, a_Local.x);
    vec2 bottom = mix(a_CD.zw, a_CD.xy, a_Local.x);
    vec2 position = mix(top, bottom, a_Local.y);
    gl_Position = vec4(position.x / 640.0 - 1.0, 1.0 - position.y / 400.0, 0.0, 1.0);
    v_Color = a_Color;
}
