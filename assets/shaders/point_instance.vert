precision mediump float;

uniform mat4  mvp;
uniform mat4  smash;
uniform mat3  rotation;
uniform float point_size;
in vec3       vertices;
in vec3       center;
out vec2      v_texcoord;

void main()
{
    gl_Position =
        mvp * (vec4(rotation * (point_size * vertices), 1.0) + vec4((smash * vec4(center - 0.5, 1.0)).xyz, 0.0));
    v_texcoord = 2.0 * vertices.xy;
}