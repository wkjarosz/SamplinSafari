/** \file arcball.h
    \author Wojciech Jarosz
*/

#include "linalg.h"
#include <cmath>

namespace linalg
{
template <class T>
linalg::mat<T, 4, 4> ortho_matrix(T left, T right, T bottom, T top, T near_, T far_)
{
    T rl = 1 / (right - left), tb = 1 / (top - bottom), fn = 1 / (far_ - near_);
    return {{2 * rl, 0, 0, 0},
            {0, 2 * tb, 0, 0},
            {0, 0, -2 * fn, 0},
            {-(right + left) * rl, -(top + bottom) * tb, -(far_ + near_) * fn, 1}};
}

} // namespace linalg

//! Based on the arcball in nanogui 1
struct Arcball
{
    using Quatf  = linalg::vec<float, 4>;
    using Vec2i  = linalg::vec<int, 2>;
    using Vec3f  = linalg::vec<float, 3>;
    using Mat44f = linalg::mat<float, 4, 4>;

    Arcball(float speed_factor = 2.f) :
        m_active(false), m_last_pos(0, 0), m_size(0, 0), m_quat(0, 0, 0, 1), m_incr(0, 0, 0, 1),
        m_speed_factor(speed_factor)
    {
    }

    /**
     * \brief The internal rotation of the Arcball.
     *
     * Call \ref Arcball::matrix for drawing loops, this method will not return
     * any updates while \ref m_active is ``true``.
     */
    Quatf &state()
    {
        return m_quat;
    }

    /// ``const`` version of \ref Arcball::state.
    const Quatf &state() const
    {
        return m_quat;
    }

    /// Sets the rotation of this Arcball.  The Arcball will be marked as **not** active.
    void set_state(const Quatf &state)
    {
        m_active   = false;
        m_last_pos = {0, 0};
        m_quat     = state;
        m_incr     = {0, 0, 0, 1};
    }

    void set_size(Vec2i size)
    {
        m_size = size;
    }

    const Vec2i &size() const
    {
        return m_size;
    }

    void button(Vec2i pos, bool pressed)
    {
        m_active   = pressed;
        m_last_pos = pos;
        if (!m_active)
            m_quat = linalg::normalize(linalg::qmul(m_incr, m_quat));
        m_incr = {0, 0, 0, 1};
    }

    bool motion(Vec2i pos)
    {
        if (!m_active)
            return false;

        /* Based on the rotation controller from AntTweakBar */
        float inv_min_dim = 1.f / std::min(m_size.x, m_size.y);
        float w = (float)m_size.x, h = (float)m_size.y;

        float ox = (m_speed_factor * (2 * m_last_pos.x - w) + w) - w - 1.f;
        float tx = (m_speed_factor * (2 * pos.x - w) + w) - w - 1.f;
        float oy = (m_speed_factor * (h - 2 * m_last_pos.y) + h) - h - 1.f;
        float ty = (m_speed_factor * (h - 2 * pos.y) + h) - h - 1.f;

        ox *= inv_min_dim;
        oy *= inv_min_dim;
        tx *= inv_min_dim;
        ty *= inv_min_dim;

        Vec3f v0{ox, oy, 1.f}, v1{tx, ty, 1.f};
        if (linalg::length2(v0) > 1e-4f && linalg::length2(v1) > 1e-4f)
        {
            v0         = linalg::normalize(v0);
            v1         = linalg::normalize(v1);
            Vec3f axis = linalg::cross(v0, v1);
            float sa = std::sqrt(dot(axis, axis)), ca = linalg::dot(v0, v1), angle = std::atan2(sa, ca);
            if (tx * tx + ty * ty > 1.f)
                angle *= 1.f + 0.2f * (std::sqrt(tx * tx + ty * ty) - 1.f);
            m_incr = linalg::rotation_quat(linalg::normalize(axis), angle);
            if (!std::isfinite(linalg::length(m_incr)))
                m_incr = {0, 0, 0, 1};
        }
        return true;
    }

    Mat44f matrix() const
    {
        auto m = linalg::qmat(linalg::qmul(m_incr, m_quat));
        return {{m.x, 0.f}, {m.y, 0.f}, {m.z, 0.f}, {0.f, 0.f, 0.f, 1.f}};
    }

private:
    /// Whether or not this Arcball is currently active.
    bool m_active;

    /// The last click position (which triggered the Arcball to be active / non-active).
    Vec2i m_last_pos;

    /// The size of this Arcball.
    Vec2i m_size;

    /**
     * The current stable state.  When this Arcball is active, represents the
     * state of this Arcball when \ref Arcball::button was called with
     * ``down = true``.
     */
    Quatf m_quat;

    /// When active, tracks the overall update to the state.  Identity when non-active.
    Quatf m_incr;

    /**
     * The speed at which this Arcball rotates.  Smaller values mean it rotates
     * more slowly, higher values mean it rotates more quickly.
     */
    float m_speed_factor;
};