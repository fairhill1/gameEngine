$input v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_timeOfDay;    // Time info: x=normalizedTime, y=sunHeight

void main()
{
    // Get sun height for color calculation
    float sunHeight = u_timeOfDay.y;
    
    // Calculate distance from sphere center for glow effect
    float dist = length(v_worldPos);
    float glow = 1.0 - smoothstep(0.0, 1.0, dist);
    
    // Sun color changes based on height:
    // High in sky: bright yellow/white
    // Low near horizon: red/orange
    vec3 highColor = vec3(1.0, 1.0, 0.8);  // Bright yellow-white
    vec3 lowColor = vec3(1.0, 0.4, 0.2);   // Red-orange
    
    // Mix colors based on sun height (0 = horizon, 1 = zenith)
    float heightFactor = clamp(abs(sunHeight), 0.0, 1.0);
    vec3 sunColor = mix(lowColor, highColor, heightFactor);
    
    // Apply glow and intensity - BLAZING bright
    float intensity = 50.0 + sunHeight * 20.0; // Extremely high base intensity
    vec3 finalColor = sunColor * glow * intensity;
    
    // Add BLAZING bright core
    float coreBrightness = 1.0 - smoothstep(0.0, 0.8, dist);
    finalColor += vec3(5.0, 5.0, 3.0) * coreBrightness * 30.0; // Blazing white core
    
    // Massive overall brightness boost
    finalColor *= 5.0;
    
    gl_FragColor = vec4(finalColor, glow);
}