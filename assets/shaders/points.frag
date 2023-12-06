#version 100
precision     mediump float;
uniform vec3  color;
uniform float point_size;
void          main()
{
    float alpha = 1.0;
    if (point_size > 3.0)
    {
        vec2  circCoord = 2.0 * gl_PointCoord - 1.0;
        float radius2   = dot(circCoord, circCoord);
        if (radius2 > 1.0)
            discard;
        // alpha = 1.0 - smoothstep(1.0 - 2.0 / point_size, 1.0, sqrt(radius2));
    }
    gl_FragColor = vec4(color * alpha, alpha);
}
