$input v_texcoord0, v_normal

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0);
    
    // Simple lighting based on normal
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float ndotl = max(dot(normalize(v_normal), lightDir), 0.3); // Min 30% ambient
    
    gl_FragColor = vec4(color.rgb * ndotl, color.a);
}