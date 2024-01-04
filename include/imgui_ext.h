#include "imgui.h"
#include "imgui_internal.h"

namespace ImGui
{

inline bool ToggleButton(const char *label, bool *active)
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

inline void Text(const string &text)
{
    return TextUnformatted(text.c_str());
}

inline void TextUnformatted(const string &text)
{
    return TextUnformatted(text.c_str());
}

// return true when activated.
inline bool MenuItem(const string &label, const string &shortcut = "", bool selected = false, bool enabled = true)
{
    return MenuItem(label.c_str(), shortcut.c_str(), selected, enabled);
}

// return true when activated + toggle (*p_selected) if p_selected != NULL
inline bool MenuItem(const string &label, const string &shortcut, bool *p_selected, bool enabled = true)
{
    return MenuItem(label.c_str(), shortcut.c_str(), p_selected, enabled);
}

// from https://github.com/ocornut/imgui/issues/3379#issuecomment-1678718752
void ScrollWhenDraggingOnVoid(const ImVec2 &delta, ImGuiMouseButton mouse_button)
{
    ImGuiContext &g       = *ImGui::GetCurrentContext();
    ImGuiWindow  *window  = g.CurrentWindow;
    bool          hovered = false;
    bool          held    = false;
    ImGuiID       id      = window->GetID("##scrolldraggingoverlay");
    ImGui::KeepAliveID(id);
    ImGuiButtonFlags button_flags = (mouse_button == 0)   ? ImGuiButtonFlags_MouseButtonLeft
                                    : (mouse_button == 1) ? ImGuiButtonFlags_MouseButtonRight
                                                          : ImGuiButtonFlags_MouseButtonMiddle;
    if (g.HoveredId == 0) // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!)
        ImGui::ButtonBehavior(window->Rect(), id, &hovered, &held, button_flags);
    if (held && delta.x != 0.0f)
        ImGui::SetScrollX(window, window->Scroll.x + delta.x);
    if (held && delta.y != 0.0f)
        ImGui::SetScrollY(window, window->Scroll.y + delta.y);
}

} // namespace ImGui