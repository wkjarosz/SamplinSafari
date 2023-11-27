
#pragma once

#include "hello_imgui/hello_imgui.h"
#include "linalg.h"
#include <string>
using namespace linalg::aliases;

//! Base class for all applications.
class GUIApp
{
public:
    GUIApp();
    ~GUIApp();

    /// Singleton GUIApp instance
    static GUIApp *instance()
    {
        return s_instance;
    }

    void run()
    {
        HelloImGui::Run(m_params);
    }

    virtual void initialize_GL();
    virtual void draw() = 0;

    /// Handle a mouse button event (default implementation: do nothing)
    virtual bool mouse_button_event(const int2 &p, int button, bool down, int modifiers);

    /// Handle a mouse motion event (default implementation: do nothing)
    virtual bool mouse_motion_event(const int2 &p, const int2 &rel, int button, int modifiers);

    /// Handle a mouse scroll event (default implementation: do nothing)
    virtual bool scroll_event(const int2 &p, const float2 &rel);

    /// Handle a focus change event (default implementation: do nothing)
    virtual bool focus_event(bool focused);

    /// Handle a file drop event (default implementation: do nothing)
    virtual bool drop_event(const std::vector<std::string> &filenames);

    /// Handle a keyboard event (default implementation: do nothing)
    virtual bool keyboard_event(int key, int scancode, int action, int modifiers);

    /// Handle text input (UTF-32 format) (default implementation: do nothing)
    virtual bool keyboard_character_event(unsigned int codepoint);

    /// Window resize event handler
    virtual bool resize_event(const int2 &size);

    /// Window maximize event handler (default implementation: do nothing)
    virtual bool maximize_event(bool maximized);

    /// Return the ratio between pixel and device coordinates (e.g. >= 2 on Mac Retina displays)
    float pixel_ratio() const
    {
        return m_pixel_ratio;
    }

protected:
    void cursor_pos_event(double xf, double yf);
    void mouse_button_callback_event(int button, int action, int mods);
    void resize_callback_event();
    void scroll_callback_event(double xoffset, double yoffset);
    void char_callback_event(unsigned int c);
    void key_callback_event(int key, int scancode, int action, int mods);
    void drop_callback_event(int nb, const char **p);
    void register_callbacks();

    static GUIApp           *s_instance; ///< pointer to the singleton GUIApp instance
    HelloImGui::RunnerParams m_params;
    float                    m_pixel_ratio;
    int                      m_mouse_state = 0, m_modifiers = 0;
    int2                     m_mouse_pos{0, 0};
    int2                     m_size{0, 0}, m_fbsize{0, 0};
    double                   m_last_interaction;
    bool                     m_redraw = false;
};
