#version 330 core
in vec4 v_Color;
layout(location = 0) out vec4 FragColor;
uniform bool u_World;

vec3 toLinear(vec3 color)
{
    vec3 base = clamp(color, 0.0, 1.0);
    vec3 linear =
        mix(base / 12.92, pow((base + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), base));
    // Values above 1 are additional linear emission, preserved by RGBA16F.
    return linear + max(color - 1.0, 0.0);
}

void main()
{
    FragColor = vec4(u_World ? toLinear(v_Color.rgb) : v_Color.rgb, v_Color.a);
}
