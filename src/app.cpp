/** \file SampleViewer.cpp
    \author Wojciech Jarosz
*/

#include "app.h"

#include "hello_imgui/hello_imgui.h"
#include "hello_imgui/hello_imgui_include_opengl.h" // cross-platform way to include OpenGL headers
#include "imgui_ext.h"
#include "imgui_internal.h"

// #include "hello_imgui/icons_font_awesome_4.h"
#include "hello_imgui/icons_font_awesome_6.h"

#include "opengl_check.h"

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
#include <utility>

#ifdef __EMSCRIPTEN__
#include "emscripten_browser_file.h"
#include <string_view>
using std::string_view;
#else
#include "portable-file-dialogs.h"
#endif

#ifdef HELLOIMGUI_USE_SDL2
#include <SDL.h>
#endif

using namespace linalg::ostream_overloads;

using std::pair;
using std::to_string;

static int g_dismissed_version = 0;

static bool g_open_help = false;

static const vector<pair<string, string>> g_help_strings = {
    {"h", "Toggle this help window"},
    {"Left click+drag", "Rotate the camera"},
    {"Scroll mouse/pinch", "Zoom the camera"},
    {"1", "Switch to XY orthographic view"},
    {"2", "Switch to XZ orthographic view"},
    {"3", "Switch to ZY orthographic view"},
    {"4", "Switch to XYZ perspective view"},
    {"0", "Switch to viewing 2D projections of all pairs of dimensions"},
    {ICON_FA_ARROW_LEFT " , " ICON_FA_ARROW_RIGHT,
     "Decrease (" ICON_FA_ARROW_LEFT ") or increase (" ICON_FA_ARROW_RIGHT
     ") the target number of points to generate. For samplers that only admit certain numbers of "
     "points (e.g. powers of 2), this target value will be snapped to the nearest admissable value"},
    {ICON_FA_ARROW_UP " , " ICON_FA_ARROW_DOWN,
     "Switch to the previous (" ICON_FA_ARROW_UP ") or next (" ICON_FA_ARROW_DOWN ") sampler to generate the points"},
    {"Shift + " ICON_FA_ARROW_UP " , " ICON_FA_ARROW_DOWN, "Cycle through offset types (for OA samplers)"},
    {"d , D", "Decrease (d) or increase (D) the number of dimensions to generate for each point"},
    {"s , S", "Decrease (s) or increase (S) the random seed. Randomization is turned off for seed=0."},
    {"t , T", "Decrease (t) or increase (T) the strength (for OA samplers)"},
    {"j , J", "Decrease (j) or increase (J) the amount the points should be jittered within their strata"},
    {"g , G", "Toggle whether to draw the coarse (g) and fine (G) grid"},
    {"b", "Toggle whether to draw the bounding box"},
    {"p", "Toggle display of 1D X, Y, Z projections of the points"}};
static const map<string, string> g_tooltip_map(g_help_strings.begin(), g_help_strings.end());

static auto tooltip(const char *text, float wrap_width = 400.f)
{
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(wrap_width);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static auto hotkey_tooltip(const char *name, float wrap_width = 400.f)
{
    if (auto t = g_tooltip_map.find(name); t != g_tooltip_map.end())
        tooltip(fmt::format("{}.\nKey: {}", t->second, t->first).c_str(), wrap_width);
}

static float4x4 layout_2d_matrix(int num_dims, int2 dims)
{
    float cell_spacing = 1.f / (num_dims - 1);
    float cell_size    = 0.96f / (num_dims - 1);

    float2 offset = (dims - int2{0, 1} - float2{(num_dims - 2) / 2.0f}) * float2{1, -1};

    return mul(translation_matrix(float3{offset * cell_spacing, 1}), scaling_matrix(float3{float2{cell_size}, 1}));
}

#ifdef __EMSCRIPTEN__
EM_JS(int, screen_width, (), { return screen.width; });
EM_JS(int, screen_height, (), { return screen.height; });
EM_JS(int, window_width, (), { return window.innerWidth; });
EM_JS(int, window_height, (), { return window.innerHeight; });
#endif

SampleViewer::SampleViewer()
{
    m_custom_line_counts.fill(1);

    m_samplers.emplace_back(new Random(m_num_dimensions));
    m_samplers.emplace_back(new Jittered(1, 1, m_jitter * 0.01f));
    m_samplers.emplace_back(new CorrelatedMultiJitteredInPlace(1, 1, m_num_dimensions, 0, m_jitter * 0.01f, false));
    m_samplers.emplace_back(new CorrelatedMultiJitteredInPlace(1, 1, m_num_dimensions, 0, m_jitter * 0.01f, true));
    m_samplers.emplace_back(new CMJNDInPlace(1, 3, MJ_STYLE, 0, m_jitter * 0.01f));
    m_samplers.emplace_back(new SudokuInPlace(1, 1, m_num_dimensions, 0, 0.0f, false));
    m_samplers.emplace_back(new SudokuInPlace(1, 1, m_num_dimensions, 0, 0.0f, true));
    m_samplers.emplace_back(new BoseOAInPlace(1, MJ_STYLE, 0, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BoseGaloisOAInPlace(1, MJ_STYLE, 0, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BushOAInPlace(1, 3, MJ_STYLE, 0, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BushGaloisOAInPlace(1, 3, MJ_STYLE, 0, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new AddelmanKempthorneOAInPlace(2, MJ_STYLE, 0, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new BoseBushOAInPlace(2, MJ_STYLE, 0, m_jitter * 0.01f, m_num_dimensions));
    m_samplers.emplace_back(new NRooksInPlace(m_num_dimensions, 1, 0, m_jitter * 0.01f));
    m_samplers.emplace_back(new Sobol(m_num_dimensions));
    m_samplers.emplace_back(new SSobol(m_num_dimensions));
    m_samplers.emplace_back(new ZSobol(m_num_dimensions));
    m_samplers.emplace_back(new ZeroTwo(1, m_num_dimensions, false));
    m_samplers.emplace_back(new ZeroTwo(1, m_num_dimensions, true));
    m_samplers.emplace_back(
        new CascadedSobol(HelloImGui::assetFileFullPath("cascaded_sobol_init_tab.dat"), m_num_dimensions));
    m_samplers.emplace_back(new OneTwo(1, m_num_dimensions, 0));
    m_samplers.emplace_back(new Faure(m_num_dimensions, 1));
    m_samplers.emplace_back(new Halton(m_num_dimensions));
    m_samplers.emplace_back(new HaltonZaremba(m_num_dimensions));
    m_samplers.emplace_back(new Hammersley<Halton>(m_num_dimensions, 1));
    m_samplers.emplace_back(new Hammersley<HaltonZaremba>(m_num_dimensions, 1));
    m_samplers.emplace_back(new LarcherPillichshammerGK(3, 1, false));
    m_samplers.emplace_back(new GrayCode(1));
    m_samplers.emplace_back(new XiSequence(1));
    m_samplers.emplace_back(new CSVFile());

    m_camera[CAMERA_XY].arcball.set_state({0, 0, 0, 1});
    m_camera[CAMERA_XY].persp_factor = 0.0f;
    m_camera[CAMERA_XY].camera_type  = CAMERA_XY;

    m_camera[CAMERA_ZY].arcball.set_state(linalg::rotation_quat({0.f, -1.f, 0.f}, float(M_PI_2)));
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

    m_params.iniFolderType = HelloImGui::IniFolderType::AppUserConfigFolder;
    m_params.iniFilename   = "SamplinSafari/settings.ini";

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

        HelloImGui::DockingSplit splitMainConsole{"MainDockSpace", "ConsoleSpace", ImGuiDir_Down, 0.25f};

        m_params.dockingParams.dockingSplits = {
            HelloImGui::DockingSplit{"MainDockSpace", "EditorSpace", ImGuiDir_Left, 0.2f}, splitMainConsole};

        HelloImGui::DockingParams right_layout, portrait_layout, landscape_layout;

        right_layout.layoutName      = "Settings on right";
        right_layout.dockableWindows = {editorWindow, consoleWindow};
        right_layout.dockingSplits   = {HelloImGui::DockingSplit{"MainDockSpace", "EditorSpace", ImGuiDir_Right, 0.2f},
                                        splitMainConsole};

        consoleWindow.dockSpaceName = "EditorSpace";

        portrait_layout.layoutName      = "Mobile device (portrait orientation)";
        portrait_layout.dockableWindows = {editorWindow, consoleWindow};
        portrait_layout.dockingSplits = {HelloImGui::DockingSplit{"MainDockSpace", "EditorSpace", ImGuiDir_Down, 0.5f}};

        landscape_layout.layoutName      = "Mobile device (landscape orientation)";
        landscape_layout.dockableWindows = {editorWindow, consoleWindow};
        landscape_layout.dockingSplits   = {
            HelloImGui::DockingSplit{"MainDockSpace", "EditorSpace", ImGuiDir_Left, 0.5f}};

        m_params.alternativeDockingLayouts = {right_layout, portrait_layout, landscape_layout};

#ifdef __EMSCRIPTEN__
        HelloImGui::Log(HelloImGui::LogLevel::Info, "Screen size: %d, %d\nWindow size: %d, %d", screen_width(),
                        screen_height(), window_width(), window_height());
        if (std::min(screen_width(), screen_height()) < 500)
        {
            HelloImGui::Log(
                HelloImGui::LogLevel::Info, "Switching to %s layout",
                m_params.alternativeDockingLayouts[window_width() < window_height() ? 1 : 2].layoutName.c_str());
            std::swap(m_params.dockingParams,
                      m_params.alternativeDockingLayouts[window_width() < window_height() ? 1 : 2]);
        }
#endif
    }

    m_params.callbacks.defaultIconFont = HelloImGui::DefaultIconFont::FontAwesome6;

    m_params.callbacks.LoadAdditionalFonts = [this]()
    {
        Timer timer;
        HelloImGui::Log(HelloImGui::LogLevel::Info, "Loading fonts...");
        auto load_font = [](const string &font_path, float size = 14.f)
        {
            if (!HelloImGui::AssetExists(font_path))
                HelloImGui::Log(HelloImGui::LogLevel::Error, "Cannot find the font asset '%s'!", font_path.c_str());

            return HelloImGui::LoadFontTTF_WithFontAwesomeIcons(font_path, size);
        };
        printf("Loading fonts...\n");

        m_regular = load_font("fonts/Roboto/Roboto-Regular.ttf");
        m_bold    = load_font("fonts/Roboto/Roboto-Bold.ttf");
        HelloImGui::Log(HelloImGui::LogLevel::Info, "done loading fonts.");
    };

    m_params.callbacks.ShowMenus = []()
    {
        string text = ICON_FA_CIRCLE_INFO;
        auto   posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(text.c_str()).x -
                     ImGui::GetScrollX() - 2 * ImGui::GetStyle().ItemSpacing.x);
        if (posX > ImGui::GetCursorPosX())
            ImGui::SetCursorPosX(posX);
        if (ImGui::MenuItem(text))
            g_open_help = true;
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
            if (ImGui::MenuItem(fmt::format("{}  Export as {}...", ICON_FA_FLOPPY_DISK, to_upper(ext))))
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
            if (ImGui::BeginMenu(fmt::format("{}  Download as {}...", ICON_FA_FLOPPY_DISK, to_upper(ext)).c_str()))
            {
                string basename;

                if (ImGui::InputText("Base filename", &basename, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    try
                    {
                        ImGui::CloseCurrentPopup();
                        string buffer = draw_points_csv(m_subset_points, get_draw_range());
                        emscripten_browser_file::download(
                            "test_file.csv", // the default filename for the browser to save.
                            "text/csv", // the MIME type of the data, treated as if it were a webserver serving a file
                            string_view(buffer.c_str(), buffer.length()) // a buffer describing the data to download
                        );
                        // vector<string> saved_files = save_files(basename, ext);
                        // for (auto &saved_file : saved_files)
                        //     emscripten_run_script(fmt::format("saveFileFromMemoryFSToDisk('{}');",
                        //     saved_file).c_str());
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
        ImGui::Text("%3.3f / %3.3f ms (%3.0f pps)", m_time2, m_time1 + m_time2, m_point_count / (m_time1 + m_time2));
        tooltip("Shows A/B (points per second) where A is how long it took to call Sampler::sample(), and B includes "
                "other setup costs.");
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
            auto quad_verts =
                vector<float3>{{-0.5f, -0.5f, 0.f}, {0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f}};
            m_2d_point_shader = new Shader(&m_render_pass, "2D point shader", "shaders/point.vert",
                                           "shaders/point.frag", Shader::BlendMode::AlphaBlend);
            m_2d_point_shader->set_buffer("vertices", quad_verts);
            m_2d_point_shader->set_buffer_divisor("vertices", 0);

            m_3d_point_shader = new Shader(&m_render_pass, "3D point shader", "shaders/point.vert",
                                           "shaders/point.frag", Shader::BlendMode::AlphaBlend);
            m_3d_point_shader->set_buffer("vertices", quad_verts);
            m_3d_point_shader->set_buffer_divisor("vertices", 0);

            m_grid_shader = new Shader(&m_render_pass, "Grid shader", "shaders/grid.vert", "shaders/grid.frag",
                                       Shader::BlendMode::AlphaBlend);
            m_grid_shader->set_buffer(
                "position",
                vector<float3>{{-0.5f, -0.5f, 0.5f}, {1.5f, -0.5f, 0.5f}, {1.5f, 1.5f, 0.5f}, {-0.5f, 1.5f, 0.5f}});

            HelloImGui::Log(HelloImGui::LogLevel::Info, "Successfully initialized GL!");
        }
        catch (const std::exception &e)
        {
            fmt::print(stderr, "Shader initialization failed!:\n\t{}.", e.what());
            HelloImGui::Log(HelloImGui::LogLevel::Error, "Shader initialization failed!:\n\t%s.", e.what());
        }

        // check whether we saved the version that the user saw and dismissed the about dialog last
        // show the about dialog on startup only if it was last shown at a previous version
        auto s              = HelloImGui::LoadUserPref("AboutDismissedVersion");
        g_dismissed_version = strtol(s.c_str(), nullptr, 10);
        if (g_dismissed_version < version_combined())
            g_open_help = true;
    };

    m_params.callbacks.BeforeExit = []
    {
        if (g_dismissed_version != 0)
            HelloImGui::SaveUserPref("AboutDismissedVersion", to_string(version_combined()));
    };

    m_params.callbacks.ShowGui                 = [this]() { draw_about_dialog(); };
    m_params.callbacks.CustomBackground        = [this]() { draw_background(); };
    m_params.callbacks.AnyBackendEventCallback = [this](void *event) { return process_event(event); };
}

void SampleViewer::run() { HelloImGui::Run(m_params); }

SampleViewer::~SampleViewer()
{
    for (size_t i = 0; i < m_samplers.size(); ++i) delete m_samplers[i];
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

void SampleViewer::draw_about_dialog()
{
    if (g_open_help)
        ImGui::OpenPopup("About");

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowFocus();
    constexpr float icon_size    = 128.f;
    float2          col_width    = {icon_size + HelloImGui::EmSize(), 32 * HelloImGui::EmSize()};
    float2          display_size = ImGui::GetIO().DisplaySize;
#ifdef __EMSCRIPTEN__
    display_size = float2((float)window_width(), (float)window_height());
#endif
    col_width[1] = clamp(col_width[1], 5 * HelloImGui::EmSize(),
                         display_size.x - ImGui::GetStyle().WindowPadding.x - 2 * ImGui::GetStyle().ItemSpacing.x -
                             ImGui::GetStyle().ScrollbarSize - col_width[0]);

    ImGui::SetNextWindowContentSize(float2{col_width[0] + col_width[1] + ImGui::GetStyle().ItemSpacing.x, 0});

    bool about_open = true;
    if (ImGui::BeginPopupModal("About", &about_open,
                               ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_AlwaysAutoResize))
    {

        ImGui::Spacing();

        if (ImGui::BeginTable("about_table1", 2))
        {
            ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed, col_width[0]);
            ImGui::TableSetupColumn("description", ImGuiTableColumnFlags_WidthFixed, col_width[1]);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            // right align the image
            {
                auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - icon_size -
                             2 * ImGui::GetStyle().ItemSpacing.x);
                if (posX > ImGui::GetCursorPosX())
                    ImGui::SetCursorPosX(posX);
            }
            HelloImGui::ImageFromAsset("app_settings/icon.png", {icon_size, icon_size}); // show the app icon

            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[1]);

            ImGui::PushFont(m_bold, 30);
            ImGui::Text("Samplin' Safari");
            ImGui::PopFont();

            ImGui::PushFont(m_bold, 18);
            ImGui::Text(version());
            ImGui::PopFont();
            ImGui::PushFont(m_regular, 10);
#if defined(__EMSCRIPTEN__)
            ImGui::Text(fmt::format("Built with emscripten using the {} backend on {}.", backend(), build_timestamp()));
#else
            ImGui::Text(fmt::format("Built using the {} backend on {}.", backend(), build_timestamp()));
#endif
            ImGui::PopFont();

            ImGui::Spacing();

            ImGui::PushFont(m_bold, 16);
            ImGui::Text("Samplin' Safari is a research tool to visualize and interactively inspect high-dimensional "
                        "(quasi) Monte Carlo samplers.");
            ImGui::PopFont();

            ImGui::Spacing();

            ImGui::Text("It is developed by Wojciech Jarosz, and is available under a 3-clause BSD license.");

            ImGui::PopTextWrapPos();
            ImGui::EndTable();
        }

        auto right_align = [](const string &text)
        {
            auto posX = (ImGui::GetCursorPosX() + ImGui::GetColumnWidth() - ImGui::CalcTextSize(text.c_str()).x -
                         2 * ImGui::GetStyle().ItemSpacing.x);
            if (posX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(posX);
            ImGui::Text(text);
        };

        auto item_and_description = [this, right_align, col_width](string name, string desc)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            ImGui::PushFont(m_bold, 14);
            right_align(name);
            ImGui::PopFont();

            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[1] - HelloImGui::EmSize());
            ImGui::PushFont(m_regular, 14);
            ImGui::Text(desc);
            ImGui::PopFont();
        };

        if (ImGui::BeginTabBar("AboutTabBar"))
        {
            if (ImGui::BeginTabItem("Keybindings", nullptr))
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[0] + col_width[1]);
                ImGui::Text("The following keyboard shortcuts are available (these are also described in tooltips over "
                            "their respective controls).");

                ImGui::Spacing();
                ImGui::PopTextWrapPos();

                if (ImGui::BeginTable("about_table3", 2))
                {
                    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, col_width[0]);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, col_width[1]);

                    for (auto item : g_help_strings) item_and_description(item.first, item.second);

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Credits"))
            {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + col_width[0] + col_width[1]);
                ImGui::Text("Samplin' Safari was originally created as part of the publication:");

                ImGui::Spacing();

                ImGui::Indent(HelloImGui::EmSize() * 1);
                ImGui::PushFont(m_bold, 14);
                ImGui::Text("Orthogonal Array Sampling for Monte Carlo Rendering");
                ImGui::PopFont();
                ImGui::Text("Wojciech Jarosz, Afnan Enayet, Andrew Kensler, Charlie Kilpatrick, Per Christensen.\n"
                            "In Computer Graphics Forum (Proceedings of EGSR), 38(4), July 2019.");
                ImGui::Unindent(HelloImGui::EmSize() * 1);

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Text("It additionally makes use of the following external libraries and techniques (in "
                            "alphabetical order):\n\n");
                ImGui::PopTextWrapPos();

                if (ImGui::BeginTable("about_table2", 2))
                {
                    ImGui::TableSetupColumn("one", ImGuiTableColumnFlags_WidthFixed, col_width[0]);
                    ImGui::TableSetupColumn("two", ImGuiTableColumnFlags_WidthFixed, col_width[1]);

                    item_and_description("bitcount",
                                         "Mike Pedersen's MIT-licensed fast, cross-platform bit counting functions.");
                    item_and_description("CascadedSobol",
                                         "Loïs Paulin's MIT-licensed implementation of Cascaed Sobol' sampling.");
                    item_and_description("Dear ImGui",
                                         "Omar Cornut's immediate-mode graphical user interface for C++.");
#ifdef __EMSCRIPTEN__
                    item_and_description("emscripten", "An MIT-licensed LLVM-to-WebAssembly compiler.");
                    item_and_description("emscripten-browser-file",
                                         "Armchair Software's MIT-licensed header-only C++ library "
                                         "to open and save files in the browser.");
#endif
                    item_and_description("{fmt}", "A modern formatting library.");
                    item_and_description("galois++",
                                         "My C++ port of Art Owen's Statlib code for arithmetic over Galois fields.");
                    item_and_description("halton/sobol",
                                         "Leonhard Gruenschloss's MIT-licensed code for Halton and Sobol sequences.");
                    item_and_description("Hello ImGui", "Pascal Thomet's cross-platform starter-kit for Dear ImGui.");
                    item_and_description(
                        "linalg", "Sterling Orsten's public domain, single header short vector math library for C++.");
                    item_and_description("NanoGUI", "Bits of code from Wenzel Jakob's BSD-licensed NanoGUI library.");
                    item_and_description(
                        "pcg32", "Wenzel Jakob's tiny C++ version of Melissa O'Neill's random number generator.");
#ifndef __EMSCRIPTEN__
                    item_and_description("portable-file-dialogs",
                                         "Sam Hocevar's WTFPL portable GUI dialogs library, C++11, single-header.");
#endif
                    item_and_description("stochastic-generation",
                                         "MIT-licensed C++ implementation of \"Stochastic Generation of (t,s) "
                                         "Sample Sequences\", by Helmer, Christensen, and Kensler (2021).");
                    item_and_description(
                        "xi-sequence/graycode",
                        "Abdalla Ahmed's code to generate Gray-code-ordered (0,m,2) nets and (0,2) xi-sequences.");
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // ImGui::SetKeyboardFocusHere();
        if (ImGui::Button("Dismiss", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
            (!g_open_help && ImGui::IsKeyPressed(ImGuiKey_H)))
        {
            ImGui::CloseCurrentPopup();
            g_dismissed_version = version_combined();
        }
        // ImGui::SetItemDefaultFocus();

        ImGui::ScrollWhenDraggingOnVoid(ImVec2(0.0f, -ImGui::GetIO().MouseDelta.y), ImGuiMouseButton_Left);
        ImGui::EndPopup();
        g_open_help = false;
    }
}

void SampleViewer::draw_editor()
{
    auto big_header = [this](const char *label, ImGuiTreeNodeFlags flags = 0)
    {
        ImGui::PushFont(m_bold, 16);
        bool ret = ImGui::CollapsingHeader(label, flags | ImGuiTreeNodeFlags_DefaultOpen);
        ImGui::PopFont();
        return ret;
    };

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.7f);

    // =========================================================
    if (big_header(ICON_FA_SLIDERS "  Sampler settings"))
    // =========================================================
    {
        if (ImGui::BeginCombo("##Sampler combo", m_samplers[m_sampler]->name().c_str(), ImGuiComboFlags_HeightLargest))
        {
            for (int n = 0; n < (int)m_samplers.size(); n++)
            {
                Sampler   *sampler     = m_samplers[n];
                const bool is_selected = (m_sampler == n);
                if (ImGui::Selectable(sampler->name().c_str(), is_selected))
                {
                    m_sampler = n;
                    sampler->setJitter(m_jitter * 0.01f);
                    sampler->setSeed(m_seed);
                    m_gpu_points_dirty = m_cpu_points_dirty = true;

                    HelloImGui::Log(HelloImGui::LogLevel::Debug, "Switching to sampler %d: %s.", m_sampler,
                                    sampler->name().c_str());
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        hotkey_tooltip(ICON_FA_ARROW_UP " , " ICON_FA_ARROW_DOWN);

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
                    that->m_gpu_points_dirty = that->m_cpu_points_dirty = true;
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
                    m_gpu_points_dirty = m_cpu_points_dirty = true;
                }
#endif
            }
            tooltip("Load points from a CSV text file with points for rows and individual point coordinates as comma "
                    "separated values per row.");
        }

        int num_points = m_point_count;
        if (ImGui::SliderInt("Num points", &num_points, 1, 1 << 17, "%d", ImGuiSliderFlags_Logarithmic))
        {
            m_target_point_count = std::clamp(num_points, 1, 1 << 20);
            HelloImGui::Log(HelloImGui::LogLevel::Debug, "Setting target point count to %d.", m_target_point_count);
            m_gpu_points_dirty = m_cpu_points_dirty = true;
            HelloImGui::Log(HelloImGui::LogLevel::Debug, "Regenerated %d points.", m_point_count);
        }
        hotkey_tooltip(ICON_FA_ARROW_LEFT " , " ICON_FA_ARROW_RIGHT);

        if (ImGui::SliderInt("Dimensions", &m_num_dimensions, 2, MAX_DIMENSIONS, "%d", ImGuiSliderFlags_AlwaysClamp))
            m_gpu_points_dirty = m_cpu_points_dirty = true;
        hotkey_tooltip("d , D");

        if (!csv)
        {
            int s = m_seed;
            if (ImGui::SliderInt("Seed", &s, 0, 10000))
                m_gpu_points_dirty = m_cpu_points_dirty = true;
            m_seed = s;
            hotkey_tooltip("s , S");

            if (ImGui::SliderFloat("Jitter", &m_jitter, 0.f, 100.f, "%3.1f%%"))
            {
                m_samplers[m_sampler]->setJitter(m_jitter * 0.01f);
                m_gpu_points_dirty = m_cpu_points_dirty = true;
            }
            hotkey_tooltip("j , J");
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
                    m_gpu_points_dirty = m_cpu_points_dirty = true;
                }
            };
            int strength = oa->strength();
            if (ImGui::InputInt("Strength", &strength, 1))
                change_strength(std::max(2, strength));
            hotkey_tooltip("t , T");

            // Controls for the offset type of the OA
            auto offset_names       = oa->offsetTypeNames();
            auto change_offset_type = [oa, this](int offset)
            {
                oa->setOffsetType(offset);
                m_jitter           = oa->jitter();
                m_gpu_points_dirty = m_cpu_points_dirty = true;
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
            hotkey_tooltip("Shift + " ICON_FA_ARROW_UP " , " ICON_FA_ARROW_DOWN);
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
            hotkey_tooltip(fmt::format("{:d}", (i + 1) % IM_ARRAYSIZE(items)).c_str());
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
        ImGui::SliderFloat("Radius", &m_radius, 0.f, 1.f, "%4.3f");
        ImGui::SameLine();
        {
            if (ImGui::ToggleButton(ICON_FA_COMPRESS, &m_scale_radius_with_points))
                m_radius *= m_scale_radius_with_points ? std::sqrt(m_point_count) : 1.f / std::sqrt(m_point_count);
            tooltip("Automatically scale radius with number of points");
        }

        ImGui::Checkbox("1D projections", &m_show_1d_projections);
        hotkey_tooltip("p");
        ImGui::Checkbox("Point indices", &m_show_point_nums);
        tooltip("Show the index above each point?");
        ImGui::Checkbox("Point coords", &m_show_point_coords);
        tooltip("Show the XYZ coordinates below each point?");
        ImGui::Checkbox("Fine grid", &m_show_fine_grid);
        hotkey_tooltip("g , G");
        ImGui::Checkbox("Coarse grid", &m_show_coarse_grid);
        hotkey_tooltip("g , G");

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
                    m_custom_line_counts[i] = std::max(1, m_custom_line_counts[i]);

                ImGui::SameLine();
                ImGui::SetCursorPosX(available + 4 * HelloImGui::EmSize());
                ImGui::TextUnformatted(to_string(i).c_str());
            }
            tooltip("Create a grid with this many subdivisions along each of the dimensions.");
            ImGui::Unindent();
        }
        ImGui::Checkbox("Bounding box", &m_show_bbox);
        hotkey_tooltip("b");

        ImGui::Dummy({0, HelloImGui::EmSize(0.25f)});
    }

    // =========================================================
    if (big_header(ICON_FA_SHUFFLE "  Dimension mapping"))
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
            if (ImGui::Checkbox("Disjoint batches", &disjoint))
                m_gpu_points_dirty = true;
            if (ImGui::SliderInt("First point", &m_first_draw_point, 0, m_point_count - 1, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                m_gpu_points_dirty = true;
            if (disjoint)
                // round to nearest multiple of m_point_draw_count
                m_first_draw_point = (m_first_draw_point / m_point_draw_count) * m_point_draw_count;

            tooltip("Display points starting at this index.");

            if (ImGui::SliderInt("Num points##2", &m_point_draw_count, 1, m_point_count - m_first_draw_point, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                m_gpu_points_dirty = true;
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
    ImGui::ScrollWhenDraggingOnVoid(ImVec2(0.0f, -ImGui::GetIO().MouseDelta.y), ImGuiMouseButton_Left);
}

bool SampleViewer::process_event(void *e)
{
#ifdef HELLOIMGUI_USE_SDL2
    if (ImGui::GetIO().WantCaptureMouse)
        return false;

    static bool sPinch = false;
    SDL_Event  *event  = static_cast<SDL_Event *>(e);
    switch (event->type)
    {
    // case SDL_QUIT: HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_QUIT event"); break;
    // case SDL_WINDOWEVENT: HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_WINDOWEVENT event"); break;
    // case SDL_MOUSEWHEEL: HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_MOUSEWHEEL event"); break;
    case SDL_MOUSEMOTION:
        if (sPinch)
            return true;
        // HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_MOUSEMOTION event");
        break;
    // case SDL_MOUSEBUTTONDOWN: HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_MOUSEBUTTONDOWN event");
    // break; case SDL_MOUSEBUTTONUP: HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_MOUSEBUTTONUP
    // event"); break; case SDL_FINGERMOTION: HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_FINGERMOTION
    // event"); break;
    // case SDL_FINGERDOWN: HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_FINGERDOWN event"); break;
    case SDL_MULTIGESTURE:
    {
        constexpr float cPinchZoomThreshold(0.0001f);
        constexpr float cPinchScale(80.0f);
        if (event->mgesture.numFingers == 2 && fabs(event->mgesture.dDist) >= cPinchZoomThreshold)
        {
            // HelloImGui::Log(HelloImGui::LogLevel::Debug,
            //                 fmt::format("Got an SDL_MULTIGESTURE event; numFingers: {}; dDist: {}; x: {}, y: {}",
            //                             event->mgesture.numFingers, event->mgesture.dDist, event->mgesture.x,
            //                             event->mgesture.y)
            //                     .c_str());
            sPinch = true;
            // Zoom in/out by positive/negative mPinch distance
            float zoomDelta            = event->mgesture.dDist * cPinchScale;
            m_camera[CAMERA_NEXT].zoom = std::max(0.001, m_camera[CAMERA_NEXT].zoom * pow(1.1, zoomDelta));

            // add a dummy event so that idling doesn't happen
            auto            g = ImGui::GetCurrentContext();
            ImGuiInputEvent e;
            e.Type                   = ImGuiInputEventType_None;
            e.Source                 = ImGuiInputSource_Mouse;
            e.EventId                = g->InputEventsNextEventId++;
            e.MouseWheel.MouseSource = ImGuiMouseSource_TouchScreen;
            g->InputEventsQueue.push_back(e);

            return true;
        }
    }
    break;
    case SDL_FINGERUP:
        // HelloImGui::Log(HelloImGui::LogLevel::Debug, "Got an SDL_FINGERUP event");
        sPinch = false;
        break;
    }
#endif
    return false;
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
            m_gpu_points_dirty = m_cpu_points_dirty = true;
        }
    };
    auto change_offset_type = [oa, this](int offset)
    {
        oa->setOffsetType(offset);
        m_jitter           = oa->jitter();
        m_gpu_points_dirty = m_cpu_points_dirty = true;
    };

    if ((ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)) &&
        !ImGui::IsKeyDown(ImGuiMod_Shift))
    {
        int delta        = ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : -1;
        m_sampler        = mod(m_sampler + delta, (int)m_samplers.size());
        Sampler *sampler = m_samplers[m_sampler];
        sampler->setJitter(m_jitter * 0.01f);
        sampler->setSeed(m_seed);
        m_gpu_points_dirty = m_cpu_points_dirty = true;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        m_target_point_count =
            std::max(1u, ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? roundUpPow2(m_target_point_count + 1)
                                                                  : roundDownPow2(m_target_point_count - 1));

        HelloImGui::Log(HelloImGui::LogLevel::Debug, "Setting target point count to %d.", m_target_point_count);
        m_gpu_points_dirty = m_cpu_points_dirty = true;
        HelloImGui::Log(HelloImGui::LogLevel::Debug, "Regenerated %d points.", m_point_count);
        // m_target_point_count = m_point_count;
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_D))
    {
        m_num_dimensions   = std::clamp(m_num_dimensions + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 1 : -1), 2, 10);
        m_gpu_points_dirty = m_cpu_points_dirty = true;
    }
    // else if (ImGui::IsKeyPressed(ImGuiKey_R))
    // {
    //     if (ImGui::IsKeyDown(ImGuiMod_Shift))
    //     {
    //         m_randomize = true;
    //         sampler->setSeed(true);
    //     }
    //     else
    //         m_randomize = !m_randomize;
    //     m_gpu_points_dirty = m_cpu_points_dirty = true;
    // }
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
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_B))
        m_show_bbox = !m_show_bbox;
    else if (ImGui::IsKeyPressed(ImGuiKey_H))
        g_open_help = !g_open_help;
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
            if (generator->seed() != m_seed)
                generator->setSeed(m_seed);

            generator->setDimensions(m_num_dimensions);

            int num_pts   = generator->setNumSamples(m_target_point_count);
            m_point_count = num_pts >= 0 ? num_pts : m_target_point_count;

            m_time1 = timer.elapsed();

            m_points.resize(m_num_dimensions, m_point_count);
            m_points.reset(0.5f);
            m_3d_points.resize(m_point_count);

            timer.reset();
            for (int i = 0; i < m_point_count; ++i) generator->sample(m_points.row(i), i);
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
        for (int i = 0; i < m_points.sizeY(); ++i)
        {
            float v = m_points(std::clamp(m_subset_axis, 0, m_num_dimensions - 1), i);
            if (v >= (m_subset_level + 0.0f) / m_num_subset_levels && v < (m_subset_level + 1.0f) / m_num_subset_levels)
            {
                // copy all dimensions (rows) of point i
                for (int dim = 0; dim < m_points.sizeX(); ++dim)
                    m_subset_points(dim, m_subset_count) = m_points(dim, i);
                ++m_subset_count;
            }
        }
    }

    int3 dims = linalg::clamp(m_dimension, int3{0}, int3{m_num_dimensions - 1});
    for (size_t i = 0; i < m_3d_points.size(); ++i)
        m_3d_points[i] = float3{m_subset_points(dims.x, i), m_subset_points(dims.y, i), m_subset_points(dims.z, i)};

    //
    // Upload points to the GPU
    //

    auto range = get_draw_range();
    m_3d_point_shader->set_buffer("center", m_3d_points, range.x, range.y);
    m_3d_point_shader->set_buffer_divisor("center", 1); // one center per quad/instance

    //
    // create a temporary array to store all the 2D projections of the points.
    // each 2D plot actually needs 3D points, and there are num2DPlots of them
    int num2DPlots = m_num_dimensions * (m_num_dimensions - 1) / 2;
    m_2d_points.resize(num2DPlots * m_subset_count);
    for (int y = 0, plot_index = 0; y < m_num_dimensions; ++y)
        for (int x = 0; x < y; ++x, ++plot_index)
            for (int i = 0; i < m_subset_count; ++i)
                m_2d_points[plot_index * m_subset_count + i] =
                    float3{m_subset_points(x, i), m_subset_points(y, i), -0.5f};

    m_2d_point_shader->set_buffer("center", m_2d_points);
    m_2d_point_shader->set_buffer_divisor("center", 1); // one center per quad/instance

    m_gpu_points_dirty = false;
}

void SampleViewer::draw_2D_points_and_grid(const float4x4 &mvp, int2 dims, int plot_index)
{
    float4x4 pos = layout_2d_matrix(m_num_dimensions, dims);

    // Render the point set
    m_2d_point_shader->set_uniform("mvp", mul(mvp, pos));
    m_2d_point_shader->set_uniform("rotation", float3x3(linalg::identity));
    m_2d_point_shader->set_uniform("smash", float4x4(linalg::identity));
    float radius = m_radius / (m_scale_radius_with_points ? std::sqrt(m_point_count) : 1.0f);
    m_2d_point_shader->set_uniform("point_size", radius);
    m_2d_point_shader->set_uniform("color", m_point_color);
    int2 range = get_draw_range();

    m_2d_point_shader->set_buffer_pointer_offset("center",
                                                 size_t(m_subset_count * plot_index + range.x) * sizeof(float3));
    m_2d_point_shader->set_buffer_divisor("center", 1); // one center per quad/instance

    if (range.y > 0)
    {
        m_2d_point_shader->begin();
        m_2d_point_shader->draw_array(Shader::PrimitiveType::TriangleFan, 0, 4, false, range.y);
        m_2d_point_shader->end();
    }

    auto mat = mul(mvp, mul(translation_matrix(float3{0, 0, -1.001}), pos));

    if (m_show_bbox)
        draw_grid(mat, int2{1}, 1.f);

    if (m_show_coarse_grid)
        draw_grid(mat, int2{m_samplers[m_sampler]->coarseGridRes(m_point_count)}, 0.6f);

    if (m_show_fine_grid)
        draw_grid(mat, int2{m_point_count}, 0.2f);

    if (m_show_custom_grid)
        draw_grid(mat, int2{m_custom_line_counts[dims.x], m_custom_line_counts[dims.y]}, 1.f);
}

void SampleViewer::draw_background()
{
    auto &io = ImGui::GetIO();

    process_hotkeys();

    try
    {
        // update the points and grids if outdated
        if (m_gpu_points_dirty || m_cpu_points_dirty)
            update_points(m_cpu_points_dirty);

        //
        // calculate the viewport sizes
        // fbsize is the size of the window in physical pixels while accounting for dpi factor on retina
        // screens. For retina displays, io.DisplaySize is the size of the window in logical pixels so we it by
        // io.DisplayFramebufferScale to get the physical pixel size for the framebuffer.
        int2 fbscale = io.DisplayFramebufferScale;
        auto fbsize  = int2{io.DisplaySize} * fbscale;
        // spdlog::trace("DisplayFramebufferScale: {}, DpiWindowSizeFactor: {}, DpiFontLoadingFactor: {}",
        //               float2{io.DisplayFramebufferScale}, HelloImGui::DpiWindowSizeFactor(),
        //               HelloImGui::DpiFontLoadingFactor());
        int2 viewport_offset = {0, 0};
        int2 viewport_size   = io.DisplaySize;
        if (auto id = m_params.dockingParams.dockSpaceIdFromName("MainDockSpace"))
            if (auto central_node = ImGui::DockBuilderGetCentralNode(*id))
            {
                viewport_size   = int2{int(central_node->Size.x), int(central_node->Size.y)};
                viewport_offset = int2{int(central_node->Pos.x), int(central_node->Pos.y)};
            }

        // inform the arcballs of the viewport size
        for (int i = 0; i < NUM_CAMERA_TYPES; ++i) m_camera[i].arcball.set_size(viewport_size);

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
                m_camera[CAMERA_NEXT].arcball.button(int2{io.MousePos} - viewport_offset, io.MouseDown[0]);
                m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
            }

            // the second condition rejects the case where mouse down was on the GUI, but the mouse release is on the
            // background (e.g. when dismissing a popup with a mouse click)
            if (io.MouseReleased[0] && !io.MouseDownOwned[0])
            {
                m_camera[CAMERA_NEXT].arcball.button(int2{io.MousePos} - viewport_offset, io.MouseDown[0]);
                // since the time between mouse down and up could be shorter
                // than the animation duration, we override the previous
                // camera's arcball on mouse up to complete the animation
                m_camera[CAMERA_PREVIOUS].arcball     = m_camera[CAMERA_NEXT].arcball;
                m_camera[CAMERA_PREVIOUS].camera_type = m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
            }

            if (io.MouseDown[0])
                m_camera[CAMERA_NEXT].arcball.motion(int2{io.MousePos} - viewport_offset);
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

        //
        // clear the framebuffer and set up the viewport
        //

        m_render_pass.resize(fbsize);
        m_render_pass.set_viewport(viewport_offset * fbscale, viewport_size * fbscale);
        m_render_pass.set_clear_color(float4{m_bg_color, 1.f});
        m_render_pass.set_cull_mode(RenderPass::CullMode::Disabled);
        m_render_pass.set_depth_test(RenderPass::DepthTest::Less, true);

        m_render_pass.begin();

        float4x4 mvp = m_camera[CAMERA_CURRENT].matrix(float(viewport_size.x) / viewport_size.y);

        //
        // Now render the points and grids
        //
        if (m_view == CAMERA_2D)
        {
            int plot_index = 0;
            for (int y = 0; y < m_num_dimensions; ++y)
                for (int x = 0; x < y; ++x, ++plot_index) draw_2D_points_and_grid(mvp, int2{x, y}, plot_index);

            // draw the text labels for the grid of 2D projections
            for (int i = 0; i < m_num_dimensions - 1; ++i)
            {
                float4x4 pos      = layout_2d_matrix(m_num_dimensions, int2{i, m_num_dimensions - 1});
                float4   text_pos = mul(mvp, mul(pos, float4{0.f, -0.5f, -1.0f, 1.0f}));
                float2   text_2d_pos((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
                draw_text(viewport_offset + int2(int((text_2d_pos.x) * viewport_size.x),
                                                 int((1.f - text_2d_pos.y) * viewport_size.y) + 16),
                          to_string(i), float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular, 16,
                          TextAlign_CENTER | TextAlign_BOTTOM);

                pos         = layout_2d_matrix(m_num_dimensions, int2{0, i + 1});
                text_pos    = mul(mvp, mul(pos, float4{-0.5f, 0.f, -1.0f, 1.0f}));
                text_2d_pos = float2((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
                draw_text(viewport_offset + int2(int((text_2d_pos.x) * viewport_size.x) - 4,
                                                 int((1.f - text_2d_pos.y) * viewport_size.y)),
                          to_string(i + 1), float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular, 16,
                          TextAlign_RIGHT | TextAlign_MIDDLE);
            }
        }
        else
        {
            if (m_show_1d_projections)
            {
                // smash the points against the axes and draw
                float4x4 smashX =
                    mul(translation_matrix(float3{-0.51f, 0.f, 0.f}), scaling_matrix(float3{0.f, 1.f, 1.f}));
                draw_points(mvp, smashX, {0.8f, 0.3f, 0.3f});

                float4x4 smashY =
                    mul(translation_matrix(float3{0.f, -0.51f, 0.f}), scaling_matrix(float3{1.f, 0.f, 1.f}));
                draw_points(mvp, smashY, {0.3f, 0.8f, 0.3f});

                float4x4 smashZ =
                    mul(translation_matrix(float3{0.f, 0.f, -0.51f}), scaling_matrix(float3{1.f, 1.f, 0.f}));
                draw_points(mvp, smashZ, {0.3f, 0.3f, 0.8f});
            }

            draw_points(mvp, float4x4(linalg::identity), m_point_color);

            if (m_show_custom_grid)
            {
                // compute the three dimension pairs we use for the XY, XZ, and ZY axes
                int3   dims = linalg::clamp(m_dimension, int3{0}, int3{m_num_dimensions - 1});
                int2x3 counts{{m_custom_line_counts[dims.x], m_custom_line_counts[dims.y]},
                              {m_custom_line_counts[dims.x], m_custom_line_counts[dims.z]},
                              {m_custom_line_counts[dims.z], m_custom_line_counts[dims.y]}};
                draw_trigrid(m_grid_shader, mvp, 1.f, counts);
            }

            if (m_show_bbox)
                draw_trigrid(m_grid_shader, mvp, 1.0f, int2x3{1});

            if (m_show_coarse_grid)
                draw_trigrid(m_grid_shader, mvp, 0.6f, int2x3{m_samplers[m_sampler]->coarseGridRes(m_point_count)});

            if (m_show_fine_grid)
                draw_trigrid(m_grid_shader, mvp, 0.2f, int2x3{m_point_count});

            //
            // draw the index or coordinate labels around each point
            //
            int2  range  = get_draw_range();
            float radius = m_radius / (m_scale_radius_with_points ? std::sqrt(m_point_count) : 1.0f);

            if (m_show_point_nums || m_show_point_coords)
                for (int p = range.x; p < range.x + range.y; ++p)
                {
                    float4 text_pos = mul(mvp, float4{m_3d_points[p] - 0.5f, 1.f});
                    float2 text_2d_pos((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
                    int2   draw_pos = viewport_offset + int2{int((text_2d_pos.x) * viewport_size.x),
                                                           int((1.f - text_2d_pos.y) * viewport_size.y)};
                    if (m_show_point_nums)
                        draw_text(draw_pos - int2{0, int(radius / 4)}, fmt::format("{:d}", p),
                                  float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular, 12, TextAlign_CENTER | TextAlign_BOTTOM);
                    if (m_show_point_coords)
                        draw_text(draw_pos + int2{0, int(radius / 4)},
                                  fmt::format("({:0.2f}, {:0.2f}, {:0.2f})", m_3d_points[p].x, m_3d_points[p].y,
                                              m_3d_points[p].z),
                                  float4(1.0f, 1.0f, 1.0f, 0.75f), m_regular, 11, TextAlign_CENTER | TextAlign_TOP);
                }
        }

        m_render_pass.end();
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

        m_params.fpsIdling.fpsIdle = 0.f; // during animation, increase FPS
    }
}

void SampleViewer::draw_points(const float4x4 &mvp, const float4x4 &smash, const float3 &color)
{
    auto range = get_draw_range();
    if (range.y <= 0)
        return;

    // Render the point set
    m_3d_point_shader->set_uniform("mvp", mvp);
    m_3d_point_shader->set_uniform("rotation", qmat(qconj(m_camera[CAMERA_CURRENT].arcball.quat())));
    m_3d_point_shader->set_uniform("smash", smash);
    float radius = m_radius / (m_scale_radius_with_points ? std::sqrt(m_point_count) : 1.0f);
    m_3d_point_shader->set_uniform("point_size", radius);
    m_3d_point_shader->set_uniform("color", color);

    m_3d_point_shader->begin();
    m_3d_point_shader->draw_array(Shader::PrimitiveType::TriangleFan, 0, 4, false, range.y);
    m_3d_point_shader->end();
}

void SampleViewer::draw_grid(const float4x4 &mat, int2 size, float alpha)
{
    m_grid_shader->set_uniform("mvp", mat);
    m_grid_shader->set_uniform("size", size);
    m_grid_shader->set_uniform("alpha", alpha);

    auto backup = m_render_pass.depth_test();
    m_render_pass.set_depth_test(RenderPass::DepthTest::Always, false);

    m_grid_shader->begin();
    m_grid_shader->draw_array(Shader::PrimitiveType::TriangleFan, 0, 4);
    m_grid_shader->end();

    m_render_pass.set_depth_test(backup.first, backup.second);
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
void SampleViewer::draw_trigrid(Shader *shader, const float4x4 &mvp, float alpha, const int2x3 &counts)
{
    for (int axis = CAMERA_XY; axis < CAMERA_CURRENT; ++axis)
        if (m_camera[CAMERA_CURRENT].camera_type == axis || m_camera[CAMERA_CURRENT].camera_type == CAMERA_CURRENT)
            draw_grid(mul(mvp, m_camera[axis].arcball.inv_matrix()), counts[axis], alpha);
}

void SampleViewer::draw_text(const int2 &pos, const string &text, const float4 &color, ImFont *font, float font_size,
                             int align) const
{
    ImGui::PushFont(font, font_size);
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
    float radius = m_radius / (m_scale_radius_with_points ? std::sqrt(m_point_count) : 1.0f);

    string out = (format == "eps") ? header_eps(m_point_color, radius) : header_svg(m_point_color);

    float4x4 mvp = m_camera[CAMERA_CURRENT].matrix(1.0f);

    for (int axis = CAMERA_XY; axis < CAMERA_CURRENT; ++axis)
    {
        if (format == "eps")
            out += draw_grids_eps(mul(mvp, m_camera[axis].arcball.inv_matrix()), m_point_count,
                                  m_samplers[m_sampler]->coarseGridRes(m_point_count), m_show_fine_grid,
                                  m_show_coarse_grid, m_show_bbox);
        else
            out += draw_grids_svg(mul(mvp, m_camera[axis].arcball.inv_matrix()), m_point_count,
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
    float radius = m_radius / (m_scale_radius_with_points ? std::sqrt(m_point_count) : 1.0f);

    string out = (format == "eps") ? header_eps(m_point_color, radius) : header_svg(m_point_color);

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

    float radius = m_radius / (m_scale_radius_with_points ? std::sqrt(m_point_count) : 1.0f);

    string out = (format == "eps") ? header_eps(m_point_color, radius * scale) : header_svg(m_point_color);

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
