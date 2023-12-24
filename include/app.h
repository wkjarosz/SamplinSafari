/** \file SampleViewer.h
    \author Wojciech Jarosz
*/

#pragma once

#include "common.h"
#include "linalg.h"
using namespace linalg::aliases;

// define extra conversion here before including imgui, don't do it in the imconfig.h
#define IM_VEC2_CLASS_EXTRA                                                                                            \
    constexpr ImVec2(const float2 &f) : x(f.x), y(f.y)                                                                 \
    {                                                                                                                  \
    }                                                                                                                  \
    operator float2() const                                                                                            \
    {                                                                                                                  \
        return float2(x, y);                                                                                           \
    }                                                                                                                  \
    constexpr ImVec2(const int2 &i) : x(i.x), y(i.y)                                                                   \
    {                                                                                                                  \
    }                                                                                                                  \
    operator int2() const                                                                                              \
    {                                                                                                                  \
        return int2((int)x, (int)y);                                                                                   \
    }

#define IM_VEC4_CLASS_EXTRA                                                                                            \
    constexpr ImVec4(const float4 &f) : x(f.x), y(f.y), z(f.z), w(f.w)                                                 \
    {                                                                                                                  \
    }                                                                                                                  \
    operator float4() const                                                                                            \
    {                                                                                                                  \
        return float4(x, y, z, w);                                                                                     \
    }

#include "arcball.h"
#include "hello_imgui/hello_imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "shader.h"
#include <galois++/array2d.h>
#include <map>
#include <string>
#include <vector>

// class Sampler;
#include <sampler/fwd.h>

using std::map;
using std::ofstream;
using std::string;
using std::vector;

enum CameraType
{
    CAMERA_XY = 0,
    CAMERA_XZ,
    CAMERA_ZY,
    CAMERA_CURRENT,
    CAMERA_2D,
    CAMERA_PREVIOUS,
    CAMERA_NEXT,
    NUM_CAMERA_TYPES
};

struct CameraParameters
{
    Arcball    arcball;
    float      persp_factor = 0.0f;
    float      zoom = 1.0f, view_angle = 30.0f;
    float      dnear = 0.05f, dfar = 1000.0f;
    float3     eye         = float3{0.0f, 0.0f, 2.0f};
    float3     center      = float3{0.0f, 0.0f, 0.0f};
    float3     up          = float3{0.0f, 1.0f, 0.0f};
    CameraType camera_type = CAMERA_CURRENT;

    float4x4 matrix(float window_aspect) const;
};

enum TextAlign : int
{
    // Horizontal align
    TextAlign_LEFT   = 1 << 0, // Default, align text horizontally to left.
    TextAlign_CENTER = 1 << 1, // Align text horizontally to center.
    TextAlign_RIGHT  = 1 << 2, // Align text horizontally to right.
    // Vertical align
    TextAlign_TOP    = 1 << 3, // Align text vertically to top.
    TextAlign_MIDDLE = 1 << 4, // Align text vertically to middle.
    TextAlign_BOTTOM = 1 << 5, // Align text vertically to bottom.
};

class SampleViewer
{
public:
    SampleViewer();
    virtual ~SampleViewer();

    void draw_scene();
    void draw_gui();
    void run()
    {
        HelloImGui::Run(m_params);
    }

private:
    string export_XYZ_points(const string &format);
    string export_points_2d(const string &format, CameraType camera, int3 dim);
    string export_all_points_2d(const string &format);

    void update_points(bool regenerate = true);
    void set_view(CameraType view);
    void draw_editor();
    void draw_about_dialog();
    void process_hotkeys();
    void populate_point_subset();
    void draw_text(const int2 &pos, const std::string &text, const float4 &col, ImFont *font = nullptr,
                   int align = TextAlign_RIGHT | TextAlign_BOTTOM) const;
    void draw_points(const float4x4 &mvp, const float3 &color);
    void draw_trigrid(Shader *shader, const float4x4 &mvp, float alpha, const int2x3 &count);
    void draw_2D_points_and_grid(const float4x4 &mvp, int2 dims, int plotIndex);
    int2 get_draw_range() const;

    /// X, Y, Z, and user-defined cameras
    CameraParameters m_camera[NUM_CAMERA_TYPES];
    int              m_view = CAMERA_XY;

    int            m_num_dimensions = 3;
    int3           m_dimension{0, 1, 2};
    Array2d<float> m_points, m_subset_points;
    vector<float3> m_3d_points;
    int            m_target_point_count = 256, m_point_count = 256;
    int            m_subset_count = 0;

    static constexpr int            MAX_DIMENSIONS = 10;
    std::array<int, MAX_DIMENSIONS> m_custom_line_counts;

    vector<Sampler *> m_samplers;
    int               m_sampler                  = 0;
    bool              m_randomize                = false;
    float             m_jitter                   = 80.f;
    float             m_radius                   = 0.5f;
    bool              m_scale_radius_with_points = true;
    bool              m_show_1d_projections = false, m_show_point_nums = false, m_show_point_coords = false,
         m_show_coarse_grid = false, m_show_fine_grid = false, m_show_custom_grid = false, m_show_bbox = false;

    Shader *m_point_shader = nullptr, *m_2d_point_shader = nullptr, *m_grid_shader = nullptr;

    int2  m_viewport_pos, m_viewport_pos_GL, m_viewport_size;
    float m_animate_start_time = 0.0f;

    bool m_subset_by_index   = false;
    int  m_first_draw_point  = 0;
    int  m_point_draw_count  = 1;
    bool m_subset_by_coord   = false;
    int  m_subset_axis       = 0;
    int  m_num_subset_levels = 1;
    int  m_subset_level      = 0;

    bool m_gpu_points_dirty = true, m_cpu_points_dirty = true;

    map<int, ImFont *> m_regular, m_bold; // regular and bold fonts at various sizes

    float                    m_time1 = 0.f, m_time2 = 0.f;
    float3                   m_point_color = {0.9f, 0.55f, 0.1f};
    float3                   m_bg_color    = {0.0f, 0.0f, 0.0f};
    HelloImGui::RunnerParams m_params;

    bool m_idling_backup = false;
};
