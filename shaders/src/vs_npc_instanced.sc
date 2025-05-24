$input a_position, a_normal, a_texcoord0, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0, v_normal

#include <bgfx_shader.sh>

void main()
{
    // Extract instance matrix from instance data  
    mat4 instanceMatrix = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    
    // Apply instance transformation directly to vertex position (no skeletal animation for now)
    vec4 worldPosition = mul(instanceMatrix, vec4(a_position, 1.0));
    
    gl_Position = mul(u_viewProj, worldPosition);
    v_texcoord0 = a_texcoord0;
    
    // Transform normal to world space (assuming uniform scaling)
    v_normal = mul(instanceMatrix, vec4(a_normal, 0.0)).xyz;
}