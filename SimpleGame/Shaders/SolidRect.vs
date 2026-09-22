#version 330 core
layout(location = 1) in vec4 a_AB;
layout(location = 2) in vec4 a_CD;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in float a_Mesh;
uniform samplerBuffer u_MeshAtlas;
out vec4 v_Color;

void main()
{
    // Three file-cached meshes, 48 slots each. Unused triples are degenerate.
    // Instance order stays identical to the painter-ordered submission stream.
    vec2 local = texelFetch(u_MeshAtlas, int(a_Mesh) * 48 + gl_VertexID).xy;
    vec2 top = mix(a_AB.xy, a_AB.zw, local.x);
    vec2 bottom = mix(a_CD.zw, a_CD.xy, local.x);
    vec2 position = mix(top, bottom, local.y);
    gl_Position = vec4(position.x / 640.0 - 1.0, 1.0 - position.y / 400.0, 0.0, 1.0);
    v_Color = a_Color;
}
