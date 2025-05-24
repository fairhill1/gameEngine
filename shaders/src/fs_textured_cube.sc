$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_skyColor;     // Current sky color for ambient tint
uniform vec4 u_sunDirection; // Sun direction for basic lighting
uniform vec4 u_timeOfDay;    // Time info: x=normalizedTime, y=sunHeight

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);
    
    // Basic lighting based on sun height
    float sunHeight = u_timeOfDay.y;
    float lightIntensity = max(0.3, sunHeight * 0.7 + 0.3); // Never completely dark
    
    // Apply sky color tint and lighting
    vec3 litColor = color.rgb * lightIntensity;
    vec3 finalColor = mix(litColor, litColor * u_skyColor.rgb, 0.2);
    
    gl_FragColor = vec4(finalColor, color.a);
}