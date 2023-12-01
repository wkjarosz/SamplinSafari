/** \file SampleViewer.cpp
    \author Wojciech Jarosz
*/

#define _USE_MATH_DEFINES
#define NOMINMAX

#include "app.h"

using namespace linalg::ostream_overloads;

using std::cerr;
using std::cout;
using std::endl;
using std::ofstream;
using std::to_string;

#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/hello_imgui_include_opengl.h" // cross-platform way to include OpenGL headers
#include "imgui.h"
#include "imgui_internal.h"
#include "portable-file-dialogs.h"

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

#include "export_to_file.h"
#include "timer.h"

#include <cmath>
#include <fmt/core.h>
#include <fstream>
#include <string>
#include <vector>

namespace ImGui
{

bool ToggleButton(const char *label, bool *active)
{
    ImGui::PushStyleColor(ImGuiCol_Button, *active ? GetColorU32(ImGuiCol_ButtonActive) : GetColorU32(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, GetColorU32(ImGuiCol_FrameBgHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, GetColorU32(ImGuiCol_FrameBgActive));

    bool ret;
    if ((ret = ImGui::Button(label)))
        *active = !*active;
    ImGui::PopStyleColor(3);
    return ret;
}

} // namespace ImGui

static float map_slider_to_radius(float sliderValue)
{
    return sliderValue * sliderValue * 32.0f + 2.0f;
}

static float4x4 layout_2d_matrix(int num_dims, int dim_x, int dim_y)
{
    float cell_spacing = 1.f / (num_dims - 1);
    float cell_size    = 0.96f / (num_dims - 1);

    float x_offset = dim_x - (num_dims - 2) / 2.0f;
    float y_offset = -(dim_y - 1 - (num_dims - 2) / 2.0f);

    return mul(translation_matrix(float3{cell_spacing * x_offset, cell_spacing * y_offset, 1}),
               scaling_matrix(float3{cell_size, cell_size, 1}));
}

SampleViewer::SampleViewer()
{
    m_samplers.push_back(new Random(m_num_dimensions));
    m_samplers.push_back(new Jittered(1, 1, m_jitter * 0.01f));
    m_samplers.push_back(new MultiJitteredInPlace(1, 1, false, m_jitter * 0.01f));
    m_samplers.push_back(new CorrelatedMultiJitteredInPlace(1, 1, m_num_dimensions, false, m_jitter * 0.01f));
    m_samplers.push_back(new CMJNDInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f));
    m_samplers.push_back(new BoseOAInPlace(1, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.push_back(new BoseGaloisOAInPlace(1, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.push_back(new BushOAInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.push_back(new BushGaloisOAInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.push_back(new AddelmanKempthorneOAInPlace(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.push_back(new BoseBushOA(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.push_back(new BoseBushOAInPlace(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.push_back(new NRooksInPlace(m_num_dimensions, 1, false, m_jitter * 0.01f));
    m_samplers.push_back(new Sobol(m_num_dimensions));
    m_samplers.push_back(new ZeroTwo(1, m_num_dimensions, false));
    m_samplers.push_back(new ZeroTwo(1, m_num_dimensions, true));
    m_samplers.push_back(new Halton(m_num_dimensions));
    m_samplers.push_back(new HaltonZaremba(m_num_dimensions));
    m_samplers.push_back(new Hammersley<Halton>(m_num_dimensions, 1));
    m_samplers.push_back(new Hammersley<HaltonZaremba>(m_num_dimensions, 1));
    m_samplers.push_back(new LarcherPillichshammerGK(3, 1, false));

    m_camera[CAMERA_XY].arcball.set_state({0, 0, 0, 1});
    m_camera[CAMERA_XY].persp_factor = 0.0f;
    m_camera[CAMERA_XY].camera_type  = CAMERA_XY;

    m_camera[CAMERA_YZ].arcball.set_state(linalg::rotation_quat({0.f, -1.f, 0.f}, float(M_PI_2)));
    m_camera[CAMERA_YZ].persp_factor = 0.0f;
    m_camera[CAMERA_YZ].camera_type  = CAMERA_YZ;

    m_camera[CAMERA_XZ].arcball.set_state(linalg::rotation_quat({1.f, 0.f, 0.f}, float(M_PI_2)));
    m_camera[CAMERA_XZ].persp_factor = 0.0f;
    m_camera[CAMERA_XZ].camera_type  = CAMERA_XZ;

    m_camera[CAMERA_2D]      = m_camera[CAMERA_XY];
    m_camera[CAMERA_CURRENT] = m_camera[CAMERA_XY];
    m_camera[CAMERA_NEXT]    = m_camera[CAMERA_XY];

    // set up HelloImGui parameters
    m_params.appWindowParams.windowGeometry.size     = {1200, 800};
    m_params.appWindowParams.windowTitle             = "Samplin' Safari";
    m_params.appWindowParams.restorePreviousGeometry = false;

    // Menu bar
    m_params.imGuiWindowParams.showMenuBar            = true;
    m_params.imGuiWindowParams.showStatusBar          = true;
    m_params.imGuiWindowParams.defaultImGuiWindowType = HelloImGui::DefaultImGuiWindowType::ProvideFullScreenDockSpace;

    // Setting this to true allows multiple viewports where you can drag windows outside out the main window in order to
    // put their content into new native windows m_params.imGuiWindowParams.enableViewports = true;
    m_params.imGuiWindowParams.enableViewports = false;
    m_params.imGuiWindowParams.menuAppTitle    = "File";

    // Dockable windows
    // the parameter editor
    HelloImGui::DockableWindow editorWindow;
    editorWindow.label         = "Settings";
    editorWindow.dockSpaceName = "EditorSpace";
    editorWindow.GuiFunction   = [this] { draw_editor(); };

    // A console window named "Console" will be placed in "ConsoleSpace". It uses the HelloImGui logger gui
    HelloImGui::DockableWindow consoleWindow;
    consoleWindow.label             = "Console";
    consoleWindow.dockSpaceName     = "ConsoleSpace";
    consoleWindow.isVisible         = false;
    consoleWindow.rememberIsVisible = true;
    consoleWindow.GuiFunction       = [] { HelloImGui::LogGui(); };

    // docking layouts
    {
        m_params.dockingParams.layoutName      = "Settings on left";
        m_params.dockingParams.dockableWindows = {editorWindow, consoleWindow};

        HelloImGui::DockingSplit splitEditorMain{"MainDockSpace", "EditorSpace", ImGuiDir_Left, 0.2f};

        HelloImGui::DockingSplit splitMainConsole{"MainDockSpace", "ConsoleSpace", ImGuiDir_Down, 0.25f};

        m_params.dockingParams.dockingSplits = {splitEditorMain, splitMainConsole};

        HelloImGui::DockingParams alternativeLayout;
        alternativeLayout.layoutName       = "Settings on right";
        alternativeLayout.dockableWindows  = {editorWindow, consoleWindow};
        splitEditorMain.direction          = ImGuiDir_Right;
        alternativeLayout.dockingSplits    = {splitEditorMain, splitMainConsole};
        m_params.alternativeDockingLayouts = {alternativeLayout};
    }

    m_params.callbacks.LoadAdditionalFonts = [this]()
    {
        std::string roboto_r = "fonts/Roboto/Roboto-Regular.ttf";
        std::string roboto_b = "fonts/Roboto/Roboto-Bold.ttf";
        if (!HelloImGui::AssetExists(roboto_r) || !HelloImGui::AssetExists(roboto_b))
            return;

        for (auto font_size : {14.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 15.f, 16.f, 17.f, 18.f})
        {
            m_regular[(int)font_size] = HelloImGui::LoadFontTTF_WithFontAwesomeIcons(roboto_r, font_size);
            m_bold[(int)font_size]    = HelloImGui::LoadFontTTF_WithFontAwesomeIcons(roboto_b, font_size);
        }
    };

    m_params.callbacks.ShowAppMenuItems = [this]()
    {
        if (ImGui::MenuItem(ICON_FA_SAVE "  Export as EPS"))
        {
            try
            {
                auto basename = pfd::save_file("Base filename").result();
                if (basename.empty())
                    return;

                HelloImGui::Log(HelloImGui::LogLevel::Info, "Saving to base filename: %s.", basename.c_str());

                ofstream fileAll(basename + "_all2D.eps");
                fileAll << export_all_points_2d("eps");

                ofstream fileXYZ(basename + "_012.eps");
                fileXYZ << export_XYZ_points("eps");

                for (int y = 0; y < m_num_dimensions; ++y)
                    for (int x = 0; x < y; ++x)
                    {
                        ofstream fileXY(fmt::format("{:s}_{:d}{:d}.eps", basename, x, y));
                        fileXY << export_points_2d("eps", CAMERA_XY, {x, y, 2});
                    }
            }
            catch (const std::exception &e)
            {
                fmt::print(stderr, "An error occurred: {}.", e.what());
                HelloImGui::Log(HelloImGui::LogLevel::Error, "An error occurred: %s.", e.what());
            }
        }

        if (ImGui::MenuItem(ICON_FA_SAVE "  Export as SVG"))
        {
            try
            {
                auto basename = pfd::save_file("Base filename").result();
                if (basename.empty())
                    return;

                HelloImGui::Log(HelloImGui::LogLevel::Info, "Saving to base filename: %s.", basename.c_str());

                ofstream fileAll(basename + "_all2D.svg");
                fileAll << export_all_points_2d("svg");

                ofstream fileXYZ(basename + "_012.svg");
                fileXYZ << export_XYZ_points("svg");

                for (int y = 0; y < m_num_dimensions; ++y)
                    for (int x = 0; x < y; ++x)
                    {
                        ofstream fileXY(fmt::format("{:s}_{:d}{:d}.svg", basename, x, y));
                        fileXY << export_points_2d("svg", CAMERA_XY, {x, y, 2});
                    }
            }
            catch (const std::exception &e)
            {
                fmt::print(stderr, "An error occurred: {}.", e.what());
                HelloImGui::Log(HelloImGui::LogLevel::Error, "An error occurred: %s.", e.what());
            }
        }
    };

    m_params.callbacks.ShowStatus = [this]()
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetFontSize() * 0.15f);
        ImGui::Text("Reset + randomize: %3.3f ms   |   Sampling: %3.3f ms   |   Total: %3.3f ms (%3.0f pps)", m_time1,
                    m_time2, m_time1 + m_time2, m_point_count / (m_time1 + m_time2));
        // ImGui::SameLine();
        ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 16.f * ImGui::GetFontSize());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetFontSize() * 0.15f);
        ImGui::ToggleButton(ICON_FA_TERMINAL, &m_params.dockingParams.dockableWindows[1].isVisible);
    };

    m_params.callbacks.SetupImGuiStyle = [this]()
    {
        // set the default theme for first startup
        m_params.imGuiWindowParams.tweakedTheme.Theme = ImGuiTheme::ImGuiTheme_FromName("MaterialFlat");
    };
    m_params.callbacks.PostInit = [this]()
    {
        try
        {
#ifndef __EMSCRIPTEN__
            glEnable(GL_PROGRAM_POINT_SIZE);
            glEnable(GL_LINE_SMOOTH);
            glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
#endif

            m_point_shader =
                new Shader("Point shader", "shaders/points.vert", "shaders/points.frag", Shader::BlendMode::AlphaBlend);
            m_grid_shader =
                new Shader("Grid shader", "shaders/lines.vert", "shaders/lines.frag", Shader::BlendMode::AlphaBlend);
            m_point_2d_shader = new Shader("Point shader 2D", "shaders/points.vert", "shaders/points.frag",
                                           Shader::BlendMode::AlphaBlend);

            update_GPU_points();
            update_GPU_grids();

            HelloImGui::Log(HelloImGui::LogLevel::Info, "Successfully initialized GL!");
        }
        catch (const std::exception &e)
        {
            fmt::print(stderr, "Shader initialization failed!:\n\t{}.", e.what());
            HelloImGui::Log(HelloImGui::LogLevel::Error, "Shader initialization failed!:\n\t%s.", e.what());
        }
    };
    m_params.callbacks.ShowGui          = [this]() { draw_gui(); };
    m_params.callbacks.CustomBackground = [this]() { draw_scene(); };
}

SampleViewer::~SampleViewer()
{
    for (size_t i = 0; i < m_samplers.size(); ++i)
        delete m_samplers[i];
}

int2 SampleViewer::get_draw_range() const
{
    int start = 0, count = m_point_count;
    if (m_subset_by_coord)
        count = std::min(count, m_subset_count);
    else if (m_subset_by_index)
    {
        start = m_first_draw_point;
        count = std::min(m_point_draw_count, m_point_count - m_first_draw_point);
    }
    return {start, count};
}

void SampleViewer::draw_gui()
{
    auto &io = ImGui::GetIO();

    m_viewport_pos_GL = m_viewport_pos = {0, 0};
    m_viewport_size                    = io.DisplaySize;
    if (auto id = m_params.dockingParams.dockSpaceIdFromName("MainDockSpace"))
    {
        auto central_node = ImGui::DockBuilderGetCentralNode(*id);
        m_viewport_size   = int2{int(central_node->Size.x), int(central_node->Size.y)};
        m_viewport_pos    = int2{int(central_node->Pos.x), int(central_node->Pos.y)};
        // flip y coordinates between ImGui and OpenGL screen coordinates
        m_viewport_pos_GL =
            int2{int(central_node->Pos.x), int(io.DisplaySize.y - (central_node->Pos.y + central_node->Size.y))};
    }

    float radius = map_slider_to_radius(m_radius);
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);

    float4x4 mvp = m_camera[CAMERA_CURRENT].matrix(float(m_viewport_size.x) / m_viewport_size.y);

    if (m_view == CAMERA_2D)
    {
        for (int i = 0; i < m_num_dimensions - 1; ++i)
        {
            float4x4 pos      = layout_2d_matrix(m_num_dimensions, i, m_num_dimensions - 1);
            float4   text_pos = mul(mvp, mul(pos, float4{0.f, -0.5f, 0.0f, 1.0f}));
            float2   text_2d_pos((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
            draw_text(m_viewport_pos +
                          int2((text_2d_pos.x) * m_viewport_size.x, (1.f - text_2d_pos.y) * m_viewport_size.y + 16),
                      to_string(i), float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[16],
                      TextAlign_CENTER | TextAlign_BOTTOM);

            pos         = layout_2d_matrix(m_num_dimensions, 0, i + 1);
            text_pos    = mul(mvp, mul(pos, float4{-0.5f, 0.f, 0.0f, 1.0f}));
            text_2d_pos = float2((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
            draw_text(m_viewport_pos +
                          int2((text_2d_pos.x) * m_viewport_size.x - 4, (1.f - text_2d_pos.y) * m_viewport_size.y),
                      to_string(i + 1), float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[16],
                      TextAlign_RIGHT | TextAlign_MIDDLE);
        }
    }
    else
    {
        int2 range = get_draw_range();

        if (m_show_point_nums || m_show_point_coords)
            for (int p = range.x; p < range.x + range.y; ++p)
            {
                float3 point = m_3d_points[p];

                float4 text_pos = mul(mvp, float4{point.x, point.y, point.z, 1.f});
                float2 text_2d_pos((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
                if (m_show_point_nums)
                    draw_text(m_viewport_pos + int2((text_2d_pos.x) * m_viewport_size.x,
                                                    (1.f - text_2d_pos.y) * m_viewport_size.y - radius / 4),
                              fmt::format("{:d}", p), float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[12],
                              TextAlign_CENTER | TextAlign_BOTTOM);
                if (m_show_point_coords)
                    draw_text(m_viewport_pos + int2((text_2d_pos.x) * m_viewport_size.x,
                                                    (1.f - text_2d_pos.y) * m_viewport_size.y + radius / 4),
                              fmt::format("({:0.2f}, {:0.2f}, {:0.2f})", point.x + 0.5, point.y + 0.5, point.z + 0.5),
                              float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[11], TextAlign_CENTER | TextAlign_TOP);
            }
    }
}

void SampleViewer::draw_editor()
{
    auto big_header = [this](const char *label, ImGuiTreeNodeFlags flags = 0)
    {
        ImGui::PushFont(m_bold[16.f]);
        bool ret = ImGui::CollapsingHeader(label, flags | ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopFont();
        return ret;
    };

    auto tooltip = [](const char *text, float wrap_width = 400.f)
    {
        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(wrap_width);
            ImGui::TextWrapped("%s", text);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    };

    // =========================================================
    if (big_header(ICON_FA_SLIDERS_H "  Sampler settings"))
    // =========================================================
    {
        if (ImGui::BeginCombo("##", m_samplers[m_sampler]->name().c_str()))
        {
            for (int n = 0; n < (int)m_samplers.size(); n++)
            {
                Sampler   *sampler     = m_samplers[n];
                const bool is_selected = (m_sampler == n);
                if (ImGui::Selectable(sampler->name().c_str(), is_selected))
                {
                    m_sampler = n;
                    sampler->setJitter(m_jitter * 0.01f);
                    sampler->setRandomized(m_randomize);
                    update_GPU_points();
                    update_GPU_grids();
                    HelloImGui::Log(HelloImGui::LogLevel::Debug, "Switching to sampler %d: %s.", m_sampler,
                                    sampler->name().c_str());
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        tooltip("Set the sampler used to generate the points (Key: Up/Down)");

        if (ImGui::SliderInt("Num points", &m_target_point_count, 1, 1 << 17, "%d",
                             ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp))
        {
            HelloImGui::Log(HelloImGui::LogLevel::Debug, "Setting target point count to %d.", m_target_point_count);
            update_GPU_points();
            update_GPU_grids();
            HelloImGui::Log(HelloImGui::LogLevel::Debug, "Regenerated %d points.", m_point_count);
        }
        // now that the user has finished editing, sync the GUI value
        if (ImGui::IsItemDeactivatedAfterEdit())
            m_target_point_count = m_point_count;

        tooltip(
            "Set the target number of points to generate. For samplers that only support certain numbers of points "
            "(e.g. powers of 2) this target value will be snapped to the nearest admissable value (Key: Left/Right).");

        if (ImGui::SliderInt("Dimensions", &m_num_dimensions, 2, 10, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            update_GPU_points();
            update_GPU_grids();
        }
        tooltip("The number of dimensions to generate for each point (Key: D/d).");

        if (ImGui::Checkbox("Randomize", &m_randomize))
            update_GPU_points();
        tooltip("Whether to randomize the points, or show the deterministic configuration (Key: r/R).");

        if (ImGui::SliderFloat("Jitter", &m_jitter, 0.f, 100.f, "%3.1f%%"))
        {
            m_samplers[m_sampler]->setJitter(m_jitter * 0.01f);
            update_GPU_points();
            update_GPU_grids();
        }
        tooltip("How much the points should be jittered within their strata (Key: j/J).");

        // add optional widgets for OA samplers
        if (OrthogonalArray *oa = dynamic_cast<OrthogonalArray *>(m_samplers[m_sampler]))
        {
            // Controls for the strengths of the OA
            auto change_strength = [oa, this](int strength)
            {
                if ((unsigned)strength != oa->strength())
                {
                    oa->setStrength(strength);
                    update_GPU_points();
                    update_GPU_grids();
                }
            };
            int strength = oa->strength();
            if (ImGui::InputInt("Strength", &strength, 1))
                change_strength(std::max(2, strength));
            ImGui::SetItemTooltip("Key: T/t");

            // Controls for the offset type of the OA
            auto offset_names       = oa->offsetTypeNames();
            auto change_offset_type = [oa, this](int offset)
            {
                oa->setOffsetType(offset);
                m_jitter = oa->jitter();
                update_GPU_points();
                update_GPU_grids();
            };
            if (ImGui::BeginCombo("Offset type", offset_names[oa->offsetType()].c_str()))
            {
                for (unsigned n = 0; n < offset_names.size(); n++)
                {
                    const bool is_selected = (oa->offsetType() == n);
                    if (ImGui::Selectable(offset_names[n].c_str(), is_selected))
                        change_offset_type(n);

                    // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            tooltip("Choose the type of offset witin each stratum (Key: Shift+Up/Down).");
        }
        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }
    // =========================================================
    if (big_header(ICON_FA_CAMERA "  Camera/view"))
    // =========================================================
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2{ImGui::GetStyle().ItemSpacing.y, ImGui::GetStyle().ItemSpacing.y});
        const char *items[] = {"XY", "YZ", "XZ", "XYZ", "2D"};
        bool        is_selected;
        for (int i = 0; i < IM_ARRAYSIZE(items); ++i)
        {
            if (i > CAMERA_XY)
                ImGui::SameLine();

            is_selected = m_view == i;
            if (is_selected)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

            if (ImGui::Button(items[i], float2{40.f, 0.f}))
            {
                set_view((CameraType)i);
            }
            tooltip(fmt::format("(Key: {:d}).", (i + 1) % IM_ARRAYSIZE(items)).c_str());
            ImGui::PopStyleColor(is_selected);
        }
        ImGui::PopStyleVar();
        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }

    // =========================================================
    if (big_header(ICON_FA_EYE "  Display/visibility"))
    // =========================================================
    {
        ImGui::ColorEdit3("Color", (float *)&m_point_color);
        ImGui::SliderFloat("Radius", &m_radius, 0.f, 1.f, "");
        ImGui::SameLine();
        {
            ImGui::ToggleButton(ICON_FA_COMPRESS, &m_scale_radius_with_points);
            ImGui::SetItemTooltip("Scale radius with number of points");
        }

        ImGui::Checkbox("1D projections", &m_show_1d_projections);
        tooltip("Also show the X-, Y-, and Z-axis projections of the 3D points (Key: p)?");
        ImGui::Checkbox("Point indices", &m_show_point_nums);
        tooltip("Show the index above each point?");
        ImGui::Checkbox("Point coords", &m_show_point_coords);
        tooltip("Show the XYZ coordinates below each point?");
        ImGui::Checkbox("Coarse grid", &m_show_coarse_grid);
        tooltip("Show a coarse grid (Key: g)?");
        ImGui::Checkbox("Fine grid", &m_show_fine_grid);
        tooltip("Show a fine grid (Key: G)?");
        ImGui::Checkbox("Bounding box", &m_show_bbox);
        tooltip("Show the XY, YZ, and XZ bounding boxes (Key: b)?");

        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }

    // =========================================================
    if (big_header(ICON_FA_RANDOM "  Dimension mapping"))
    // =========================================================
    {
        if (ImGui::SliderInt3("XYZ", &m_dimension[0], 0, m_num_dimensions - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
            update_GPU_points(false);
        tooltip("Set which dimensions should be used for the XYZ dimensions of the displayed 3D points.");

        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }

    // =========================================================
    if (big_header(ICON_FA_FILTER "  Filter points"))
    // =========================================================
    {
        if (ImGui::Checkbox("Filter by point index", &m_subset_by_index))
            update_GPU_points(false);
        tooltip("Choose which points to show based on each point's index.");

        if (m_subset_by_index)
        {
            m_subset_by_coord = false;
            ImGui::SliderInt("First point", &m_first_draw_point, 0, m_point_count - 1, "%d",
                             ImGuiSliderFlags_AlwaysClamp);
            tooltip("Display points starting at this index.");

            ImGui::SliderInt("Num points##2", &m_point_draw_count, 1, m_point_count - m_first_draw_point, "%d",
                             ImGuiSliderFlags_AlwaysClamp);
            tooltip("Display this many points from the first index.");
        }

        if (ImGui::Checkbox("Filter by coordinates", &m_subset_by_coord))
            update_GPU_points(false);
        tooltip("Show only points that fall within an interval along one of its dimensions.");
        if (m_subset_by_coord)
        {
            m_subset_by_index = false;
            if (ImGui::SliderInt("Axis", &m_subset_axis, 0, m_num_dimensions - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
                update_GPU_points(false);
            tooltip("Filter points based on this axis.");

            if (ImGui::SliderInt("Num levels", &m_num_subset_levels, 1, m_point_count, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                update_GPU_points(false);
            tooltip("Split the unit interval along the chosen axis into this many consecutive levels (or bins).");

            if (ImGui::SliderInt("Level", &m_subset_level, 0, m_num_subset_levels - 1, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                update_GPU_points(false);
            tooltip("Show only points within this bin along the filtered axis.");
        }

        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }

    process_hotkeys();
}

void SampleViewer::process_hotkeys()
{
    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    Sampler         *sampler         = m_samplers[m_sampler];
    OrthogonalArray *oa              = dynamic_cast<OrthogonalArray *>(sampler);
    auto             change_strength = [oa, this](int strength)
    {
        if ((unsigned)strength != oa->strength())
        {
            oa->setStrength(strength);
            update_GPU_points();
            update_GPU_grids();
        }
    };
    auto change_offset_type = [oa, this](int offset)
    {
        oa->setOffsetType(offset);
        m_jitter = oa->jitter();
        update_GPU_points();
        update_GPU_grids();
    };

    if ((ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) &&
        !ImGui::IsKeyDown(ImGuiMod_Shift))
    {
        int delta        = ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : -1;
        m_sampler        = mod(m_sampler + delta, (int)m_samplers.size());
        Sampler *sampler = m_samplers[m_sampler];
        sampler->setJitter(m_jitter * 0.01f);
        sampler->setRandomized(m_randomize);
        update_GPU_points();
        update_GPU_grids();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        m_target_point_count =
            std::max(1u, ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? roundUpPow2(m_target_point_count + 1)
                                                                  : roundDownPow2(m_target_point_count - 1));
        update_GPU_points();
        update_GPU_grids();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_D))
    {
        m_num_dimensions = std::clamp(m_num_dimensions + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 1 : -1), 2, 10);
        m_dimension      = linalg::clamp(m_dimension, int3{0}, int3{m_num_dimensions - 1});
        update_GPU_points();
        update_GPU_grids();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_R))
    {
        if (ImGui::IsKeyDown(ImGuiMod_Shift))
        {
            m_randomize = true;
            sampler->setRandomized(true);
        }
        else
            m_randomize = !m_randomize;
        update_GPU_points();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_J))
    {
        m_jitter = std::clamp(m_jitter + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 10.f : -10.f), 0.f, 100.f);
        sampler->setJitter(m_jitter * 0.01f);
        update_GPU_points();
    }
    else if (oa && ImGui::IsKeyPressed(ImGuiKey_T))
        change_strength(std::max(2u, oa->strength() + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 1 : -1)));
    else if (oa && ImGui::IsKeyDown(ImGuiMod_Shift) &&
             (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)))
        change_offset_type(
            mod((int)oa->offsetType() + (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : -1), (int)NUM_OFFSET_TYPES));
    else if (ImGui::IsKeyPressed(ImGuiKey_1, false))
        set_view(CAMERA_XY);
    else if (ImGui::IsKeyPressed(ImGuiKey_2, false))
        set_view(CAMERA_YZ);
    else if (ImGui::IsKeyPressed(ImGuiKey_3, false))
        set_view(CAMERA_XZ);
    else if (ImGui::IsKeyPressed(ImGuiKey_4, false))
        set_view(CAMERA_CURRENT);
    else if (ImGui::IsKeyPressed(ImGuiKey_0, false))
        set_view(CAMERA_2D);
    else if (ImGui::IsKeyPressed(ImGuiKey_P))
        m_show_1d_projections = !m_show_1d_projections;
    else if (ImGui::IsKeyPressed(ImGuiKey_G))
    {
        if (ImGui::IsKeyDown(ImGuiMod_Shift))
            m_show_fine_grid = !m_show_fine_grid;
        else
            m_show_coarse_grid = !m_show_coarse_grid;
        update_GPU_grids();
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_B))
    {
        m_show_bbox = !m_show_bbox;
        update_GPU_grids();
    }
}

void SampleViewer::update_GPU_points(bool regenerate)
{
    //
    // Generate the point positions
    //
    if (regenerate)
    {
        try
        {
            Timer    timer;
            Sampler *generator = m_samplers[m_sampler];
            if (generator->randomized() != m_randomize)
                generator->setRandomized(m_randomize);

            generator->setDimensions(m_num_dimensions);

            int num_pts   = generator->setNumSamples(m_target_point_count);
            m_point_count = num_pts > 0 ? num_pts : m_target_point_count;

            m_time1 = timer.elapsed();

            m_points.resize(m_point_count, m_num_dimensions);
            m_3d_points.resize(m_point_count);

            timer.reset();
            for (int i = 0; i < m_point_count; ++i)
            {
                vector<float> r(m_num_dimensions, 0.5f);
                generator->sample(r.data(), i);
                for (int j = 0; j < m_points.sizeY(); ++j)
                    m_points(i, j) = r[j] - 0.5f;
            }
            m_time2 = timer.elapsed();
        }
        catch (const std::exception &e)
        {
            fmt::print(stderr, "An error occurred while generating points: {}.", e.what());
            HelloImGui::Log(HelloImGui::LogLevel::Error, "An error occurred while generating points: %s.", e.what());
            return;
        }
    }

    //
    // Populate point subsets
    //
    m_subset_points = m_points;
    m_subset_count  = m_point_count;
    if (m_subset_by_coord)
    {
        m_subset_count = 0;
        for (int i = 0; i < m_points.sizeX(); ++i)
        {
            float v = m_points(i, std::clamp(m_subset_axis, 0, m_num_dimensions - 1));
            v += 0.5f;
            if (v >= (m_subset_level + 0.0f) / m_num_subset_levels && v < (m_subset_level + 1.0f) / m_num_subset_levels)
            {
                // copy all dimensions (rows) of point i
                for (int j = 0; j < m_subset_points.sizeY(); ++j)
                    m_subset_points(m_subset_count, j) = m_points(i, j);
                ++m_subset_count;
            }
        }
    }

    for (size_t i = 0; i < m_3d_points.size(); ++i)
        m_3d_points[i] = float3{m_subset_points(i, m_dimension.x), m_subset_points(i, m_dimension.y),
                                m_subset_points(i, m_dimension.z)};

    //
    // Upload points to the GPU
    //

    m_point_shader->set_buffer("position", m_3d_points);

    // create a temporary matrix to store all the 2D projections of the points
    // each 2D plot actually needs 3D points, and there are num2DPlots of them
    int            num2DPlots = m_num_dimensions * (m_num_dimensions - 1) / 2;
    vector<float3> points2D(num2DPlots * m_subset_count);
    int            plot_index = 0;
    for (int y = 0; y < m_num_dimensions; ++y)
        for (int x = 0; x < y; ++x, ++plot_index)
            for (int i = 0; i < m_subset_count; ++i)
                points2D[plot_index * m_subset_count + i] = float3{m_subset_points(i, x), m_subset_points(i, y), 0.5f};

    m_point_2d_shader->set_buffer("position", points2D);
}

void SampleViewer::generate_grid(vector<float3> &positions, int grid_res)
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
            positions[idx++] = float3(j * fine_scale - 0.5f, i * coarse_scale - 0.5f, z * 0.5f);
            positions[idx++] = float3((j + 1) * fine_scale - 0.5f, i * coarse_scale - 0.5f, z * 0.5f);
            positions[idx++] = float3(i * coarse_scale - 0.5f, j * fine_scale - 0.5f, z * 0.5f);
            positions[idx++] = float3(i * coarse_scale - 0.5f, (j + 1) * fine_scale - 0.5f, z * 0.5f);
        }
    }
}

void SampleViewer::update_GPU_grids()
{
    vector<float3> bbox_grid, coarse_grid, fine_grid;
    generate_grid(bbox_grid, 1);
    generate_grid(coarse_grid, m_samplers[m_sampler]->coarseGridRes(m_point_count));
    generate_grid(fine_grid, m_point_count);
    m_coarse_line_count = coarse_grid.size();
    m_fine_line_count   = fine_grid.size();
    vector<float3> positions;
    positions.reserve(bbox_grid.size() + coarse_grid.size() + fine_grid.size());
    positions.insert(positions.end(), bbox_grid.begin(), bbox_grid.end());
    positions.insert(positions.end(), coarse_grid.begin(), coarse_grid.end());
    positions.insert(positions.end(), fine_grid.begin(), fine_grid.end());
    m_grid_shader->set_buffer("position", positions);
}

void SampleViewer::draw_2D_points_and_grid(const float4x4 &mvp, int dim_x, int dim_y, int plot_index)
{
    float4x4 pos = layout_2d_matrix(m_num_dimensions, dim_x, dim_y);

    // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
    // Render the point set
    m_point_2d_shader->set_uniform("mvp", mul(mvp, pos));
    float radius = map_slider_to_radius(m_radius / (m_num_dimensions - 1));
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);
    m_point_2d_shader->set_uniform("pointSize", radius);
    m_point_2d_shader->set_uniform("color", m_point_color);
    int2 range = get_draw_range();

    m_point_2d_shader->begin();
    m_point_2d_shader->draw_array(Shader::PrimitiveType::Point, m_subset_count * plot_index + range.x, range.y);
    m_point_2d_shader->end();

    // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
    if (m_show_bbox)
    {
        m_grid_shader->set_uniform("alpha", 1.0f);
        m_grid_shader->set_uniform("mvp", mul(mvp, mul(pos, m_camera[CAMERA_2D].arcball.matrix())));
        m_grid_shader->begin();
        m_grid_shader->draw_array(Shader::PrimitiveType::Line, 0, 8);
        m_grid_shader->end();
    }
    if (m_show_coarse_grid)
    {
        m_grid_shader->set_uniform("alpha", 0.6f);
        m_grid_shader->set_uniform("mvp", mul(mvp, mul(pos, m_camera[CAMERA_2D].arcball.matrix())));
        m_grid_shader->begin();
        m_grid_shader->draw_array(Shader::PrimitiveType::Line, 8, m_coarse_line_count);
        m_grid_shader->end();
    }
    if (m_show_fine_grid)
    {
        m_grid_shader->set_uniform("alpha", 0.2f);
        m_grid_shader->set_uniform("mvp", mul(mvp, mul(pos, m_camera[CAMERA_2D].arcball.matrix())));
        m_grid_shader->begin();
        m_grid_shader->draw_array(Shader::PrimitiveType::Line, 8 + m_coarse_line_count, m_fine_line_count);
        m_grid_shader->end();
    }
}

void SampleViewer::draw_scene()
{
    auto &io = ImGui::GetIO();

    //
    // clear the scene and set up viewports
    //
    try
    {
        const float4 background_color{0.f, 0.f, 0.f, 1.f};

        // first clear the entire window with the background color
        // display_size is the size of the window in pixels while accounting for dpi factor on retina screens.
        //     for retina displays, io.DisplaySize is the size of the window in points (logical pixels)
        //     but we need the size in pixels. So we scale io.DisplaySize by io.DisplayFramebufferScale
        auto display_size = int2{io.DisplaySize} * int2{io.DisplayFramebufferScale};
        CHK(glViewport(0, 0, display_size.x, display_size.y));
        CHK(glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]));
        CHK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        // now set up a new viewport for the rest of the drawing
        CHK(glViewport(
            m_viewport_pos_GL.x * io.DisplayFramebufferScale.x, m_viewport_pos_GL.y * io.DisplayFramebufferScale.y,
            m_viewport_size.x * io.DisplayFramebufferScale.x, m_viewport_size.y * io.DisplayFramebufferScale.y));

        // inform the arcballs of the viewport size
        for (int i = 0; i < NUM_CAMERA_TYPES; ++i)
            m_camera[i].arcball.set_size(m_viewport_size);

        // enable depth testing
        CHK(glEnable(GL_DEPTH_TEST));
        CHK(glDepthFunc(GL_LESS));
        CHK(glDepthMask(GL_TRUE));
    }
    catch (const std::exception &e)
    {
        fmt::print(stderr, "OpenGL drawing failed:\n\t{}.", e.what());
        HelloImGui::Log(HelloImGui::LogLevel::Error, "OpenGL drawing failed:\n\t%s.", e.what());
    }

    //
    // process camera movement
    //
    if (!io.WantCaptureMouse)
    {
        m_camera[CAMERA_NEXT].zoom = std::max(0.001, m_camera[CAMERA_NEXT].zoom * pow(1.1, io.MouseWheel));
        if (io.MouseClicked[0])
        {
            // on mouse down we start switching to a perspective camera
            // and start recording the arcball rotation in CAMERA_NEXT
            set_view(CAMERA_CURRENT);
            m_camera[CAMERA_NEXT].arcball.button(int2{io.MousePos} - m_viewport_pos, io.MouseDown[0]);
            m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
        }
        if (io.MouseReleased[0])
        {
            m_camera[CAMERA_NEXT].arcball.button(int2{io.MousePos} - m_viewport_pos, io.MouseDown[0]);
            // since the time between mouse down and up could be shorter
            // than the animation duration, we override the previous
            // camera's arcball on mouse up to complete the animation
            m_camera[CAMERA_PREVIOUS].arcball     = m_camera[CAMERA_NEXT].arcball;
            m_camera[CAMERA_PREVIOUS].camera_type = m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
        }

        m_camera[CAMERA_NEXT].arcball.motion(int2{io.MousePos} - m_viewport_pos);
    }

    //
    // perform animation of camera parameters, e.g. after clicking one of the view buttons
    //
    {
        CameraParameters &camera0 = m_camera[CAMERA_PREVIOUS];
        CameraParameters &camera1 = m_camera[CAMERA_NEXT];
        CameraParameters &camera  = m_camera[CAMERA_CURRENT];

        float time_now  = (float)ImGui::GetTime();
        float time_diff = (m_animate_start_time != 0.0f) ? (time_now - m_animate_start_time) : 1.0f;

        // interpolate between the "perspectiveness" at the previous keyframe,
        // and the "perspectiveness" of the currently selected camera
        float t = smoothStep(0.0f, 1.0f, time_diff);

        camera.zoom         = lerp(camera0.zoom, camera1.zoom, t);
        camera.view_angle   = lerp(camera0.view_angle, camera1.view_angle, t);
        camera.persp_factor = lerp(camera0.persp_factor, camera1.persp_factor, t);
        if (t >= 1.0f)
        {
            camera.camera_type         = camera1.camera_type;
            m_params.fpsIdling.fpsIdle = 9.f; // animation is done, reduce FPS
            m_animate_start_time       = 0.f;
        }

        // if we are dragging the mouse, then just use the current arcball
        // rotation, otherwise, interpolate between the previous and next camera
        // orientations
        if (io.MouseDown[0])
            camera.arcball = camera1.arcball;
        else
            camera.arcball.set_state(qslerp(camera0.arcball.state(), camera1.arcball.state(), t));
    }

    float4x4 mvp = m_camera[CAMERA_CURRENT].matrix(float(m_viewport_size.x) / m_viewport_size.y);

    //
    // Now render the points and grids
    //
    if (m_view == CAMERA_2D)
    {
        int plot_index = 0;
        for (int y = 0; y < m_num_dimensions; ++y)
            for (int x = 0; x < y; ++x, ++plot_index)
                draw_2D_points_and_grid(mvp, x, y, plot_index);
    }
    else
    {

        if (m_show_1d_projections)
        {
            // smash the points against the axes and draw
            float4x4 smashX =
                mul(mvp, mul(translation_matrix(float3{-0.51f, 0.f, 0.f}), scaling_matrix(float3{0.f, 1.f, 1.f})));
            draw_points(smashX, {0.8f, 0.3f, 0.3f});

            float4x4 smashY =
                mul(mvp, mul(translation_matrix(float3{0.f, -0.51f, 0.f}), scaling_matrix(float3{1.f, 0.f, 1.f})));
            draw_points(smashY, {0.3f, 0.8f, 0.3f});

            float4x4 smashZ =
                mul(mvp, mul(translation_matrix(float3{0.f, 0.f, -0.51f}), scaling_matrix(float3{1.f, 1.f, 0.f})));
            draw_points(smashZ, {0.3f, 0.3f, 0.8f});
        }

        draw_points(mvp, m_point_color);

        if (m_show_bbox)
            draw_grid(mvp, 1.0f, 0, 8);

        if (m_show_coarse_grid)
            draw_grid(mvp, 0.6f, 8, m_coarse_line_count);

        if (m_show_fine_grid)
            draw_grid(mvp, 0.2f, 8 + m_coarse_line_count, m_fine_line_count);
    }
}

void SampleViewer::set_view(CameraType view)
{
    m_animate_start_time                 = (float)ImGui::GetTime();
    m_camera[CAMERA_PREVIOUS]            = m_camera[CAMERA_CURRENT];
    m_camera[CAMERA_NEXT]                = m_camera[view];
    m_camera[CAMERA_NEXT].persp_factor   = (view == CAMERA_CURRENT) ? 1.0f : 0.0f;
    m_camera[CAMERA_NEXT].camera_type    = view;
    m_camera[CAMERA_CURRENT].camera_type = (view == m_camera[CAMERA_CURRENT].camera_type) ? view : CAMERA_CURRENT;
    m_view                               = view;

    m_params.fpsIdling.fpsIdle = 0.f; // during animation, increase FPS
}

void SampleViewer::draw_points(const float4x4 &mvp, const float3 &color)
{
    // Render the point set
    m_point_shader->set_uniform("mvp", mvp);
    float radius = map_slider_to_radius(m_radius);
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);
    m_point_shader->set_uniform("point_size", radius);
    m_point_shader->set_uniform("color", color);

    int2 range = get_draw_range();

    m_point_shader->begin();
    m_point_shader->draw_array(Shader::PrimitiveType::Point, range.x, range.y);
    m_point_shader->end();
}

void SampleViewer::draw_grid(const float4x4 &mvp, float alpha, uint32_t offset, uint32_t count)
{
    m_grid_shader->set_uniform("alpha", alpha);

    for (int axis = CAMERA_XY; axis <= CAMERA_XZ; ++axis)
    {
        if (m_camera[CAMERA_CURRENT].camera_type == axis || m_camera[CAMERA_CURRENT].camera_type == CAMERA_CURRENT)
        {
            m_grid_shader->set_uniform("mvp", mul(mvp, m_camera[axis].arcball.matrix()));
            m_grid_shader->begin();
            m_grid_shader->draw_array(Shader::PrimitiveType::Line, offset, count);
            m_grid_shader->end();
        }
    }
}

void SampleViewer::draw_text(const int2 &pos, const string &text, const float4 &color, ImFont *font, int align) const
{
    ImGui::PushFont(font);
    float2 apos{pos};
    float2 size = ImGui::CalcTextSize(text.c_str());

    if (align & TextAlign_LEFT)
        apos.x = pos.x;
    else if (align & TextAlign_CENTER)
        apos.x -= 0.5f * size.x;
    else if (align & TextAlign_RIGHT)
        apos.x -= size.x;

    if (align & TextAlign_TOP)
        apos.y = pos.y;
    else if (align & TextAlign_MIDDLE)
        apos.y -= 0.5f * size.y;
    else if (align & TextAlign_BOTTOM)
        apos.y -= size.y;

    ImGui::GetBackgroundDrawList()->AddText(apos, ImColor(color), text.c_str());
    ImGui::PopFont();
}

string SampleViewer::export_XYZ_points(const string &format)
{
    float radius = map_slider_to_radius(m_radius);
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);

    string out = (format == "eps") ? header_eps(m_point_color, 1.f, radius) : header_svg(m_point_color);

    float4x4 mvp = m_camera[CAMERA_CURRENT].matrix(1.0f);

    for (int axis = CAMERA_XY; axis <= CAMERA_XZ; ++axis)
    {
        if (format == "eps")
            out += draw_grids_eps(mul(mvp, m_camera[axis].arcball.matrix()), m_point_count,
                                  m_samplers[m_sampler]->coarseGridRes(m_point_count), m_show_fine_grid,
                                  m_show_coarse_grid, m_show_bbox);
        else
            out += draw_grids_svg(mul(mvp, m_camera[axis].arcball.matrix()), m_point_count,
                                  m_samplers[m_sampler]->coarseGridRes(m_point_count), m_show_fine_grid,
                                  m_show_coarse_grid, m_show_bbox);
    }

    out += (format == "eps") ? draw_points_eps(mvp, m_dimension, m_subset_points, get_draw_range())
                             : draw_points_svg(mvp, m_dimension, m_subset_points, get_draw_range(), radius);

    out += (format == "eps") ? footer_eps() : footer_svg();
    return out;
}

string SampleViewer::export_points_2d(const string &format, CameraType camera_type, int3 dim)
{
    float radius = map_slider_to_radius(m_radius);
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);

    string out = (format == "eps") ? header_eps(m_point_color, 1.f, radius) : header_svg(m_point_color);

    float4x4 mvp = m_camera[camera_type].matrix(1.0f);

    if (format == "eps")
    {
        out += draw_grids_eps(mvp, m_point_count, m_samplers[m_sampler]->coarseGridRes(m_point_count), m_show_fine_grid,
                              m_show_coarse_grid, m_show_bbox);
        out += draw_points_eps(mvp, dim, m_subset_points, get_draw_range());
    }
    else
    {
        out += draw_grids_svg(mvp, m_point_count, m_samplers[m_sampler]->coarseGridRes(m_point_count), m_show_fine_grid,
                              m_show_coarse_grid, m_show_bbox);
        out += draw_points_svg(mvp, dim, m_subset_points, get_draw_range(), radius);
    }

    out += (format == "eps") ? footer_eps() : footer_svg();
    return out;
}

string SampleViewer::export_all_points_2d(const string &format)
{
    float scale = 1.0f / (m_num_dimensions - 1);

    float radius = map_slider_to_radius(m_radius);
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);

    string out = (format == "eps") ? header_eps(m_point_color, scale, radius) : header_svg(m_point_color, scale);

    float4x4 mvp = m_camera[CAMERA_2D].matrix(1.0f);

    for (int y = 0; y < m_num_dimensions; ++y)
        for (int x = 0; x < y; ++x)
        {
            float4x4 pos = layout_2d_matrix(m_num_dimensions, x, y);

            if (format == "eps")
            {
                out += draw_grids_eps(mul(mvp, pos), m_point_count, m_samplers[m_sampler]->coarseGridRes(m_point_count),
                                      m_show_fine_grid, m_show_coarse_grid, m_show_bbox);
                out += draw_points_eps(mul(mvp, pos), {x, y, 2}, m_subset_points, get_draw_range());
            }
            else
            {
                out += draw_grids_svg(mul(mvp, pos), m_point_count, m_samplers[m_sampler]->coarseGridRes(m_point_count),
                                      m_show_fine_grid, m_show_coarse_grid, m_show_bbox);
                out += draw_points_svg(mul(mvp, pos), {x, y, 2}, m_subset_points, get_draw_range(), radius * scale);
            }
        }

    out += (format == "eps") ? footer_eps() : footer_svg();

    return out;
}

float4x4 CameraParameters::matrix(float window_aspect) const
{
    float4x4 model = scaling_matrix(float3(zoom));

    float fH = std::tan(view_angle / 360.0f * M_PI) * dnear;
    float fW = fH * window_aspect;

    float    oFF   = eye.z / dnear;
    float4x4 orth  = linalg::ortho_matrix(-fW * oFF, fW * oFF, -fH * oFF, fH * oFF, dnear, dfar);
    float4x4 frust = linalg::frustum_matrix(-fW, fW, -fH, fH, dnear, dfar);

    float4x4 proj   = lerp(orth, frust, persp_factor);
    float4x4 lookat = linalg::lookat_matrix(eye, center, up);
    float4x4 view   = arcball.matrix();
    return mul(proj, mul(lookat, mul(view, model)));
}

int main(int argc, char **argv)
{
    vector<string> args;
    bool           help                 = false;
    bool           error                = false;
    bool           launched_from_finder = false;

    try
    {
        for (int i = 1; i < argc; ++i)
        {
            if (strcmp("--help", argv[i]) == 0 || strcmp("-h", argv[i]) == 0)
                help = true;
            else if (strncmp("-psn", argv[i], 4) == 0)
                launched_from_finder = true;
            else
            {
                if (strncmp(argv[i], "-", 1) == 0)
                {
                    cerr << "Invalid argument: \"" << argv[i] << "\"!" << endl;
                    help  = true;
                    error = true;
                }
                args.push_back(argv[i]);
            }
        }
        (void)launched_from_finder;
    }
    catch (const std::exception &e)
    {
        cout << "Error: " << e.what() << endl;
        help  = true;
        error = true;
    }
    if (help)
    {
        cout << "Syntax: " << argv[0] << endl;
        cout << "Options:" << endl;
        cout << "   -h, --help                Display this message" << endl;
        return error ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    try
    {
        SampleViewer viewer;
        viewer.run();
    }
    catch (const std::runtime_error &e)
    {
        cerr << "Caught a fatal error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
