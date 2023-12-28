precision mediump float;

uniform mat4 mvp;
in vec3      position;
out vec2     v_texcoord;

void main()
{
    gl_Position = mvp * vec4(vec3(position) - vec3(0.5), 1.0);
    v_texcoord  = position.xy;
}