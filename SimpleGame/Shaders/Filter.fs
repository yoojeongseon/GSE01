#version 330 core
in vec2 v_UV;
layout(location = 0) out vec4 FragColor;
uniform sampler2D u_Source;
uniform int u_Mode; // 0: downsample + bright pass, 1: Gaussian, 2: downsample scene
uniform vec2 u_Direction;
uniform float u_Threshold;

vec3 prefilter(vec3 color) {
    if (u_Mode != 0) return color;
    float brightness = max(color.r, max(color.g, color.b));
    float knee = max(u_Threshold * 0.5, 0.0001);
    float soft = clamp(brightness - u_Threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(brightness - u_Threshold, soft) / max(brightness, 0.0001);
    return color * contribution;
}
void main() {
    vec2 texel = 1.0 / vec2(textureSize(u_Source, 0));
    vec3 result;
    if (u_Mode == 1) {
        vec2 stepUV = texel * u_Direction;
        result = texture(u_Source, v_UV).rgb * 0.2270270270;
        result += texture(u_Source, v_UV + stepUV * 1.3846153846).rgb * 0.3162162162;
        result += texture(u_Source, v_UV - stepUV * 1.3846153846).rgb * 0.3162162162;
        result += texture(u_Source, v_UV + stepUV * 3.2307692308).rgb * 0.0702702703;
        result += texture(u_Source, v_UV - stepUV * 3.2307692308).rgb * 0.0702702703;
    } else {
        // Threshold each source sample before downsampling to keep tiny sparks.
        result = prefilter(texture(u_Source, v_UV + texel * vec2(-0.5, -0.5)).rgb);
        result += prefilter(texture(u_Source, v_UV + texel * vec2(0.5, -0.5)).rgb);
        result += prefilter(texture(u_Source, v_UV + texel * vec2(-0.5, 0.5)).rgb);
        result += prefilter(texture(u_Source, v_UV + texel * vec2(0.5, 0.5)).rgb);
        result *= 0.25;
    }
    FragColor = vec4(result, 1.0);
}
