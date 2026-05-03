#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

#include "ShaderInterop.h"
#include "PBR.glsl"

vec3 CalcDirectionLight(DirectionLight light, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic)
{
    vec3 L = normalize(-light.direction);
    vec3 H = normalize(V + L);

    vec3 F0 = ComputeF0(albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    float illuminance = light.intensity;

    return (kD * albedo / PI + specular) * light.color * illuminance * NdotL;
}

vec3 CalcPunctualLight(PunctualLight light, vec3 worldPos, vec3 N, vec3 V, vec3 albedo, float roughness, float metallic, bool isSpot)
{
    vec3 L = light.position - worldPos;
    float distance = length(L);
    L = normalize(L);

    float attenuation = 1.0 / (distance * distance);

    float invRadius = light.invRadius;
    float factor = distance * invRadius;
    float smoothFactor = max(1.0 - factor * factor, 0.0);
    attenuation = (smoothFactor * smoothFactor) / (distance * distance + 1.0);

    float spotAttenuation = 1.0;
    if (isSpot)
    {
        float cosTheta = dot(-L, normalize(light.direction));
        float cosOuter = cos(light.coneAngle * 0.5);
        float cosInner = cos(light.innerAngle * 0.5);
        float epsilon = cosInner - cosOuter;
        spotAttenuation = clamp((cosTheta - cosOuter) / epsilon, 0.0, 1.0);
    }

    vec3 H = normalize(V + L);

    vec3 F0 = ComputeF0(albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    float luminousIntensity = light.intensity;
    if (!isSpot)
    {
        luminousIntensity = light.intensity / (4.0 * PI);
    }
    else
    {
        float cosHalfAngle = cos(light.coneAngle * 0.5);
        float solidAngle = 2.0 * PI * (1.0 - cosHalfAngle);
        luminousIntensity = light.intensity / max(solidAngle, 1.0e-4);
    }

    return (kD * albedo / PI + specular) * light.color * luminousIntensity * NdotL * attenuation * spotAttenuation;
}

#endif
