#version 330 core
in vec2 v_UV;
layout(location = 0) out vec4 FragColor;
uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform sampler2D u_Blurred;
uniform float u_Exposure;
uniform float u_BloomStrength;
uniform float u_VignetteStrength;
uniform float u_EdgeBlurStrength;

vec3 toSRGB(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(max(color, 0.0), vec3(1.0 / 2.4)) - 0.055,
               step(vec3(0.0031308), color));
}
void main() {
    vec3 scene = texture(u_Scene, v_UV).rgb;
    // Elliptical in screen pixels; centered within the scene, excluding letterbox bars.
    float radius = length((v_UV - 0.5) * 2.0);
    float blurWeight = smoothstep(0.48, 1.15, radius) * u_EdgeBlurStrength;
    if (u_EdgeBlurStrength > 0.0)
        scene = mix(scene, texture(u_Blurred, v_UV).rgb, blurWeight);
    if (u_BloomStrength > 0.0)
        scene += texture(u_Bloom, v_UV).rgb * u_BloomStrength;
    scene *= 1.0 - smoothstep(0.35, 1.30, radius) * u_VignetteStrength;
    // Exposure in linear light, exponential tone mapping, then exactly one sRGB encoding.
    vec3 mapped = 1.0 - exp(-max(scene, 0.0) * u_Exposure);
    FragColor = vec4(toSRGB(mapped), 1.0);
}
