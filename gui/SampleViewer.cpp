/** \file SampleViewer.cpp
    \author Wojciech Jarosz
*/
#include "SampleViewer.h"

#include <nanogui/screen.h>
#include <nanogui/theme.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/combobox.h>
#include <nanogui/slider.h>
#include <nanogui/textbox.h>
#include <nanogui/entypo.h>
#include <nanogui/checkbox.h>
#include <nanogui/messagedialog.h>
#include <tinyformat.h>

#include "Well.h"
#include <sampler/Misc.h>
#include <sampler/MultiJittered.h>
#include <sampler/NRooks.h>
#include <sampler/Random.h>
#include <sampler/OA.h>
#include <sampler/OAAddelmanKempthorne.h>
#include <sampler/OABoseBush.h>
#include <sampler/OABush.h>
#include <sampler/OACMJND.h>
#include <sampler/Jittered.h>
#include <sampler/Sobol.h>
#include <sampler/Halton.h>
#include <sampler/Hammersley.h>
#include <sampler/LP.h>

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

void computeCameraMatrices(Matrix4f &model,
                           Matrix4f &lookat,
                           Matrix4f &view,
                           Matrix4f &proj,
                           const CameraParameters & c,
                           float windowAspect);
void drawEPSGrid(const Matrix4f & mvp, int gridRes, ofstream & file);
void writeEPSPoints(const MatrixXf & points, int start, int count, const Matrix4f & mvp, ofstream & file,
                    int dimX, int dimY, int dimZ);

float mapCount2Slider(int count)
{
    return (log((float) count) / log(2.f)) / 20;
}

int mapSlider2Count(float sliderValue)
{
    return (int) pow(2.f, round(80 * sliderValue)/4.0f);
}


// float mapRadius2Slider(float radius)
// {
//     return sqrt((radius - 1)/32.0f);
// }
float mapSlider2Radius(float sliderValue)
{
    return sliderValue * sliderValue * 32.0f + 1.0f;
}

void layout2DMatrix(Matrix4f & position, int numDims, int dimX, int dimY)
{
    float cellSpacing = 1.f/(numDims-1);
    float cellSize = 0.96f/(numDims-1);

    float xOffset = dimX - (numDims-2)/2.0f;
    float yOffset = -(dimY - 1 - (numDims-2)/2.0f);

    position = translate(Vector3f(cellSpacing*xOffset, cellSpacing*yOffset, 1)) * scale(Vector3f(cellSize, cellSize, 1));
}
} // namespace


SampleViewer::SampleViewer()
    : Screen(Vector2i(800,600), "Samplin' Safari", true, false, 8, 8, 24, 8, 4)
{
    m_samplers.resize(NUM_POINT_TYPES, nullptr);
    m_samplers[RANDOM]                     = new Random(m_numDimensions);
    m_samplers[JITTERED]                   = new Jittered(1, 1, 1.0f);
    // m_samplers[MULTI_JITTERED]             = new MultiJittered(1, 1, false, 0.0f);
    m_samplers[MULTI_JITTERED_IP]          = new MultiJitteredInPlace(1, 1, false, 0.0f);
    // m_samplers[CORRELATED_MULTI_JITTERED]  = new CorrelatedMultiJittered(1, 1, false, 0.0f);
    m_samplers[CORRELATED_MULTI_JITTERED_IP] = new CorrelatedMultiJitteredInPlace(1, 1, m_numDimensions, false, 0.0f);
    m_samplers[CMJND]                      = new CMJNDInPlace(1, 3, MJ_STYLE, false, 0.8);
    m_samplers[BOSE_OA_IP]                 = new BoseOAInPlace(1, MJ_STYLE, false, 0.8f, m_numDimensions);
    m_samplers[BOSE_GALOIS_OA_IP]          = new BoseGaloisOAInPlace(1, MJ_STYLE, false, 0.8f, m_numDimensions);
    m_samplers[BUSH_OA_IP]                 = new BushOAInPlace(1, 3, MJ_STYLE, false, 0.8f, m_numDimensions);
    m_samplers[BUSH_GALOIS_OA_IP]          = new BushGaloisOAInPlace(1, 3, MJ_STYLE, false, 0.8f, m_numDimensions);
    m_samplers[ADDEL_KEMP_OA_IP]           = new AddelmanKempthorneOAInPlace(2, MJ_STYLE, false, 0.8f, m_numDimensions);
    m_samplers[BOSE_BUSH_OA]               = new BoseBushOA(2, MJ_STYLE, false, 0.8f, m_numDimensions);
    m_samplers[BOSE_BUSH_OA_IP]            = new BoseBushOAInPlace(2, MJ_STYLE, false, 0.8f, m_numDimensions);
    m_samplers[N_ROOKS_IP]                 = new NRooksInPlace(m_numDimensions, 1, false, 1.0f);
    m_samplers[SOBOL]                      = new Sobol(m_numDimensions);
    m_samplers[ZERO_TWO]                   = new ZeroTwo(1, m_numDimensions, false);
    m_samplers[ZERO_TWO_SHUFFLED]          = new ZeroTwo(1, m_numDimensions, true);
    m_samplers[HALTON]                     = new Halton(m_numDimensions);
    m_samplers[HALTON_ZAREMBA]             = new HaltonZaremba(m_numDimensions);
    m_samplers[HAMMERSLEY]                 = new Hammersley<Halton>(m_numDimensions, 1);
    m_samplers[HAMMERSLEY_ZAREMBA]         = new Hammersley<HaltonZaremba>(m_numDimensions, 1);
    m_samplers[LARCHER_PILLICHSHAMMER]     = new LarcherPillichshammerGK(3, 1, false);

    m_camera[CAMERA_XY].arcball.setState(Quaternionf::FromTwoVectors(Vector3f(0,0,1), Vector3f(0,0,1)));
    m_camera[CAMERA_XY].perspFactor = 0.0f;
    m_camera[CAMERA_XY].cameraType = CAMERA_XY;

    m_camera[CAMERA_YZ].arcball.setState(Quaternionf::FromTwoVectors(Vector3f(0,0,1), Vector3f(-1,0,0)));
    m_camera[CAMERA_YZ].perspFactor = 0.0f;
    m_camera[CAMERA_YZ].cameraType = CAMERA_YZ;

    m_camera[CAMERA_XZ].arcball.setState(Quaternionf::FromTwoVectors(Vector3f(0,0,1), Vector3f(0,-1,0)));
    m_camera[CAMERA_XZ].perspFactor = 0.0f;
    m_camera[CAMERA_XZ].cameraType = CAMERA_XZ;

    m_camera[CAMERA_2D] = m_camera[CAMERA_XY];
    m_camera[CAMERA_CURRENT] = m_camera[CAMERA_XY];
    m_camera[CAMERA_NEXT]    = m_camera[CAMERA_XY];

    // initialize the GUI elements
    initializeGUI();

    //
    // OpenGL setup
    //

    glEnable(GL_PROGRAM_POINT_SIZE);

    m_pointShader = new GLShader();
    m_pointShader->init("Point shader", pointVertexShader, pointFragmentShader);

    m_gridShader = new GLShader();
    m_gridShader->init("Grid shader", gridVertexShader, gridFragmentShader);

    m_point2DShader = new GLShader();
    m_point2DShader->init("Point shader 2D", pointVertexShader, pointFragmentShader);

    mBackground.setZero();
    updateGPUPoints();
    updateGPUGrids();
    setVisible(true);
    resizeEvent(mSize);
}

SampleViewer::~SampleViewer()
{
    delete m_pointShader;
    delete m_gridShader;
    delete m_point2DShader;
    for (size_t i = 0; i < m_samplers.size(); ++i)
        delete m_samplers[i];
}

void SampleViewer::initializeGUI()
{
	auto thm = new Theme(mNVGContext);
	thm->mStandardFontSize     = 14;
	thm->mButtonFontSize       = 14;
	thm->mTextBoxFontSize      = 14;
	// thm->mWindowCornerRadius   = 4;
	thm->mWindowFillUnfocused  = Color(60, 250);
	thm->mWindowFillFocused    = Color(65, 250);
  setTheme(thm);
	thm->mButtonCornerRadius   = 2;
    auto createCollapsableSection = [this](Widget * parent, const string & title, bool collapsed = false)
    {
        auto group = new Widget(parent);
        group->setLayout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 0));

        auto btn = new Button(group, title, !collapsed ? ENTYPO_ICON_CHEVRON_DOWN : ENTYPO_ICON_CHEVRON_LEFT);
        btn->setFlags(Button::ToggleButton);
        btn->setPushed(!collapsed);
        btn->setFixedSize(Vector2i(180, 22));
        btn->setFontSize(16);
        btn->setIconPosition(Button::IconPosition::Right);

        auto panel = new Well(group);
        panel->setLayout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));
        panel->setVisible(!collapsed);

        btn->setChangeCallback([this,btn,panel](bool value)
                            {
                                btn->setIcon(value ? ENTYPO_ICON_CHEVRON_DOWN : ENTYPO_ICON_CHEVRON_LEFT);
                                panel->setVisible(value);
                                this->performLayout(this->mNVGContext);
                            });
        return panel;
    };


    // create parameters dialog
    {
        m_parametersDialog = new Window(this, "Sample parameters");
        m_parametersDialog->setTheme(thm);
        m_parametersDialog->setPosition(Vector2i(15, 15));
        m_parametersDialog->setLayout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 5, 5));


        m_helpButton = new Button(m_parametersDialog->buttonPanel(), "", ENTYPO_ICON_HELP);
        m_helpButton->setFlags(Button::ToggleButton);
        m_helpButton->setChangeCallback([&](bool b)
            {
                m_helpDialog->setVisible(b);
                if (b)
                {
                    m_helpDialog->center();
                    m_helpDialog->requestFocus();
                }
            });

        {
            auto section = createCollapsableSection(m_parametersDialog, "Point set");

            vector<string> pointTypeNames(NUM_POINT_TYPES);
            for (int i = 0; i < NUM_POINT_TYPES; ++i)
                pointTypeNames[i] = m_samplers[i]->name();
            m_pointTypeBox = new ComboBox(section, pointTypeNames);
            m_pointTypeBox->setTheme(thm);
            m_pointTypeBox->setFixedHeight(20);
            m_pointTypeBox->setFixedWidth(170);
            m_pointTypeBox->setCallback([&](int)
                {
                    Sampler * sampler = m_samplers[m_pointTypeBox->selectedIndex()];
                    OrthogonalArray * oa = dynamic_cast<OrthogonalArray*>(sampler);
                    m_strengthLabel->setVisible(oa);
                    m_strengthBox->setVisible(oa);
                    m_strengthBox->setEnabled(oa);
                    m_offsetTypeLabel->setVisible(oa);
                    m_offsetTypeBox->setVisible(oa);
                    m_offsetTypeBox->setEnabled(oa);
                    m_jitterSlider->setValue(sampler->jitter());
                    if (oa)
                    {
                        m_offsetTypeBox->setSelectedIndex(oa->offsetType());
                        m_strengthBox->setValue(oa->strength());
                    }
                    this->performLayout();
                    updateGPUPoints();
                    updateGPUGrids();
                });
            m_pointTypeBox->setTooltip("Key: Up/Down");

            auto panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));
            
            auto l = new Label(panel, "Num points", "sans");
            l->setFixedWidth(100);
            m_pointCountBox = new IntBox<int>(panel);
            m_pointCountBox->setEditable(true);
            m_pointCountBox->setFixedSize(Vector2i(70, 20));
            m_pointCountBox->setAlignment(TextBox::Alignment::Right);
            m_pointCountBox->setSpinnable(true);
            m_pointCountBox->setCallback([&](int v)
                {
                    m_targetPointCount = v;
                    updateGPUPoints();
                    updateGPUGrids();
                });

            // Add a slider and set defaults
            m_pointCountSlider = new Slider(section);
            m_pointCountSlider->setCallback([&](float v)
                {
                    m_targetPointCount = mapSlider2Count(v);
                    updateGPUPoints();
                    updateGPUGrids();
                });
            m_pointCountSlider->setValue(6.f/15.f);
            // m_pointCountSlider->setFixedWidth(100);
            m_pointCountSlider->setTooltip("Key: Left/Right");
            m_targetPointCount = m_pointCount = mapSlider2Count(m_pointCountSlider->value());


            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Dimensions", "sans");
            l->setFixedWidth(100);
            m_dimensionBox = new IntBox<int>(panel);
            m_dimensionBox->setEditable(true);
            m_dimensionBox->setFixedSize(Vector2i(70, 20));
            m_dimensionBox->setValue(3);
            m_dimensionBox->setAlignment(TextBox::Alignment::Right);
            m_dimensionBox->setDefaultValue("3");
            m_dimensionBox->setFormat("[2-9]*");
            m_dimensionBox->setSpinnable(true);
            m_dimensionBox->setMinValue(2);
            // m_dimensionBox->setMaxValue(32);
            m_dimensionBox->setValueIncrement(1);
            m_dimensionBox->setTooltip("Key: D/d");
            m_dimensionBox->setCallback([&](int v)
                {
                    m_numDimensions = v;
                    m_subsetAxis->setMaxValue(v-1);
                    m_xDimension->setMaxValue(v-1);
                    m_yDimension->setMaxValue(v-1);
                    m_zDimension->setMaxValue(v-1);
                    
                    updateGPUPoints();
                    updateGPUGrids();
                });

            m_randomizeCheckBox = new CheckBox(section, "Randomize");
            m_randomizeCheckBox->setChecked(true);
            m_randomizeCheckBox->setCallback([&](bool){updateGPUPoints();});

            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Jitter", "sans");
            l->setFixedWidth(100);
            // Add a slider for the point radius
            m_jitterSlider = new Slider(panel);
            m_jitterSlider->setValue(0.5f);
            m_jitterSlider->setFixedWidth(70);
            m_jitterSlider->setCallback([&](float j)
                {
                    m_samplers[m_pointTypeBox->selectedIndex()]->setJitter(j);
                    updateGPUPoints();
                    updateGPUGrids();
                });
            
            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));
            
            m_strengthLabel = new Label(panel, "Strength", "sans");
            m_strengthLabel->setFixedWidth(100);
            m_strengthLabel->setVisible(false);
            m_strengthBox = new IntBox<int>(panel);
            m_strengthBox->setEditable(true);
            m_strengthBox->setEnabled(false);
            m_strengthBox->setVisible(false);
            m_strengthBox->setFixedSize(Vector2i(70, 20));
            m_strengthBox->setValue(2);
            m_strengthBox->setAlignment(TextBox::Alignment::Right);
            m_strengthBox->setDefaultValue("2");
            m_strengthBox->setFormat("[2-9]*");
            m_strengthBox->setSpinnable(true);
            m_strengthBox->setMinValue(2);
            // m_strengthBox->setMaxValue(32);
            m_strengthBox->setValueIncrement(1);
            // m_strengthBox->setTooltip("Key: D/d");
            m_strengthBox->setCallback([&](int v)
                {
                    if (OrthogonalArray* oa = dynamic_cast<OrthogonalArray*>(m_samplers[m_pointTypeBox->selectedIndex()]))
                    {
                        m_strengthBox->setValue(oa->setStrength(v));
                    }
                    updateGPUPoints();
                    updateGPUGrids();
                });

            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            m_offsetTypeLabel = new Label(panel, "Offset type", "sans");
            m_offsetTypeLabel->setFixedWidth(100);
            m_offsetTypeLabel->setVisible(false);
            OrthogonalArray * oa = dynamic_cast<OrthogonalArray*>(m_samplers[BOSE_OA_IP]);
            m_offsetTypeBox = new ComboBox(panel, oa->offsetTypeNames());
            m_offsetTypeBox->setVisible(false);
            m_offsetTypeBox->setTheme(thm);
            m_offsetTypeBox->setFixedSize(Vector2i(70, 20));
            m_offsetTypeBox->setCallback([&](int ot)
                {
                    if (OrthogonalArray* oa = dynamic_cast<OrthogonalArray*>(m_samplers[m_pointTypeBox->selectedIndex()]))
                    {
                        oa->setOffsetType(ot);
                    }
                    updateGPUPoints();
                    updateGPUGrids();
                });
            m_offsetTypeBox->setTooltip("Key: Shift+Up/Down");

            auto epsBtn = new Button(section, "Save as EPS", ENTYPO_ICON_SAVE);
            epsBtn->setFixedHeight(22);
            epsBtn->setCallback([&]{
                try
                {
                    string basename = file_dialog({ {"", "Base filename"} }, true);
                    if (!basename.size())
                        return;

                    ofstream fileAll(basename + "_all2D.eps");
                    drawContents2DEPS(fileAll);

                    ofstream fileXYZ(basename + "_012.eps");
                    drawContentsEPS(fileXYZ, CAMERA_CURRENT,
                                    m_xDimension->value(),
                                    m_yDimension->value(),
                                    m_zDimension->value());

                    for (int y = 0; y < m_numDimensions; ++y)
                        for (int x = 0; x < y; ++x)
                        {
                            ofstream fileXY(tfm::format("%s_%d%d.eps", basename, x, y));
                            drawContentsEPS(fileXY, CAMERA_XY, x, y, 2);
                        }
                }
                catch (const std::exception &e)
                {
                    new MessageDialog(this, MessageDialog::Type::Warning, "Error", "An error occurred: " + std::string(e.what()));
                }
            });
        }

        {
            auto section = createCollapsableSection(m_parametersDialog, "Camera/view:");

            auto panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 2));

            m_viewButtons[CAMERA_XY] = new Button(panel, "XY");
            m_viewButtons[CAMERA_XY]->setFlags(Button::RadioButton);
            m_viewButtons[CAMERA_XY]->setPushed(true);
            m_viewButtons[CAMERA_XY]->setCallback([&] {setView(CAMERA_XY);});
            m_viewButtons[CAMERA_XY]->setTooltip("Key: 1");
            m_viewButtons[CAMERA_XY]->setFixedSize(Vector2i(30, 22));

            m_viewButtons[CAMERA_YZ] = new Button(panel, "YZ");
            m_viewButtons[CAMERA_YZ]->setFlags(Button::RadioButton);
            m_viewButtons[CAMERA_YZ]->setCallback([&] {setView(CAMERA_YZ);});
            m_viewButtons[CAMERA_YZ]->setTooltip("Key: 2");
            m_viewButtons[CAMERA_YZ]->setFixedSize(Vector2i(30, 22));

            m_viewButtons[CAMERA_XZ] = new Button(panel, "XZ");
            m_viewButtons[CAMERA_XZ]->setFlags(Button::RadioButton);
            m_viewButtons[CAMERA_XZ]->setCallback([&] {setView(CAMERA_XZ);});
            m_viewButtons[CAMERA_XZ]->setTooltip("Key: 3");
            m_viewButtons[CAMERA_XZ]->setFixedSize(Vector2i(30, 22));

            m_viewButtons[CAMERA_CURRENT] = new Button(panel, "XYZ");
            m_viewButtons[CAMERA_CURRENT]->setFlags(Button::RadioButton);
            m_viewButtons[CAMERA_CURRENT]->setCallback([&] {setView(CAMERA_CURRENT);});
            m_viewButtons[CAMERA_CURRENT]->setTooltip("Key: 4");
            m_viewButtons[CAMERA_CURRENT]->setFixedSize(Vector2i(35, 22));

            m_viewButtons[CAMERA_2D] = new Button(panel, "2D");
            m_viewButtons[CAMERA_2D]->setFlags(Button::RadioButton);
            m_viewButtons[CAMERA_2D]->setCallback([&] {setView(CAMERA_2D);});
            m_viewButtons[CAMERA_2D]->setTooltip("Key: 0");
            m_viewButtons[CAMERA_2D]->setFixedSize(Vector2i(30, 22));
        }

        // Display options panel
        {
            auto displayPanel = createCollapsableSection(m_parametersDialog, "Display options");

            auto panel = new Widget(displayPanel);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            new Label(panel, "Radius", "sans");
            // Add a slider for the point radius
            m_pointRadiusSlider = new Slider(panel);
            m_pointRadiusSlider->setValue(0.5f);
            m_pointRadiusSlider->setFixedWidth(115);
            m_pointRadiusSlider->setCallback([&](float v){drawContents();});

            m_scaleRadiusWithPoints = new Button(panel, "", ENTYPO_ICON_RESIZE_FULL_SCREEN);
            m_scaleRadiusWithPoints->setFlags(Button::ToggleButton);
            m_scaleRadiusWithPoints->setPushed(true);
            m_scaleRadiusWithPoints->setFixedSize(Vector2i(19, 19));
            m_scaleRadiusWithPoints->setChangeCallback([this](bool){drawContents();});
            m_scaleRadiusWithPoints->setTooltip("Scale radius with number of points");

            m_show1DProjections = new CheckBox(displayPanel, "1D projections");
            m_show1DProjections->setChecked(false);
            m_show1DProjections->setTooltip("Key: P");
            m_show1DProjections->setCallback([this](bool){drawContents();});

            m_showPointNums = new CheckBox(displayPanel, "Point indices");
            m_showPointNums->setChecked(false);
            m_showPointNums->setCallback([this](bool){drawContents();});

            m_showPointCoords = new CheckBox(displayPanel, "Point coords");
            m_showPointCoords->setChecked(false);
            m_showPointCoords->setCallback([this](bool){drawContents();});

            m_coarseGridCheckBox = new CheckBox(displayPanel, "Coarse grid");
            m_coarseGridCheckBox->setChecked(false);
            m_coarseGridCheckBox->setTooltip("Key: g");
            m_coarseGridCheckBox->setCallback([this](bool){updateGPUGrids();});

            m_fineGridCheckBox = new CheckBox(displayPanel, "Fine grid");
            m_fineGridCheckBox->setTooltip("Key: G");
            m_fineGridCheckBox->setCallback([this](bool){updateGPUGrids();});

            m_bboxGridCheckBox = new CheckBox(displayPanel, "Bounding box");
            m_bboxGridCheckBox->setTooltip("Key: B");
            m_bboxGridCheckBox->setCallback([this](bool){updateGPUGrids();});
        }

        {
            auto section = createCollapsableSection(m_parametersDialog, "Dimension mapping", true);
            // new Label(displayPanel, "Dimension mapping:", "sans-bold");

            auto panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));
            auto createDimMapping = [this,panel](IntBox<int> *& box, const string & label, int dim)
            {
                auto group = new Widget(panel);
                group->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

                new Label(group, label, "sans");
                box = new IntBox<int>(group);
                box->setEditable(true);
                box->setValue(dim);
                box->setAlignment(TextBox::Alignment::Right);
                // box->setFixedWidth(30);
                box->setDefaultValue(to_string(dim));
                box->setSpinnable(true);
                box->setMinValue(0);
                box->setMaxValue(m_numDimensions - 1);
                box->setValueIncrement(1);
                box->setCallback([&](int v){updateGPUPoints(false);});
            };
            createDimMapping(m_xDimension, "X:", 0);
            createDimMapping(m_yDimension, "Y:", 1);
            createDimMapping(m_zDimension, "Z:", 2);
        }

        // point subset controls
        {
            auto section = createCollapsableSection(m_parametersDialog, "Visible subset", true);

            m_subsetByIndex = new CheckBox(section, "Subset by point index");
            m_subsetByIndex->setChecked(false);
            m_subsetByIndex->setCallback(
                [&](bool b)
                {
                    m_firstDrawPointBox->setEnabled(b);
                    m_pointDrawCountBox->setEnabled(b);
                    m_pointDrawCountSlider->setEnabled(b);
                    if (b)
                        m_subsetByCoord->setChecked(false);
                    updateGPUPoints(false);
                    drawContents();
                });

            // Create an empty panel with a horizontal layout
            auto panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            auto l = new Label(panel, "First point", "sans");
            l->setFixedWidth(100);
            m_firstDrawPointBox = new IntBox<int>(panel);
            m_firstDrawPointBox->setEditable(true);
            m_firstDrawPointBox->setEnabled(m_subsetByIndex->checked());
            m_firstDrawPointBox->setFixedSize(Vector2i(70, 20));
            m_firstDrawPointBox->setValue(0);
            m_firstDrawPointBox->setAlignment(TextBox::Alignment::Right);
            m_firstDrawPointBox->setDefaultValue("0");
            m_firstDrawPointBox->setSpinnable(true);
            m_firstDrawPointBox->setMinValue(0);
            m_firstDrawPointBox->setMaxValue(m_pointCount-1);
            m_firstDrawPointBox->setValueIncrement(1);
            m_firstDrawPointBox->setCallback([&](int v)
                {
                    m_pointDrawCountBox->setMaxValue(m_pointCount-m_firstDrawPointBox->value());
                    m_pointDrawCountBox->setValue(m_pointDrawCountBox->value());
                });

            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Num points", "sans");
            l->setFixedWidth(100);
            m_pointDrawCountBox = new IntBox<int>(panel);
            m_pointDrawCountBox->setEditable(true);
            m_pointDrawCountBox->setEnabled(m_subsetByIndex->checked());
            m_pointDrawCountBox->setFixedSize(Vector2i(70, 20));
            m_pointDrawCountBox->setValue(m_pointCount-m_firstDrawPointBox->value());
            m_pointDrawCountBox->setAlignment(TextBox::Alignment::Right);
            m_pointDrawCountBox->setDefaultValue("0");
            m_pointDrawCountBox->setSpinnable(true);
            m_pointDrawCountBox->setMinValue(0);
            m_pointDrawCountBox->setMaxValue(m_pointCount-m_firstDrawPointBox->value());
            m_pointDrawCountBox->setValueIncrement(1);


            m_pointDrawCountSlider = new Slider(section);
            m_pointDrawCountSlider->setEnabled(m_subsetByIndex->checked());
            m_pointDrawCountSlider->setValue(1.0f);
            m_pointDrawCountSlider->setCallback([&](float v)
                {
                    m_pointDrawCountBox->setValue(round((m_pointCount-m_firstDrawPointBox->value()) * v));
                });

            m_subsetByCoord = new CheckBox(section, "Subset by coordinate");
            m_subsetByCoord->setChecked(false);
            m_subsetByCoord->setCallback(
                [&](bool b)
                {
                    m_subsetAxis->setEnabled(b);
                    m_numSubsetLevels->setEnabled(b);
                    m_subsetLevel->setEnabled(b);
                    if (b)
                        m_subsetByIndex->setChecked(false);
                    updateGPUPoints(false);
                    drawContents();
                });

            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Subset axis", "sans");
            l->setFixedWidth(100);
            m_subsetAxis = new IntBox<int>(panel);
            m_subsetAxis->setEditable(true);
            m_subsetAxis->setEnabled(m_subsetByCoord->checked());
            m_subsetAxis->setFixedSize(Vector2i(70, 20));
            m_subsetAxis->setValue(0);
            m_subsetAxis->setAlignment(TextBox::Alignment::Right);
            m_subsetAxis->setDefaultValue("0");
            m_subsetAxis->setSpinnable(true);
            m_subsetAxis->setMinValue(0);
            m_subsetAxis->setMaxValue(m_numDimensions-1);
            m_subsetAxis->setValueIncrement(1);
            m_subsetAxis->setCallback([&](int v){updateGPUPoints(false);});

            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Num levels", "sans");
            l->setFixedWidth(100);
            m_numSubsetLevels = new IntBox<int>(panel);
            m_numSubsetLevels->setEditable(true);
            m_numSubsetLevels->setEnabled(m_subsetByCoord->checked());
            m_numSubsetLevels->setFixedSize(Vector2i(70, 20));
            m_numSubsetLevels->setValue(2);
            m_numSubsetLevels->setAlignment(TextBox::Alignment::Right);
            m_numSubsetLevels->setDefaultValue("2");
            m_numSubsetLevels->setSpinnable(true);
            m_numSubsetLevels->setMinValue(1);
            m_numSubsetLevels->setMaxValue(m_pointCount);
            m_numSubsetLevels->setValueIncrement(1);
            m_numSubsetLevels->setCallback([&](int v){m_subsetLevel->setMaxValue(v-1);updateGPUPoints(false);});

            panel = new Widget(section);
            panel->setLayout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 0));

            l = new Label(panel, "Level", "sans");
            l->setFixedWidth(100);
            m_subsetLevel = new IntBox<int>(panel);
            m_subsetLevel->setEditable(true);
            m_subsetLevel->setEnabled(m_subsetByCoord->checked());
            m_subsetLevel->setFixedSize(Vector2i(70, 20));
            m_subsetLevel->setValue(0);
            m_subsetLevel->setAlignment(TextBox::Alignment::Right);
            m_subsetLevel->setDefaultValue("0");
            m_subsetLevel->setSpinnable(true);
            m_subsetLevel->setMinValue(0);
            m_subsetLevel->setMaxValue(m_numSubsetLevels->value()-1);
            m_subsetLevel->setValueIncrement(1);
            m_subsetLevel->setCallback([&](int v){updateGPUPoints(false);});
        }
    }

    //
    // create help dialog
    //
    {
        vector<pair<string, string> > helpStrings =
        {
            {"h",       "Toggle this help panel"},
            {"LMB",     "Rotate camera"},
            {"Scroll",  "Zoom camera"},
            {"1/2/3",   "Axis-aligned orthographic projections"},
            {"4",       "Perspective view"},
            {"0",       "Grid of all 2D projections"},
            {"Left",    "Decrease point count (+Shift: bigger increment)"},
            {"Right",   "Increase point count (+Shift: bigger increment)"},
            {"Up/Down", "Cycle through samplers"},
            {"Shift+Up/Down", "Cycle through offset types (for OA samplers)"},
            {"d/D",     "Decrease/Increase maximum dimension"},
            {"t/T",     "Decrease/Increase the strength (for OA samplers)"},
            {"r/R",     "Toggle/re-seed randomization"},
            {"g/G",     "Toggle coarse/fine grid"},
            {"P",       "Toggle 1D projections"},
            {"Space",   "Toggle controls"},
        };

        m_helpDialog = new Window(this, "Help");
        m_helpDialog->setTheme(thm);
        m_helpDialog->setId("help dialog");
        m_helpDialog->setVisible(false);
        GridLayout *layout = new GridLayout(Orientation::Horizontal,
                                            2,
                                            Alignment::Middle,
                                            15,
                                            5);
        layout->setColAlignment({ Alignment::Maximum, Alignment::Fill });
        layout->setSpacing(0, 10);
        m_helpDialog->setLayout(layout);

        new Label(m_helpDialog, "key", "sans-bold");
        new Label(m_helpDialog, "Action", "sans-bold");

        for (auto item : helpStrings)
        {
            new Label(m_helpDialog, item.first);
            new Label(m_helpDialog, item.second);
        }

        m_helpDialog->center();

        Button *button = new Button(m_helpDialog->buttonPanel(), "", ENTYPO_ICON_CROSS);
        button->setCallback([&]
            {
                m_helpDialog->setVisible(false);
                m_helpButton->setPushed(false);
            });
    }
}


void SampleViewer::updateGPUPoints(bool regenerate)
{
    // Generate the point positions
    if (regenerate)
    {
        try
        {
            generatePoints();
        }
        catch (const std::exception &e)
        {
            new MessageDialog(this, MessageDialog::Type::Warning, "Error", "An error occurred: " + std::string(e.what()));
            return;
        }
    }

    // Upload points to GPU
    m_pointShader->bind();

    populatePointSubset();

    m_pointShader->uploadAttrib("position", m_3DPoints);

    // Upload 2D points to GPU
    m_point2DShader->bind();
    // create a temporary matrix to store all the 2D projections of the points
    // each 2D plot actually needs 3D points, and there are num2DPlots of them
    int num2DPlots = m_numDimensions*(m_numDimensions-1)/2;
    MatrixXf points2D(3, num2DPlots*m_subsetCount);
    int plotIndex = 0;
    for (int y = 0; y < m_numDimensions; ++y)
        for (int x = 0; x < y; ++x, ++plotIndex)
        {
            points2D.block(0,plotIndex*m_subsetCount, 1,m_subsetCount) = m_subsetPoints.leftCols(m_subsetCount).row(x);
            points2D.block(1,plotIndex*m_subsetCount, 1,m_subsetCount) = m_subsetPoints.leftCols(m_subsetCount).row(y);
            points2D.block(2,plotIndex*m_subsetCount, 1,m_subsetCount) = VectorXf::Ones(1,m_subsetCount) * 0.5f;
        }
    m_point2DShader->uploadAttrib("position", points2D);

    m_pointCountBox->setValue(m_pointCount);
    m_pointCountSlider->setValue(mapCount2Slider(m_pointCount));
}

void SampleViewer::updateGPUGrids()
{
    MatrixXf bboxGrid(3,0), coarseGrid(3,0), fineGrid(3,0);
    PointType pointType = (PointType) m_pointTypeBox->selectedIndex();
    generateGrid(bboxGrid, 1);
    generateGrid(coarseGrid, m_samplers[pointType]->coarseGridRes(m_pointCount));
    generateGrid(fineGrid, m_pointCount);
    m_coarseLineCount = coarseGrid.cols();
    m_fineLineCount = fineGrid.cols();
    MatrixXf positions(3, bboxGrid.cols() + coarseGrid.cols() + fineGrid.cols());
    positions << bboxGrid, coarseGrid, fineGrid;
    m_gridShader->bind();
    m_gridShader->uploadAttrib("position", positions);
}

bool SampleViewer::resizeEvent(const Vector2i &)
{
    for (int i = 0; i < NUM_CAMERA_TYPES; ++i)
        m_camera[i].arcball.setSize(mSize);

    if (m_helpDialog->visible())
        m_helpDialog->center();

    performLayout();
    drawAll();
    return true;
}

bool SampleViewer::mouseMotionEvent(const Vector2i &p, const Vector2i &rel, int button, int modifiers)
{
    if (!Screen::mouseMotionEvent(p, rel, button, modifiers))
    {
        if (m_camera[CAMERA_NEXT].arcball.motion(p))
            drawAll();
    }
    return true;
}

bool SampleViewer::mouseButtonEvent(const Vector2i &p, int button, bool down, int modifiers)
{
    if (!Screen::mouseButtonEvent(p, button, down, modifiers))
    {
        if (button == GLFW_MOUSE_BUTTON_1)
        {
            if (down)
            {
                m_mouseDown = true;
                // on mouse down we start switching to a perspective camera
                // and start recording the arcball rotation in CAMERA_NEXT
                setView(CAMERA_CURRENT);
                m_camera[CAMERA_NEXT].arcball.button(p, down);
                m_camera[CAMERA_NEXT].cameraType = CAMERA_CURRENT;
            }
            else
            {
                m_mouseDown = false;
                m_camera[CAMERA_NEXT].arcball.button(p, down);
                // since the time between mouse down and up could be shorter
                // than the animation duration, we override the previous
                // camera's arcball on mouse up to complete the animation
                m_camera[CAMERA_PREVIOUS].arcball = m_camera[CAMERA_NEXT].arcball;
                m_camera[CAMERA_PREVIOUS].cameraType = m_camera[CAMERA_NEXT].cameraType = CAMERA_CURRENT;
            }
        }
    }
    return true;
}


bool SampleViewer::scrollEvent(const Vector2i &p, const Vector2f &rel)
{
    if (!Screen::scrollEvent(p, rel))
    {
        m_camera[CAMERA_NEXT].zoom = max(0.001, m_camera[CAMERA_NEXT].zoom * pow(1.1, rel.y()));
        drawAll();
    }
    return true;
}

void SampleViewer::drawPoints(const Matrix4f & mvp, const Vector3f & color, bool showPointNums, bool showPointCoords) const
{
    // Render the point set
    m_pointShader->bind();
    m_pointShader->setUniform("mvp", mvp);
    float radius = mapSlider2Radius(m_pointRadiusSlider->value());
    if (m_scaleRadiusWithPoints->pushed())
        radius *= 64.0f/std::sqrt(m_pointCount);
    m_pointShader->setUniform("pointSize", radius);
    m_pointShader->setUniform("color", color);

    int start = 0, count = m_pointCount;
    if (m_subsetByCoord->checked())
    {
        count = min(count, m_subsetCount);
    }
    else if (m_subsetByIndex->checked())
    {
        start = m_firstDrawPointBox->value();
        count = m_pointDrawCountBox->value();
    }
    m_pointShader->drawArray(GL_POINTS, start, count);

    if (!showPointNums && !showPointCoords)
        return;

    for (int p = start; p < start+count; ++p)
    {
        Vector3f point = m_3DPoints.col(p);

        Vector4f textPos = mvp * Vector4f(point(0), point(1), point(2), 1.0f);
        Vector2f text2DPos((textPos.x()/textPos.w() + 1)/2, (textPos.y()/textPos.w() + 1)/2);
        if (showPointNums)
            drawText(Vector2i((text2DPos.x()) * mSize.x(), (1.f - text2DPos.y()) * mSize.y() - radius/4),
                    tfm::format("%d", p),
                    Color(1.0f, 1.0f, 1.0f, 0.75f), 12, 0,
                    NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
        if (showPointCoords)
            drawText(Vector2i((text2DPos.x()) * mSize.x(), (1.f - text2DPos.y()) * mSize.y() + radius/4),
                    tfm::format("(%0.2f, %0.2f, %0.2f)", point(0)+0.5, point(1)+0.5, point(2)+0.5),
                    Color(1.0f, 1.0f, 1.0f, 0.75f), 11, 0,
                    NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    }
}


void SampleViewer::drawGrid(const Matrix4f & mvp, float alpha, uint32_t offset, uint32_t count) const
{
    m_gridShader->bind();
    m_gridShader->setUniform("alpha", alpha);

    for (int axis = CAMERA_XY; axis <= CAMERA_XZ; ++axis)
    {
        if (m_camera[CAMERA_CURRENT].cameraType == axis ||
            m_camera[CAMERA_CURRENT].cameraType == CAMERA_CURRENT)
        {
            m_gridShader->setUniform("mvp", Matrix4f(mvp * m_camera[axis].arcball.matrix()));
            m_gridShader->drawArray(GL_LINES, offset, count);
        }
    }
}


void SampleViewer::draw2DPointsAndGrid(const Matrix4f & mvp, int dimX, int dimY, int plotIndex) const
{
    Matrix4f pos;
    layout2DMatrix(pos, m_numDimensions, dimX, dimY);

    glDisable(GL_DEPTH_TEST);
    // Render the point set
    m_point2DShader->bind();
    m_point2DShader->setUniform("mvp", Matrix4f(mvp * pos));
    float radius = mapSlider2Radius(m_pointRadiusSlider->value()) / (m_numDimensions - 1);
    if (m_scaleRadiusWithPoints->pushed())
        radius *= 64.0f/std::sqrt(m_pointCount);
    m_point2DShader->setUniform("pointSize", radius);
    m_point2DShader->setUniform("color", Vector3f(0.9f, 0.55f, 0.1f));

    int start = 0, count = m_pointCount;
    if (m_subsetByCoord->checked())
    {
        count = min(count, m_subsetCount);
    }
    else if (m_subsetByIndex->checked())
    {
        start = m_firstDrawPointBox->value();
        count = m_pointDrawCountBox->value();
    }
    m_point2DShader->drawArray(GL_POINTS, m_subsetCount*plotIndex + start, count);

    glEnable(GL_DEPTH_TEST);
    if (m_bboxGridCheckBox->checked())
    {
        m_gridShader->bind();
        m_gridShader->setUniform("alpha", 1.0f);
        m_gridShader->setUniform("mvp", Matrix4f(mvp * pos * m_camera[CAMERA_2D].arcball.matrix()));
        m_gridShader->drawArray(GL_LINES, 0, 8);
    }
    if (m_coarseGridCheckBox->checked())
    {
        m_gridShader->bind();
        m_gridShader->setUniform("alpha", 0.6f);
        m_gridShader->setUniform("mvp", Matrix4f(mvp * pos * m_camera[CAMERA_2D].arcball.matrix()));
        m_gridShader->drawArray(GL_LINES, 8, m_coarseLineCount);
    }
    if (m_fineGridCheckBox->checked())
    {
        m_gridShader->bind();
        m_gridShader->setUniform("alpha", 0.2f);
        m_gridShader->setUniform("mvp", Matrix4f(mvp * pos * m_camera[CAMERA_2D].arcball.matrix()));
        m_gridShader->drawArray(GL_LINES, 8+m_coarseLineCount, m_fineLineCount);
    }
}

void SampleViewer::drawContents()
{
    // update/move the camera
    updateCurrentCamera();

    Matrix4f model, lookat, view, proj, mvp;
    computeCameraMatrices(model, lookat, view, proj, m_camera[CAMERA_CURRENT],
                          float(mSize.x()) / mSize.y());
    mvp = proj * lookat * view * model;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_viewButtons[CAMERA_2D]->pushed())
    {
        Matrix4f pos;

        for (int i = 0; i < m_numDimensions-1; ++i)
        {
            layout2DMatrix(pos, m_numDimensions, i, m_numDimensions-1);
            Vector4f textPos = mvp * pos * Vector4f(0.f, -0.5f, 0.0f, 1.0f);
            Vector2f text2DPos((textPos.x()/textPos.w() + 1)/2, (textPos.y()/textPos.w() + 1)/2);
            drawText(Vector2i((text2DPos.x()) * mSize.x(),
                              (1.f - text2DPos.y()) * mSize.y() + 16), to_string(i),
                     Color(1.0f, 1.0f, 1.0f, 0.75f), 16, 0,
                     NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

            layout2DMatrix(pos, m_numDimensions, 0, i+1);
            textPos = mvp * pos * Vector4f(-0.5f, 0.f, 0.0f, 1.0f);
            text2DPos = Vector2f((textPos.x()/textPos.w() + 1)/2, (textPos.y()/textPos.w() + 1)/2);
            drawText(Vector2i((text2DPos.x()) * mSize.x() - 4,
                              (1.f - text2DPos.y()) * mSize.y()), to_string(i+1),
                     Color(1.0f, 1.0f, 1.0f, 0.75f), 16, 0,
                     NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }

        int plotIndex = 0;
        for (int y = 0; y < m_numDimensions; ++y)
            for (int x = 0; x < y; ++x, ++plotIndex)
            {
                // cout << "drawing plotIndex: " << plotIndex << endl;
                draw2DPointsAndGrid(mvp, x, y, plotIndex);
            }
        // cout << endl;
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
        drawPoints(mvp, Vector3f(0.9f, 0.55f, 0.1f), m_showPointNums->checked(), m_showPointCoords->checked());

        if (m_show1DProjections->checked())
        {
            // smash the points against the axes and draw
            Matrix4f smashX = mvp * translate(Vector3f(-0.51, 0, 0)) * scale(Vector3f(0,1,1));
            drawPoints(smashX, Vector3f(0.8f, 0.3f, 0.3f));

            Matrix4f smashY = mvp * translate(Vector3f(0, -0.51, 0)) * scale(Vector3f(1,0,1));
            drawPoints(smashY, Vector3f(0.3f, 0.8f, 0.3f));

            Matrix4f smashZ = mvp * translate(Vector3f(0, 0, -0.51)) * scale(Vector3f(1,1,0));
            drawPoints(smashZ, Vector3f(0.3f, 0.3f, 0.8f));
        }

        glEnable(GL_DEPTH_TEST);
        if (m_bboxGridCheckBox->checked())
            drawGrid(mvp, 1.0f, 0, 8);

        if (m_coarseGridCheckBox->checked())
            drawGrid(mvp, 0.6f, 8, m_coarseLineCount);

        if (m_fineGridCheckBox->checked())
            drawGrid(mvp, 0.2f, 8+m_coarseLineCount, m_fineLineCount);

        for (int i = 0; i < 3; ++i)
            drawText(mSize-Vector2i(10, (2-i)*14 + 20), m_timeStrings[i], Color(1.0f, 1.0f, 1.0f, 0.75f), 16, 0);
    }
}


void SampleViewer::drawText(const Vector2i & pos,
                              const string & text,
                              const Color & color,
                              int fontSize,
                              int fixedWidth,
                              int align) const
{
    nvgFontFace(mNVGContext, "sans");
    nvgFontSize(mNVGContext, (float) fontSize);
    nvgFillColor(mNVGContext, color);
    if (fixedWidth > 0)
    {
        nvgTextAlign(mNVGContext, align);
        nvgTextBox(mNVGContext, (float) pos.x(), (float) pos.y(), (float) fixedWidth, text.c_str(), nullptr);
    }
    else
    {
        nvgTextAlign(mNVGContext, align);
        nvgText(mNVGContext, (float) pos.x(), (float) pos.y(), text.c_str(), nullptr);
    }
}


void SampleViewer::drawContentsEPS(ofstream & file, CameraType cameraType, int dimX, int dimY, int dimZ)
{
    int size = 900;
    int crop = 720;


    int pointScale = m_samplers[m_pointTypeBox->selectedIndex()]->coarseGridRes(m_pointCount);

    file << "%!PS-Adobe-3.0 EPSF-3.0\n";
    file << "%%HiResBoundingBox: " << (-crop)  << " " << (-crop)  << " " << crop << " " << crop << "\n";
    file << "%%BoundingBox: " << (-crop)  << " " << (-crop)  << " " << crop << " " << crop << "\n";
    file << "%%CropBox: " << (-crop)  << " " << (-crop)  << " " << crop << " " << crop << "\n";
    file << "gsave " << size << " " << size << " scale\n";
    file << "/radius { " << (0.5f/pointScale) << " } def %define variable for point radius\n";
    file << "/p { radius 0 360 arc closepath fill } def %define point command\n";
    file << "/blw " << (0.01f) << " def %define variable for bounding box linewidth\n";
    file << "/clw " << (0.003f) << " def %define variable for coarse linewidth\n";
    file << "/flw " << (0.0004f) << " def %define variable for fine linewidth\n";
    file << "/pfc " << "{0.9 0.55 0.1}" << " def %define variable for point fill color\n";
    file << "/blc " << (0.0f) << " def %define variable for bounding box color\n";
    file << "/clc " << (0.8f) << " def %define variable for coarse line color\n";
    file << "/flc " << (0.9f) << " def %define variable for fine line color\n";

    Matrix4f model, lookat, view, proj, mvp, mvp2;
    computeCameraMatrices(model, lookat, view, proj,
                          m_camera[cameraType], 1.0f);
    mvp = proj * lookat * view * model;
    int startAxis = CAMERA_XY, endAxis = CAMERA_XZ;
    if (cameraType != CAMERA_CURRENT)
        startAxis = endAxis = cameraType;
    if (m_fineGridCheckBox->checked())
    {
        file << "% Draw fine grids \n";
        file << "flc setgray %fill color for fine grid \n";
        file << "flw setlinewidth\n";
        for (int axis = startAxis; axis <= endAxis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            Matrix4f mvp2 = mvp * m_camera[axis].arcball.matrix();
            drawEPSGrid(mvp2, m_pointCount, file);
        }
    }

    if (m_coarseGridCheckBox->checked())
    {
        file << "% Draw coarse grids \n";
        file << "clc setgray %fill color for coarse grid \n";
        file << "clw setlinewidth\n";
        for (int axis = startAxis; axis <= endAxis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            Matrix4f mvp2 = mvp * m_camera[axis].arcball.matrix();
            PointType pointType = (PointType) m_pointTypeBox->selectedIndex();
            drawEPSGrid(mvp2, m_samplers[pointType]->coarseGridRes(m_pointCount), file);
        }
    }

    if (m_bboxGridCheckBox->checked())
    {
        file << "% Draw bounding boxes \n";
        file << "blc setgray %fill color for bounding box \n";
        file << "blw setlinewidth\n";
        for (int axis = startAxis; axis <= endAxis; ++axis)
        {
            // this extra matrix multiply is needed to properly rotate the different grids for the XYZ view
            Matrix4f mvp2 = mvp * m_camera[axis].arcball.matrix();
            drawEPSGrid(mvp2, 1, file);
        }
    }

    // Generate and render the point set
    {
        generatePoints();
        populatePointSubset();
        file << "% Draw points \n";
        file << "pfc setrgbcolor %fill color for points\n";

        int start = 0, count = m_pointCount;
        if (m_subsetByCoord->checked())
            count = min(count, m_subsetCount);
        else if (m_subsetByIndex->checked())
        {
            start = m_firstDrawPointBox->value();
            count = m_pointDrawCountBox->value();
        }
        writeEPSPoints(m_subsetPoints, start, count, mvp, file, dimX, dimY, dimZ);
    }

    file << "grestore\n";
}


void SampleViewer::drawContents2DEPS(ofstream & file)
{
    int size = 900*108.0f/720.0f;
    int crop = 108;//720;
    float scale = 1.0f/(m_numDimensions-1);
    // int pointScale = m_samplers[m_pointTypeBox->selectedIndex()]->coarseGridRes(m_pointCount);

    file << "%!PS-Adobe-3.0 EPSF-3.0\n";
    file << "%%HiResBoundingBox: " << (-crop)  << " " << (-crop)  << " " << crop << " " << crop << "\n";
    file << "%%BoundingBox: " << (-crop)  << " " << (-crop)  << " " << crop << " " << crop << "\n";
    file << "%%CropBox: " << (-crop)  << " " << (-crop)  << " " << crop << " " << crop << "\n";
    file << "gsave " << size << " " << size << " scale\n";
    // file << "/radius { " << (0.5f/pointScale*scale) << " } def %define variable for point radius\n";
    file << "/radius { " << (0.02*scale) << " } def %define variable for point radius\n";
    file << "/p { radius 0 360 arc closepath fill } def %define point command\n";
    file << "/blw " << (0.020f*scale) << " def %define variable for bounding box linewidth\n";
    file << "/clw " << (0.01f*scale) << " def %define variable for coarse linewidth\n";
    file << "/flw " << (0.005f*scale) << " def %define variable for fine linewidth\n";
    file << "/pfc " << "{0.9 0.55 0.1}" << " def %define variable for point fill color\n";
    file << "/blc " << (0.0f) << " def %define variable for bounding box color\n";
    file << "/clc " << (0.5f) << " def %define variable for coarse line color\n";
    file << "/flc " << (0.9f) << " def %define variable for fine line color\n";

    Matrix4f model, lookat, view, proj, mvp, mvp2;
    computeCameraMatrices(model, lookat, view, proj,
                          m_camera[CAMERA_2D], 1.0f);
    mvp = proj * lookat * view * model;

    // Generate and render the point set
    {
        generatePoints();
        populatePointSubset();

        for (int y = 0; y < m_numDimensions; ++y)
            for (int x = 0; x < y; ++x)
            {
                Matrix4f pos;
                layout2DMatrix(pos, m_numDimensions, x, y);

                if (m_fineGridCheckBox->checked())
                {
                    file << "% Draw fine grid \n";
                    file << "flc setgray %fill color for fine grid \n";
                    file << "flw setlinewidth\n";
                    drawEPSGrid(mvp * pos, m_pointCount, file);
                }
                if (m_coarseGridCheckBox->checked())
                {
                    file << "% Draw coarse grids \n";
                    file << "clc setgray %fill color for coarse grid \n";
                    file << "clw setlinewidth\n";
                    PointType pointType = (PointType) m_pointTypeBox->selectedIndex();
                    drawEPSGrid(mvp * pos, m_samplers[pointType]->coarseGridRes(m_pointCount), file);
                }
                if (m_bboxGridCheckBox->checked())
                {
                    file << "% Draw bounding box \n";
                    file << "blc setgray %fill color for bounding box \n";
                    file << "blw setlinewidth\n";
                    drawEPSGrid(mvp * pos, 1, file);
                }
            }

        file << "% Draw points \n";
        file << "pfc setrgbcolor %fill color for points\n";
        for (int y = 0; y < m_numDimensions; ++y)
            for (int x = 0; x < y; ++x)
            {
                Matrix4f pos;
                layout2DMatrix(pos, m_numDimensions, x, y);
                file << "% Draw (" << x << "," << y << ") points\n";

                int start = 0, count = m_pointCount;
                if (m_subsetByCoord->checked())
                    count = min(count, m_subsetCount);
                else if (m_subsetByIndex->checked())
                {
                    start = m_firstDrawPointBox->value();
                    count = m_pointDrawCountBox->value();
                }

                writeEPSPoints(m_subsetPoints, start, count, mvp * pos, file, x, y, 2);
            }
    }

    file << "grestore\n";
}

bool SampleViewer::keyboardEvent(int key, int scancode, int action, int modifiers)
{
     if (Screen::keyboardEvent(key, scancode, action, modifiers))
        return true;

    if (!action)
        return false;

    switch (key)
    {
        case GLFW_KEY_ESCAPE:
        {
            this->setVisible(false);
            return true;
        }
        case ' ':
            m_parametersDialog->setVisible(!m_parametersDialog->visible());
            return true;
        case 'H':
            m_helpDialog->center();
            m_helpDialog->setVisible(!m_helpDialog->visible());
            m_helpButton->setPushed(m_helpDialog->visible());
            return true;
        case '1':
            setView(CAMERA_XY);
            return true;
        case '2':
            setView(CAMERA_YZ);
            return true;
        case '3':
            setView(CAMERA_XZ);
            return true;
        case '4':
            setView(CAMERA_CURRENT);
            return true;
        case '0':
            setView(CAMERA_2D);
            return true;
        case 'P':
            m_show1DProjections->setChecked(!m_show1DProjections->checked());
            drawContents();
            return true;
        case 'B':
            m_bboxGridCheckBox->setChecked(!m_bboxGridCheckBox->checked());
            updateGPUGrids();
            return true;
        case 'G':
            if (modifiers & GLFW_MOD_SHIFT)
                m_fineGridCheckBox->setChecked(!m_fineGridCheckBox->checked());
            else
                m_coarseGridCheckBox->setChecked(!m_coarseGridCheckBox->checked());
            updateGPUGrids();
            return true;
        case 'R':
            if (modifiers & GLFW_MOD_SHIFT)
            {
                // re-randomize
                PointType pointType = (PointType) m_pointTypeBox->selectedIndex();
                m_randomizeCheckBox->setChecked(true);
                m_samplers[pointType]->setRandomized(true);
                updateGPUPoints();
                drawAll();
            }
            else
            {
                m_randomizeCheckBox->setChecked(!m_randomizeCheckBox->checked());
                updateGPUPoints();
            }
            return true;
        case 'D':
            m_dimensionBox->setValue(m_dimensionBox->value() + ((modifiers & GLFW_MOD_SHIFT) ? 1 : -1));
            m_numDimensions = m_dimensionBox->value();
            m_subsetAxis->setMaxValue(m_numDimensions-1);
            m_xDimension->setMaxValue(m_numDimensions-1);
            m_yDimension->setMaxValue(m_numDimensions-1);
            m_zDimension->setMaxValue(m_numDimensions-1);
            m_subsetAxis->setValue(min(m_subsetAxis->value(), m_numDimensions-1));
            m_xDimension->setValue(min(m_xDimension->value(), m_numDimensions-1));
            m_yDimension->setValue(min(m_yDimension->value(), m_numDimensions-1));
            m_zDimension->setValue(min(m_zDimension->value(), m_numDimensions-1));
            updateGPUPoints();
            updateGPUGrids();
            return true;

        case 'T':
        {
            Sampler * sampler = m_samplers[m_pointTypeBox->selectedIndex()];
            OrthogonalArray * oa = dynamic_cast<OrthogonalArray*>(sampler);
            if (oa)
            {
                int t = m_strengthBox->value() + ((modifiers & GLFW_MOD_SHIFT) ? 1 : -1);
                t = (t < 2) ? 2 : t;
                m_strengthBox->setValue(oa->setStrength(t));
            }
            updateGPUPoints();
            updateGPUGrids();
            return true;
        }
        
        case GLFW_KEY_UP:
        case GLFW_KEY_DOWN:
        {
            int delta = (key == GLFW_KEY_DOWN) ? 1 : -1;
            if (modifiers & GLFW_MOD_SHIFT)
            {
                if (OrthogonalArray* oa = dynamic_cast<OrthogonalArray*>(m_samplers[m_pointTypeBox->selectedIndex()]))
                {
                    m_offsetTypeBox->setSelectedIndex(mod(m_offsetTypeBox->selectedIndex() + delta, (int)NUM_OFFSET_TYPES));
                    oa->setOffsetType(m_offsetTypeBox->selectedIndex());

                    updateGPUPoints();
                    updateGPUGrids();
                }
            }
            else
            {
                m_pointTypeBox->setSelectedIndex(mod(m_pointTypeBox->selectedIndex() + delta, (int)NUM_POINT_TYPES));
                Sampler * sampler = m_samplers[m_pointTypeBox->selectedIndex()];
                OrthogonalArray * oa = dynamic_cast<OrthogonalArray*>(sampler);
                m_strengthLabel->setVisible(oa);
                m_strengthBox->setVisible(oa);
                m_strengthBox->setEnabled(oa);
                m_offsetTypeLabel->setVisible(oa);
                m_offsetTypeBox->setVisible(oa);
                m_offsetTypeBox->setEnabled(oa);
                m_jitterSlider->setValue(sampler->jitter());
                if (oa)
                {
                    m_offsetTypeBox->setSelectedIndex(oa->offsetType());
                    m_strengthBox->setValue(oa->strength());
                }
                performLayout();
                updateGPUPoints();
                updateGPUGrids();
            }
            return true;
        }
        case GLFW_KEY_LEFT:
        case GLFW_KEY_RIGHT:
        {
            float delta = (modifiers & GLFW_MOD_SHIFT) ? 1.0f/15 : 1.0f/60;
            delta *= (key == GLFW_KEY_RIGHT) ? 1.f : -1.f;
            m_targetPointCount = mapSlider2Count(clamp(m_pointCountSlider->value()+delta, 0.0f, 1.0f));
            updateGPUPoints();
            updateGPUGrids();
            return true;
        }
    }

    return false;
}

void SampleViewer::setView(CameraType view)
{
    m_animateStartTime = (float) glfwGetTime();
    m_camera[CAMERA_PREVIOUS] = m_camera[CAMERA_CURRENT];
    m_camera[CAMERA_NEXT] = m_camera[view];
    m_camera[CAMERA_NEXT].perspFactor = (view == CAMERA_CURRENT) ? 1.0f : 0.0f;
    m_camera[CAMERA_NEXT].cameraType = view;
    m_camera[CAMERA_CURRENT].cameraType = (view == m_camera[CAMERA_CURRENT].cameraType) ? view : CAMERA_CURRENT;

    for (int i = CAMERA_XY; i <= CAMERA_CURRENT; ++i)
        m_viewButtons[i]->setPushed(false);
    m_viewButtons[view]->setPushed(true);
}

void SampleViewer::updateCurrentCamera()
{
    CameraParameters & camera0 = m_camera[CAMERA_PREVIOUS];
    CameraParameters & camera1 = m_camera[CAMERA_NEXT];
    CameraParameters & camera  = m_camera[CAMERA_CURRENT];

    float timeNow = (float) glfwGetTime();
    float timeDiff = (m_animateStartTime != 0.0f) ? (timeNow - m_animateStartTime) : 1.0f;

    // interpolate between the "perspectiveness" at the previous keyframe,
    // and the "perspectiveness" of the currently selected camera
    float t = smoothStep(0.0f, 0.5f, timeDiff);

    camera.zoom = lerp(camera0.zoom, camera1.zoom, t);
    camera.viewAngle = lerp(camera0.viewAngle, camera1.viewAngle, t);
    camera.perspFactor = lerp(camera0.perspFactor, camera1.perspFactor, t);
    if (t >= 1.0f)
        camera.cameraType = camera1.cameraType;

    // if we are dragging the mouse, then just use the current arcball
    // rotation, otherwise, interpolate between the previous and next camera
    // orientations
    if (m_mouseDown)
        camera.arcball = camera1.arcball;
    else
        camera.arcball.setState(camera0.arcball.state().slerp(t, camera1.arcball.state()));
}

void SampleViewer::generatePoints()
{
    PointType pointType = (PointType) m_pointTypeBox->selectedIndex();
    bool randomize = m_randomizeCheckBox->checked();

    float time0 = (float) glfwGetTime();
    Sampler * generator = m_samplers[pointType];
    if (generator->randomized() != randomize)
        generator->setRandomized(randomize);

    generator->setDimensions(m_numDimensions);

    int numPts = generator->setNumSamples(m_targetPointCount);
    m_pointCount = numPts > 0 ? numPts : m_targetPointCount;
    m_firstDrawPointBox->setMaxValue(m_pointCount-1);
    m_firstDrawPointBox->setValue(m_firstDrawPointBox->value());
    m_pointDrawCountBox->setMaxValue(m_pointCount-m_firstDrawPointBox->value());
    m_pointDrawCountBox->setValue(m_pointDrawCountBox->value());


    float time1 = (float) glfwGetTime();
    float timeDiff1 = ((time1-time0)*1000.0f);

    m_points.resize(m_numDimensions, m_pointCount);
    m_3DPoints.resize(3, m_pointCount);

    time1 = (float) glfwGetTime();
    const VectorXf half = VectorXf::Ones(m_numDimensions,1) * 0.5f;
    for (int i = 0; i < m_pointCount; ++i)
    {
        VectorXf r = half;
        generator->sample(r.data(), i);
        r -= half;
        m_points.col(i) << r;
    }
    float time2 = (float) glfwGetTime();
    float timeDiff2 = ((time2-time1)*1000.0f);

    m_timeStrings.resize(3);
    m_timeStrings[0] = tfm::format("Reset and randomize: %3.3f ms", timeDiff1);
    m_timeStrings[1] = tfm::format("Sampling: %3.3f ms", timeDiff2);
    m_timeStrings[2] = tfm::format("Total generation time: %3.3f ms", timeDiff1 + timeDiff2);
}

void SampleViewer::populatePointSubset()
{
    //
    // Populate point subsets
    //
    m_subsetPoints = m_points;
    m_subsetCount = m_pointCount;
    if (m_subsetByCoord->checked())
    {
        // cout << "only accepting points where the coordinates along the " << m_subsetAxis->value()
        //      << " axis are within: ["
        //      << ((m_subsetLevel->value() + 0.0f)/m_numSubsetLevels->value()) << ", "
        //      << ((m_subsetLevel->value()+1.0f)/m_numSubsetLevels->value()) << ")." << endl;
        m_subsetCount = 0;
        for (int i = 0; i < m_points.cols(); ++i)
        {
            float v = m_points(m_subsetAxis->value(), i);
            // cout << v << endl;
            v += 0.5f;
            if (v >= (m_subsetLevel->value() + 0.0f)/m_numSubsetLevels->value() &&
                v  < (m_subsetLevel->value()+1.0f)/m_numSubsetLevels->value())
            {
                m_subsetPoints.col(m_subsetCount++) = m_points.col(i);
            }
        }
    }

    for (int i = 0; i < m_3DPoints.cols(); ++i)
    {
        m_3DPoints(0,i) = m_subsetPoints(m_xDimension->value(),i);
        m_3DPoints(1,i) = m_subsetPoints(m_yDimension->value(),i);
        m_3DPoints(2,i) = m_subsetPoints(m_zDimension->value(),i);
    }
}


void SampleViewer::generateGrid(MatrixXf & positions, int gridRes)
{
    int fineGridRes = 1;
    int idx = 0;
    positions.resize(3, 4 * (gridRes+1) * (fineGridRes));
    float coarseScale = 1.f / gridRes,
          fineScale = 1.f / fineGridRes;
    // for (int z = -1; z <= 1; z+=2)
    int z = 0;
        for (int i = 0; i <= gridRes; ++i)
        {
            for (int j = 0; j < fineGridRes; ++j)
            {
                positions.col(idx++) = Vector3f(j    *fineScale - 0.5f, i * coarseScale   - 0.5f, z*0.5f);
                positions.col(idx++) = Vector3f((j+1)*fineScale - 0.5f, i * coarseScale   - 0.5f, z*0.5f);
                positions.col(idx++) = Vector3f(i*coarseScale   - 0.5f, j     * fineScale - 0.5f, z*0.5f);
                positions.col(idx++) = Vector3f(i*coarseScale   - 0.5f, (j+1) * fineScale - 0.5f, z*0.5f);
            }
        }
}


namespace
{

void computeCameraMatrices(Matrix4f &model,
                           Matrix4f &lookat,
                           Matrix4f &view,
                           Matrix4f &proj,
                           const CameraParameters & c,
                           float windowAspect)
{
    model = scale(Vector3f::Constant(c.zoom));
    if (c.cameraType == CAMERA_XY || c.cameraType == CAMERA_2D)
        model = scale(Vector3f(1,1,0)) * model;
    if (c.cameraType == CAMERA_XZ)
        model = scale(Vector3f(1,0,1)) * model;
    if (c.cameraType == CAMERA_YZ)
        model = scale(Vector3f(0,1,1)) * model;

    float fH = tan(c.viewAngle / 360.0f * M_PI) * c.dnear;
    float fW = fH * windowAspect;

    float oFF = c.eye.z() / c.dnear;
    Matrix4f orth = ortho(-fW*oFF, fW*oFF, -fH*oFF, fH*oFF, c.dnear, c.dfar);
    Matrix4f frust = frustum(-fW, fW, -fH, fH, c.dnear, c.dfar);

    proj = lerp(orth, frust, c.perspFactor);
    lookat = lookAt(c.eye, c.center, c.up);
    view = c.arcball.matrix();
}


void drawEPSGrid(const Matrix4f & mvp, int gridRes, ofstream & file)
{
    Vector4f vA4d, vB4d;
    Vector2f vA, vB;

    int fineGridRes = 2;
    float coarseScale = 1.f / gridRes,
          fineScale = 1.f / fineGridRes;

    // draw the bounding box
    // if (gridRes == 1)
    {
        Vector4f c004d = mvp * Vector4f(0.0f - 0.5f, 0.0f - 0.5f, 0.0f, 1.0f);
        Vector4f c104d = mvp * Vector4f(1.0f - 0.5f, 0.0f - 0.5f, 0.0f, 1.0f);
        Vector4f c114d = mvp * Vector4f(1.0f - 0.5f, 1.0f - 0.5f, 0.0f, 1.0f);
        Vector4f c014d = mvp * Vector4f(0.0f - 0.5f, 1.0f - 0.5f, 0.0f, 1.0f);
        Vector2f c00 = Vector2f(c004d.x()/c004d.w(), c004d.y()/c004d.w());
        Vector2f c10 = Vector2f(c104d.x()/c104d.w(), c104d.y()/c104d.w());
        Vector2f c11 = Vector2f(c114d.x()/c114d.w(), c114d.y()/c114d.w());
        Vector2f c01 = Vector2f(c014d.x()/c014d.w(), c014d.y()/c014d.w());

        file << "newpath\n";
        file << c00.x() << " " << c00.y() << " moveto\n";
        file << c10.x() << " " << c10.y() << " lineto\n";
        file << c11.x() << " " << c11.y() << " lineto\n";
        file << c01.x() << " " << c01.y() << " lineto\n";
        file << "closepath\n";
        file << "stroke\n";
    }

    for (int i = 1; i <= gridRes-1; i++)
    {
        // draw horizontal lines
        file << "newpath\n";
        for (int j = 0; j < fineGridRes; j++)
        {
            vA4d = mvp * Vector4f(j    *fineScale - 0.5f, i * coarseScale - 0.5f, 0.0f, 1.0f);
            vB4d = mvp * Vector4f((j+1)*fineScale - 0.5f, i * coarseScale - 0.5f, 0.0f, 1.0f);

            vA = Vector2f(vA4d.x()/vA4d.w(), vA4d.y()/vA4d.w());
            vB = Vector2f(vB4d.x()/vB4d.w(), vB4d.y()/vB4d.w());

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
        for (int j = 0; j < fineGridRes; j++)
        {
            vA4d = mvp * Vector4f(i*coarseScale - 0.5f, j     * fineScale - 0.5f, 0.0f, 1.0f);
            vB4d = mvp * Vector4f(i*coarseScale - 0.5f, (j+1) * fineScale - 0.5f, 0.0f, 1.0f);

            vA = Vector2f(vA4d.x()/vA4d.w(), vA4d.y()/vA4d.w());
            vB = Vector2f(vB4d.x()/vB4d.w(), vB4d.y()/vB4d.w());

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


void writeEPSPoints(const MatrixXf & points, int start, int count,
                    const Matrix4f & mvp, ofstream & file,
                    int dimX, int dimY, int dimZ)
{
    for (int i = start; i < start+count; i++)
    {
        Vector4f v4d = mvp * Vector4f(points(dimX, i),
                                      points(dimY, i),
                                      points(dimZ, i), 1.0f);
        Vector2f v2d(v4d.x()/v4d.w(), v4d.y()/v4d.w());
        file << v2d.x() << " " << v2d.y() << " p\n";
    }
}


} // namespace
