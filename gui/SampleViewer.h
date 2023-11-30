/** \file SampleViewer.h
    \author Wojciech Jarosz
*/

#pragma once

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
#include "gui_app.h"
#include "shader.h"
#include <galois++/array2d.h>
#include <map>
#include <sampler/fwd.h>
#include <string>
#include <vector>

using std::map;
using std::ofstream;
using std::string;
using std::vector;

enum CameraType
{
    CAMERA_XY = 0,
    CAMERA_YZ,
    CAMERA_XZ,
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
    float      zoom = 1.0f, view_angle = 35.0f;
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

class SampleViewer : public GUIApp
{
public:
    SampleViewer();
    virtual ~SampleViewer();

    void mouse_motion_event(const float2 &pos);

    void initialize_GL() override;
    void draw() override;
    void draw_gui();

    bool mouse_button_event(const int2 &p, int button, bool down, int modifiers) override;
    bool mouse_motion_event(const int2 &p, const int2 &rel, int button, int modifiers) override;
    bool scroll_event(const int2 &p, const float2 &rel) override;

    // bool keyboard_event(int key, int scancode, int action, int modifiers) override;
    // bool resize_event(const int2 &size) override;
    // bool focus_event(bool focused) override;
    // bool maximize_event(bool maximized) override;

private:
    string export_XYZ_points(const string &format);
    string export_points_2d(const string &format, CameraType camera, int3 dim);
    string export_all_points_2d(const string &format);

    void update_GPU_points(bool regenerate = true);
    void update_GPU_grids();
    void set_view(CameraType view);
    void update_current_camera();
    void draw_editor();
    void process_hotkeys();
    void generate_points();
    void populate_point_subset();
    void generate_grid(vector<float3> &positions, int gridRes);
    void draw_text(const int2 &pos, const std::string &text, const float4 &col, ImFont *font = nullptr,
                   int align = TextAlign_RIGHT | TextAlign_BOTTOM) const;
    void draw_points(const float4x4 &mvp, const float3 &color);
    void draw_grid(const float4x4 &mvp, float alpha, uint32_t offset, uint32_t count);
    void draw_2D_points_and_grid(const float4x4 &mvp, int dimX, int dimY, int plotIndex);
    void clear_and_setup_viewport();
    int2 get_draw_range() const;

    /// X, Y, Z, and user-defined cameras
    CameraParameters m_camera[NUM_CAMERA_TYPES];
    int              m_view = CAMERA_XY;

    int            m_num_dimensions = 3;
    int3           m_dimension{0, 1, 2};
    Array2d<float> m_points, m_subset_points;
    vector<float3> m_3d_points;
    int            m_target_point_count = 256, m_point_count = 256;
    int            m_subset_count = 0, m_coarse_line_count, m_fine_line_count;

    vector<Sampler *> m_samplers;
    int               m_sampler                  = 0;
    bool              m_randomize                = false;
    float             m_jitter                   = 80.f;
    float             m_radius                   = 0.5f;
    bool              m_scale_radius_with_points = true;
    bool              m_show_1d_projections = false, m_show_point_nums = false, m_show_point_coords = false,
         m_show_coarse_grid = false, m_show_fine_grid = false, m_show_bbox = false;

    Shader *m_point_shader = nullptr, *m_grid_shader = nullptr, *m_point_2d_shader = nullptr;

    int2  m_viewport_pos, m_viewport_pos_GL, m_viewport_size;
    float m_animate_start_time = 0.0f;
    bool  m_mouse_down         = false;

    bool m_subset_by_index   = false;
    int  m_first_draw_point  = 0;
    int  m_point_draw_count  = 1;
    bool m_subset_by_coord   = false;
    int  m_subset_axis       = 0;
    int  m_num_subset_levels = 1;
    int  m_subset_level      = 0;

    map<int, ImFont *> m_regular, m_bold; // regular and bold fonts at various sizes

    float  m_time1 = 0.f, m_time2 = 0.f;
    float3 m_point_color = {0.9f, 0.55f, 0.1f};
};
