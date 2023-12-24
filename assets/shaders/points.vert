uniform mat4  mvp;
uniform float point_size;
in vec3       position;

void main()
{
    gl_Position  = mvp * vec4(position - vec3(0.5), 1.0);
    gl_PointSize = point_size;
}