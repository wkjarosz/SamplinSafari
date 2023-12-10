#version 100
precision      mediump float;
uniform mat4   mvp;
attribute vec3 position;
void           main()
{
    gl_Position = mvp * vec4(position - vec3(0.5, 0.5, 0.0), 1.0);
}
