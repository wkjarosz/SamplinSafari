/** \file export_to_file.h
    \author Wojciech Jarosz
*/

#pragma once

#include "linalg.h"
#include <galois++/array2d.h>
#include <string>
using namespace linalg::aliases;

// encapsulated postscript file
std::string header_eps(const float3 &point_color, float radius);
std::string footer_eps();
std::string draw_grid_eps(const float4x4 &mvp, int grid_res);
std::string draw_grids_eps(float4x4 mat, int fgrid_res, int cgrid_res, bool fine_grid, bool coarse_grid, bool bbox);
std::string draw_points_eps(float4x4 mat, int3 dim, const Array2d<float> &points, int2 range);

// scalable vector graphics file
std::string header_svg(const float3 &point_color);
std::string footer_svg();
std::string draw_grid_svg(const float4x4 &mvp, int grid_res, const std::string &css_class);
std::string draw_grids_svg(float4x4 mat, int fgrid_res, int cgrid_res, bool fine_grid, bool coarse_grid, bool bbox);
std::string draw_points_svg(float4x4 mat, int3 dim, const Array2d<float> &points, int2 range, float radius);

// comma separated value file
std::string draw_points_csv(const Array2d<float> &points, int2 range);
