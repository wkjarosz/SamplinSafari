/** \file SampleViewer.h
    \author Wojciech Jarosz
*/

#include <fstream>
#include <nanogui/common.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanogui/textbox.h>
#include <thread>

#include <galois++/array2d.h>
#include <sampler/fwd.h>

#include "utils.h"

using namespace nanogui;
using namespace std;

enum PointType
{
    RANDOM = 0,
    JITTERED,
    // MULTI_JITTERED,
    MULTI_JITTERED_IP,
    // CORRELATED_MULTI_JITTERED,
    // CORRELATED_MULTI_JITTERED_3D,
    CORRELATED_MULTI_JITTERED_IP,
    CMJND,
    BOSE_OA_IP,
    BOSE_GALOIS_OA_IP,
    BUSH_OA_IP,
    BUSH_GALOIS_OA_IP,
    ADDEL_KEMP_OA_IP,
    BOSE_BUSH_OA,
    BOSE_BUSH_OA_IP,
    N_ROOKS_IP,
    SOBOL,
    ZERO_TWO,
    ZERO_TWO_SHUFFLED,
    HALTON,
    HALTON_ZAREMBA,
    HAMMERSLEY,
    HAMMERSLEY_ZAREMBA,
    LARCHER_PILLICHSHAMMER,
    NUM_POINT_TYPES
};

enum CameraType
{
    CAMERA_XY = 0,
    CAMERA_YZ,
    CAMERA_XZ,
    CAMERA_2D,
    CAMERA_CURRENT,
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
    Vector3f   eye         = Vector3f(0.0f, 0.0f, 2.0f);
    Vector3f   center      = Vector3f(0.0f, 0.0f, 0.0f);
    Vector3f   up          = Vector3f(0.0f, 1.0f, 0.0f);
    CameraType camera_type = CAMERA_CURRENT;
};

class SampleViewer : public Screen
{
public:
    SampleViewer();
    ~SampleViewer();

    bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers);
    bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers);
    bool scroll_event(const Vector2i &p, const Vector2f &rel);
    void draw_contents();
    bool keyboard_event(int key, int scancode, int action, int modifiers);

private:
    void draw_contents_EPS(ofstream &file, CameraType camera, int dimX, int dimY, int dimZ);
    void draw_contents_2D_EPS(ofstream &file);
    void update_GPU_points(bool regenerate = true);
    void update_GPU_grids();
    void set_view(CameraType view);
    void update_current_camera();
    void initialize_GUI();
    void generate_points();
    void populate_point_subset();
    void generate_grid(vector<Vector3f> &positions, int gridRes);
    void draw_text(const Vector2i &pos, const std::string &text, const Color &col = Color(1.0f, 1.0f, 1.0f, 1.0f),
                   int fontSize = 10, int fixedWidth = 0, int align = NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM) const;
    void draw_points(const Matrix4f &mvp, const Vector3f &color, bool showPointNums = false,
                     bool showPointCoords = false);
    void draw_grid(const Matrix4f &mvp, float alpha, uint32_t offset, uint32_t count);
    void draw_2D_points_and_grid(const Matrix4f &mvp, int dimX, int dimY, int plotIndex);

    // GUI elements
    Window      *m_parameters_dialog = nullptr;
    Window      *m_help_dialog       = nullptr;
    Button      *m_help_button       = nullptr;
    Slider      *m_point_count_slider, *m_point_radius_slider, *m_point_draw_count_slider, *m_jitter_slider;
    IntBox<int> *m_dimension_box, *m_x_dimension, *m_y_dimension, *m_z_dimension, *m_point_count_box,
        *m_first_draw_point_box, *m_point_draw_count_box, *m_subset_axis, *m_num_subset_levels, *m_subset_level;
    ComboBox *m_point_type_box;

    // Extra OA parameters
    Label       *m_strength_label;
    IntBox<int> *m_strength_box;
    Label       *m_offset_type_label;
    ComboBox    *m_offset_type_box;

    CheckBox *m_coarse_grid_checkbox, *m_fine_grid_checkbox, *m_bbox_grid_checkbox, *m_randomize_checkbox,
        *m_subset_by_index, *m_subset_by_coord, *m_show_1d_projections, *m_show_point_nums, *m_show_point_coords;
    Button             *m_view_btns[CAMERA_CURRENT + 1];
    Button             *m_scale_radius_with_points;
    vector<std::string> m_time_strings;

    /// X, Y, Z, and user-defined cameras
    CameraParameters m_camera[NUM_CAMERA_TYPES];

    float m_animate_start_time = 0.0f;
    bool  m_mouse_down         = false;
    int   m_num_dimensions     = 3;

    nanogui::ref<RenderPass> m_render_pass;
    nanogui::ref<Shader>     m_point_shader;
    nanogui::ref<Shader>     m_grid_shader;
    nanogui::ref<Shader>     m_point_2d_shader;
    Array2d<float>           m_points, m_subset_points;
    vector<Vector3f>         m_3d_points;
    int m_target_point_count, m_point_count, m_subset_count = 0, m_coarse_line_count, m_fine_line_count;

    vector<Sampler *> m_samplers;

    std::thread       m_gui_refresh_thread;
    std::atomic<bool> m_gui_refresh    = false;
    std::atomic<bool> m_join_requested = false;
};
