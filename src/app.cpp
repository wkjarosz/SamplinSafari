/** \file SampleViewer.cpp
    \author Wojciech Jarosz
*/

#include "app.h"

#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/hello_imgui_include_opengl.h" // cross-platform way to include OpenGL headers
#include "imgui_ext.h"
#include "imgui_internal.h"
#include "portable-file-dialogs.h"

#include <sampler/CSVFile.h>
#include <sampler/CascadedSobol.h>
#include <sampler/Faure.h>
#include <sampler/GrayCode.h>
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
#include <sampler/Sudoku.h>
#include <sampler/XiSequence.h>

#include "export_to_file.h"
#include "timer.h"

#include <cmath>
#include <fmt/core.h>
#include <fstream>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include "emscripten_browser_file.h"
#include <string_view>
using std::string_view;
#endif

using namespace linalg::ostream_overloads;

using std::to_string;

static bool g_show_modal = true;

static const auto rot90 = linalg::rotation_matrix(linalg::rotation_quat({0.f, 0.f, 1.f}, float(M_PI_2)));

static float map_slider_to_radius(float sliderValue)
{
    return sliderValue * sliderValue * 32.0f + 2.0f;
}

static float4x4 layout_2d_matrix(int num_dims, int2 dims)
{
    float cell_spacing = 1.f / (num_dims - 1);
    float cell_size    = 0.96f / (num_dims - 1);

    float2 offset = (dims - int2{0, 1} - float2{(num_dims - 2) / 2.0f}) * float2{1, -1};

    return mul(translation_matrix(float3{offset * cell_spacing, 1}), scaling_matrix(float3{float2{cell_size}, 1}));
}

static void draw_grid(Shader *shader, const float4x4 &mat, int2 start, int2 count, float alpha)
{
    shader->set_uniform("alpha", alpha);

    // draw vertical lines
    shader->set_uniform("mvp", mat);
    shader->begin();
    shader->draw_array(Shader::PrimitiveType::Line, start.x, count.x);
    shader->end();

    // draw horizontal lines
    shader->set_uniform("mvp", mul(mat, rot90));
    shader->begin();
    shader->draw_array(Shader::PrimitiveType::Line, start.y, count.y);
    shader->end();
}

SampleViewer::SampleViewer()
{
    m_custom_line_counts.fill(1);

    m_samplers.emplace_back(new Random(m_num_dimensions));
    m_samplers.emplace_back(new Jittered(1, 1, m_jitter * 0.01f));
    m_samplers.emplace_back(new CorrelatedMultiJitteredInPlace(1, 1, m_num_dimensions, false, m_jitter * 0.01f, false));
    m_samplers.emplace_back(new CorrelatedMultiJitteredInPlace(1, 1, m_num_dimensions, false, m_jitter * 0.01f, true));
    m_samplers.emplace_back(new CMJNDInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f));
    m_samplers.emplace_back(new SudokuInPlace(1, 1, m_num_dimensions, false, 0.0f, false));
    m_samplers.emplace_back(new SudokuInPlace(1, 1, m_num_dimensions, false, 0.0f, true));
    m_samplers.emplace_back(new BoseOAInPlace(1, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BoseGaloisOAInPlace(1, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BushOAInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BushGaloisOAInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new AddelmanKempthorneOAInPlace(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BoseBushOA(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BoseBushOAInPlace(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new NRooksInPlace(m_num_dimensions, 1, false, m_jitter * 0.01f));
    m_samplers.emplace_back(new Sobol(m_num_dimensions));
    m_samplers.emplace_back(new SSobol(m_num_dimensions));
    m_samplers.emplace_back(new ZeroTwo(1, m_num_dimensions, false));
    m_samplers.emplace_back(new ZeroTwo(1, m_num_dimensions, true));
    m_samplers.emplace_back(
        new CascadedSobol(HelloImGui::assetFileFullPath("cascaded_sobol_init_tab.dat"), m_num_dimensions));
    m_samplers.emplace_back(new Faure(m_num_dimensions, 1));
    m_samplers.emplace_back(new Halton(m_num_dimensions));
    m_samplers.emplace_back(new HaltonZaremba(m_num_dimensions));
    m_samplers.emplace_back(new Hammersley<Halton>(m_num_dimensions, 1));
    m_samplers.emplace_back(new Hammersley<HaltonZaremba>(m_num_dimensions, 1));
    m_samplers.emplace_back(new LarcherPillichshammerGK(3, 1, false));
    m_samplers.emplace_back(new GrayCode(4));
    m_samplers.emplace_back(new XiSequence(1));
    m_samplers.emplace_back(new CSVFile());

    m_camera[CAMERA_XY].arcball.set_state({0, 0, 0, 1});
    m_camera[CAMERA_XY].persp_factor = 0.0f;
    m_camera[CAMERA_XY].camera_type  = CAMERA_XY;

    m_camera[CAMERA_ZY].arcball.set_state(linalg::rotation_quat({0.f, 1.f, 0.f}, float(M_PI_2)));
    m_camera[CAMERA_ZY].persp_factor = 0.0f;
    m_camera[CAMERA_ZY].camera_type  = CAMERA_ZY;

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

        for (auto font_size :
             {14.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 15.f, 16.f, 17.f, 18.f, 20.f, 22.f, 24.f, 26.f, 28.f, 30.f})
        {
            m_regular[(int)font_size] = HelloImGui::LoadFontTTF_WithFontAwesomeIcons(roboto_r, font_size);
            m_bold[(int)font_size]    = HelloImGui::LoadFontTTF_WithFontAwesomeIcons(roboto_b, font_size);
        }
    };

    m_params.callbacks.ShowMenus = []()
    {
        string text = ICON_FA_INFO_CIRCLE;
        auto   posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(text.c_str()).x -
                     ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (posX > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(posX);
        if (ImGui::MenuItem(text))
            g_show_modal = true;
    };

    m_params.callbacks.ShowAppMenuItems = [this]()
    {
        auto save_files = [this](const string &basename, const string &ext)
        {
            vector<string> saved_files;
            if (ext == "csv")
            {
                HelloImGui::Log(HelloImGui::LogLevel::Info, "Saving to: %s.", basename.c_str());
                saved_files.push_back(basename);
                ofstream csv_file(saved_files.back());
                csv_file << draw_points_csv(m_subset_points, get_draw_range());
            }
            else
            {
                HelloImGui::Log(HelloImGui::LogLevel::Info, "Saving to base filename: %s.", basename.c_str());

                saved_files.push_back(basename + "_all2D." + ext);
                ofstream fileAll(saved_files.back());
                fileAll << export_all_points_2d(ext);

                saved_files.push_back(basename + "_012." + ext);
                ofstream fileXYZ(saved_files.back());
                fileXYZ << export_XYZ_points(ext);

                for (int y = 0; y < m_num_dimensions; ++y)
                    for (int x = 0; x < y; ++x)
                    {
                        saved_files.push_back(fmt::format("{:s}_{:d}{:d}.{}", basename, x, y, ext));
                        ofstream fileXY(saved_files.back());
                        fileXY << export_points_2d(ext, CAMERA_XY, {x, y, 2});
                    }
            }
            return saved_files;
        };

        for (string ext : {"eps", "svg", "csv"})
        {
#ifndef __EMSCRIPTEN__
            if (ImGui::MenuItem(fmt::format("{}  Export as {}...", ICON_FA_SAVE, to_upper(ext))))
            {
                try
                {
                    auto basename = pfd::save_file("Base filename").result();
                    if (!basename.empty())
                        (void)save_files(basename, ext);
                }
                catch (const std::exception &e)
                {
                    fmt::print(stderr, "An error occurred while exporting to {}: {}.", ext, e.what());
                    HelloImGui::Log(HelloImGui::LogLevel::Error,
                                    fmt::format("An error occurred while exporting to {}: {}.", ext, e.what()).c_str());
                }
            }
#else
            if (ImGui::BeginMenu(fmt::format("{}  Download as {}...", ICON_FA_SAVE, to_upper(ext)).c_str()))
            {
                string basename;

                if (ImGui::InputText("Base filename", &basename, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    try
                    {
                        ImGui::CloseCurrentPopup();
                        vector<string> saved_files = save_files(basename, ext);
                        for (auto &saved_file : saved_files)
                            emscripten_run_script(fmt::format("saveFileFromMemoryFSToDisk('{}');", saved_file).c_str());
                    }
                    catch (const std::exception &e)
                    {
                        fmt::print(stderr, "An error occurred while exporting to {}: {}.", ext, e.what());
                        HelloImGui::Log(
                            HelloImGui::LogLevel::Error,
                            fmt::format("An error occurred while exporting to {}: {}.", ext, e.what()).c_str());
                    }
                }
                ImGui::EndMenu();
            }
#endif
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
            m_2d_point_shader = new Shader("Point shader 2D", "shaders/points.vert", "shaders/points.frag",
                                           Shader::BlendMode::AlphaBlend);
            m_2d_grid_shader =
                new Shader("Grid shader 2D", "shaders/lines.vert", "shaders/lines.frag", Shader::BlendMode::AlphaBlend);

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
    m_idling_backup                     = m_params.fpsIdling.enableIdling;
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

    // draw text labels
    if (m_view == CAMERA_2D)
    {
        // draw the text labels for the grid of 2D projections
        for (int i = 0; i < m_num_dimensions - 1; ++i)
        {
            float4x4 pos      = layout_2d_matrix(m_num_dimensions, int2{i, m_num_dimensions - 1});
            float4   text_pos = mul(mvp, mul(pos, float4{0.f, -0.5f, 0.0f, 1.0f}));
            float2   text_2d_pos((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
            draw_text(m_viewport_pos + int2(int((text_2d_pos.x) * m_viewport_size.x),
                                            int((1.f - text_2d_pos.y) * m_viewport_size.y) + 16),
                      to_string(i), float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[16],
                      TextAlign_CENTER | TextAlign_BOTTOM);

            pos         = layout_2d_matrix(m_num_dimensions, int2{0, i + 1});
            text_pos    = mul(mvp, mul(pos, float4{-0.5f, 0.f, 0.0f, 1.0f}));
            text_2d_pos = float2((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
            draw_text(m_viewport_pos + int2(int((text_2d_pos.x) * m_viewport_size.x) - 4,
                                            int((1.f - text_2d_pos.y) * m_viewport_size.y)),
                      to_string(i + 1), float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[16],
                      TextAlign_RIGHT | TextAlign_MIDDLE);
        }
    }
    else
    {
        int2 range = get_draw_range();

        // draw the index or coordinate labels around each point
        if (m_show_point_nums || m_show_point_coords)
            for (int p = range.x; p < range.x + range.y; ++p)
            {
                float4 text_pos = mul(mvp, float4{m_3d_points[p] - 0.5f, 1.f});
                float2 text_2d_pos((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
                int2   draw_pos = m_viewport_pos + int2{int((text_2d_pos.x) * m_viewport_size.x),
                                                      int((1.f - text_2d_pos.y) * m_viewport_size.y)};
                if (m_show_point_nums)
                    draw_text(draw_pos - int2{0, int(radius / 4)}, fmt::format("{:d}", p),
                              float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[12], TextAlign_CENTER | TextAlign_BOTTOM);
                if (m_show_point_coords)
                    draw_text(draw_pos + int2{0, int(radius / 4)},
                              fmt::format("({:0.2f}, {:0.2f}, {:0.2f})", m_3d_points[p].x, m_3d_points[p].y,
                                          m_3d_points[p].z),
                              float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular[11], TextAlign_CENTER | TextAlign_TOP);
            }
    }

    draw_about_dialog();
}

void SampleViewer::draw_about_dialog()
{
    if (g_show_modal)
    {
        ImGui::OpenPopup("About");

        // I think HelloGui's way of rendering the frame multiple times before it determines window sizes is
        // closing this popup before we see the app, so we need this hack to show it at startup
        static int count = 0;
        if (count > 0)
            g_show_modal = false;
        count++;
    }

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();

    if (ImGui::BeginPopup("About", ImGuiWindowFlags_AlwaysAutoResize))
    {
        const int col_width[2] = {11, 34};

        ImGui::Spacing();

        if (ImGui::BeginTable("about_table1", 2))
        {
            ImGui::TableSetupColumn("one", ImGuiTableColumnFlags_WidthFixed, HelloImGui::EmSize() * col_width[0]);
            ImGui::TableSetupColumn("two", ImGuiTableColumnFlags_WidthFixed, HelloImGui::EmSize() * col_width[1]);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // right align the image
            {
                auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - 128.f - ImGui::GetScrollX() -
                             2 * ImGui::GetStyle().ItemSpacing.x);
                if (posX > ImGui::GetCursorPosX())
                    ImGui::SetCursorPosX(posX);
            }
            HelloImGui::ImageFromAsset("app_settings/icon.png", {128, 128}); // show the app icon

            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + HelloImGui::EmSize() * col_width[1]);

            ImGui::PushFont(m_bold[30]);
            ImGui::Text("Samplin' Safari");
            ImGui::PopFont();

            ImGui::PushFont(m_bold[18]);
            ImGui::Text(version());
            ImGui::PopFont();
            ImGui::PushFont(m_regular[10]);
            ImGui::Text(fmt::format("Built using the {} backend on {}.", backend(), build_timestamp()));
            ImGui::PopFont();

            ImGui::Spacing();

            ImGui::PushFont(m_bold[16]);
            ImGui::Text("Samplin' Safari is a research tool to visualize and interactively inspect high-dimensional "
                        "(quasi) Monte Carlo samplers.");
            ImGui::PopFont();

            ImGui::Spacing();

            ImGui::PopTextWrapPos();
            ImGui::EndTable();
        }

        auto right_align = [](const string &text)
        {
            auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(text.c_str()).x -
                         ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
            if (posX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(posX);
            ImGui::Text(text);
        };

        auto add_library = [this, right_align, col_width](string name, string desc)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::PushFont(m_bold[14]);
            right_align(name);
            ImGui::PopFont();

            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + HelloImGui::EmSize() * (col_width[1] - 1));
            ImGui::PushFont(m_regular[14]);
            ImGui::Text(desc);
            ImGui::PopFont();
        };

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("It is freely available under a 3-clause BSD license and is developed by Wojciech Jarosz, "
                    "initially as part of the publication:");

        ImGui::Spacing();

        ImGui::Indent(HelloImGui::EmSize() * 1);
        ImGui::PushFont(m_bold[14]);
        ImGui::Text("Orthogonal Array Sampling for Monte Carlo Rendering");
        ImGui::PopFont();
        ImGui::Text("Wojciech Jarosz, Afnan Enayet, Andrew Kensler, Charlie Kilpatrick, Per Christensen.\n"
                    "In Computer Graphics Forum (Proceedings of EGSR), 38(4), July 2019.");
        ImGui::Unindent(HelloImGui::EmSize() * 1);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Text("It additionally makes use of the following external libraries:\n\n");

        if (ImGui::BeginTable("about_table2", 2))
        {
            ImGui::TableSetupColumn("one", ImGuiTableColumnFlags_WidthFixed, HelloImGui::EmSize() * col_width[0]);
            ImGui::TableSetupColumn("two", ImGuiTableColumnFlags_WidthFixed, HelloImGui::EmSize() * col_width[1]);

            add_library("Dear ImGui", "Omar Cornut's immediate-mode graphical user interface for C++");
            add_library("Hello ImGui", "Pascal Thomet's cross-platform starter-kit for Dear ImGui");
            add_library("NanoGUI", "Bits of code from Wenzel Jakob's BSD-licensed NanoGUI library (the shader "
                                   "abstraction and arcball)");
            add_library("{fmt}", "A modern formatting library");
            add_library("halton/sobol", "Leonhard Gruenschloss's MIT-licensed code for Halton and Sobol sequences");
            add_library("linalg", "Sterling Orsten's public domain, single header short vector math library for C++");
            add_library("portable-file-dialogs",
                        "Sam Hocevar's WTFPL portable GUI dialogs library, C++11, single-header");
            add_library("pcg32", "Wenzel Jakob's tiny self-contained C++ version of Melissa O'Neill's PCG32 "
                                 "pseudorandom number generator");
            add_library("galois++", "My small C++ library for arithmetic over general Galois fields based on "
                                    "Art Owen's code in Statlib");
            ImGui::EndTable();
        }

        ImGui::SetKeyboardFocusHere();
        if (ImGui::Button("Dismiss", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space))
            ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();

        ImGui::EndPopup();
    }
}

void SampleViewer::draw_editor()
{
    auto big_header = [this](const char *label, ImGuiTreeNodeFlags flags = 0)
    {
        ImGui::PushFont(m_bold[16]);
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

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);

    // =========================================================
    if (big_header(ICON_FA_SLIDERS_H "  Sampler settings"))
    // =========================================================
    {
        if (ImGui::BeginCombo("##Sampler combo", m_samplers[m_sampler]->name().c_str()))
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
                    m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;

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

        CSVFile *csv = dynamic_cast<CSVFile *>(m_samplers[m_sampler]);

        if (csv)
        {
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_FOLDER_OPEN))
            {
#ifdef __EMSCRIPTEN__
                static CSVFile *s_csv_file = csv;
                auto            handle_upload_file =
                    [](const string &filename, const string &mime_type, string_view buffer, void *my_data = nullptr)
                {
                    auto that{reinterpret_cast<SampleViewer *>(my_data)};
                    HelloImGui::Log(HelloImGui::LogLevel::Debug, "Loading file '%s' of mime type '%s' ...",
                                    filename.c_str(), mime_type.c_str());
                    s_csv_file->read(filename, buffer);
                    that->m_gpu_points_dirty = that->m_cpu_points_dirty = that->m_gpu_grids_dirty = true;
                };

                // open the browser's file selector, and pass the file to the upload handler
                emscripten_browser_file::upload(".csv,.txt", handle_upload_file, this);
                HelloImGui::Log(HelloImGui::LogLevel::Debug, "Requesting file from user");
#else
                auto result = pfd::open_file("Open CSV file", "", {"CSV files", "*.csv *.txt"}).result();
                if (!result.empty())
                {
                    HelloImGui::Log(HelloImGui::LogLevel::Debug, "Loading file '%s'...", result.front().c_str());
                    csv->read(result.front());
                    m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
                }
#endif
            }
        }

        int num_points = m_point_count;
        if (ImGui::SliderInt("Num points", &num_points, 1, 1 << 17, "%d", ImGuiSliderFlags_Logarithmic))
        {
            m_target_point_count = std::clamp(num_points, 1, 1 << 20);
            HelloImGui::Log(HelloImGui::LogLevel::Debug, "Setting target point count to %d.", m_target_point_count);
            m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
            HelloImGui::Log(HelloImGui::LogLevel::Debug, "Regenerated %d points.", m_point_count);
        }

        tooltip(
            "Set the target number of points to generate. For samplers that only support certain numbers of points "
            "(e.g. powers of 2) this target value will be snapped to the nearest admissable value (Key: Left/Right).");

        if (ImGui::SliderInt("Dimensions", &m_num_dimensions, 2, MAX_DIMENSIONS, "%d", ImGuiSliderFlags_AlwaysClamp))
            m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
        tooltip("The number of dimensions to generate for each point (Key: D/d).");

        if (!csv)
        {
            if (ImGui::Checkbox("Randomize", &m_randomize))
                m_gpu_points_dirty = m_cpu_points_dirty = true;

            tooltip("Whether to randomize the points, or show the deterministic configuration (Key: r/R).");

            if (ImGui::SliderFloat("Jitter", &m_jitter, 0.f, 100.f, "%3.1f%%"))
            {
                m_samplers[m_sampler]->setJitter(m_jitter * 0.01f);
                m_gpu_points_dirty = m_cpu_points_dirty = true;
            }
            tooltip("How much the points should be jittered within their strata (Key: j/J).");
        }

        // add optional widgets for OA samplers
        if (OrthogonalArray *oa = dynamic_cast<OrthogonalArray *>(m_samplers[m_sampler]))
        {
            // Controls for the strengths of the OA
            auto change_strength = [oa, this](int strength)
            {
                if ((unsigned)strength != oa->strength())
                {
                    oa->setStrength(strength);
                    m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
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
                m_jitter           = oa->jitter();
                m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
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
        const char *items[] = {"XY", "XZ", "ZY", "XYZ", "2D"};
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
        ImGui::ColorEdit3("Bg color", (float *)&m_bg_color);
        ImGui::ColorEdit3("Point color", (float *)&m_point_color);
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
        ImGui::Checkbox("Fine grid", &m_show_fine_grid);
        tooltip("Show a fine grid (Key: G)?");
        ImGui::Checkbox("Coarse grid", &m_show_coarse_grid);
        tooltip("Show a coarse grid (Key: g)?");

        ImGui::Checkbox("Custom grid", &m_show_custom_grid);
        if (m_show_custom_grid)
        {
            float available = ImGui::GetContentRegionAvail().x * 0.7f - 2 * HelloImGui::EmSize();
            ImGui::Indent(2 * HelloImGui::EmSize());
            ImGui::SetNextItemWidth(available);
            ImGui::TextUnformatted("Number of subdivisions");
            ImGui::SameLine();
            ImGui::SetCursorPosX(available + 3 * HelloImGui::EmSize());
            ImGui::TextUnformatted("Dim");
            for (int i = 0; i < m_num_dimensions; ++i)
            {
                ImGui::SetNextItemWidth(available);
                if (ImGui::SliderInt(fmt::format("##Subds for dim {}", i).c_str(), &m_custom_line_counts[i], 1, 1000,
                                     "%d", ImGuiSliderFlags_Logarithmic))
                {
                    m_custom_line_counts[i] = std::max(1, m_custom_line_counts[i]);
                    m_gpu_grids_dirty       = true;
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(available + 4 * HelloImGui::EmSize());
                ImGui::TextUnformatted(to_string(i).c_str());
            }
            tooltip("Create a grid with this many subdivisions along each of the dimensions.");
            ImGui::Unindent();
        }
        ImGui::Checkbox("Bounding box", &m_show_bbox);
        tooltip("Show the XY, YZ, and XZ bounding boxes (Key: b)?");

        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }

    // =========================================================
    if (big_header(ICON_FA_RANDOM "  Dimension mapping"))
    // =========================================================
    {
        int3 dims = m_dimension;
        if (ImGui::SliderInt3("XYZ", &dims[0], 0, m_num_dimensions - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            m_gpu_points_dirty = true;
            m_dimension        = dims;
        }

        tooltip("Set which dimensions should be used for the XYZ dimensions of the displayed 3D points.");

        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }

    // =========================================================
    if (big_header(ICON_FA_FILTER "  Filter points"))
    // =========================================================
    {
        if (ImGui::Checkbox("Filter by point index", &m_subset_by_index))
            m_gpu_points_dirty = true;
        tooltip("Choose which points to show based on each point's index.");

        if (m_subset_by_index)
        {
            float indent_w = 2 * HelloImGui::EmSize();
            float widget_w = ImGui::GetContentRegionAvail().x * 0.7f - indent_w;
            ImGui::Indent(indent_w);
            ImGui::PushItemWidth(widget_w);
            m_subset_by_coord    = false;
            static bool disjoint = true;
            ImGui::Checkbox("Disjoint batches", &disjoint);
            ImGui::SliderInt("First point", &m_first_draw_point, 0, m_point_count - 1, "%d",
                             ImGuiSliderFlags_AlwaysClamp);
            if (disjoint)
                // round to nearest multiple of m_point_draw_count
                m_first_draw_point = (m_first_draw_point / m_point_draw_count) * m_point_draw_count;

            tooltip("Display points starting at this index.");

            ImGui::SliderInt("Num points##2", &m_point_draw_count, 1, m_point_count - m_first_draw_point, "%d",
                             ImGuiSliderFlags_AlwaysClamp);
            tooltip("Display this many points from the first index.");
            ImGui::PopItemWidth();
            ImGui::Unindent();
        }

        if (ImGui::Checkbox("Filter by coordinates", &m_subset_by_coord))
            m_gpu_points_dirty = true;
        tooltip("Show only points that fall within an interval along one of its dimensions.");
        if (m_subset_by_coord)
        {
            float indent_w = 2 * HelloImGui::EmSize();
            float widget_w = ImGui::GetContentRegionAvail().x * 0.7f - indent_w;
            ImGui::Indent(indent_w);
            ImGui::PushItemWidth(widget_w);
            m_subset_by_index = false;
            if (ImGui::SliderInt("Axis", &m_subset_axis, 0, m_num_dimensions - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
                m_gpu_points_dirty = true;
            tooltip("Filter points based on this axis.");

            if (ImGui::SliderInt("Num levels", &m_num_subset_levels, 1, m_point_count, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                m_gpu_points_dirty = true;
            tooltip("Split the unit interval along the chosen axis into this many consecutive levels (or bins).");

            if (ImGui::SliderInt("Level", &m_subset_level, 0, m_num_subset_levels - 1, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                m_gpu_points_dirty = true;
            tooltip("Show only points within this bin along the filtered axis.");
            ImGui::PopItemWidth();
            ImGui::Unindent();
        }

        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }
    ImGui::PopItemWidth();
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
            m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
        }
    };
    auto change_offset_type = [oa, this](int offset)
    {
        oa->setOffsetType(offset);
        m_jitter           = oa->jitter();
        m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
    };

    if ((ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) &&
        !ImGui::IsKeyDown(ImGuiMod_Shift))
    {
        int delta        = ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : -1;
        m_sampler        = mod(m_sampler + delta, (int)m_samplers.size());
        Sampler *sampler = m_samplers[m_sampler];
        sampler->setJitter(m_jitter * 0.01f);
        sampler->setRandomized(m_randomize);
        m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        m_target_point_count =
            std::max(1u, ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? roundUpPow2(m_target_point_count + 1)
                                                                  : roundDownPow2(m_target_point_count - 1));

        HelloImGui::Log(HelloImGui::LogLevel::Debug, "Setting target point count to %d.", m_target_point_count);
        m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
        HelloImGui::Log(HelloImGui::LogLevel::Debug, "Regenerated %d points.", m_point_count);
        // m_target_point_count = m_point_count;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_D))
    {
        m_num_dimensions   = std::clamp(m_num_dimensions + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 1 : -1), 2, 10);
        m_gpu_points_dirty = m_cpu_points_dirty = m_gpu_grids_dirty = true;
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
        m_gpu_points_dirty = m_cpu_points_dirty = true;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_J))
    {
        m_jitter = std::clamp(m_jitter + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 10.f : -10.f), 0.f, 100.f);
        sampler->setJitter(m_jitter * 0.01f);
        m_gpu_points_dirty = m_cpu_points_dirty = true;
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
        set_view(CAMERA_XZ);
    else if (ImGui::IsKeyPressed(ImGuiKey_3, false))
        set_view(CAMERA_ZY);
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
        m_gpu_grids_dirty = true;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_B))
    {
        m_show_bbox       = !m_show_bbox;
        m_gpu_grids_dirty = true;
    }
}

void SampleViewer::update_points(bool regenerate)
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
            m_point_count = num_pts >= 0 ? num_pts : m_target_point_count;

            m_time1 = timer.elapsed();

            m_points.resize(m_point_count, m_num_dimensions);
            m_3d_points.resize(m_point_count);

            timer.reset();
            for (int i = 0; i < m_point_count; ++i)
            {
                vector<float> r(m_num_dimensions, 0.5f);
                generator->sample(r.data(), i);
                for (int j = 0; j < m_points.sizeY(); ++j)
                    m_points(i, j) = r[j];
            }
            m_time2 = timer.elapsed();
        }
        catch (const std::exception &e)
        {
            fmt::print(stderr, "An error occurred while generating points: {}.", e.what());
            HelloImGui::Log(HelloImGui::LogLevel::Error, "An error occurred while generating points: %s.", e.what());
            return;
        }
        m_cpu_points_dirty = false;
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
            if (v >= (m_subset_level + 0.0f) / m_num_subset_levels && v < (m_subset_level + 1.0f) / m_num_subset_levels)
            {
                // copy all dimensions (rows) of point i
                for (int j = 0; j < m_subset_points.sizeY(); ++j)
                    m_subset_points(m_subset_count, j) = m_points(i, j);
                ++m_subset_count;
            }
        }
    }

    int3 dims = linalg::clamp(m_dimension, int3{0}, int3{m_num_dimensions - 1});
    for (size_t i = 0; i < m_3d_points.size(); ++i)
        m_3d_points[i] = float3{m_subset_points(i, dims.x), m_subset_points(i, dims.y), m_subset_points(i, dims.z)};

    //
    // Upload points to the GPU
    //

    m_point_shader->set_buffer("position", m_3d_points);

    //
    // create a temporary array to store all the 2D projections of the points.
    // each 2D plot actually needs 3D points, and there are num2DPlots of them
    int            num2DPlots = m_num_dimensions * (m_num_dimensions - 1) / 2;
    vector<float3> points2D(num2DPlots * m_subset_count);
    for (int y = 0, plot_index = 0; y < m_num_dimensions; ++y)
        for (int x = 0; x < y; ++x, ++plot_index)
            for (int i = 0; i < m_subset_count; ++i)
                points2D[plot_index * m_subset_count + i] = float3{m_subset_points(i, x), m_subset_points(i, y), 0.5f};

    m_2d_point_shader->set_buffer("position", points2D);

    m_gpu_points_dirty = false;
}

void SampleViewer::update_grids()
{
    auto generate_lines = [](vector<float3> &positions, int grid_res, int axis = 0)
    {
        float coarse_scale = 1.f / grid_res;
        for (int i = 0; i <= grid_res; ++i)
        {
            float2 a{0.f}, b{1.f};
            a[axis] = b[axis] = i * coarse_scale;

            positions.emplace_back(float3{a, 0.f});
            positions.emplace_back(float3{b, 0.f});
        }
        return 2 * (grid_res + 1);
    };

    // auto generate_grid = [&generate_lines](vector<float3> &positions, int2 grid_res)
    // {
    //     auto x = generate_lines(positions, grid_res.x, 0);
    //     auto y = generate_lines(positions, grid_res.y, 1);
    //     return x + y;
    // };

    auto count = [](int res) { return 2 * (res + 1); };

    // expected vertex counts
    int bbox_line_count = count(1);
    m_coarse_line_count = count(m_samplers[m_sampler]->coarseGridRes(m_point_count));
    m_fine_line_count   = count(m_point_count);

    vector<float3> positions;
    positions.reserve(bbox_line_count + m_coarse_line_count + m_fine_line_count);

    // generate vertices and ensure the counts are right
    if (bbox_line_count != generate_lines(positions, 1))
        assert(false);
    if (m_coarse_line_count != generate_lines(positions, m_samplers[m_sampler]->coarseGridRes(m_point_count)))
        assert(false);
    if (m_fine_line_count != generate_lines(positions, m_point_count))
        assert(false);

    m_grid_shader->set_buffer("position", positions);
    m_gpu_grids_dirty = false;

    // handle all 2D grid projections
    // create a temporary array to store the vertices of grid lines in all 2D projections
    m_custom_line_count = 0;
    for (int d = 0; d < m_num_dimensions; ++d)
        m_custom_line_count += m_custom_line_counts[d] + 1;
    positions.resize(0);
    positions.reserve(2 * m_custom_line_count);

    m_custom_line_ranges.resize(m_num_dimensions);

    for (int d = 0; d < m_num_dimensions; ++d)
        m_custom_line_ranges[d] = {int(positions.size()), generate_lines(positions, m_custom_line_counts[d])};

    m_2d_grid_shader->set_buffer("position", positions);
}

void SampleViewer::draw_2D_points_and_grid(const float4x4 &mvp, int2 dims, int plot_index)
{
    float4x4 pos = layout_2d_matrix(m_num_dimensions, dims);

    // Render the point set
    m_2d_point_shader->set_uniform("mvp", mul(mvp, pos));
    float radius = map_slider_to_radius(m_radius / (m_num_dimensions - 1));
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);
    m_2d_point_shader->set_uniform("point_size", radius);
    m_2d_point_shader->set_uniform("color", m_point_color);
    int2 range = get_draw_range();

    m_2d_point_shader->begin();
    m_2d_point_shader->draw_array(Shader::PrimitiveType::Point, m_subset_count * plot_index + range.x, range.y);
    m_2d_point_shader->end();

    auto mat = mul(mvp, mul(pos, m_camera[CAMERA_2D].arcball.matrix()));

    int2 starts{0};
    int2 counts{4};
    if (m_show_bbox)
        draw_grid(m_grid_shader, mat, starts, counts, 1.f);

    starts += counts;
    counts = int2{m_coarse_line_count};
    if (m_show_coarse_grid)
        draw_grid(m_grid_shader, mat, starts, counts, 0.6f);

    starts += counts;
    counts = int2{m_fine_line_count};
    if (m_show_fine_grid)
        draw_grid(m_grid_shader, mat, starts, counts, 0.2f);

    if (m_show_custom_grid)
        draw_grid(m_2d_grid_shader, mat, int2{m_custom_line_ranges[dims.x][0], m_custom_line_ranges[dims.y][0]},
                  int2{m_custom_line_ranges[dims.x][1], m_custom_line_ranges[dims.y][1]}, 1.0f);
}

void SampleViewer::draw_scene()
{
    auto &io = ImGui::GetIO();

    process_hotkeys();

    try
    {
        // update the points and grids if outdated
        if (m_gpu_points_dirty || m_cpu_points_dirty)
            update_points(m_cpu_points_dirty);
        if (m_gpu_grids_dirty)
            update_grids();

        //
        // clear the scene and set up viewports
        //

        // first clear the entire window with the background color
        // display_size is the size of the window in pixels while accounting for dpi factor on retina screens.
        //     for retina displays, io.DisplaySize is the size of the window in points (logical pixels)
        //     but we need the size in pixels. So we scale io.DisplaySize by io.DisplayFramebufferScale
        auto display_size = int2{io.DisplaySize} * int2{io.DisplayFramebufferScale};
        CHK(glViewport(0, 0, display_size.x, display_size.y));
        CHK(glClearColor(m_bg_color[0], m_bg_color[1], m_bg_color[2], 1.f));
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

            // the second condition rejects the case where mouse down was on the GUI, but the mouse release is on the
            // background (e.g. when dismissing a popup with a mouse click)
            if (io.MouseReleased[0] && !io.MouseDownOwned[0])
            {
                m_camera[CAMERA_NEXT].arcball.button(int2{io.MousePos} - m_viewport_pos, io.MouseDown[0]);
                // since the time between mouse down and up could be shorter
                // than the animation duration, we override the previous
                // camera's arcball on mouse up to complete the animation
                m_camera[CAMERA_PREVIOUS].arcball     = m_camera[CAMERA_NEXT].arcball;
                m_camera[CAMERA_PREVIOUS].camera_type = m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
            }

            if (io.MouseDown[0])
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
                camera.camera_type = camera1.camera_type;
                // m_params.fpsIdling.fpsIdle = 9.f; // animation is done, reduce FPS
                if (m_animate_start_time != 0.0f)
                    m_params.fpsIdling.enableIdling = m_idling_backup;
                m_animate_start_time = 0.f;
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
                    draw_2D_points_and_grid(mvp, int2{x, y}, plot_index);
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

            int2x3 starts{0};
            int2x3 counts{4};
            if (m_show_bbox)
                draw_trigrid(m_grid_shader, mvp, 1.0f, starts, counts);

            starts += counts;
            counts = int2x3{m_coarse_line_count};
            if (m_show_coarse_grid)
                draw_trigrid(m_grid_shader, mvp, 0.6f, starts, counts);

            starts += counts;
            counts = int2x3{m_fine_line_count};
            if (m_show_fine_grid)
                draw_trigrid(m_grid_shader, mvp, 0.2f, starts, counts);

            if (m_show_custom_grid)
            {
                // compute the three dimension pairs we use for the XY, XZ, and ZY axes
                int3   dims = linalg::clamp(m_dimension, int3{0}, int3{m_num_dimensions - 1});
                int2x3 starts{{m_custom_line_ranges[dims.x][0], m_custom_line_ranges[dims.y][0]},
                              {m_custom_line_ranges[dims.x][0], m_custom_line_ranges[dims.z][0]},
                              {m_custom_line_ranges[dims.z][0], m_custom_line_ranges[dims.y][0]}};
                int2x3 counts{{m_custom_line_ranges[dims.x][1], m_custom_line_ranges[dims.y][1]},
                              {m_custom_line_ranges[dims.x][1], m_custom_line_ranges[dims.z][1]},
                              {m_custom_line_ranges[dims.z][1], m_custom_line_ranges[dims.y][1]}};
                draw_trigrid(m_2d_grid_shader, mvp, 1.f, starts, counts);
            }
        }
    }
    catch (const std::exception &e)
    {
        fmt::print(stderr, "OpenGL drawing failed:\n\t{}.", e.what());
        HelloImGui::Log(HelloImGui::LogLevel::Error, "OpenGL drawing failed:\n\t%s.", e.what());
    }
}

void SampleViewer::set_view(CameraType view)
{
    if (m_view != view)
    {
        m_animate_start_time                 = (float)ImGui::GetTime();
        m_camera[CAMERA_PREVIOUS]            = m_camera[CAMERA_CURRENT];
        m_camera[CAMERA_NEXT]                = m_camera[view];
        m_camera[CAMERA_NEXT].persp_factor   = (view == CAMERA_CURRENT) ? 1.0f : 0.0f;
        m_camera[CAMERA_NEXT].camera_type    = view;
        m_camera[CAMERA_CURRENT].camera_type = (view == m_camera[CAMERA_CURRENT].camera_type) ? view : CAMERA_CURRENT;
        m_view                               = view;

        // m_params.fpsIdling.fpsIdle = 0.f; // during animation, increase FPS
        m_idling_backup                 = m_params.fpsIdling.enableIdling;
        m_params.fpsIdling.enableIdling = false;
    }
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

/*!
    Draw grids along the XY, XZ, and ZY planes.

    @param shader
        The shader to draw with.
    @param mvp
        The model-view projection matrix to orient the grid.
    @param alpha
        The opacity of the grid lines.
    @param starts
        Stores the buffer starting offsets for drawing the grid lines. There is one column for each of the three planes,
        and a row for the horizontal and vertical lines within each plane. This can be the same value in each element if
        you want to draw the same grid lines along all axes.
    @param counts
        Stores the buffer line counts for drawing the grid lines. There is one column for each of the three planes,
        and a row for the horizontal and vertical lines within each plane. This can be the same value in each element if
        you want to draw the same grid lines along all axes.
*/
void SampleViewer::draw_trigrid(Shader *shader, const float4x4 &mvp, float alpha, const int2x3 &starts,
                                const int2x3 &counts)
{
    for (int axis = CAMERA_XY; axis < CAMERA_CURRENT; ++axis)
        if (m_camera[CAMERA_CURRENT].camera_type == axis || m_camera[CAMERA_CURRENT].camera_type == CAMERA_CURRENT)
            draw_grid(shader, mul(mvp, m_camera[axis].arcball.matrix()), starts[axis], counts[axis], alpha);
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

    for (int axis = CAMERA_XY; axis < CAMERA_CURRENT; ++axis)
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

    int3 dims = linalg::clamp(m_dimension, int3{0}, int3{m_num_dimensions - 1});
    out += (format == "eps") ? draw_points_eps(mvp, dims, m_subset_points, get_draw_range())
                             : draw_points_svg(mvp, dims, m_subset_points, get_draw_range(), radius);

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
            float4x4 pos = layout_2d_matrix(m_num_dimensions, int2{x, y});

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
                    fmt::print(stderr, "Invalid argument: \"{}\"!\n", argv[i]);
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
        fmt::print(stderr, "Error: {}\n", e.what());
        help  = true;
        error = true;
    }
    if (help)
    {
        fmt::print(error ? stderr : stdout, R"(Syntax: {} [options]
Options:
   -h, --help                Display this message
)",
                   argv[0]);
        return error ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    try
    {
        SampleViewer viewer;
        viewer.run();
    }
    catch (const std::runtime_error &e)
    {
        fmt::print(stderr, "Caught a fatal error: {}\n", e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
