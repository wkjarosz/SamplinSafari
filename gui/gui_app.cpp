
#include "gui_app.h"
#include "GLFW/glfw3.h"
#include <stdexcept>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

GUIApp *GUIApp::s_instance = nullptr;

// Calculate pixel ratio for hi-dpi devices.
static float get_pixel_ratio(GLFWwindow *window)
{
#ifdef __EMSCRIPTEN__
    return emscripten_get_device_pixel_ratio();
#else
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    return xscale;
#endif
}

static GUIApp *get_app(GLFWwindow *w)
{
    if (auto p = glfwGetWindowUserPointer(w))
        return static_cast<GUIApp *>(glfwGetWindowUserPointer(w));
    else
        throw std::runtime_error("Expected the GLFW user pointer to be set.");
}

GUIApp::GUIApp()
{
    m_last_interaction          = glfwGetTime();
    s_instance                  = this;
    m_params.callbacks.PostInit = [this]()
    {
        register_callbacks();
        initialize_GL();
    };
    m_params.callbacks.CustomBackground = [this]() { draw(); };
}

GUIApp::~GUIApp()
{
}

void GUIApp::initialize_GL()
{
}

void GUIApp::register_callbacks()
{
    auto w = static_cast<GLFWwindow *>(m_params.backendPointers.glfwWindow);

    glfwGetWindowSize(w, &m_size[0], &m_size[1]);
    glfwGetFramebufferSize(w, &m_fbsize[0], &m_fbsize[1]);
    m_pixel_ratio = get_pixel_ratio(w);
    resize_callback_event();

    // store the GUIApp pointer so we can grab it in the callbacks
    glfwSetWindowUserPointer(w, this);

    // Propagate GLFW events to the appropriate virtual functions
    glfwSetCursorPosCallback(w,
                             [](GLFWwindow *w, double xf, double yf)
                             {
                                 if (ImGui::GetIO().WantCaptureMouse)
                                     return;

                                 get_app(w)->cursor_pos_event(xf, yf);
                             });

    glfwSetMouseButtonCallback(w,
                               [](GLFWwindow *w, int button, int action, int mods)
                               {
                                   if (ImGui::GetIO().WantCaptureMouse)
                                       return;
                                   get_app(w)->mouse_button_callback_event(button, action, mods);
                               });

    glfwSetScrollCallback(w,
                          [](GLFWwindow *w, double xoffset, double yoffset)
                          {
                              if (ImGui::GetIO().WantCaptureMouse)
                                  return;

                              get_app(w)->scroll_callback_event(xoffset, yoffset);
                          });

    glfwSetCharCallback(w,
                        [](GLFWwindow *w, unsigned int c)
                        {
                            if (ImGui::GetIO().WantCaptureKeyboard)
                                return;

                            get_app(w)->char_callback_event(c);
                        });

    glfwSetKeyCallback(w,
                       [](GLFWwindow *w, int key, int scancode, int action, int mods)
                       {
                           if (ImGui::GetIO().WantCaptureKeyboard)
                               return;

                           get_app(w)->key_callback_event(key, scancode, action, mods);
                       });

    glfwSetDropCallback(w, [](GLFWwindow *w, int nb, const char **p) { get_app(w)->drop_callback_event(nb, p); });

    // React to framebuffer size events -- includes window size events and also catches things like dragging a window
    // from a Retina-capable screen to a normal screen on Mac OS X
    glfwSetFramebufferSizeCallback(w, [](GLFWwindow *w, int, int) { get_app(w)->resize_callback_event(); });

    // notify when the screen has lost focus (e.g. application switch)
    glfwSetWindowFocusCallback(w, [](GLFWwindow *w, int focused) { get_app(w)->focus_event(focused != 0); });

    // notify when the screen was maximized or restored
    glfwSetWindowMaximizeCallback(w, [](GLFWwindow *w, int maximized) { get_app(w)->maximize_event(maximized != 0); });

    glfwSetWindowContentScaleCallback(w,
                                      [](GLFWwindow *w, float, float)
                                      {
                                          auto app           = get_app(w);
                                          app->m_pixel_ratio = get_pixel_ratio(w);
                                          app->resize_callback_event();
                                      });
}

void GUIApp::cursor_pos_event(double x, double y)
{
    int2 p{int(x), int(y)};

#if defined(_WIN32) || defined(__linux__) || defined(EMSCRIPTEN)
    p = int2(float2(p) / m_pixel_ratio);
#endif

    // m_last_interaction = glfwGetTime();
    try
    {
        p -= int2{1, 2};

        bool ret    = mouse_motion_event(p, p - m_mouse_pos, m_mouse_state, m_modifiers);
        m_mouse_pos = p;
        m_redraw |= ret;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught exception in event handler: " << e.what() << std::endl;
    }
}

void GUIApp::mouse_button_callback_event(int button, int action, int modifiers)
{
    m_modifiers        = modifiers;
    m_last_interaction = glfwGetTime();

#if defined(__APPLE__)
    if (button == GLFW_MOUSE_BUTTON_1 && modifiers == GLFW_MOD_CONTROL)
        button = GLFW_MOUSE_BUTTON_2;
#endif

    try
    {
        if (action == GLFW_PRESS)
            m_mouse_state |= 1 << button;
        else
            m_mouse_state &= ~(1 << button);

        m_redraw |= mouse_button_event(m_mouse_pos, button, action == GLFW_PRESS, m_modifiers);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught exception in event handler: " << e.what() << std::endl;
    }
}

void GUIApp::scroll_callback_event(double x, double y)
{
    m_last_interaction = glfwGetTime();
    try
    {
        m_redraw |= scroll_event(m_mouse_pos, float2(x, y));
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught exception in event handler: " << e.what() << std::endl;
    }
}

void GUIApp::char_callback_event(unsigned int codepoint)
{
    m_last_interaction = glfwGetTime();
    try
    {
        m_redraw |= keyboard_character_event(codepoint);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught exception in event handler: " << e.what() << std::endl;
    }
}

void GUIApp::key_callback_event(int key, int scancode, int action, int mods)
{
    m_last_interaction = glfwGetTime();
    try
    {
        m_redraw |= keyboard_event(key, scancode, action, mods);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught exception in event handler: " << e.what() << std::endl;
    }
}

void GUIApp::drop_callback_event(int count, const char **filenames)
{
    std::vector<std::string> arg(count);
    for (int i = 0; i < count; ++i)
        arg[i] = filenames[i];
    m_redraw |= drop_event(arg);
}

void GUIApp::resize_callback_event()
{
    auto window = static_cast<GLFWwindow *>(m_params.backendPointers.glfwWindow);
#if defined(EMSCRIPTEN)
    return;
#endif
    int2 fb_size, size;
    glfwGetFramebufferSize(window, &fb_size[0], &fb_size[1]);
    glfwGetWindowSize(window, &size[0], &size[1]);
    if (fb_size == int2{0, 0} || size == int2{0, 0})
        return;

    m_fbsize = fb_size;
    m_size   = size;

#if defined(_WIN32) || defined(__linux__) || defined(EMSCRIPTEN)
    m_size = int2(float2(m_size) / m_pixel_ratio);
#endif

    m_last_interaction = glfwGetTime();

#if defined(NANOGUI_USE_METAL)
    if (m_depth_stencil_texture)
        m_depth_stencil_texture->resize(fb_size);
#endif

    try
    {
        resize_event(m_size);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Caught exception in event handler: " << e.what() << std::endl;
    }
    // redraw();
}

bool GUIApp::resize_event(const int2 &size)
{
    return false;
}
bool GUIApp::maximize_event(bool maximized)
{
    return false;
}
bool GUIApp::mouse_button_event(const int2 &p, int button, bool down, int modifiers)
{
    return false;
}
bool GUIApp::mouse_motion_event(const int2 &p, const int2 &rel, int button, int modifiers)
{
    return false;
}
bool GUIApp::scroll_event(const int2 &p, const float2 &rel)
{
    return false;
}
bool GUIApp::focus_event(bool focused)
{
    return false;
}
bool GUIApp::drop_event(const std::vector<std::string> &)
{
    return false;
}
bool GUIApp::keyboard_event(int key, int scancode, int action, int modifiers)
{
    return false;
}
bool GUIApp::keyboard_character_event(unsigned int codepoint)
{
    return false;
}
