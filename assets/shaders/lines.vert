#version 100
precision      mediump float;
uniform mat4   mvp;
attribute vec3 position;
void           main()
{
    gl_Position = mvp * vec4(position, 1.0);
}
