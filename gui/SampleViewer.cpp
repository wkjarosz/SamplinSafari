/** \file SampleViewer.cpp
    \author Wojciech Jarosz
*/
#define _USE_MATH_DEFINES

#define NOMINMAX

#include "SampleViewer.h"

#include <filesystem>

namespace fs = std::filesystem;
using namespace linalg::ostream_overloads;

using std::cerr;
using std::cout;
using std::endl;
using std::ofstream;
using std::to_string;

#include "hello_imgui/hello_imgui.h"
#include "imgui.h"
#include "imgui_internal.h"

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

#include <cmath>
#include <fmt/core.h>
#include <fstream>
#include <string>
#include <vector>

#include "portable-file-dialogs.h"

#ifndef __EMSCRIPTEN__
#include <glad/glad.h>
#else
#include <GLES3/gl3.h>
#endif

#include "GLFW/glfw3.h"

// clang-format off
#ifdef __EMSCRIPTEN__
    #define VERT_SHADER_HEADER "#version 100\n#define in attribute\n#define out varying\nprecision mediump float;\n"
    #define FRAG_SHADER_HEADER "#version 100\n#define in varying\n#define fo_FragColor gl_FragColor\nprecision mediump float;\n"
#else
    #define VERT_SHADER_HEADER "#version 330\n"
    #define FRAG_SHADER_HEADER "#version 330\nout vec4 fo_FragColor;\n"
#endif
// clang-format on

// Shader sources

static const string pointVertexShader = FRAG_SHADER_HEADER R"(
uniform mat4 mvp;
uniform float pointSize;
in vec3 position;
void main()
{
    gl_Position = mvp * vec4(position, 1.0);
    gl_PointSize = pointSize;
}
)";

static const string pointFragmentShader = FRAG_SHADER_HEADER R"(
uniform vec3 color;
uniform float pointSize;
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
    fo_FragColor = vec4(color, alpha);
}
)";

static const string gridVertexShader = VERT_SHADER_HEADER R"(
uniform mat4 mvp;
in vec3 position;
void main()
{
    gl_Position = mvp * vec4(position, 1.0);
}
)";

static const string gridFragmentShader = FRAG_SHADER_HEADER R"(
uniform float alpha;
void main()
{
    fo_FragColor = vec4(vec3(1.0), alpha);
}
)";

static void computeCameraMatrices(float4x4 &model, float4x4 &lookat, float4x4 &view, float4x4 &proj,
                                  const CameraParameters &c, float window_aspect);
static void drawEPSGrid(const float4x4 &mvp, int grid_res, ofstream &file);
static void writeEPSPoints(const Array2d<float> &points, int start, int count, const float4x4 &mvp, ofstream &file,
                           int dim_x, int dim_y, int dim_z);

static float map_slider_to_radius(float sliderValue)
{
    return sliderValue * sliderValue * 32.0f + 2.0f;
}

static void layout_2d_matrix(float4x4 &position, int numDims, int dim_x, int dim_y)
{
    float cellSpacing = 1.f / (numDims - 1);
    float cellSize    = 0.96f / (numDims - 1);

    float xOffset = dim_x - (numDims - 2) / 2.0f;
    float yOffset = -(dim_y - 1 - (numDims - 2) / 2.0f);

    position = mul(translation_matrix(float3{cellSpacing * xOffset, cellSpacing * yOffset, 1}),
                   scaling_matrix(float3{cellSize, cellSize, 1}));
}

SampleViewer::SampleViewer() : GUIApp()
{
    m_time_strings.resize(3);

    m_samplers.resize(NUM_POINT_TYPES, nullptr);
    m_samplers[RANDOM]   = new Random(m_num_dimensions);
    m_samplers[JITTERED] = new Jittered(1, 1, m_jitter * 0.01f);
    // m_samplers[MULTI_JITTERED]             = new MultiJittered(1, 1, false, 0.0f);
    m_samplers[MULTI_JITTERED_IP] = new MultiJitteredInPlace(1, 1, false, m_jitter * 0.01f);
    // m_samplers[CORRELATED_MULTI_JITTERED]  = new CorrelatedMultiJittered(1, 1, false, 0.0f);
    m_samplers[CORRELATED_MULTI_JITTERED_IP] =
        new CorrelatedMultiJitteredInPlace(1, 1, m_num_dimensions, false, m_jitter * 0.01f);
    m_samplers[CMJND]             = new CMJNDInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f);
    m_samplers[BOSE_OA_IP]        = new BoseOAInPlace(1, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions);
    m_samplers[BOSE_GALOIS_OA_IP] = new BoseGaloisOAInPlace(1, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions);
    m_samplers[BUSH_OA_IP]        = new BushOAInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions);
    m_samplers[BUSH_GALOIS_OA_IP] = new BushGaloisOAInPlace(1, 3, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions);
    m_samplers[ADDEL_KEMP_OA_IP] =
        new AddelmanKempthorneOAInPlace(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions);
    m_samplers[BOSE_BUSH_OA]           = new BoseBushOA(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions);
    m_samplers[BOSE_BUSH_OA_IP]        = new BoseBushOAInPlace(2, MJ_STYLE, false, m_jitter * 0.01f, m_num_dimensions);
    m_samplers[N_ROOKS_IP]             = new NRooksInPlace(m_num_dimensions, 1, false, m_jitter * 0.01f);
    m_samplers[SOBOL]                  = new Sobol(m_num_dimensions);
    m_samplers[ZERO_TWO]               = new ZeroTwo(1, m_num_dimensions, false);
    m_samplers[ZERO_TWO_SHUFFLED]      = new ZeroTwo(1, m_num_dimensions, true);
    m_samplers[HALTON]                 = new Halton(m_num_dimensions);
    m_samplers[HALTON_ZAREMBA]         = new HaltonZaremba(m_num_dimensions);
    m_samplers[HAMMERSLEY]             = new Hammersley<Halton>(m_num_dimensions, 1);
    m_samplers[HAMMERSLEY_ZAREMBA]     = new Hammersley<HaltonZaremba>(m_num_dimensions, 1);
    m_samplers[LARCHER_PILLICHSHAMMER] = new LarcherPillichshammerGK(3, 1, false);

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
    HelloImGui::DockableWindow winEditor;
    winEditor.label         = "Editor";
    winEditor.dockSpaceName = "EditorDock";
    winEditor.canBeClosed   = true;
    winEditor.callBeginEnd  = false;
    winEditor.GuiFunction   = [this] { draw_editor(); };

    // A Log window named "Logs" will be placed in "MiscSpace". It uses the HelloImGui logger gui
    HelloImGui::DockableWindow logsWindow;
    logsWindow.label         = "Logs";
    logsWindow.dockSpaceName = "MiscSpace";
    logsWindow.GuiFunction   = [] { HelloImGui::LogGui(); };

    m_params.dockingParams.dockableWindows = {winEditor, logsWindow};

    // Docking Splits
    {
        HelloImGui::DockingSplit split;
        split.initialDock                    = "MainDockSpace";
        split.newDock                        = "EditorDock";
        split.direction                      = ImGuiDir_Right;
        split.ratio                          = 0.25f;
        m_params.dockingParams.dockingSplits = {split};
    }

    m_params.callbacks.LoadAdditionalFonts = []()
    {
        std::string fontFilename = "fonts/Roboto/Roboto-Regular.ttf";
        // std::string fontFilename = "fonts/DroidSans.ttf";
        if (HelloImGui::AssetExists(fontFilename))
        {
            float   fontSize = 14.f;
            ImFont *font     = HelloImGui::LoadFontTTF_WithFontAwesomeIcons(fontFilename, fontSize, false);
            (void)font;
        }
    };

    m_params.callbacks.ShowAppMenuItems = [this]()
    {
        if (ImGui::MenuItem(ICON_FA_SAVE "  Save as EPS"))
        {
            try
            {
                auto basename = pfd::save_file("Base filename").result();
                if (basename.empty())
                    return;

                HelloImGui::Log(HelloImGui::LogLevel::Info, "Saving to base filename: %s.", basename.c_str());

                ofstream fileAll(basename + "_all2D.eps");
                draw_contents_2D_EPS(fileAll);

                ofstream fileXYZ(basename + "_012.eps");
                draw_contents_EPS(fileXYZ, CAMERA_CURRENT, m_dimension.x, m_dimension.y, m_dimension.z);

                for (int y = 0; y < m_num_dimensions; ++y)
                    for (int x = 0; x < y; ++x)
                    {
                        ofstream fileXY(fmt::format("{:s}_{:d}{:d}.eps", basename, x, y));
                        draw_contents_EPS(fileXY, CAMERA_XY, x, y, 2);
                    }
            }
            catch (const std::exception &e)
            {
                HelloImGui::Log(HelloImGui::LogLevel::Error, "An error occurred: %s.", e.what());
            }
        }
    };

    m_params.callbacks.ShowGui = [this]() { draw_gui(); };
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
        count = m_point_draw_count;
    }
    return {start, count};
}

void SampleViewer::draw_gui()
{
    HelloImGui::Log(HelloImGui::LogLevel::Info, "Current working directory: %s.", fs::current_path().c_str());

    m_viewport_pos_GL = m_viewport_pos = {0, 0};
    m_viewport_size                    = m_fbsize;
    if (auto id = m_params.dockingParams.dockSpaceIdFromName("MainDockSpace"))
    {
        auto central_node = ImGui::DockBuilderGetCentralNode(*id);
        m_viewport_size   = int2{int(central_node->Size.x), int(central_node->Size.y)};
        m_viewport_pos    = int2{int(central_node->Pos.x), int(central_node->Pos.y)};
        // flip y coordinates between ImGui and OpenGL screen coordinates
        m_viewport_pos_GL = int2{int(central_node->Pos.x),
                                 int(ImGui::GetIO().DisplaySize.y - (central_node->Pos.y + central_node->Size.y))};
    }

    float radius = map_slider_to_radius(m_radius);
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);

    float4x4 model, lookat, view, proj, mvp;
    computeCameraMatrices(model, lookat, view, proj, m_camera[CAMERA_CURRENT],
                          float(m_viewport_size.x) / m_viewport_size.y);
    mvp = mul(proj, mul(lookat, mul(view, model)));

    if (m_view == CAMERA_2D)
    {
        float4x4 pos;

        for (int i = 0; i < m_num_dimensions - 1; ++i)
        {
            layout_2d_matrix(pos, m_num_dimensions, i, m_num_dimensions - 1);
            float4 text_pos = mul(mvp, mul(pos, float4{0.f, -0.5f, 0.0f, 1.0f}));
            float2 text_2d_pos((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
            draw_text(m_viewport_pos +
                          int2((text_2d_pos.x) * m_viewport_size.x, (1.f - text_2d_pos.y) * m_viewport_size.y + 16),
                      to_string(i), float4(1.0f, 1.0f, 1.0f, 0.75f), 16, 0, TextAlign_CENTER | TextAlign_BOTTOM);

            layout_2d_matrix(pos, m_num_dimensions, 0, i + 1);
            text_pos    = mul(mvp, mul(pos, float4{-0.5f, 0.f, 0.0f, 1.0f}));
            text_2d_pos = float2((text_pos.x / text_pos.w + 1) / 2, (text_pos.y / text_pos.w + 1) / 2);
            draw_text(m_viewport_pos +
                          int2((text_2d_pos.x) * m_viewport_size.x - 4, (1.f - text_2d_pos.y) * m_viewport_size.y),
                      to_string(i + 1), float4(1.0f, 1.0f, 1.0f, 0.75f), 16, 0, TextAlign_RIGHT | TextAlign_MIDDLE);
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i)
            draw_text(m_viewport_pos + m_viewport_size - int2(200, (2 - i) * 14 + 20), m_time_strings[i],
                      float4(1.0f, 1.0f, 1.0f, 0.75f), 16, 0);

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
                              fmt::format("{:d}", p), float4(1.0f, 1.0f, 1.0f, 0.75f), 12, 0,
                              TextAlign_CENTER | TextAlign_BOTTOM);
                if (m_show_point_coords)
                    draw_text(m_viewport_pos + int2((text_2d_pos.x) * m_viewport_size.x,
                                                    (1.f - text_2d_pos.y) * m_viewport_size.y + radius / 4),
                              fmt::format("({:0.2f}, {:0.2f}, {:0.2f})", point.x + 0.5, point.y + 0.5, point.z + 0.5),
                              float4(1.0f, 1.0f, 1.0f, 0.75f), 11, 0, TextAlign_CENTER | TextAlign_TOP);
            }
    }
}

void SampleViewer::draw_editor()
{
    bool accept_keys = !ImGui::GetIO().WantCaptureKeyboard;
    if (accept_keys && ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        m_params.appShallExit = true;
        return;
    }

    ImGui::Begin("Sample parameters");
    {
        // =========================================================
        ImGui::SeparatorText("Point set");
        // =========================================================

        if (ImGui::BeginCombo("##", m_samplers[m_sampler]->name().c_str()))
        {
            for (int n = 0; n < NUM_POINT_TYPES; n++)
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
        ImGui::SetItemTooltip("Key: Up/Down");
        if (accept_keys && !ImGui::IsKeyDown(ImGuiMod_Shift) &&
            (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)))
        {
            int delta        = ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : -1;
            m_sampler        = mod(m_sampler + delta, (int)NUM_POINT_TYPES);
            Sampler *sampler = m_samplers[m_sampler];
            sampler->setJitter(m_jitter * 0.01f);
            sampler->setRandomized(m_randomize);
            update_GPU_points();
            update_GPU_grids();
        }

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
        ImGui::SetItemTooltip("Key: Left/Right");
        if (accept_keys && (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow)))
        {
            m_target_point_count =
                std::max(1u, ImGui::IsKeyPressed(ImGuiKey_RightArrow) ? roundUpPow2(m_target_point_count + 1)
                                                                      : roundDownPow2(m_target_point_count - 1));
            update_GPU_points();
            update_GPU_grids();
        }

        if (ImGui::SliderInt("Dimensions", &m_num_dimensions, 2, 10, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            update_GPU_points();
            update_GPU_grids();
        }
        ImGui::SetItemTooltip("Key: D/d");
        if (accept_keys && ImGui::IsKeyPressed(ImGuiKey_D))
        {
            m_num_dimensions = std::clamp(m_num_dimensions + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 1 : -1), 2, 10);
            update_GPU_points();
            update_GPU_grids();
        }

        if (ImGui::Checkbox("Randomize", &m_randomize))
            update_GPU_points();
        ImGui::SetItemTooltip("Key: R/r");
        if (accept_keys && ImGui::IsKeyPressed(ImGuiKey_R))
        {
            if (ImGui::IsKeyDown(ImGuiMod_Shift))
            {
                m_randomize = true;
                m_samplers[m_sampler]->setRandomized(true);
            }
            else
                m_randomize = !m_randomize;
            update_GPU_points();
        }

        if (ImGui::SliderFloat("Jitter", &m_jitter, 0.f, 100.f, "%3.1f%%"))
        {
            m_samplers[m_sampler]->setJitter(m_jitter * 0.01f);
            update_GPU_points();
            update_GPU_grids();
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
                    update_GPU_points();
                    update_GPU_grids();
                }
            };
            int strength = oa->strength();
            if (ImGui::InputInt("Strength", &strength, 1))
                change_strength(std::max(2, strength));
            ImGui::SetItemTooltip("Key: T/t");
            if (accept_keys && ImGui::IsKeyPressed(ImGuiKey_T))
                change_strength(std::max(2, strength + (ImGui::IsKeyDown(ImGuiMod_Shift) ? 1 : -1)));

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
            ImGui::SetItemTooltip("Key: Shift+Up/Down");
            if (accept_keys && ImGui::IsKeyDown(ImGuiMod_Shift) &&
                (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow)))
                change_offset_type(mod((int)oa->offsetType() + (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : -1),
                                       (int)NUM_OFFSET_TYPES));
        }

        // =========================================================
        ImGui::SeparatorText("Camera/view");
        // =========================================================

        // ImGui::RadioButton("XY", &m_view, 0);
        // ImGui::SameLine();
        // ImGui::RadioButton("YZ", &m_view, 1);
        // ImGui::SameLine();
        // ImGui::RadioButton("XZ", &m_view, 2);
        // ImGui::SameLine();
        // ImGui::RadioButton("XYZ", &m_view, 3);
        // ImGui::SameLine();
        // ImGui::RadioButton("2D", &m_view, 4);

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
            ImGui::SetItemTooltip("Key: %d", (i + 1) % IM_ARRAYSIZE(items));
            ImGui::PopStyleColor(is_selected);
        }

        ImGui::PopStyleVar();

        if (accept_keys)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_1))
                set_view(CAMERA_XY);
            else if (ImGui::IsKeyPressed(ImGuiKey_2))
                set_view(CAMERA_YZ);
            else if (ImGui::IsKeyPressed(ImGuiKey_3))
                set_view(CAMERA_XZ);
            else if (ImGui::IsKeyPressed(ImGuiKey_4))
                set_view(CAMERA_CURRENT);
            else if (ImGui::IsKeyPressed(ImGuiKey_0))
                set_view(CAMERA_2D);
        }

        // =========================================================
        ImGui::SeparatorText("Display options");
        // =========================================================

        bool pushed = m_scale_radius_with_points;
        if (pushed)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);

        ImGui::SliderFloat("Radius", &m_radius, 0.f, 1.f, "");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_COMPRESS))
            m_scale_radius_with_points = !m_scale_radius_with_points;
        ImGui::SetItemTooltip("Scale radius with number of points");
        if (pushed)
            ImGui::PopStyleColor();

        ImGui::Checkbox("1D projections", &m_show_1d_projections);
        ImGui::SetItemTooltip("Key: p");
        if (accept_keys && ImGui::IsKeyPressed(ImGuiKey_P))
            m_show_1d_projections = !m_show_1d_projections;

        ImGui::Checkbox("Point indices", &m_show_point_nums);
        ImGui::Checkbox("Point coords", &m_show_point_coords);

        ImGui::Checkbox("Coarse grid", &m_show_coarse_grid);
        ImGui::SetItemTooltip("Key: g");
        ImGui::Checkbox("Fine grid", &m_show_fine_grid);
        ImGui::SetItemTooltip("Key: G");
        if (accept_keys && ImGui::IsKeyPressed(ImGuiKey_G))
        {
            if (ImGui::IsKeyDown(ImGuiMod_Shift))
                m_show_fine_grid = !m_show_fine_grid;
            else
                m_show_coarse_grid = !m_show_coarse_grid;
            update_GPU_grids();
        }

        ImGui::Checkbox("Bounding box", &m_show_bbox);
        ImGui::SetItemTooltip("Key: b");
        if (accept_keys && ImGui::IsKeyPressed(ImGuiKey_B))
        {
            m_show_bbox = !m_show_bbox;
            update_GPU_grids();
        }

        // =========================================================
        ImGui::SeparatorText("Dimension mapping");
        // =========================================================

        if (ImGui::SliderInt3("XYZ", &m_dimension[0], 0, m_num_dimensions - 1, "%d", ImGuiSliderFlags_AlwaysClamp))
            update_GPU_points(false);

        // =========================================================
        ImGui::SeparatorText("Visible subset");
        // =========================================================

        if (ImGui::Checkbox("Subset by point index", &m_subset_by_index))
            update_GPU_points(false);
        if (m_subset_by_index)
        {
            m_subset_by_coord = false;
            ImGui::SliderInt("First point", &m_first_draw_point, 0, m_point_count - 1, "%d",
                             ImGuiSliderFlags_AlwaysClamp);
            ImGui::SliderInt("Num subset points", &m_point_draw_count, 0, m_point_count - m_first_draw_point, "%d",
                             ImGuiSliderFlags_AlwaysClamp);
        }

        if (ImGui::Checkbox("Subset by coordinates", &m_subset_by_coord))
            update_GPU_points(false);
        if (m_subset_by_coord)
        {
            m_subset_by_index = false;
            if (ImGui::SliderInt("Subset axis", &m_subset_axis, 0, m_num_dimensions - 1, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                update_GPU_points(false);
            if (ImGui::SliderInt("Num levels", &m_num_subset_levels, 1, m_point_count, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                update_GPU_points(false);
            if (ImGui::SliderInt("Level", &m_subset_level, 0, m_num_subset_levels - 1, "%d",
                                 ImGuiSliderFlags_AlwaysClamp))
                update_GPU_points(false);
        }
    }
    ImGui::End();
}

void SampleViewer::initialize_GL()
{
    try
    {
#ifndef __EMSCRIPTEN__
        glEnable(GL_PROGRAM_POINT_SIZE);
        glEnable(GL_LINE_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
#endif
        m_point_shader =
            new Shader("Point shader", pointVertexShader, pointFragmentShader, Shader::BlendMode::AlphaBlend);

        m_grid_shader = new Shader("Grid shader", gridVertexShader, gridFragmentShader, Shader::BlendMode::AlphaBlend);

        m_point_2d_shader =
            new Shader("Point shader 2D", pointVertexShader, pointFragmentShader, Shader::BlendMode::AlphaBlend);

        update_GPU_points();
        update_GPU_grids();

        HelloImGui::Log(HelloImGui::LogLevel::Info, "Successfully initialized GL!");
    }
    catch (const std::exception &e)
    {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Shader initialization failed!:\n\t%s.", e.what());
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
            HelloImGui::Log(HelloImGui::LogLevel::Error, "An error occurred while generating points: %s.", e.what());
            return;
        }
    }

    // Upload points to GPU

    populate_point_subset();

    m_point_shader->set_buffer("position", m_3d_points);

    // Upload 2D points to GPU
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

    // m_point_count_box->set_value(m_point_count);
    // m_point_count_slider->set_value(mapCount2Slider(m_point_count));
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

bool SampleViewer::mouse_motion_event(const int2 &p, const int2 &rel, int button, int modifiers)
{
    if (m_camera[CAMERA_NEXT].arcball.motion(p - m_viewport_pos))
    {
        draw();
        return true;
    }

    return false;
}

bool SampleViewer::mouse_button_event(const int2 &p, int button, bool down, int modifiers)
{
    if (button == GLFW_MOUSE_BUTTON_1)
    {
        if (down)
        {
            m_mouse_down = true;
            // on mouse down we start switching to a perspective camera
            // and start recording the arcball rotation in CAMERA_NEXT
            set_view(CAMERA_CURRENT);
            m_camera[CAMERA_NEXT].arcball.button(p - m_viewport_pos, down);
            m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
        }
        else
        {
            m_mouse_down = false;
            m_camera[CAMERA_NEXT].arcball.button(p - m_viewport_pos, down);
            // since the time between mouse down and up could be shorter
            // than the animation duration, we override the previous
            // camera's arcball on mouse up to complete the animation
            m_camera[CAMERA_PREVIOUS].arcball     = m_camera[CAMERA_NEXT].arcball;
            m_camera[CAMERA_PREVIOUS].camera_type = m_camera[CAMERA_NEXT].camera_type = CAMERA_CURRENT;
        }
        return true;
    }

    return false;
}

bool SampleViewer::scroll_event(const int2 &p, const float2 &rel)
{
    m_camera[CAMERA_NEXT].zoom = std::max(0.001, m_camera[CAMERA_NEXT].zoom * pow(1.1, rel.y));
    draw();
    return true;
}

void SampleViewer::draw_points(const float4x4 &mvp, const float3 &color)
{
    // Render the point set
    m_point_shader->set_uniform("mvp", mvp);
    float radius = map_slider_to_radius(m_radius);
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);
    m_point_shader->set_uniform("pointSize", radius);
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

void SampleViewer::draw_2D_points_and_grid(const float4x4 &mvp, int dim_x, int dim_y, int plot_index)
{
    float4x4 pos;
    layout_2d_matrix(pos, m_num_dimensions, dim_x, dim_y);

    // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
    // Render the point set
    m_point_2d_shader->set_uniform("mvp", mul(mvp, pos));
    float radius = map_slider_to_radius(m_radius / (m_num_dimensions - 1));
    if (m_scale_radius_with_points)
        radius *= 64.0f / std::sqrt(m_point_count);
    m_point_2d_shader->set_uniform("pointSize", radius);
    m_point_2d_shader->set_uniform("color", float3(0.9f, 0.55f, 0.1f));

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

void SampleViewer::clear_and_setup_viewport()
{
    try
    {
        // account for dpi factor on retina screens
        float scale = pixel_ratio();

        const float4 background_color{0.f, 0.f, 0.f, 1.f};

        // first clear the entire window with the background color
        CHK(glViewport(0, 0, m_fbsize.x, m_fbsize.y));
        CHK(glClearColor(background_color[0], background_color[1], background_color[2], background_color[3]));
        CHK(glClear(GL_COLOR_BUFFER_BIT));

        // now set up a new viewport for the rest of the drawing
        CHK(glViewport(m_viewport_pos_GL.x * scale, m_viewport_pos_GL.y * scale, m_viewport_size.x * scale,
                       m_viewport_size.y * scale));

        // inform the arcballs of the viewport size
        for (int i = 0; i < NUM_CAMERA_TYPES; ++i)
            m_camera[i].arcball.set_size(m_viewport_size);
    }
    catch (const std::exception &e)
    {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "OpenGL drawing failed:\n\t%s.", e.what());
    }
}

void SampleViewer::draw()
{
    clear_and_setup_viewport();

    // update/move the camera
    update_current_camera();

    float4x4 model, lookat, view, proj, mvp;
    computeCameraMatrices(model, lookat, view, proj, m_camera[CAMERA_CURRENT],
                          float(m_viewport_size.x) / m_viewport_size.y);
    mvp = mul(proj, mul(lookat, mul(view, model)));

    if (m_view == CAMERA_2D)
    {
        int plot_index = 0;
        for (int y = 0; y < m_num_dimensions; ++y)
            for (int x = 0; x < y; ++x, ++plot_index)
                draw_2D_points_and_grid(mvp, x, y, plot_index);
    }
    else
    {
        // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
        draw_points(mvp, {0.9f, 0.55f, 0.1f});

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

        // m_render_pass->set_depth_test(RenderPass::DepthTest::Less, true);
        if (m_show_bbox)
            draw_grid(mvp, 1.0f, 0, 8);

        if (m_show_coarse_grid)
            draw_grid(mvp, 0.6f, 8, m_coarse_line_count);

        if (m_show_fine_grid)
            draw_grid(mvp, 0.2f, 8 + m_coarse_line_count, m_fine_line_count);
    }
}

void SampleViewer::draw_text(const int2 &pos, const string &text, const float4 &color, int font_size, int fixed_width,
                             int align) const
{
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
}

void SampleViewer::draw_contents_EPS(ofstream &file, CameraType camera_type, int dim_x, int dim_y, int dim_z)
{
    int size = 900;
    int crop = 720;

    int pointScale = m_samplers[m_sampler]->coarseGridRes(m_point_count);

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

    float4x4 model, lookat, view, proj, mvp;
    computeCameraMatrices(model, lookat, view, proj, m_camera[camera_type], 1.0f);
    mvp            = mul(proj, mul(lookat, mul(view, model)));
    int start_axis = CAMERA_XY, end_axis = CAMERA_XZ;
    if (camera_type != CAMERA_CURRENT)
        start_axis = end_axis = camera_type;
    if (m_show_fine_grid)
    {
        file << "% Draw fine grids \n";
        file << "flc setgray %fill color for fine grid \n";
        file << "flw setlinewidth\n";
        for (int axis = start_axis; axis <= end_axis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            float4x4 mvp2 = mul(mvp, m_camera[axis].arcball.matrix());
            drawEPSGrid(mvp2, m_point_count, file);
        }
    }

    if (m_show_coarse_grid)
    {
        file << "% Draw coarse grids \n";
        file << "clc setgray %fill color for coarse grid \n";
        file << "clw setlinewidth\n";
        for (int axis = start_axis; axis <= end_axis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            float4x4 mvp2 = mul(mvp, m_camera[axis].arcball.matrix());
            drawEPSGrid(mvp2, m_samplers[m_sampler]->coarseGridRes(m_point_count), file);
        }
    }

    if (m_show_bbox)
    {
        file << "% Draw bounding boxes \n";
        file << "blc setgray %fill color for bounding box \n";
        file << "blw setlinewidth\n";
        for (int axis = start_axis; axis <= end_axis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            float4x4 mvp2 = mul(mvp, m_camera[axis].arcball.matrix());
            drawEPSGrid(mvp2, 1, file);
        }
    }

    // Generate and render the point set
    {
        generate_points();
        populate_point_subset();
        file << "% Draw points \n";
        file << "pfc setrgbcolor %fill color for points\n";

        int2 range = get_draw_range();
        writeEPSPoints(m_subset_points, range.x, range.y, mvp, file, dim_x, dim_y, dim_z);
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

    float4x4 model, lookat, view, proj, mvp;
    computeCameraMatrices(model, lookat, view, proj, m_camera[CAMERA_2D], 1.0f);
    mvp = mul(proj, mul(lookat, mul(view, model)));

    // Generate and render the point set
    {
        generate_points();
        populate_point_subset();

        for (int y = 0; y < m_num_dimensions; ++y)
            for (int x = 0; x < y; ++x)
            {
                float4x4 pos;
                layout_2d_matrix(pos, m_num_dimensions, x, y);

                if (m_show_fine_grid)
                {
                    file << "% Draw fine grid \n";
                    file << "flc setgray %fill color for fine grid \n";
                    file << "flw setlinewidth\n";
                    drawEPSGrid(mul(mvp, pos), m_point_count, file);
                }
                if (m_show_coarse_grid)
                {
                    file << "% Draw coarse grids \n";
                    file << "clc setgray %fill color for coarse grid \n";
                    file << "clw setlinewidth\n";
                    drawEPSGrid(mul(mvp, pos), m_samplers[m_sampler]->coarseGridRes(m_point_count), file);
                }
                if (m_show_bbox)
                {
                    file << "% Draw bounding box \n";
                    file << "blc setgray %fill color for bounding box \n";
                    file << "blw setlinewidth\n";
                    drawEPSGrid(mul(mvp, pos), 1, file);
                }
            }

        file << "% Draw points \n";
        file << "pfc setrgbcolor %fill color for points\n";
        for (int y = 0; y < m_num_dimensions; ++y)
            for (int x = 0; x < y; ++x)
            {
                float4x4 pos;
                layout_2d_matrix(pos, m_num_dimensions, x, y);
                file << "% Draw (" << x << "," << y << ") points\n";

                int2 range = get_draw_range();

                writeEPSPoints(m_subset_points, range.x, range.y, mul(mvp, pos), file, x, y, 2);
            }
    }

    file << "grestore\n";
}
void SampleViewer::set_view(CameraType view)
{
    m_animate_start_time                 = (float)glfwGetTime();
    m_camera[CAMERA_PREVIOUS]            = m_camera[CAMERA_CURRENT];
    m_camera[CAMERA_NEXT]                = m_camera[view];
    m_camera[CAMERA_NEXT].persp_factor   = (view == CAMERA_CURRENT) ? 1.0f : 0.0f;
    m_camera[CAMERA_NEXT].camera_type    = view;
    m_camera[CAMERA_CURRENT].camera_type = (view == m_camera[CAMERA_CURRENT].camera_type) ? view : CAMERA_CURRENT;
    m_view                               = view;

    m_params.fpsIdling.fpsIdle = 0.f; // during animation, increase FPS
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
        camera.camera_type         = camera1.camera_type;
        m_params.fpsIdling.fpsIdle = 9.f; // animation is done, reduce FPS
        m_animate_start_time       = 0.f;
    }

    // if we are dragging the mouse, then just use the current arcball
    // rotation, otherwise, interpolate between the previous and next camera
    // orientations
    // auto w = static_cast<GLFWwindow *>(m_params.backendPointers.glfwWindow);
    // if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    if (m_mouse_down)
        camera.arcball = camera1.arcball;
    else
        camera.arcball.set_state(qslerp(camera0.arcball.state(), camera1.arcball.state(), t));
}

void SampleViewer::generate_points()
{
    float    time0     = (float)glfwGetTime();
    Sampler *generator = m_samplers[m_sampler];
    if (generator->randomized() != m_randomize)
        generator->setRandomized(m_randomize);

    generator->setDimensions(m_num_dimensions);

    int num_pts   = generator->setNumSamples(m_target_point_count);
    m_point_count = num_pts > 0 ? num_pts : m_target_point_count;

    float time1      = (float)glfwGetTime();
    float time_diff1 = ((time1 - time0) * 1000.0f);

    m_points.resize(m_point_count, m_num_dimensions);
    m_3d_points.resize(m_point_count);

    time1 = (float)glfwGetTime();
    for (int i = 0; i < m_point_count; ++i)
    {
        vector<float> r(m_num_dimensions, 0.5f);
        generator->sample(r.data(), i);
        for (int j = 0; j < m_points.sizeY(); ++j)
        {
            m_points(i, j) = r[j] - 0.5f;
            // HelloImGui::Log(HelloImGui::LogLevel::Debug, "%f", m_points(i, j));
        }
    }
    float time2      = (float)glfwGetTime();
    float time_diff2 = ((time2 - time1) * 1000.0f);

    m_time_strings[0] = fmt::format("Reset and m_randomize: {:3.3f} ms", time_diff1);
    m_time_strings[1] = fmt::format("Sampling: {:3.3f} ms", time_diff2);
    m_time_strings[2] = fmt::format("Total generation time: {:3.3f} ms", time_diff1 + time_diff2);
}

void SampleViewer::populate_point_subset()
{
    //
    // Populate point subsets
    //
    m_subset_points = m_points;
    m_subset_count  = m_point_count;
    if (m_subset_by_coord)
    {
        // cout << "only accepting points where the coordinates along the " << m_subset_axis->value()
        //      << " axis are within: ["
        //      << ((m_subset_level->value() + 0.0f)/m_num_subset_levels->value()) << ", "
        //      << ((m_subset_level->value()+1.0f)/m_num_subset_levels->value()) << ")." << endl;
        m_subset_count = 0;
        for (int i = 0; i < m_points.sizeX(); ++i)
        {
            float v = m_points(i, m_subset_axis);
            // cout << v << endl;
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

static void computeCameraMatrices(float4x4 &model, float4x4 &lookat, float4x4 &view, float4x4 &proj,
                                  const CameraParameters &c, float window_aspect)
{
    model = scaling_matrix(float3(c.zoom));
    if (c.camera_type == CAMERA_XY || c.camera_type == CAMERA_2D)
        model = mul(scaling_matrix(float3(1, 1, 0)), model);
    if (c.camera_type == CAMERA_XZ)
        model = mul(scaling_matrix(float3(1, 0, 1)), model);
    if (c.camera_type == CAMERA_YZ)
        model = mul(scaling_matrix(float3(0, 1, 1)), model);

    float fH = std::tan(c.view_angle / 360.0f * M_PI) * c.dnear;
    float fW = fH * window_aspect;

    float    oFF   = c.eye.z / c.dnear;
    float4x4 orth  = linalg::ortho_matrix(-fW * oFF, fW * oFF, -fH * oFF, fH * oFF, c.dnear, c.dfar);
    float4x4 frust = linalg::frustum_matrix(-fW, fW, -fH, fH, c.dnear, c.dfar);

    proj   = lerp(orth, frust, c.persp_factor);
    lookat = linalg::lookat_matrix(c.eye, c.center, c.up);
    view   = c.arcball.matrix();
}

static void drawEPSGrid(const float4x4 &mvp, int grid_res, ofstream &file)
{
    float4 vA4d, vB4d;
    float2 vA, vB;

    int   fine_grid_res = 2;
    float coarse_scale = 1.f / grid_res, fine_scale = 1.f / fine_grid_res;

    // draw the bounding box
    // if (grid_res == 1)
    {
        float4 c004d = mul(mvp, float4{0.f - 0.5f, 0.f - 0.5f, 0.f, 1.f});
        float4 c104d = mul(mvp, float4{1.f - 0.5f, 0.f - 0.5f, 0.f, 1.f});
        float4 c114d = mul(mvp, float4{1.f - 0.5f, 1.f - 0.5f, 0.f, 1.f});
        float4 c014d = mul(mvp, float4{0.f - 0.5f, 1.f - 0.5f, 0.f, 1.f});
        float2 c00   = float2(c004d.x / c004d.w, c004d.y / c004d.w);
        float2 c10   = float2(c104d.x / c104d.w, c104d.y / c104d.w);
        float2 c11   = float2(c114d.x / c114d.w, c114d.y / c114d.w);
        float2 c01   = float2(c014d.x / c014d.w, c014d.y / c014d.w);

        file << "newpath\n";
        file << c00.x << " " << c00.y << " moveto\n";
        file << c10.x << " " << c10.y << " lineto\n";
        file << c11.x << " " << c11.y << " lineto\n";
        file << c01.x << " " << c01.y << " lineto\n";
        file << "closepath\n";
        file << "stroke\n";
    }

    for (int i = 1; i <= grid_res - 1; i++)
    {
        // draw horizontal lines
        file << "newpath\n";
        for (int j = 0; j < fine_grid_res; j++)
        {
            vA4d = mul(mvp, float4{j * fine_scale - 0.5f, i * coarse_scale - 0.5f, 0.0f, 1.0f});
            vB4d = mul(mvp, float4{(j + 1) * fine_scale - 0.5f, i * coarse_scale - 0.5f, 0.0f, 1.0f});

            vA = float2(vA4d.x / vA4d.w, vA4d.y / vA4d.w);
            vB = float2(vB4d.x / vB4d.w, vB4d.y / vB4d.w);

            file << vA.x << " " << vA.y;

            if (j == 0)
                file << " moveto\n";
            else
                file << " lineto\n";

            file << vB.x << " " << vB.y;
            file << " lineto\n";
        }
        file << "stroke\n";

        // draw vertical lines
        file << "newpath\n";
        for (int j = 0; j < fine_grid_res; j++)
        {
            vA4d = mul(mvp, float4{i * coarse_scale - 0.5f, j * fine_scale - 0.5f, 0.0f, 1.0f});
            vB4d = mul(mvp, float4{i * coarse_scale - 0.5f, (j + 1) * fine_scale - 0.5f, 0.0f, 1.0f});

            vA = float2(vA4d.x / vA4d.w, vA4d.y / vA4d.w);
            vB = float2(vB4d.x / vB4d.w, vB4d.y / vB4d.w);

            file << vA.x << " " << vA.y;

            if (j == 0)
                file << " moveto\n";
            else
                file << " lineto\n";

            file << vB.x << " " << vB.y;
            file << " lineto\n";
        }
        file << "stroke\n";
    }
}

static void writeEPSPoints(const Array2d<float> &points, int start, int count, const float4x4 &mvp, ofstream &file,
                           int dim_x, int dim_y, int dim_z)
{
    for (int i = start; i < start + count; i++)
    {
        float4 v4d = mul(mvp, float4{points(i, dim_x), points(i, dim_y), points(i, dim_z), 1.0f});
        float2 v2d(v4d.x / v4d.w, v4d.y / v4d.w);
        file << v2d.x << " " << v2d.y << " p\n";
    }
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
