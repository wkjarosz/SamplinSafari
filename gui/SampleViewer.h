/*! \file SampleViewer.h
    \author Wojciech Jarosz
*/

#include <nanogui/screen.h>
#include <nanogui/common.h>
#include <nanogui/textbox.h>
#include <nanogui/glutil.h>
#include <fstream>

#include <sampler/Sampler.h>

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
    Arcball arcball;
    float perspFactor = 0.0f;
    float zoom = 1.0f, viewAngle = 35.0f;
    float dnear = 0.05f, dfar = 1000.0f;
    Vector3f eye = Vector3f(0.0f, 0.0f, 2.0f);
    Vector3f center = Vector3f(0.0f, 0.0f, 0.0f);
    Vector3f up = Vector3f(0.0f, 1.0f, 0.0f);
    CameraType cameraType = CAMERA_CURRENT;
};

class SampleViewer : public Screen
{
public:
    SampleViewer();
    ~SampleViewer();

    bool resizeEvent(const Vector2i &);
    bool mouseMotionEvent(const Vector2i &p, const Vector2i &rel, int button, int modifiers);
    bool mouseButtonEvent(const Vector2i &p, int button, bool down, int modifiers);
    bool scrollEvent(const Vector2i &p, const Vector2f &rel);
    void drawContents();
    bool keyboardEvent(int key, int scancode, int action, int modifiers);

private:
    void drawContentsEPS(ofstream & file, CameraType camera, int dimX, int dimY, int dimZ);
    void drawContents2DEPS(ofstream & file);
    void updateGPUPoints(bool regenerate = true);
    void updateGPUGrids();
    void setView(CameraType view);
    void updateCurrentCamera();
    void initializeGUI();
    void generatePoints();
    void populatePointSubset();
    void generateGrid(MatrixXf & positions, int gridRes);
    void drawText(const Vector2i & pos,
                  const std::string & text,
                  const Color & col = Color(1.0f, 1.0f, 1.0f, 1.0f),
                  int fontSize = 10,
                  int fixedWidth = 0,
                  int align = NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM) const;
    void drawPoints(const Matrix4f & mvp, const Vector3f & color,
                    bool showPointNums = false, bool showPointCoords = false) const;
    void drawGrid(const Matrix4f & mvp, float alpha, uint32_t offset, uint32_t count) const;
    void draw2DPointsAndGrid(const Matrix4f & mvp, int dimX, int dimY, int plotIndex) const;

    // GUI elements
    Window * m_parametersDialog = nullptr;
    Window * m_helpDialog = nullptr;
    Button * m_helpButton = nullptr;
    Slider *m_pointCountSlider,
           *m_pointRadiusSlider,
           *m_pointDrawCountSlider,
           *m_jitterSlider;
    IntBox<int> *m_dimensionBox,
                *m_xDimension,
                *m_yDimension,
                *m_zDimension,
                *m_pointCountBox,
                *m_firstDrawPointBox,
                *m_pointDrawCountBox,
                *m_subsetAxis,
                *m_numSubsetLevels,
                *m_subsetLevel;
    ComboBox *m_pointTypeBox;

    // Extra OA parameters
    Label * m_strengthLabel;
    IntBox<int> *m_strengthBox;
    Label * m_offsetTypeLabel;
    ComboBox *m_offsetTypeBox;

    CheckBox *m_coarseGridCheckBox,
             *m_fineGridCheckBox,
             *m_bboxGridCheckBox,
             *m_randomizeCheckBox,
             *m_subsetByIndex,
             *m_subsetByCoord,
             *m_show1DProjections,
             *m_showPointNums,
             *m_showPointCoords;
    Button * m_viewButtons[CAMERA_CURRENT+1];
    Button *m_scaleRadiusWithPoints;
    std::vector<std::string> m_timeStrings;

    //! X, Y, Z, and user-defined cameras
    CameraParameters m_camera[NUM_CAMERA_TYPES];

    float m_animateStartTime = 0.0f;
    bool m_mouseDown = false;
    int m_numDimensions = 3;

    GLShader *m_pointShader = nullptr;
    GLShader *m_gridShader = nullptr;
    GLShader *m_point2DShader = nullptr;
    MatrixXf m_points, m_subsetPoints, m_3DPoints;
    int m_targetPointCount, m_pointCount, m_subsetCount = 0, m_coarseLineCount, m_fineLineCount;

    vector<Sampler*> m_samplers;
};
