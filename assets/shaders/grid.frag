precision mediump float;

out vec4      frag_color;
uniform float alpha;
uniform ivec2 size;
in vec2       v_texcoord;

// The MIT License
// Copyright © 2017 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright
// notice and this permission notice shall be included in all copies or substantial portions of the Software. THE
// SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Analiyically filtering a grid pattern (ie, not using supersampling or mipmapping.
//
// Info: https://iquilezles.org/articles/filterableprocedurals
//
// More filtered patterns:  https://www.shadertoy.com/playlist/l3KXR1

// --- unfiltered grid ---

float unfilteredGrid(vec2 p, vec2 lineWidth, vec2 w)
{
    // coordinates
    vec2 i = step(fract(p), lineWidth);
    // pattern
    float v = 1.0 - (1.0 - i.x) * (1.0 - i.y);
    // we only need to draw within the unit square
    vec2 s = step(0.0, p) * (1.0 - step(vec2(size) + lineWidth, p));
    v *= s.x * s.y;
    return v;
}

float filteredGrid(vec2 p, vec2 lineWidth, vec2 w)
{
    p += vec2(0.5) * lineWidth;
    vec2 a = p + 0.5 * w;
    vec2 b = p - 0.5 * w;
    vec2 i =
        lineWidth / w * ((floor(a) + min(fract(a) / lineWidth, 1.0)) - (floor(b) + min(fract(b) / lineWidth, 1.0)));

    // now fade to the average color to avoid moire
    i = mix(i, lineWidth, clamp(w * 2.0 - 1.0, 0.0, 1.0));

    float v = 1.0 - (1.0 - i.x) * (1.0 - i.y);

    // we only need to draw within the unit square
    vec2 s = step(0.0, p) * (1.0 - step(vec2(size) + lineWidth, p));
    v *= s.x * s.y;
    return v;
}

// Based off of pristine grid by Ben Golus from The Best Darn Grid Shader (yet)
// https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8
// https://www.shadertoy.com/view/mdVfWw
float pristineGrid(vec2 p, vec2 lineWidth, vec2 w)
{
    vec2 lineAA      = w * 1.5;
    vec2 invertLine  = vec2(lineWidth.x > 0.5, lineWidth.y > 0.5);
    vec2 targetWidth = mix(lineWidth, 1.0 - lineWidth, vec2(invertLine));
    vec2 drawWidth   = clamp(targetWidth, w, vec2(0.5));

    vec2 gridUV = abs(fract(p) * 2.0 - 1.0);
    gridUV      = mix(1.0 - gridUV, gridUV, invertLine);

    vec2 lines = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);

    // removed the next line so that the lines appear more even in intensity regardless of the angle we view them from
    // lines *= clamp(targetWidth / drawWidth, 0.0, 1.0);

    // now fade to the average color to avoid moire
    lines = mix(lines, drawWidth, clamp(w * 2.0 - 1.0, 0.0, 1.0));
    lines = mix(lines, 1.0 - lines, vec2(invertLine));

    float v = 1.0 - (1.0 - lines.x) * (1.0 - lines.y);

    // we only need to draw within the unit square
    vec2 s = smoothstep(-0.5 * (lineWidth + w), -0.5 * (lineWidth - w), p) *
             (1.0 - smoothstep(vec2(size) + 0.5 * (lineWidth - w), vec2(size) + (0.5 * lineWidth + w), p));
    return v * s.x * s.y;
}

void main()
{
    vec2 uv        = v_texcoord * vec2(size);
    vec2 w         = 1.5 * max(abs(dFdx(uv)), abs(dFdy(uv)));
    vec2 lineWidth = min(vec2(0.001) * vec2(size), vec2(3) * w);

    float v    = pristineGrid(uv, lineWidth, w);
    frag_color = vec4(vec3(1.0), alpha * v);
}
