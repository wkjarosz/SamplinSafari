/** \file SampleViewer.cpp
    \author Wojciech Jarosz
*/
#define _USE_MATH_DEFINES
#include "SampleViewer.h"

#include <nanogui/checkbox.h>
#include <nanogui/combobox.h>
#include <nanogui/icons.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/messagedialog.h>
#include <nanogui/renderpass.h>
#include <nanogui/screen.h>
#include <nanogui/shader.h>
#include <nanogui/slider.h>
#include <nanogui/textbox.h>
#include <nanogui/theme.h>
#include <tinyformat.h>

#include "Well.h"
#include <sampler/Halton.h>
#include <sampler/Hammersley.h>
#include <sampler/Jittered.h>
#include <sampler/LP.h>
#include <sampler/Misc.h>
#include <sampler/MultiJittered.h>
#include <sampler/NRooks.h>
#include <sampler/OA.h>
#include <sampler/OAAddelmanKempthorne.h>
#include <sampler/OABoseBush.h>
#include <sampler/OABush.h>
#include <sampler/OACMJND.h>
#include <sampler/Random.h>
#include <sampler/Sobol.h>

namespace
{

string pointVertexShader =
    R"(
    #version 330
    uniform mat4 mvp;
    uniform float pointSize;
    in vec3 position;
    void main()
    {
        gl_Position = mvp * vec4(position, 1.0);
        gl_PointSize = pointSize;
    }
)";

string pointFragmentShader =
    R"(
    #version 330
    uniform vec3 color;
    uniform float pointSize;
    out vec4 out_color;
    void main()
    {
        float alpha = 1.0;
        if (pointSize > 3)
        {
            vec2 circCoord = 2.0 * gl_PointCoord - 1.0;
            float radius2 = dot(circCoord, circCoord);
            if (radius2 > 1.0)
                discard;
            alpha = 1.0 - smoothstep(1.0 - 2.0/pointSize, 1.0, sqrt(radius2));
        }
        out_color = vec4(color, alpha);
    }
)";

string gridVertexShader =
    R"(
    #version 330
    uniform mat4 mvp;
    in vec3 position;
    void main()
    {
        gl_Position = mvp * vec4(position, 1.0);
    }
)";

string gridFragmentShader =
    R"(
    #version 330
    out vec4 out_color;
    uniform float alpha;
    void main()
    {
        out_color = vec4(vec3(1.0), alpha);
    }
)";

void computeCameraMatrices(Matrix4f &model, Matrix4f &lookat, Matrix4f &view, Matrix4f &proj, const CameraParameters &c,
                           float window_aspect);
void drawEPSGrid(const Matrix4f &mvp, int grid_res, ofstream &file);
void writeEPSPoints(const Array2D<float> &points, int start, int count, const Matrix4f &mvp, ofstream &file, int dim_x,
                    int dim_y, int dim_z);

float mapCount2Slider(int count)
{
    return (log((float)count) / log(2.f)) / 20;
}

int mapSlider2Count(float sliderValue)
{
    return (int)pow(2.f, round(80 * sliderValue) / 4.0f);
}

// float mapRadius2Slider(float radius)
// {
//     return sqrt((radius - 1)/32.0f);
// }
float map_slider_to_radius(float sliderValue)
{
    return sliderValue * sliderValue * 32.0f + 1.0f;
}

void layout_2d_matrix(Matrix4f &position, int numDims, int dim_x, int dim_y)
{
    float cellSpacing = 1.f / (numDims - 1);
    float cellSize    = 0.96f / (numDims - 1);

    float xOffset = dim_x - (numDims - 2) / 2.0f;
    float yOffset = -(dim_y - 1 - (numDims - 2) / 2.0f);

    position = Matrix4f::translate(Vector3f(cellSpacing * xOffset, cellSpacing * yOffset, 1)) *
               Matrix4f::scale(Vector3f(cellSize, cellSize, 1));
}

} // namespace

SampleViewer::SampleViewer() : Screen(Vector2i(800, 600), "Samplin' Safari")
{
    m_time_strings.resize(3);

    m_samplers.resize(NUM_POINT_TYPES, nullptr);
    m_samplers[RANDOM]   = new Random(m_num_dimensions);
    m_samplers[JITTERED] = new Jittered(1, 1, 1.0f);
    // m_samplers[MULTI_JITTERED]             = new MultiJittered(1, 1, false, 0.0f);
    m_samplers[MULTI_JITTERED_IP] = new MultiJitteredInPlace(1, 1, false, 0.0f);
    // m_samplers[CORRELATED_MULTI_JITTERED]  = new CorrelatedMultiJittered(1, 1, false, 0.0f);
    m_samplers[CORRELATED_MULTI_JITTERED_IP] = new CorrelatedMultiJitteredInPlace(1, 1, m_num_dimensions, false, 0.0f);
    m_samplers[CMJND]                        = new CMJNDInPlace(1, 3, MJ_STYLE, false, 0.8);
    m_samplers[BOSE_OA_IP]                   = new BoseOAInPlace(1, MJ_STYLE, false, 0.8f, m_num_dimensions);
    m_samplers[BOSE_GALOIS_OA_IP]            = new BoseGaloisOAInPlace(1, MJ_STYLE, false, 0.8f, m_num_dimensions);
    m_samplers[BUSH_OA_IP]                   = new BushOAInPlace(1, 3, MJ_STYLE, false, 0.8f, m_num_dimensions);
    m_samplers[BUSH_GALOIS_OA_IP]            = new BushGaloisOAInPlace(1, 3, MJ_STYLE, false, 0.8f, m_num_dimensions);
    m_samplers[ADDEL_KEMP_OA_IP]       = new AddelmanKempthorneOAInPlace(2, MJ_STYLE, false, 0.8f, m_num_dimensions);
    m_samplers[BOSE_BUSH_OA]           = new BoseBushOA(2, MJ_STYLE, false, 0.8f, m_num_dimensions);
    m_samplers[BOSE_BUSH_OA_IP]        = new BoseBushOAInPlace(2, MJ_STYLE, false, 0.8f, m_num_dimensions);
    m_samplers[N_ROOKS_IP]             = new NRooksInPlace(m_num_dimensions, 1, false, 1.0f);
    m_samplers[SOBOL]                  = new Sobol(m_num_dimensions);
    m_samplers[ZERO_TWO]               = new ZeroTwo(1, m_num_dimensions, false);
    m_samplers[ZERO_TWO_SHUFFLED]      = new ZeroTwo(1, m_num_dimensions, true);
    m_samplers[HALTON]                 = new Halton(m_num_dimensions);
    m_samplers[HALTON_ZAREMBA]         = new HaltonZaremba(m_num_dimensions);
    m_samplers[HAMMERSLEY]             = new Hammersley<Halton>(m_num_dimensions, 1);
    m_samplers[HAMMERSLEY_ZAREMBA]     = new Hammersley<HaltonZaremba>(m_num_dimensions, 1);
    m_samplers[LARCHER_PILLICHSHAMMER] = new LarcherPillichshammerGK(3, 1, false);

    m_camera[CAMERA_XY].arcball.setState({0, 0, 0, 1});
    m_camera[CAMERA_XY].persp_factor = 0.0f;
    m_camera[CAMERA_XY].camera_type  = CAMERA_XY;

    m_camera[CAMERA_YZ].arcball.setState(rotation_quat({0.f, -1.f, 0.f}, 0.5f * M_PI));
    m_camera[CAMERA_YZ].persp_factor = 0.0f;
    m_camera[CAMERA_YZ].camera_type  = CAMERA_YZ;

    m_camera[CAMERA_XZ].arcball.setState(rotation_quat({1.f, 0.f, 0.f}, 0.5f * M_PI));
    m_camera[CAMERA_XZ].persp_factor = 0.0f;
    m_camera[CAMERA_XZ].camera_type  = CAMERA_XZ;

    m_camera[CAMERA_2D]      = m_camera[CAMERA_XY];
    m_camera[CAMERA_CURRENT] = m_camera[CAMERA_XY];
    m_camera[CAMERA_NEXT]    = m_camera[CAMERA_XY];

    // initialize the GUI elements
    initialize_GUI();

    //
    // OpenGL setup
    //

    // glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    m_render_pass = new RenderPass({this});
    m_render_pass->set_clear_color(0, Color(0.f, 0.f, 0.f, 1.f));

    m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);

    m_point_shader = new Shader(m_render_pass, "Point shader", pointVertexShader, pointFragmentShader,
                                Shader::BlendMode::AlphaBlend);

    m_grid_shader =
        new Shader(m_render_pass, "Grid shader", gridVertexShader, gridFragmentShader, Shader::BlendMode::AlphaBlend);

    m_point_2d_shader = new Shader(m_render_pass, "Point shader 2D", pointVertexShader, pointFragmentShader,
                                   Shader::BlendMode::AlphaBlend);

    update_GPU_points();
    update_GPU_grids();
    set_visible(true);
    resize_event(m_size);

    // Nanogui will redraw the screen for key/mouse events, but we need to manually
    // invoke redraw for things like gui animations. do this in a separate thread
    m_gui_refresh_thread = std::thread(
        [this]
        {
            std::chrono::microseconds idle_quantum = std::chrono::microseconds(1000 * 1000 / 15);
            std::chrono::microseconds anim_quantum = std::chrono::microseconds(1000 * 1000 / 120);
            while (true)
            {
                if (m_join_requested)
                    return;
                std::this_thread::sleep_for(m_gui_refresh ? anim_quantum : idle_quantum);
                this->m_redraw = false;
                this->redraw();
            }
        });
}

SampleViewer::~SampleViewer()
{
    m_join_requested = true;

    for (size_t i = 0; i < m_samplers.size(); ++i)
        delete m_samplers[i];
    m_gui_refresh_thread.join();
}

void SampleViewer::initialize_GUI()
{
    set_resize_callback(
        [&](Vector2i size)
        {
            for (int i = 0; i < NUM_CAMERA_TYPES; ++i)
                m_camera[i].arcball.setSize(size);

            if (m_help_dialog->visible())
                m_help_dialog->center();

            perform_layout();
        });

    auto thm                     = new Theme(m_nvg_context);
    thm->m_standard_font_size    = 14;
    thm->m_button_font_size      = 14;
    thm->m_text_box_font_size    = 14;
    thm->m_window_corner_radius  = 4;
    thm->m_window_fill_unfocused = Color(60, 250);
    thm->m_window_fill_focused   = Color(65, 250);
    thm->m_button_corner_radius  = 2;
    set_theme(thm);
    auto createCollapsableSection = [this](Widget *parent, const string &title, bool collapsed = false)
    {
        auto group = new Widget(parent);
        group->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 0));

        auto btn =
            new Button(group, title, !collapsed ? m_theme->m_text_box_down_icon : m_theme->m_popup_chevron_left_icon);
        btn->set_flags(Button::ToggleButton);
        btn->set_pushed(!collapsed);
        btn->set_fixed_size(Vector2i(180, 22));
        btn->set_font_size(16);
        btn->set_icon_position(Button::IconPosition::Right);

        auto panel = new Well(group);
        panel->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));
        panel->set_visible(!collapsed);

        btn->set_change_callback(
            [this, btn, panel](bool value)
            {
                btn->set_icon(value ? m_theme->m_text_box_down_icon : m_theme->m_popup_chevron_left_icon);
                panel->set_visible(value);
                this->perform_layout(this->m_nvg_context);
            });
        return panel;
    };

    // create parameters dialog
    {
        m_parameters_dialog = new Window(this, "Sample parameters");
        m_parameters_dialog->set_theme(thm);
        m_parameters_dialog->set_position(Vector2i(15, 15));
        m_parameters_dialog->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));

        m_help_button = new Button(m_parameters_dialog->button_panel(), "", FA_QUESTION);
        m_help_button->set_flags(Button::ToggleButton);
        m_help_button->set_change_callback(
            [&](bool b)
            {
                m_help_dialog->set_visible(b);
                if (b)
                {
                    m_help_dialog->center();
                    m_help_dialog->request_focus();
                }
            });

        {
            auto section = createCollapsableSection(m_parameters_dialog, "Point set");

            vector<string> pointTypeNames(NUM_POINT_TYPES);
            for (int i = 0; i < NUM_POINT_TYPES; ++i)
                pointTypeNames[i] = m_samplers[i]->name();
            m_point_type_box = new ComboBox(section, pointTypeNames);
            m_point_type_box->set_theme(thm);
            m_point_type_box->set_fixed_height(20);
            m_point_type_box->set_fixed_width(170);
            m_point_type_box->set_callback(
                [&](int)
                {
                    Sampler         *sampler = m_samplers[m_point_type_box->selected_index()];
                    OrthogonalArray *oa      = dynamic_cast<OrthogonalArray *>(sampler);
                    m_strength_label->set_visible(oa);
                    m_strength_box->set_visible(oa);
                    m_strength_box->set_enabled(oa);
                    m_offset_type_label->set_visible(oa);
                    m_offset_type_box->set_visible(oa);
                    m_offset_type_box->set_enabled(oa);
                    m_jitter_slider->set_value(sampler->jitter());
                    if (oa)
                    {
                        m_offset_type_box->set_selected_index(oa->offsetType());
                        m_strength_box->set_value(oa->strength());
                    }
                    this->perform_layout();
                    update_GPU_points();
                    update_GPU_grids();
                });
            m_point_type_box->set_tooltip("Key: Up/Down");

            auto panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            auto l = new Label(panel, "Num points", "sans");
            l->set_fixed_width(100);
            m_point_count_box = new IntBox<int>(panel);
            m_point_count_box->set_editable(true);
            m_point_count_box->set_fixed_size(Vector2i(70, 20));
            m_point_count_box->set_alignment(TextBox::Alignment::Right);
            m_point_count_box->set_spinnable(true);
            m_point_count_box->set_callback(
                [&](int v)
                {
                    m_target_point_count = v;
                    update_GPU_points();
                    update_GPU_grids();
                });

            // Add a slider and set defaults
            m_point_count_slider = new Slider(section);
            m_point_count_slider->set_callback(
                [&](float v)
                {
                    m_target_point_count = mapSlider2Count(v);
                    update_GPU_points();
                    update_GPU_grids();
                });
            m_point_count_slider->set_value(6.f / 15.f);
            // m_point_count_slider->set_fixed_width(100);
            m_point_count_slider->set_tooltip("Key: Left/Right");
            m_target_point_count = m_point_count = mapSlider2Count(m_point_count_slider->value());

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Dimensions", "sans");
            l->set_fixed_width(100);
            m_dimension_box = new IntBox<int>(panel);
            m_dimension_box->set_editable(true);
            m_dimension_box->set_fixed_size(Vector2i(70, 20));
            m_dimension_box->set_value(3);
            m_dimension_box->set_alignment(TextBox::Alignment::Right);
            m_dimension_box->set_default_value("3");
            m_dimension_box->set_format("[2-9]*");
            m_dimension_box->set_spinnable(true);
            m_dimension_box->set_min_value(2);
            // m_dimension_box->set_max_value(32);
            m_dimension_box->set_value_increment(1);
            m_dimension_box->set_tooltip("Key: D/d");
            m_dimension_box->set_callback(
                [&](int v)
                {
                    m_num_dimensions = v;
                    m_subset_axis->set_max_value(v - 1);
                    m_x_dimension->set_max_value(v - 1);
                    m_y_dimension->set_max_value(v - 1);
                    m_z_dimension->set_max_value(v - 1);

                    update_GPU_points();
                    update_GPU_grids();
                });

            m_randomize_checkbox = new CheckBox(section, "Randomize");
            m_randomize_checkbox->set_checked(true);
            m_randomize_checkbox->set_callback([&](bool) { update_GPU_points(); });

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Jitter", "sans");
            l->set_fixed_width(100);
            // Add a slider for the point radius
            m_jitter_slider = new Slider(panel);
            m_jitter_slider->set_value(0.5f);
            m_jitter_slider->set_fixed_width(70);
            m_jitter_slider->set_callback(
                [&](float j)
                {
                    m_samplers[m_point_type_box->selected_index()]->setJitter(j);
                    update_GPU_points();
                    update_GPU_grids();
                });

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            m_strength_label = new Label(panel, "Strength", "sans");
            m_strength_label->set_fixed_width(100);
            m_strength_label->set_visible(false);
            m_strength_box = new IntBox<int>(panel);
            m_strength_box->set_editable(true);
            m_strength_box->set_enabled(false);
            m_strength_box->set_visible(false);
            m_strength_box->set_fixed_size(Vector2i(70, 20));
            m_strength_box->set_value(2);
            m_strength_box->set_alignment(TextBox::Alignment::Right);
            m_strength_box->set_default_value("2");
            m_strength_box->set_format("[2-9]*");
            m_strength_box->set_spinnable(true);
            m_strength_box->set_min_value(2);
            // m_strength_box->set_max_value(32);
            m_strength_box->set_value_increment(1);
            // m_strength_box->set_tooltip("Key: D/d");
            m_strength_box->set_callback(
                [&](int v)
                {
                    if (OrthogonalArray *oa =
                            dynamic_cast<OrthogonalArray *>(m_samplers[m_point_type_box->selected_index()]))
                    {
                        m_strength_box->set_value(oa->setStrength(v));
                    }
                    update_GPU_points();
                    update_GPU_grids();
                });

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            m_offset_type_label = new Label(panel, "Offset type", "sans");
            m_offset_type_label->set_fixed_width(100);
            m_offset_type_label->set_visible(false);
            OrthogonalArray *oa = dynamic_cast<OrthogonalArray *>(m_samplers[BOSE_OA_IP]);
            m_offset_type_box   = new ComboBox(panel, oa->offsetTypeNames());
            m_offset_type_box->set_visible(false);
            m_offset_type_box->set_theme(thm);
            m_offset_type_box->set_fixed_size(Vector2i(70, 20));
            m_offset_type_box->set_callback(
                [&](int ot)
                {
                    if (OrthogonalArray *oa =
                            dynamic_cast<OrthogonalArray *>(m_samplers[m_point_type_box->selected_index()]))
                    {
                        oa->setOffsetType(ot);
                    }
                    update_GPU_points();
                    update_GPU_grids();
                });
            m_offset_type_box->set_tooltip("Key: Shift+Up/Down");

            auto epsBtn = new Button(section, "Save as EPS", FA_SAVE);
            epsBtn->set_fixed_height(22);
            epsBtn->set_callback(
                [&]
                {
                    try
                    {
                        string basename = file_dialog({{"", "Base filename"}}, true);
                        if (!basename.size())
                            return;

                        ofstream fileAll(basename + "_all2D.eps");
                        draw_contents_2D_EPS(fileAll);

                        ofstream fileXYZ(basename + "_012.eps");
                        draw_contents_EPS(fileXYZ, CAMERA_CURRENT, m_x_dimension->value(), m_y_dimension->value(),
                                          m_z_dimension->value());

                        for (int y = 0; y < m_num_dimensions; ++y)
                            for (int x = 0; x < y; ++x)
                            {
                                ofstream fileXY(tfm::format("%s_%d%d.eps", basename, x, y));
                                draw_contents_EPS(fileXY, CAMERA_XY, x, y, 2);
                            }
                    }
                    catch (const std::exception &e)
                    {
                        new MessageDialog(this, MessageDialog::Type::Warning, "Error",
                                          "An error occurred: " + std::string(e.what()));
                    }
                });
        }

        {
            auto section = createCollapsableSection(m_parameters_dialog, "Camera/view:");

            auto panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 2));

            m_view_btns[CAMERA_XY] = new Button(panel, "XY");
            m_view_btns[CAMERA_XY]->set_flags(Button::RadioButton);
            m_view_btns[CAMERA_XY]->set_pushed(true);
            m_view_btns[CAMERA_XY]->set_callback([&] { set_view(CAMERA_XY); });
            m_view_btns[CAMERA_XY]->set_tooltip("Key: 1");
            m_view_btns[CAMERA_XY]->set_fixed_size(Vector2i(30, 22));

            m_view_btns[CAMERA_YZ] = new Button(panel, "YZ");
            m_view_btns[CAMERA_YZ]->set_flags(Button::RadioButton);
            m_view_btns[CAMERA_YZ]->set_callback([&] { set_view(CAMERA_YZ); });
            m_view_btns[CAMERA_YZ]->set_tooltip("Key: 2");
            m_view_btns[CAMERA_YZ]->set_fixed_size(Vector2i(30, 22));

            m_view_btns[CAMERA_XZ] = new Button(panel, "XZ");
            m_view_btns[CAMERA_XZ]->set_flags(Button::RadioButton);
            m_view_btns[CAMERA_XZ]->set_callback([&] { set_view(CAMERA_XZ); });
            m_view_btns[CAMERA_XZ]->set_tooltip("Key: 3");
            m_view_btns[CAMERA_XZ]->set_fixed_size(Vector2i(30, 22));

            m_view_btns[CAMERA_CURRENT] = new Button(panel, "XYZ");
            m_view_btns[CAMERA_CURRENT]->set_flags(Button::RadioButton);
            m_view_btns[CAMERA_CURRENT]->set_callback([&] { set_view(CAMERA_CURRENT); });
            m_view_btns[CAMERA_CURRENT]->set_tooltip("Key: 4");
            m_view_btns[CAMERA_CURRENT]->set_fixed_size(Vector2i(35, 22));

            m_view_btns[CAMERA_2D] = new Button(panel, "2D");
            m_view_btns[CAMERA_2D]->set_flags(Button::RadioButton);
            m_view_btns[CAMERA_2D]->set_callback([&] { set_view(CAMERA_2D); });
            m_view_btns[CAMERA_2D]->set_tooltip("Key: 0");
            m_view_btns[CAMERA_2D]->set_fixed_size(Vector2i(30, 22));
        }

        // Display options panel
        {
            auto displayPanel = createCollapsableSection(m_parameters_dialog, "Display options");

            auto panel = new Widget(displayPanel);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            new Label(panel, "Radius", "sans");
            // Add a slider for the point radius
            m_point_radius_slider = new Slider(panel);
            m_point_radius_slider->set_value(0.5f);
            m_point_radius_slider->set_fixed_width(115);
            m_point_radius_slider->set_callback([&](float v) { draw_contents(); });

            m_scale_radius_with_points = new Button(panel, "", FA_COMPRESS);
            m_scale_radius_with_points->set_flags(Button::ToggleButton);
            m_scale_radius_with_points->set_pushed(true);
            m_scale_radius_with_points->set_fixed_size(Vector2i(19, 19));
            m_scale_radius_with_points->set_change_callback([this](bool) { draw_contents(); });
            m_scale_radius_with_points->set_tooltip("Scale radius with number of points");

            m_show_1d_projections = new CheckBox(displayPanel, "1D projections");
            m_show_1d_projections->set_checked(false);
            m_show_1d_projections->set_tooltip("Key: P");
            m_show_1d_projections->set_callback([this](bool) { draw_contents(); });

            m_show_point_nums = new CheckBox(displayPanel, "Point indices");
            m_show_point_nums->set_checked(false);
            m_show_point_nums->set_callback([this](bool) { draw_contents(); });

            m_show_point_coords = new CheckBox(displayPanel, "Point coords");
            m_show_point_coords->set_checked(false);
            m_show_point_coords->set_callback([this](bool) { draw_contents(); });

            m_coarse_grid_checkbox = new CheckBox(displayPanel, "Coarse grid");
            m_coarse_grid_checkbox->set_checked(false);
            m_coarse_grid_checkbox->set_tooltip("Key: g");
            m_coarse_grid_checkbox->set_callback([this](bool) { update_GPU_grids(); });

            m_fine_grid_checkbox = new CheckBox(displayPanel, "Fine grid");
            m_fine_grid_checkbox->set_tooltip("Key: G");
            m_fine_grid_checkbox->set_callback([this](bool) { update_GPU_grids(); });

            m_bbox_grid_checkbox = new CheckBox(displayPanel, "Bounding box");
            m_bbox_grid_checkbox->set_tooltip("Key: B");
            m_bbox_grid_checkbox->set_callback([this](bool) { update_GPU_grids(); });
        }

        {
            auto section = createCollapsableSection(m_parameters_dialog, "Dimension mapping", true);
            // new Label(displayPanel, "Dimension mapping:", "sans-bold");

            auto panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));
            auto createDimMapping = [this, panel](IntBox<int> *&box, const string &label, int dim)
            {
                auto group = new Widget(panel);
                group->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

                new Label(group, label, "sans");
                box = new IntBox<int>(group);
                box->set_editable(true);
                box->set_value(dim);
                box->set_alignment(TextBox::Alignment::Right);
                // box->set_fixed_width(30);
                box->set_default_value(to_string(dim));
                box->set_spinnable(true);
                box->set_min_value(0);
                box->set_max_value(m_num_dimensions - 1);
                box->set_value_increment(1);
                box->set_callback([&](int v) { update_GPU_points(false); });
            };
            createDimMapping(m_x_dimension, "X:", 0);
            createDimMapping(m_y_dimension, "Y:", 1);
            createDimMapping(m_z_dimension, "Z:", 2);
        }

        // point subset controls
        {
            auto section = createCollapsableSection(m_parameters_dialog, "Visible subset", true);

            m_subset_by_index = new CheckBox(section, "Subset by point index");
            m_subset_by_index->set_checked(false);
            m_subset_by_index->set_callback(
                [&](bool b)
                {
                    m_first_draw_point_box->set_enabled(b);
                    m_point_draw_count_box->set_enabled(b);
                    m_point_draw_count_slider->set_enabled(b);
                    if (b)
                        m_subset_by_coord->set_checked(false);
                    update_GPU_points(false);
                    draw_contents();
                });

            // Create an empty panel with a horizontal layout
            auto panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            auto l = new Label(panel, "First point", "sans");
            l->set_fixed_width(100);
            m_first_draw_point_box = new IntBox<int>(panel);
            m_first_draw_point_box->set_editable(true);
            m_first_draw_point_box->set_enabled(m_subset_by_index->checked());
            m_first_draw_point_box->set_fixed_size(Vector2i(70, 20));
            m_first_draw_point_box->set_value(0);
            m_first_draw_point_box->set_alignment(TextBox::Alignment::Right);
            m_first_draw_point_box->set_default_value("0");
            m_first_draw_point_box->set_spinnable(true);
            m_first_draw_point_box->set_min_value(0);
            m_first_draw_point_box->set_max_value(m_point_count - 1);
            m_first_draw_point_box->set_value_increment(1);
            m_first_draw_point_box->set_callback(
                [&](int v)
                {
                    m_point_draw_count_box->set_max_value(m_point_count - m_first_draw_point_box->value());
                    m_point_draw_count_box->set_value(m_point_draw_count_box->value());
                });

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Num points", "sans");
            l->set_fixed_width(100);
            m_point_draw_count_box = new IntBox<int>(panel);
            m_point_draw_count_box->set_editable(true);
            m_point_draw_count_box->set_enabled(m_subset_by_index->checked());
            m_point_draw_count_box->set_fixed_size(Vector2i(70, 20));
            m_point_draw_count_box->set_value(m_point_count - m_first_draw_point_box->value());
            m_point_draw_count_box->set_alignment(TextBox::Alignment::Right);
            m_point_draw_count_box->set_default_value("0");
            m_point_draw_count_box->set_spinnable(true);
            m_point_draw_count_box->set_min_value(0);
            m_point_draw_count_box->set_max_value(m_point_count - m_first_draw_point_box->value());
            m_point_draw_count_box->set_value_increment(1);

            m_point_draw_count_slider = new Slider(section);
            m_point_draw_count_slider->set_enabled(m_subset_by_index->checked());
            m_point_draw_count_slider->set_value(1.0f);
            m_point_draw_count_slider->set_callback(
                [&](float v)
                { m_point_draw_count_box->set_value(round((m_point_count - m_first_draw_point_box->value()) * v)); });

            m_subset_by_coord = new CheckBox(section, "Subset by coordinate");
            m_subset_by_coord->set_checked(false);
            m_subset_by_coord->set_callback(
                [&](bool b)
                {
                    m_subset_axis->set_enabled(b);
                    m_num_subset_levels->set_enabled(b);
                    m_subset_level->set_enabled(b);
                    if (b)
                        m_subset_by_index->set_checked(false);
                    update_GPU_points(false);
                    draw_contents();
                });

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Subset axis", "sans");
            l->set_fixed_width(100);
            m_subset_axis = new IntBox<int>(panel);
            m_subset_axis->set_editable(true);
            m_subset_axis->set_enabled(m_subset_by_coord->checked());
            m_subset_axis->set_fixed_size(Vector2i(70, 20));
            m_subset_axis->set_value(0);
            m_subset_axis->set_alignment(TextBox::Alignment::Right);
            m_subset_axis->set_default_value("0");
            m_subset_axis->set_spinnable(true);
            m_subset_axis->set_min_value(0);
            m_subset_axis->set_max_value(m_num_dimensions - 1);
            m_subset_axis->set_value_increment(1);
            m_subset_axis->set_callback([&](int v) { update_GPU_points(false); });

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Num levels", "sans");
            l->set_fixed_width(100);
            m_num_subset_levels = new IntBox<int>(panel);
            m_num_subset_levels->set_editable(true);
            m_num_subset_levels->set_enabled(m_subset_by_coord->checked());
            m_num_subset_levels->set_fixed_size(Vector2i(70, 20));
            m_num_subset_levels->set_value(2);
            m_num_subset_levels->set_alignment(TextBox::Alignment::Right);
            m_num_subset_levels->set_default_value("2");
            m_num_subset_levels->set_spinnable(true);
            m_num_subset_levels->set_min_value(1);
            m_num_subset_levels->set_max_value(m_point_count);
            m_num_subset_levels->set_value_increment(1);
            m_num_subset_levels->set_callback(
                [&](int v)
                {
                    m_subset_level->set_max_value(v - 1);
                    update_GPU_points(false);
                });

            panel = new Widget(section);
            panel->set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Level", "sans");
            l->set_fixed_width(100);
            m_subset_level = new IntBox<int>(panel);
            m_subset_level->set_editable(true);
            m_subset_level->set_enabled(m_subset_by_coord->checked());
            m_subset_level->set_fixed_size(Vector2i(70, 20));
            m_subset_level->set_value(0);
            m_subset_level->set_alignment(TextBox::Alignment::Right);
            m_subset_level->set_default_value("0");
            m_subset_level->set_spinnable(true);
            m_subset_level->set_min_value(0);
            m_subset_level->set_max_value(m_num_subset_levels->value() - 1);
            m_subset_level->set_value_increment(1);
            m_subset_level->set_callback([&](int v) { update_GPU_points(false); });
        }
    }

    //
    // create help dialog
    //
    {
        vector<pair<string, string>> helpStrings = {
            {"h", "Toggle this help panel"},
            {"LMB", "Rotate camera"},
            {"Scroll", "Zoom camera"},
            {"1/2/3", "Axis-aligned orthographic projections"},
            {"4", "Perspective view"},
            {"0", "Grid of all 2D projections"},
            {"Left", "Decrease point count (+Shift: bigger increment)"},
            {"Right", "Increase point count (+Shift: bigger increment)"},
            {"Up/Down", "Cycle through samplers"},
            {"Shift+Up/Down", "Cycle through offset types (for OA samplers)"},
            {"d/D", "Decrease/Increase maximum dimension"},
            {"t/T", "Decrease/Increase the strength (for OA samplers)"},
            {"r/R", "Toggle/re-seed randomization"},
            {"g/G", "Toggle coarse/fine grid"},
            {"P", "Toggle 1D projections"},
            {"Space", "Toggle controls"},
        };

        m_help_dialog = new Window(this, "Help");
        m_help_dialog->set_theme(thm);
        m_help_dialog->set_visible(false);
        GridLayout *layout = new GridLayout(Orientation::Horizontal, 2, Alignment::Middle, 15, 5);
        layout->set_col_alignment({Alignment::Maximum, Alignment::Fill});
        layout->set_spacing(0, 10);
        m_help_dialog->set_layout(layout);

        new Label(m_help_dialog, "key", "sans-bold");
        new Label(m_help_dialog, "Action", "sans-bold");

        for (auto item : helpStrings)
        {
            new Label(m_help_dialog, item.first);
            new Label(m_help_dialog, item.second);
        }

        m_help_dialog->center();

        Button *button = new Button(m_help_dialog->button_panel(), "", FA_TIMES);
        button->set_callback(
            [&]
            {
                m_help_dialog->set_visible(false);
                m_help_button->set_pushed(false);
            });
    }
}

void SampleViewer::update_GPU_points(bool regenerate)
{
    // Generate the point positions
    if (regenerate)
    {
        try
        {
            generate_points();
        }
        catch (const std::exception &e)
        {
            new MessageDialog(this, MessageDialog::Type::Warning, "Error",
                              "An error occurred: " + std::string(e.what()));
            return;
        }
    }

    // Upload points to GPU

    populate_point_subset();

    m_point_shader->set_buffer("position", VariableType::Float32, {m_3d_points.size(), 3}, m_3d_points.data());

    // Upload 2D points to GPU
    // create a temporary matrix to store all the 2D projections of the points
    // each 2D plot actually needs 3D points, and there are num2DPlots of them
    int              num2DPlots = m_num_dimensions * (m_num_dimensions - 1) / 2;
    vector<Vector3f> points2D(num2DPlots * m_subset_count);
    int              plot_index = 0;
    for (int y = 0; y < m_num_dimensions; ++y)
        for (int x = 0; x < y; ++x, ++plot_index)
            for (int i = 0; i < m_subset_count; ++i)
                points2D[plot_index * m_subset_count + i] =
                    Vector3f{m_subset_points(i, x), m_subset_points(i, y), 0.5f};

    m_point_2d_shader->set_buffer("position", VariableType::Float32, {points2D.size(), 3}, points2D.data());

    m_point_count_box->set_value(m_point_count);
    m_point_count_slider->set_value(mapCount2Slider(m_point_count));
}

void SampleViewer::update_GPU_grids()
{
    vector<Vector3f> bboxGrid, coarseGrid, fineGrid;
    PointType        point_type = (PointType)m_point_type_box->selected_index();
    generate_grid(bboxGrid, 1);
    generate_grid(coarseGrid, m_samplers[point_type]->coarseGridRes(m_point_count));
    generate_grid(fineGrid, m_point_count);
    m_coarse_line_count = coarseGrid.size();
    m_fine_line_count   = fineGrid.size();
    vector<Vector3f> positions;
    positions.reserve(bboxGrid.size() + coarseGrid.size() + fineGrid.size());
    positions.insert(positions.end(), bboxGrid.begin(), bboxGrid.end());
    positions.insert(positions.end(), coarseGrid.begin(), coarseGrid.end());
    positions.insert(positions.end(), fineGrid.begin(), fineGrid.end());
    m_grid_shader->set_buffer("position", VariableType::Float32, {positions.size(), 3}, positions.data());
}

bool SampleViewer::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers)
{
    if (!Screen::mouse_motion_event(p, rel, button, modifiers))
    {
        if (m_camera[CAMERA_NEXT].arcball.motion(p))
            draw_all();
    }
    return true;
}

bool SampleViewer::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers)
{
    if (!Screen::mouse_button_event(p, button, down, modifiers))
    {
        if (button == GLFW_MOUSE_BUTTON_1)
        {
            if (down)
            {
                m_mouse_down = true;
                // on mouse down we start switching to a perspective camera
                // and start recording the arcball rotation in CAMERA_NEXT
                set_view(CAMERA_CURRENT);
                m_camera[CAMERA_NEXT].arcball.button(p, down);
                m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
            }
            else
            {
                m_mouse_down = false;
                m_camera[CAMERA_NEXT].arcball.button(p, down);
                // since the time between mouse down and up could be shorter
                // than the animation duration, we override the previous
                // camera's arcball on mouse up to complete the animation
                m_camera[CAMERA_PREVIOUS].arcball     = m_camera[CAMERA_NEXT].arcball;
                m_camera[CAMERA_PREVIOUS].camera_type = m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
            }
        }
    }
    return true;
}

bool SampleViewer::scroll_event(const Vector2i &p, const Vector2f &rel)
{
    if (!Screen::scroll_event(p, rel))
    {
        m_camera[CAMERA_NEXT].zoom = max(0.001, m_camera[CAMERA_NEXT].zoom * pow(1.1, rel.y()));
        draw_all();
    }
    return true;
}

void SampleViewer::draw_points(const Matrix4f &mvp, const Vector3f &color, bool show_point_nums, bool show_point_coords)
{
    // Render the point set
    m_point_shader->set_uniform("mvp", mvp);
    float radius = map_slider_to_radius(m_point_radius_slider->value());
    if (m_scale_radius_with_points->pushed())
        radius *= 64.0f / std::sqrt(m_point_count);
    m_point_shader->set_uniform("pointSize", radius);
    m_point_shader->set_uniform("color", color);

    int start = 0, count = m_point_count;
    if (m_subset_by_coord->checked())
    {
        count = min(count, m_subset_count);
    }
    else if (m_subset_by_index->checked())
    {
        start = m_first_draw_point_box->value();
        count = m_point_draw_count_box->value();
    }

    m_point_shader->begin();
    m_point_shader->draw_array(Shader::PrimitiveType::Point, start, count);
    m_point_shader->end();

    if (!show_point_nums && !show_point_coords)
        return;

    for (int p = start; p < start + count; ++p)
    {
        Vector3f point = m_3d_points[p];

        Vector4f text_pos = mvp * Vector4f(point.x(), point.y(), point.z(), 1.0f);
        Vector2f text_2d_pos((text_pos.x() / text_pos.w() + 1) / 2, (text_pos.y() / text_pos.w() + 1) / 2);
        if (show_point_nums)
            draw_text(Vector2i((text_2d_pos.x()) * m_size.x(), (1.f - text_2d_pos.y()) * m_size.y() - radius / 4),
                      tfm::format("%d", p), Color(1.0f, 1.0f, 1.0f, 0.75f), 12, 0, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        if (show_point_coords)
            draw_text(Vector2i((text_2d_pos.x()) * m_size.x(), (1.f - text_2d_pos.y()) * m_size.y() + radius / 4),
                      tfm::format("(%0.2f, %0.2f, %0.2f)", point.x() + 0.5, point.y() + 0.5, point.z() + 0.5),
                      Color(1.0f, 1.0f, 1.0f, 0.75f), 11, 0, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    }
}

void SampleViewer::draw_grid(const Matrix4f &mvp, float alpha, uint32_t offset, uint32_t count)
{
    m_grid_shader->set_uniform("alpha", alpha);

    for (int axis = CAMERA_XY; axis <= CAMERA_XZ; ++axis)
    {
        if (m_camera[CAMERA_CURRENT].camera_type == axis || m_camera[CAMERA_CURRENT].camera_type == CAMERA_CURRENT)
        {
            m_grid_shader->set_uniform("mvp", Matrix4f(mvp * m_camera[axis].arcball.matrix()));
            m_grid_shader->begin();
            m_grid_shader->draw_array(Shader::PrimitiveType::Line, offset, count);
            m_grid_shader->end();
        }
    }
}

void SampleViewer::draw_2D_points_and_grid(const Matrix4f &mvp, int dim_x, int dim_y, int plot_index)
{
    Matrix4f pos;
    layout_2d_matrix(pos, m_num_dimensions, dim_x, dim_y);

    // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
    // Render the point set
    m_point_2d_shader->set_uniform("mvp", Matrix4f(mvp * pos));
    float radius = map_slider_to_radius(m_point_radius_slider->value()) / (m_num_dimensions - 1);
    if (m_scale_radius_with_points->pushed())
        radius *= 64.0f / std::sqrt(m_point_count);
    m_point_2d_shader->set_uniform("pointSize", radius);
    m_point_2d_shader->set_uniform("color", Vector3f(0.9f, 0.55f, 0.1f));

    int start = 0, count = m_point_count;
    if (m_subset_by_coord->checked())
    {
        count = min(count, m_subset_count);
    }
    else if (m_subset_by_index->checked())
    {
        start = m_first_draw_point_box->value();
        count = m_point_draw_count_box->value();
    }

    m_point_2d_shader->begin();
    m_point_2d_shader->draw_array(Shader::PrimitiveType::Point, m_subset_count * plot_index + start, count);
    m_point_2d_shader->end();

    // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
    if (m_bbox_grid_checkbox->checked())
    {
        m_grid_shader->set_uniform("alpha", 1.0f);
        m_grid_shader->set_uniform("mvp", Matrix4f(mvp * pos * m_camera[CAMERA_2D].arcball.matrix()));
        m_grid_shader->begin();
        m_grid_shader->draw_array(Shader::PrimitiveType::Line, 0, 8);
        m_grid_shader->end();
    }
    if (m_coarse_grid_checkbox->checked())
    {
        m_grid_shader->set_uniform("alpha", 0.6f);
        m_grid_shader->set_uniform("mvp", Matrix4f(mvp * pos * m_camera[CAMERA_2D].arcball.matrix()));
        m_grid_shader->begin();
        m_grid_shader->draw_array(Shader::PrimitiveType::Line, 8, m_coarse_line_count);
        m_grid_shader->end();
    }
    if (m_fine_grid_checkbox->checked())
    {
        m_grid_shader->set_uniform("alpha", 0.2f);
        m_grid_shader->set_uniform("mvp", Matrix4f(mvp * pos * m_camera[CAMERA_2D].arcball.matrix()));
        m_grid_shader->begin();
        m_grid_shader->draw_array(Shader::PrimitiveType::Line, 8 + m_coarse_line_count, m_fine_line_count);
        m_grid_shader->end();
    }
}

void SampleViewer::draw_contents()
{
    // update/move the camera
    update_current_camera();

    Matrix4f model, lookat, view, proj, mvp;
    computeCameraMatrices(model, lookat, view, proj, m_camera[CAMERA_CURRENT], float(m_size.x()) / m_size.y());
    mvp = proj * lookat * view * model;

    m_render_pass->resize(framebuffer_size());
    m_render_pass->begin();

    if (m_view_btns[CAMERA_2D]->pushed())
    {
        Matrix4f pos;

        for (int i = 0; i < m_num_dimensions - 1; ++i)
        {
            layout_2d_matrix(pos, m_num_dimensions, i, m_num_dimensions - 1);
            Vector4f text_pos = mvp * pos * Vector4f(0.f, -0.5f, 0.0f, 1.0f);
            Vector2f text_2d_pos((text_pos.x() / text_pos.w() + 1) / 2, (text_pos.y() / text_pos.w() + 1) / 2);
            draw_text(Vector2i((text_2d_pos.x()) * m_size.x(), (1.f - text_2d_pos.y()) * m_size.y() + 16), to_string(i),
                      Color(1.0f, 1.0f, 1.0f, 0.75f), 16, 0, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

            layout_2d_matrix(pos, m_num_dimensions, 0, i + 1);
            text_pos    = mvp * pos * Vector4f(-0.5f, 0.f, 0.0f, 1.0f);
            text_2d_pos = Vector2f((text_pos.x() / text_pos.w() + 1) / 2, (text_pos.y() / text_pos.w() + 1) / 2);
            draw_text(Vector2i((text_2d_pos.x()) * m_size.x() - 4, (1.f - text_2d_pos.y()) * m_size.y()),
                      to_string(i + 1), Color(1.0f, 1.0f, 1.0f, 0.75f), 16, 0, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }

        int plot_index = 0;
        for (int y = 0; y < m_num_dimensions; ++y)
            for (int x = 0; x < y; ++x, ++plot_index)
            {
                // cout << "drawing plot_index: " << plot_index << endl;
                draw_2D_points_and_grid(mvp, x, y, plot_index);
            }
        // cout << endl;
    }
    else
    {
        // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
        draw_points(mvp, Vector3f(0.9f, 0.55f, 0.1f), m_show_point_nums->checked(), m_show_point_coords->checked());

        if (m_show_1d_projections->checked())
        {
            // smash the points against the axes and draw
            Matrix4f smashX = mvp * Matrix4f::translate(Vector3f(-0.51, 0, 0)) * Matrix4f::scale(Vector3f(0, 1, 1));
            draw_points(smashX, Vector3f(0.8f, 0.3f, 0.3f));

            Matrix4f smashY = mvp * Matrix4f::translate(Vector3f(0, -0.51, 0)) * Matrix4f::scale(Vector3f(1, 0, 1));
            draw_points(smashY, Vector3f(0.3f, 0.8f, 0.3f));

            Matrix4f smashZ = mvp * Matrix4f::translate(Vector3f(0, 0, -0.51)) * Matrix4f::scale(Vector3f(1, 1, 0));
            draw_points(smashZ, Vector3f(0.3f, 0.3f, 0.8f));
        }

        // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
        if (m_bbox_grid_checkbox->checked())
            draw_grid(mvp, 1.0f, 0, 8);

        if (m_coarse_grid_checkbox->checked())
            draw_grid(mvp, 0.6f, 8, m_coarse_line_count);

        if (m_fine_grid_checkbox->checked())
            draw_grid(mvp, 0.2f, 8 + m_coarse_line_count, m_fine_line_count);

        for (int i = 0; i < 3; ++i)
            draw_text(m_size - Vector2i(10, (2 - i) * 14 + 20), m_time_strings[i], Color(1.0f, 1.0f, 1.0f, 0.75f), 16,
                      0);
    }

    m_render_pass->end();
}

void SampleViewer::draw_text(const Vector2i &pos, const string &text, const Color &color, int font_size,
                             int fixed_width, int align) const
{
    nvgFontFace(m_nvg_context, "sans");
    nvgFontSize(m_nvg_context, (float)font_size);
    nvgFillColor(m_nvg_context, color);
    if (fixed_width > 0)
    {
        nvgTextAlign(m_nvg_context, align);
        nvgTextBox(m_nvg_context, (float)pos.x(), (float)pos.y(), (float)fixed_width, text.c_str(), nullptr);
    }
    else
    {
        nvgTextAlign(m_nvg_context, align);
        nvgText(m_nvg_context, (float)pos.x(), (float)pos.y(), text.c_str(), nullptr);
    }
}

void SampleViewer::draw_contents_EPS(ofstream &file, CameraType camera_type, int dim_x, int dim_y, int dim_z)
{
    int size = 900;
    int crop = 720;

    int pointScale = m_samplers[m_point_type_box->selected_index()]->coarseGridRes(m_point_count);

    file << "%!PS-Adobe-3.0 EPSF-3.0\n";
    file << "%%HiResBoundingBox: " << (-crop) << " " << (-crop) << " " << crop << " " << crop << "\n";
    file << "%%BoundingBox: " << (-crop) << " " << (-crop) << " " << crop << " " << crop << "\n";
    file << "%%CropBox: " << (-crop) << " " << (-crop) << " " << crop << " " << crop << "\n";
    file << "gsave " << size << " " << size << " scale\n";
    file << "/radius { " << (0.5f / pointScale) << " } def %define variable for point radius\n";
    file << "/p { radius 0 360 arc closepath fill } def %define point command\n";
    file << "/blw " << (0.01f) << " def %define variable for bounding box linewidth\n";
    file << "/clw " << (0.003f) << " def %define variable for coarse linewidth\n";
    file << "/flw " << (0.0004f) << " def %define variable for fine linewidth\n";
    file << "/pfc "
         << "{0.9 0.55 0.1}"
         << " def %define variable for point fill color\n";
    file << "/blc " << (0.0f) << " def %define variable for bounding box color\n";
    file << "/clc " << (0.8f) << " def %define variable for coarse line color\n";
    file << "/flc " << (0.9f) << " def %define variable for fine line color\n";

    Matrix4f model, lookat, view, proj, mvp, mvp2;
    computeCameraMatrices(model, lookat, view, proj, m_camera[camera_type], 1.0f);
    mvp            = proj * lookat * view * model;
    int start_axis = CAMERA_XY, end_axis = CAMERA_XZ;
    if (camera_type != CAMERA_CURRENT)
        start_axis = end_axis = camera_type;
    if (m_fine_grid_checkbox->checked())
    {
        file << "% Draw fine grids \n";
        file << "flc setgray %fill color for fine grid \n";
        file << "flw setlinewidth\n";
        for (int axis = start_axis; axis <= end_axis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            Matrix4f mvp2 = mvp * m_camera[axis].arcball.matrix();
            drawEPSGrid(mvp2, m_point_count, file);
        }
    }

    if (m_coarse_grid_checkbox->checked())
    {
        file << "% Draw coarse grids \n";
        file << "clc setgray %fill color for coarse grid \n";
        file << "clw setlinewidth\n";
        for (int axis = start_axis; axis <= end_axis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            Matrix4f  mvp2       = mvp * m_camera[axis].arcball.matrix();
            PointType point_type = (PointType)m_point_type_box->selected_index();
            drawEPSGrid(mvp2, m_samplers[point_type]->coarseGridRes(m_point_count), file);
        }
    }

    if (m_bbox_grid_checkbox->checked())
    {
        file << "% Draw bounding boxes \n";
        file << "blc setgray %fill color for bounding box \n";
        file << "blw setlinewidth\n";
        for (int axis = start_axis; axis <= end_axis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            Matrix4f mvp2 = mvp * m_camera[axis].arcball.matrix();
            drawEPSGrid(mvp2, 1, file);
        }
    }

    // Generate and render the point set
    {
        generate_points();
        populate_point_subset();
        file << "% Draw points \n";
        file << "pfc setrgbcolor %fill color for points\n";

        int start = 0, count = m_point_count;
        if (m_subset_by_coord->checked())
            count = min(count, m_subset_count);
        else if (m_subset_by_index->checked())
        {
            start = m_first_draw_point_box->value();
            count = m_point_draw_count_box->value();
        }
        writeEPSPoints(m_subset_points, start, count, mvp, file, dim_x, dim_y, dim_z);
    }

    file << "grestore\n";
}

void SampleViewer::draw_contents_2D_EPS(ofstream &file)
{
    int   size  = 900 * 108.0f / 720.0f;
    int   crop  = 108; // 720;
    float scale = 1.0f / (m_num_dimensions - 1);
    // int pointScale = m_samplers[m_point_type_box->selected_index()]->coarseGridRes(m_point_count);

    file << "%!PS-Adobe-3.0 EPSF-3.0\n";
    file << "%%HiResBoundingBox: " << (-crop) << " " << (-crop) << " " << crop << " " << crop << "\n";
    file << "%%BoundingBox: " << (-crop) << " " << (-crop) << " " << crop << " " << crop << "\n";
    file << "%%CropBox: " << (-crop) << " " << (-crop) << " " << crop << " " << crop << "\n";
    file << "gsave " << size << " " << size << " scale\n";
    // file << "/radius { " << (0.5f/pointScale*scale) << " } def %define variable for point radius\n";
    file << "/radius { " << (0.02 * scale) << " } def %define variable for point radius\n";
    file << "/p { radius 0 360 arc closepath fill } def %define point command\n";
    file << "/blw " << (0.020f * scale) << " def %define variable for bounding box linewidth\n";
    file << "/clw " << (0.01f * scale) << " def %define variable for coarse linewidth\n";
    file << "/flw " << (0.005f * scale) << " def %define variable for fine linewidth\n";
    file << "/pfc "
         << "{0.9 0.55 0.1}"
         << " def %define variable for point fill color\n";
    file << "/blc " << (0.0f) << " def %define variable for bounding box color\n";
    file << "/clc " << (0.5f) << " def %define variable for coarse line color\n";
    file << "/flc " << (0.9f) << " def %define variable for fine line color\n";

    Matrix4f model, lookat, view, proj, mvp, mvp2;
    computeCameraMatrices(model, lookat, view, proj, m_camera[CAMERA_2D], 1.0f);
    mvp = proj * lookat * view * model;

    // Generate and render the point set
    {
        generate_points();
        populate_point_subset();

        for (int y = 0; y < m_num_dimensions; ++y)
            for (int x = 0; x < y; ++x)
            {
                Matrix4f pos;
                layout_2d_matrix(pos, m_num_dimensions, x, y);

                if (m_fine_grid_checkbox->checked())
                {
                    file << "% Draw fine grid \n";
                    file << "flc setgray %fill color for fine grid \n";
                    file << "flw setlinewidth\n";
                    drawEPSGrid(mvp * pos, m_point_count, file);
                }
                if (m_coarse_grid_checkbox->checked())
                {
                    file << "% Draw coarse grids \n";
                    file << "clc setgray %fill color for coarse grid \n";
                    file << "clw setlinewidth\n";
                    PointType point_type = (PointType)m_point_type_box->selected_index();
                    drawEPSGrid(mvp * pos, m_samplers[point_type]->coarseGridRes(m_point_count), file);
                }
                if (m_bbox_grid_checkbox->checked())
                {
                    file << "% Draw bounding box \n";
                    file << "blc setgray %fill color for bounding box \n";
                    file << "blw setlinewidth\n";
                    drawEPSGrid(mvp * pos, 1, file);
                }
            }

        file << "% Draw points \n";
        file << "pfc setrgbcolor %fill color for points\n";
        for (int y = 0; y < m_num_dimensions; ++y)
            for (int x = 0; x < y; ++x)
            {
                Matrix4f pos;
                layout_2d_matrix(pos, m_num_dimensions, x, y);
                file << "% Draw (" << x << "," << y << ") points\n";

                int start = 0, count = m_point_count;
                if (m_subset_by_coord->checked())
                    count = min(count, m_subset_count);
                else if (m_subset_by_index->checked())
                {
                    start = m_first_draw_point_box->value();
                    count = m_point_draw_count_box->value();
                }

                writeEPSPoints(m_subset_points, start, count, mvp * pos, file, x, y, 2);
            }
    }

    file << "grestore\n";
}

bool SampleViewer::keyboard_event(int key, int scancode, int action, int modifiers)
{
    if (Screen::keyboard_event(key, scancode, action, modifiers))
        return true;

    if (!action)
        return false;

    switch (key)
    {
    case GLFW_KEY_ESCAPE:
    {
        this->set_visible(false);
        return true;
    }
    case ' ': m_parameters_dialog->set_visible(!m_parameters_dialog->visible()); return true;
    case 'H':
        m_help_dialog->center();
        m_help_dialog->set_visible(!m_help_dialog->visible());
        m_help_button->set_pushed(m_help_dialog->visible());
        return true;
    case '1': set_view(CAMERA_XY); return true;
    case '2': set_view(CAMERA_YZ); return true;
    case '3': set_view(CAMERA_XZ); return true;
    case '4': set_view(CAMERA_CURRENT); return true;
    case '0': set_view(CAMERA_2D); return true;
    case 'P':
        m_show_1d_projections->set_checked(!m_show_1d_projections->checked());
        draw_contents();
        return true;
    case 'B':
        m_bbox_grid_checkbox->set_checked(!m_bbox_grid_checkbox->checked());
        update_GPU_grids();
        return true;
    case 'G':
        if (modifiers & GLFW_MOD_SHIFT)
            m_fine_grid_checkbox->set_checked(!m_fine_grid_checkbox->checked());
        else
            m_coarse_grid_checkbox->set_checked(!m_coarse_grid_checkbox->checked());
        update_GPU_grids();
        return true;
    case 'R':
        if (modifiers & GLFW_MOD_SHIFT)
        {
            // re-randomize
            PointType point_type = (PointType)m_point_type_box->selected_index();
            m_randomize_checkbox->set_checked(true);
            m_samplers[point_type]->setRandomized(true);
            update_GPU_points();
            draw_all();
        }
        else
        {
            m_randomize_checkbox->set_checked(!m_randomize_checkbox->checked());
            update_GPU_points();
        }
        return true;
    case 'D':
        m_dimension_box->set_value(m_dimension_box->value() + ((modifiers & GLFW_MOD_SHIFT) ? 1 : -1));
        m_num_dimensions = m_dimension_box->value();
        m_subset_axis->set_max_value(m_num_dimensions - 1);
        m_x_dimension->set_max_value(m_num_dimensions - 1);
        m_y_dimension->set_max_value(m_num_dimensions - 1);
        m_z_dimension->set_max_value(m_num_dimensions - 1);
        m_subset_axis->set_value(min(m_subset_axis->value(), m_num_dimensions - 1));
        m_x_dimension->set_value(min(m_x_dimension->value(), m_num_dimensions - 1));
        m_y_dimension->set_value(min(m_y_dimension->value(), m_num_dimensions - 1));
        m_z_dimension->set_value(min(m_z_dimension->value(), m_num_dimensions - 1));
        update_GPU_points();
        update_GPU_grids();
        return true;

    case 'T':
    {
        Sampler         *sampler = m_samplers[m_point_type_box->selected_index()];
        OrthogonalArray *oa      = dynamic_cast<OrthogonalArray *>(sampler);
        if (oa)
        {
            int t = m_strength_box->value() + ((modifiers & GLFW_MOD_SHIFT) ? 1 : -1);
            t     = (t < 2) ? 2 : t;
            m_strength_box->set_value(oa->setStrength(t));
        }
        update_GPU_points();
        update_GPU_grids();
        return true;
    }

    case GLFW_KEY_UP:
    case GLFW_KEY_DOWN:
    {
        int delta = (key == GLFW_KEY_DOWN) ? 1 : -1;
        if (modifiers & GLFW_MOD_SHIFT)
        {
            if (OrthogonalArray *oa = dynamic_cast<OrthogonalArray *>(m_samplers[m_point_type_box->selected_index()]))
            {
                m_offset_type_box->set_selected_index(
                    mod(m_offset_type_box->selected_index() + delta, (int)NUM_OFFSET_TYPES));
                oa->setOffsetType(m_offset_type_box->selected_index());

                update_GPU_points();
                update_GPU_grids();
            }
        }
        else
        {
            m_point_type_box->set_selected_index(mod(m_point_type_box->selected_index() + delta, (int)NUM_POINT_TYPES));
            Sampler         *sampler = m_samplers[m_point_type_box->selected_index()];
            OrthogonalArray *oa      = dynamic_cast<OrthogonalArray *>(sampler);
            m_strength_label->set_visible(oa);
            m_strength_box->set_visible(oa);
            m_strength_box->set_enabled(oa);
            m_offset_type_label->set_visible(oa);
            m_offset_type_box->set_visible(oa);
            m_offset_type_box->set_enabled(oa);
            m_jitter_slider->set_value(sampler->jitter());
            if (oa)
            {
                m_offset_type_box->set_selected_index(oa->offsetType());
                m_strength_box->set_value(oa->strength());
            }
            perform_layout();
            update_GPU_points();
            update_GPU_grids();
        }
        return true;
    }
    case GLFW_KEY_LEFT:
    case GLFW_KEY_RIGHT:
    {
        float delta = (modifiers & GLFW_MOD_SHIFT) ? 1.0f / 15 : 1.0f / 60;
        delta *= (key == GLFW_KEY_RIGHT) ? 1.f : -1.f;
        m_target_point_count = mapSlider2Count(std::clamp(m_point_count_slider->value() + delta, 0.0f, 1.0f));
        update_GPU_points();
        update_GPU_grids();
        return true;
    }
    }

    return false;
}

void SampleViewer::set_view(CameraType view)
{
    m_animate_start_time                 = (float)glfwGetTime();
    m_camera[CAMERA_PREVIOUS]            = m_camera[CAMERA_CURRENT];
    m_camera[CAMERA_NEXT]                = m_camera[view];
    m_camera[CAMERA_NEXT].persp_factor   = (view == CAMERA_CURRENT) ? 1.0f : 0.0f;
    m_camera[CAMERA_NEXT].camera_type    = view;
    m_camera[CAMERA_CURRENT].camera_type = (view == m_camera[CAMERA_CURRENT].camera_type) ? view : CAMERA_CURRENT;

    for (int i = CAMERA_XY; i <= CAMERA_CURRENT; ++i)
        m_view_btns[i]->set_pushed(false);
    m_view_btns[view]->set_pushed(true);
    m_gui_refresh = true;
}

void SampleViewer::update_current_camera()
{
    CameraParameters &camera0 = m_camera[CAMERA_PREVIOUS];
    CameraParameters &camera1 = m_camera[CAMERA_NEXT];
    CameraParameters &camera  = m_camera[CAMERA_CURRENT];

    float time_now  = (float)glfwGetTime();
    float time_diff = (m_animate_start_time != 0.0f) ? (time_now - m_animate_start_time) : 1.0f;

    // interpolate between the "perspectiveness" at the previous keyframe,
    // and the "perspectiveness" of the currently selected camera
    float t = smoothStep(0.0f, 1.0f, time_diff);

    camera.zoom         = lerp(camera0.zoom, camera1.zoom, t);
    camera.view_angle   = lerp(camera0.view_angle, camera1.view_angle, t);
    camera.persp_factor = lerp(camera0.persp_factor, camera1.persp_factor, t);
    if (t >= 1.0f)
    {
        camera.camera_type   = camera1.camera_type;
        m_gui_refresh        = false;
        m_animate_start_time = 0.f;
    }

    // if we are dragging the mouse, then just use the current arcball
    // rotation, otherwise, interpolate between the previous and next camera
    // orientations
    if (m_mouse_down)
        camera.arcball = camera1.arcball;
    else
        camera.arcball.setState(qslerp(camera0.arcball.state(), camera1.arcball.state(), t));
}

void SampleViewer::generate_points()
{
    PointType point_type = (PointType)m_point_type_box->selected_index();
    bool      randomize  = m_randomize_checkbox->checked();

    float    time0     = (float)glfwGetTime();
    Sampler *generator = m_samplers[point_type];
    if (generator->randomized() != randomize)
        generator->setRandomized(randomize);

    generator->setDimensions(m_num_dimensions);

    int num_pts   = generator->setNumSamples(m_target_point_count);
    m_point_count = num_pts > 0 ? num_pts : m_target_point_count;
    m_first_draw_point_box->set_max_value(m_point_count - 1);
    m_first_draw_point_box->set_value(m_first_draw_point_box->value());
    m_point_draw_count_box->set_max_value(m_point_count - m_first_draw_point_box->value());
    m_point_draw_count_box->set_value(m_point_draw_count_box->value());

    float time1      = (float)glfwGetTime();
    float time_diff1 = ((time1 - time0) * 1000.0f);

    m_points.resize(m_point_count, m_num_dimensions);
    m_3d_points.resize(m_point_count);

    time1 = (float)glfwGetTime();
    for (int i = 0; i < m_point_count; ++i)
    {
        vector<float> r(m_num_dimensions, 0.5f);
        generator->sample(r.data(), i);
        for (int j = 0; j < m_points.size_y(); ++j)
            m_points(i, j) = r[j] - 0.5f;
    }
    float time2      = (float)glfwGetTime();
    float time_diff2 = ((time2 - time1) * 1000.0f);

    m_time_strings[0] = tfm::format("Reset and randomize: %3.3f ms", time_diff1);
    m_time_strings[1] = tfm::format("Sampling: %3.3f ms", time_diff2);
    m_time_strings[2] = tfm::format("Total generation time: %3.3f ms", time_diff1 + time_diff2);
}

void SampleViewer::populate_point_subset()
{
    //
    // Populate point subsets
    //
    m_subset_points = m_points;
    m_subset_count  = m_point_count;
    if (m_subset_by_coord->checked())
    {
        // cout << "only accepting points where the coordinates along the " << m_subset_axis->value()
        //      << " axis are within: ["
        //      << ((m_subset_level->value() + 0.0f)/m_num_subset_levels->value()) << ", "
        //      << ((m_subset_level->value()+1.0f)/m_num_subset_levels->value()) << ")." << endl;
        m_subset_count = 0;
        for (int i = 0; i < m_points.size_x(); ++i)
        {
            float v = m_points(i, m_subset_axis->value());
            // cout << v << endl;
            v += 0.5f;
            if (v >= (m_subset_level->value() + 0.0f) / m_num_subset_levels->value() &&
                v < (m_subset_level->value() + 1.0f) / m_num_subset_levels->value())
            {
                // copy all dimensions (rows) of point i
                for (int j = 0; j < m_subset_points.size_y(); ++j)
                    m_subset_points(m_subset_count, j) = m_points(i, j);
                ++m_subset_count;
            }
        }
    }

    for (size_t i = 0; i < m_3d_points.size(); ++i)
        m_3d_points[i] =
            Vector3f{m_subset_points(i, m_x_dimension->value()), m_subset_points(i, m_y_dimension->value()),
                     m_subset_points(i, m_z_dimension->value())};
}

void SampleViewer::generate_grid(vector<Vector3f> &positions, int grid_res)
{
    int fine_grid_res = 1;
    int idx           = 0;
    positions.resize(4 * (grid_res + 1) * (fine_grid_res));
    float coarse_scale = 1.f / grid_res, fine_scale = 1.f / fine_grid_res;
    // for (int z = -1; z <= 1; z+=2)
    int z = 0;
    for (int i = 0; i <= grid_res; ++i)
    {
        for (int j = 0; j < fine_grid_res; ++j)
        {
            positions[idx++] = Vector3f(j * fine_scale - 0.5f, i * coarse_scale - 0.5f, z * 0.5f);
            positions[idx++] = Vector3f((j + 1) * fine_scale - 0.5f, i * coarse_scale - 0.5f, z * 0.5f);
            positions[idx++] = Vector3f(i * coarse_scale - 0.5f, j * fine_scale - 0.5f, z * 0.5f);
            positions[idx++] = Vector3f(i * coarse_scale - 0.5f, (j + 1) * fine_scale - 0.5f, z * 0.5f);
        }
    }
}

namespace
{

void computeCameraMatrices(Matrix4f &model, Matrix4f &lookat, Matrix4f &view, Matrix4f &proj, const CameraParameters &c,
                           float window_aspect)
{
    model = Matrix4f::scale(Vector3f(c.zoom));
    if (c.camera_type == CAMERA_XY || c.camera_type == CAMERA_2D)
        model = Matrix4f::scale(Vector3f(1, 1, 0)) * model;
    if (c.camera_type == CAMERA_XZ)
        model = Matrix4f::scale(Vector3f(1, 0, 1)) * model;
    if (c.camera_type == CAMERA_YZ)
        model = Matrix4f::scale(Vector3f(0, 1, 1)) * model;

    float fH = tan(c.view_angle / 360.0f * M_PI) * c.dnear;
    float fW = fH * window_aspect;

    float    oFF   = c.eye.z() / c.dnear;
    Matrix4f orth  = Matrix4f::ortho(-fW * oFF, fW * oFF, -fH * oFF, fH * oFF, c.dnear, c.dfar);
    Matrix4f frust = frustum(-fW, fW, -fH, fH, c.dnear, c.dfar);

    proj   = lerp(orth, frust, c.persp_factor);
    lookat = Matrix4f::look_at(c.eye, c.center, c.up);
    view   = c.arcball.matrix();
}

void drawEPSGrid(const Matrix4f &mvp, int grid_res, ofstream &file)
{
    Vector4f vA4d, vB4d;
    Vector2f vA, vB;

    int   fine_grid_res = 2;
    float coarse_scale = 1.f / grid_res, fine_scale = 1.f / fine_grid_res;

    // draw the bounding box
    // if (grid_res == 1)
    {
        Vector4f c004d = mvp * Vector4f(0.0f - 0.5f, 0.0f - 0.5f, 0.0f, 1.0f);
        Vector4f c104d = mvp * Vector4f(1.0f - 0.5f, 0.0f - 0.5f, 0.0f, 1.0f);
        Vector4f c114d = mvp * Vector4f(1.0f - 0.5f, 1.0f - 0.5f, 0.0f, 1.0f);
        Vector4f c014d = mvp * Vector4f(0.0f - 0.5f, 1.0f - 0.5f, 0.0f, 1.0f);
        Vector2f c00   = Vector2f(c004d.x() / c004d.w(), c004d.y() / c004d.w());
        Vector2f c10   = Vector2f(c104d.x() / c104d.w(), c104d.y() / c104d.w());
        Vector2f c11   = Vector2f(c114d.x() / c114d.w(), c114d.y() / c114d.w());
        Vector2f c01   = Vector2f(c014d.x() / c014d.w(), c014d.y() / c014d.w());

        file << "newpath\n";
        file << c00.x() << " " << c00.y() << " moveto\n";
        file << c10.x() << " " << c10.y() << " lineto\n";
        file << c11.x() << " " << c11.y() << " lineto\n";
        file << c01.x() << " " << c01.y() << " lineto\n";
        file << "closepath\n";
        file << "stroke\n";
    }

    for (int i = 1; i <= grid_res - 1; i++)
    {
        // draw horizontal lines
        file << "newpath\n";
        for (int j = 0; j < fine_grid_res; j++)
        {
            vA4d = mvp * Vector4f(j * fine_scale - 0.5f, i * coarse_scale - 0.5f, 0.0f, 1.0f);
            vB4d = mvp * Vector4f((j + 1) * fine_scale - 0.5f, i * coarse_scale - 0.5f, 0.0f, 1.0f);

            vA = Vector2f(vA4d.x() / vA4d.w(), vA4d.y() / vA4d.w());
            vB = Vector2f(vB4d.x() / vB4d.w(), vB4d.y() / vB4d.w());

            file << vA.x() << " " << vA.y();

            if (j == 0)
                file << " moveto\n";
            else
                file << " lineto\n";

            file << vB.x() << " " << vB.y();
            file << " lineto\n";
        }
        file << "stroke\n";

        // draw vertical lines
        file << "newpath\n";
        for (int j = 0; j < fine_grid_res; j++)
        {
            vA4d = mvp * Vector4f(i * coarse_scale - 0.5f, j * fine_scale - 0.5f, 0.0f, 1.0f);
            vB4d = mvp * Vector4f(i * coarse_scale - 0.5f, (j + 1) * fine_scale - 0.5f, 0.0f, 1.0f);

            vA = Vector2f(vA4d.x() / vA4d.w(), vA4d.y() / vA4d.w());
            vB = Vector2f(vB4d.x() / vB4d.w(), vB4d.y() / vB4d.w());

            file << vA.x() << " " << vA.y();

            if (j == 0)
                file << " moveto\n";
            else
                file << " lineto\n";

            file << vB.x() << " " << vB.y();
            file << " lineto\n";
        }
        file << "stroke\n";
    }
}

void writeEPSPoints(const Array2D<float> &points, int start, int count, const Matrix4f &mvp, ofstream &file, int dim_x,
                    int dim_y, int dim_z)
{
    for (int i = start; i < start + count; i++)
    {
        Vector4f v4d = mvp * Vector4f(points(i, dim_x), points(i, dim_y), points(i, dim_z), 1.0f);
        Vector2f v2d(v4d.x() / v4d.w(), v4d.y() / v4d.w());
        file << v2d.x() << " " << v2d.y() << " p\n";
    }
}

} // namespace
