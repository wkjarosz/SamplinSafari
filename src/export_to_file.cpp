#include "export_to_file.h"
#include <fmt/core.h>
#include <iostream>

using std::ofstream;
using std::string;

string header_eps(const float3 &point_color, float scale, float radius)
{
    string out;

    float page_size = 500.f;

    out += fmt::format("%!PS-Adobe-3.0 EPSF-3.0\n");
    out += fmt::format("%%HiResBoundingBox: {} {} {} {}\n", -page_size, -page_size, page_size, page_size);
    out += fmt::format("%%BoundingBox: {} {} {} {}\n", -page_size, -page_size, page_size, page_size);
    out += fmt::format("%%CropBox: {} {} {} {}\n", -page_size, -page_size, page_size, page_size);
    out += fmt::format("/radius {{ {} }} def %define variable for point radius\n", radius * 0.3f * scale);
    out += fmt::format("/p {{ radius 0 360 arc closepath fill }} def %define point command\n");
    out += fmt::format("/blw {} def %define variable for bounding box linewidth\n", 2.f * scale);
    out += fmt::format("/clw {} def %define variable for coarse linewidth\n", 2.f * scale);
    out += fmt::format("/flw {} def %define variable for fine linewidth\n", 2.f * scale);
    out += fmt::format("/pfc {{ {} {} {} }} def %define variable for point fill color\n", point_color.x, point_color.y,
                       point_color.z);
    out += fmt::format("/blc {} def %define variable for bounding box color\n", 0.0f);
    out += fmt::format("/clc {} def %define variable for coarse line color\n", 0.5f);
    out += fmt::format("/flc {} def %define variable for fine line color\n", 0.9f);

    return out;
}

string footer_eps()
{
    // return "grestore\n";
    return "\n";
}

string draw_grid_eps(const float4x4 &mvp, int grid_res)
{
    float page_size = 500.f;

    float4 vA4d, vB4d;
    float2 vA, vB;

    int   fine_grid_res = 2;
    float coarse_scale = 1.f / grid_res, fine_scale = 1.f / fine_grid_res;

    string out;

    // draw the outer border of the grid
    {
        float4 c004d = mul(mvp, float4{0.f - 0.5f, 0.f - 0.5f, 0.f, 1.f});
        float4 c104d = mul(mvp, float4{1.f - 0.5f, 0.f - 0.5f, 0.f, 1.f});
        float4 c114d = mul(mvp, float4{1.f - 0.5f, 1.f - 0.5f, 0.f, 1.f});
        float4 c014d = mul(mvp, float4{0.f - 0.5f, 1.f - 0.5f, 0.f, 1.f});
        float2 c00   = float2(c004d.x / c004d.w, c004d.y / c004d.w) * page_size;
        float2 c10   = float2(c104d.x / c104d.w, c104d.y / c104d.w) * page_size;
        float2 c11   = float2(c114d.x / c114d.w, c114d.y / c114d.w) * page_size;
        float2 c01   = float2(c014d.x / c014d.w, c014d.y / c014d.w) * page_size;

        out += fmt::format(R"(newpath
    {} {} moveto
    {} {} lineto
    {} {} lineto
    {} {} lineto
closepath stroke
)",
                           c00.x, c00.y, c10.x, c10.y, c11.x, c11.y, c01.x, c01.y);
    }

    // draw the inner lines of the grid
    for (int i = 1; i <= grid_res - 1; ++i)
    {
        // draw horizontal lines
        out += "newpath\n";
        for (int j = 0; j < fine_grid_res; ++j)
        {
            vA4d = mul(mvp, float4{j * fine_scale - 0.5f, i * coarse_scale - 0.5f, 0.0f, 1.0f});
            vB4d = mul(mvp, float4{(j + 1) * fine_scale - 0.5f, i * coarse_scale - 0.5f, 0.0f, 1.0f});

            vA = float2{vA4d.x / vA4d.w, vA4d.y / vA4d.w} * page_size;
            vB = float2{vB4d.x / vB4d.w, vB4d.y / vB4d.w} * page_size;

            out += fmt::format("    {} {} {:s}\n", vA.x, vA.y, (j == 0) ? "moveto" : "lineto");
            out += fmt::format("    {} {} lineto\n", vB.x, vB.y);
        }
        out += "stroke\n";

        // draw vertical lines
        out += "newpath\n";
        for (int j = 0; j < fine_grid_res; ++j)
        {
            vA4d = mul(mvp, float4{i * coarse_scale - 0.5f, j * fine_scale - 0.5f, 0.0f, 1.0f});
            vB4d = mul(mvp, float4{i * coarse_scale - 0.5f, (j + 1) * fine_scale - 0.5f, 0.0f, 1.0f});

            vA = float2{vA4d.x / vA4d.w, vA4d.y / vA4d.w} * page_size;
            vB = float2{vB4d.x / vB4d.w, vB4d.y / vB4d.w} * page_size;

            out += fmt::format("    {} {} {:s}\n", vA.x, vA.y, (j == 0) ? "moveto" : "lineto");
            out += fmt::format("    {} {} lineto\n", vB.x, vB.y);
        }
        out += "stroke\n";
    }
    return out;
}

string draw_grids_eps(float4x4 mat, int fgrid_res, int cgrid_res, bool fine_grid, bool coarse_grid, bool bbox)
{
    string out;
    if (fine_grid)
    {
        out += "% Draw fine grids \n";
        out += "flc setgray %fill color for fine grid \n";
        out += "flw setlinewidth\n";

        // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
        out += draw_grid_eps(mat, fgrid_res);
    }

    if (coarse_grid)
    {
        out += "% Draw coarse grids \n";
        out += "clc setgray %fill color for coarse grid \n";
        out += "clw setlinewidth\n";

        out += draw_grid_eps(mat, cgrid_res);
    }

    if (bbox)
    {
        out += "% Draw bounding boxes \n";
        out += "blc setgray %fill color for bounding box \n";
        out += "blw setlinewidth\n";
        out += draw_grid_eps(mat, 1);
    }

    return out;
}

string draw_points_eps(float4x4 mat, int3 dim, const Array2d<float> &points, int2 range)
{
    string out;

    float page_size = 500.f;

    // Render the point set
    out += "% Draw points \n";
    out += "pfc setrgbcolor %fill color for points\n";

    for (int i = range.x; i < range.x + range.y; ++i)
    {
        auto v4d = mul(mat, float4{points(i, dim.x), points(i, dim.y), points(i, dim.z), 1.0f});
        auto v2d = float2{v4d.x / v4d.w, v4d.y / v4d.w} * page_size;
        out += fmt::format("{} {} p\n", v2d.x, v2d.y);
    }

    return out;
}

string header_svg(const float3 &point_color, float scale)
{
    string out;

    scale *= 0.5f;

    float page_size = 500.f;

    out += fmt::format(R"_(<svg
    width="{}px"
    height="{}px"
    viewBox="{} {} {} {}"
    xmlns="http://www.w3.org/2000/svg"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    version="1.1">
<style>
    circle
    {{
        stroke: none;
        fill: rgb({}, {}, {});
    }}

    .fine_grid
    {{
        fill: none;
        stroke-width: {}px;
        stroke: #000;
        opacity: 0.2;
    }}

    .coarse_grid
    {{
        fill: none;
        stroke-width: {}px;
        stroke: #000;
        opacity: 0.6;
    }}

    .bbox
    {{
        fill: none;
        stroke-width: {}px;
        stroke: #000;
        opacity: 1.0;
    }}
</style>
)_",
                       1000, 1000, -page_size, -page_size, 2 * page_size, 2 * page_size, int(point_color.x * 255),
                       int(point_color.y * 255), int(point_color.z * 255), 2.f * scale, 2.f * scale, 2.f * scale);
    return out;
}

string draw_grid_svg(const float4x4 &mvp, int grid_res, const string &css_class)
{
    float4 vA4d, vB4d;
    float2 vA, vB;

    float scale     = 1.f / grid_res;
    float page_size = 500.f;

    string out;

    // draw the outer border of the grid
    {
        float4 c004d = mul(mvp, float4{0.f - 0.5f, 0.f - 0.5f, 0.f, 1.f});
        float4 c104d = mul(mvp, float4{1.f - 0.5f, 0.f - 0.5f, 0.f, 1.f});
        float4 c114d = mul(mvp, float4{1.f - 0.5f, 1.f - 0.5f, 0.f, 1.f});
        float4 c014d = mul(mvp, float4{0.f - 0.5f, 1.f - 0.5f, 0.f, 1.f});
        float2 c00   = float2{c004d.x / c004d.w, c004d.y / c004d.w} * float2{page_size, -page_size};
        float2 c10   = float2{c104d.x / c104d.w, c104d.y / c104d.w} * float2{page_size, -page_size};
        float2 c11   = float2{c114d.x / c114d.w, c114d.y / c114d.w} * float2{page_size, -page_size};
        float2 c01   = float2{c014d.x / c014d.w, c014d.y / c014d.w} * float2{page_size, -page_size};

        out += fmt::format(R"(    <polygon points="{},{} {},{} {},{} {},{}" class="{}" />)", c00.x, c00.y, c10.x, c10.y,
                           c11.x, c11.y, c01.x, c01.y, css_class);
        out += "\n";
    }

    // draw the inner lines of the grid
    for (int i = 1; i <= grid_res - 1; ++i)
    {
        // draw horizontal lines
        vA4d = mul(mvp, float4{0.f - 0.5f, i * scale - 0.5f, 0.0f, 1.0f});
        vB4d = mul(mvp, float4{1.f - 0.5f, i * scale - 0.5f, 0.0f, 1.0f});
        vA   = float2(vA4d.x / vA4d.w, vA4d.y / vA4d.w) * float2{page_size, -page_size};
        vB   = float2(vB4d.x / vB4d.w, vB4d.y / vB4d.w) * float2{page_size, -page_size};
        out += fmt::format(R"(    <polygon points="{},{} {},{}" class="{}" />)", vA.x, vA.y, vB.x, vB.y, css_class);
        out += "\n";

        // draw vertical lines
        vA4d = mul(mvp, float4{i * scale - 0.5f, 0.f - 0.5f, 0.0f, 1.0f});
        vB4d = mul(mvp, float4{i * scale - 0.5f, 1.f - 0.5f, 0.0f, 1.0f});
        vA   = float2(vA4d.x / vA4d.w, vA4d.y / vA4d.w) * float2{page_size, -page_size};
        vB   = float2(vB4d.x / vB4d.w, vB4d.y / vB4d.w) * float2{page_size, -page_size};
        out += fmt::format(R"(    <polygon points="{},{} {},{}" class="{}" />)", vA.x, vA.y, vB.x, vB.y, css_class);
        out += "\n";
    }
    return out;
}

string draw_grids_svg(float4x4 mat, int fgrid_res, int cgrid_res, bool fine_grid, bool coarse_grid, bool bbox)
{
    string out;
    if (fine_grid)
        out += draw_grid_svg(mat, fgrid_res, "fine_grid");

    if (coarse_grid)
        out += draw_grid_svg(mat, cgrid_res, "coarse_grid");

    if (bbox)
        out += draw_grid_svg(mat, 1, "bbox");

    return out;
}

string draw_points_svg(float4x4 mat, int3 dim, const Array2d<float> &points, int2 range, float radius)
{
    string out;
    float  page_size = 500.f;

    for (int i = range.x; i < range.x + range.y; ++i)
    {
        auto v4d = mul(mat, float4{points(i, dim.x), points(i, dim.y), points(i, dim.z), 1.0f});
        auto v2d = float2{v4d.x / v4d.w, v4d.y / v4d.w} * float2{page_size, -page_size};
        out += fmt::format("    <circle cx=\"{}\" cy=\"{}\" r=\"{}\"/>\n", v2d.x, v2d.y, radius * 0.3f);
    }

    return out;
}

string footer_svg()
{
    return "</svg>";
}

string draw_points_csv(const Array2d<float> &points, int2 range)
{
    string out;
    for (int i = range.x; i < range.x + range.y; ++i)
    {
        for (int d = 0; d < points.sizeY(); ++d)
        {
            if (d > 0)
                out += ", ";
            out += fmt::format("{}", points(i, d));
        }
        out += "\n";
    }

    return out;
}