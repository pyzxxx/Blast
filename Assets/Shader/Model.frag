#version 450

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 o_fragColor;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 ambient = vec3(0.2, 0.2, 0.2);
    vec3 diffuse = vec3(0.6, 0.6, 0.6) * diff;
    vec3 color = ambient + diffuse;
    
    o_fragColor = vec4(color, 1.0);
}