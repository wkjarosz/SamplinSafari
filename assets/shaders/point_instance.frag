uniform mat4  mvp;
uniform mat3  rotation;
uniform vec3  color;
uniform float point_size;
in vec2       v_texcoord;

// taken from https://gist.github.com/romainguy/6415cbf511233c063a1abe691ad4883b
vec3 Irradiance_SphericalHarmonics(const vec3 n)
{
    // Uniform array of 4 vec3 for SH environment lighting
    // Computed from "Ditch River" (http://www.hdrlabs.com/sibl/archive.html)
    const vec3 sh0 = vec3(0.754554516862612, 0.748542953903366, 0.790921515418539);
    const mat3 sh1 = mat3(vec3(-0.188884931542396, -0.277402551592231, -0.377844212327557),
                          vec3(0.308152705331738, 0.366796330467391, 0.466698181299906),
                          vec3(-0.083856548007422, 0.092533500963210, 0.322764661032516));
    return max(sh0 + sh1 * n, 0.0);
}

void main()
{
    float alpha   = 1.0;
    float radius2 = dot(v_texcoord, v_texcoord);
    if (radius2 > 1.0)
        discard;
    vec3 n       = rotation * vec3(v_texcoord, sqrt(1.0 - radius2));
    vec3 sh      = Irradiance_SphericalHarmonics(n);
    sh           = mix(mix(sh, vec3(dot(sh, vec3(1.0 / 3.0))), 0.5), vec3(1.0), 0.25);
    fo_FragColor = vec4(sh * color * alpha, alpha);
}
