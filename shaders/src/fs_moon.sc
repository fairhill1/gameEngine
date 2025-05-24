$input v_worldPos

#include <bgfx_shader.sh>

uniform vec4 u_timeOfDay;    // Time info: x=normalizedTime, y=sunHeight
uniform vec4 u_moonData;     // Moon info: x=moonPhase (0-1), y=moonHeight

void main()
{
    // Calculate distance from sphere center
    float dist = length(v_worldPos);
    float moonSurface = 1.0 - smoothstep(0.9, 1.0, dist);
    
    if (moonSurface < 0.1) {
        discard; // Outside moon surface
    }
    
    // Moon phase calculation - creates shadow effect
    float moonPhase = u_moonData.x; // 0=new moon, 0.5=full moon, 1=new moon again
    float moonHeight = u_moonData.y;
    
    // Calculate moon surface position for phase shadow
    vec2 moonPos = v_worldPos.xy;
    float phaseX = (moonPhase - 0.5) * 4.0; // -2 to +2 range
    
    // Create crescent shadow based on phase
    float phaseMask = 1.0;
    if (moonPhase < 0.5) {
        // Waxing phases (new to full)
        phaseMask = smoothstep(phaseX - 0.2, phaseX + 0.2, moonPos.x);
    } else {
        // Waning phases (full to new)  
        phaseMask = smoothstep(-phaseX - 0.2, -phaseX + 0.2, -moonPos.x);
    }
    
    // Moon surface shading
    float lighting = dot(normalize(vec3(moonPos, sqrt(1.0 - dot(moonPos, moonPos)))), vec3(0.3, 0.5, 0.8));
    lighting = max(0.2, lighting); // Ambient lighting
    
    // Moon color - pale white/silver
    vec3 moonColor = vec3(0.9, 0.9, 1.0); // Slightly blue-white
    
    // Apply phase mask and lighting
    vec3 finalColor = moonColor * lighting * phaseMask * moonSurface;
    
    // Moon brightness based on height and phase
    float brightness = 0.8 + moonHeight * 0.3; // Brighter when higher
    if (moonPhase > 0.4 && moonPhase < 0.6) {
        brightness *= 1.5; // Full moon is brighter
    }
    
    finalColor *= brightness;
    
    gl_FragColor = vec4(finalColor, moonSurface * phaseMask);
}