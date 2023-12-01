#version 100
precision     mediump float;
uniform float alpha;
void          main()
{
    gl_FragColor = vec4(vec3(1.0), alpha);
}
