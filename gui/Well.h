//
// Copyright (C) Wojciech Jarosz. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE.txt file.
//

#pragma once

#include <nanogui/widget.h>

/// A simple container that draws a rounded background of a specified outer and
/// inner color
NAMESPACE_BEGIN(nanogui)
class Well : public Widget
{
public:
    Well(Widget *parent, float radius = 3.0f, const Color &inner = Color(0, 32), const Color &outer = Color(0, 92));

    /// Return the inner well color
    const Color &inner_color() const
    {
        return m_innerColor;
    }
    /// Set the inner well color
    void set_inner_color(const Color &innerColor)
    {
        m_innerColor = innerColor;
    }

    /// Return the outer well color
    const Color &outer_color() const
    {
        return m_outerColor;
    }
    /// Set the outer well color
    void set_outer_color(const Color &outerColor)
    {
        m_outerColor = outerColor;
    }

    void draw(NVGcontext *ctx) override;

protected:
    float m_radius;
    Color m_innerColor, m_outerColor;
};
NAMESPACE_END(nanogui)
