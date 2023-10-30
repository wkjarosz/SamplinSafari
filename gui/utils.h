/** \file utils.h
    \author Wojciech Jarosz
*/

#pragma once

#include <nanogui/vector.h>
#include <vector>

NAMESPACE_BEGIN(nanogui)

//
// Missing matrix and vector operations
//

template <typename T>
Array<T, 4> operator*(const Matrix<T, 4> &a, const Array<T, 4> &v)
{
    Array<T, 4> c;
    for (size_t i = 0; i < 4; ++i)
    {
        T accum{0};
        for (size_t j = 0; j < 4; ++j)
            accum += a.m[j][i] * v[j];
        c[i] = accum;
    }
    return c;
}

template <typename T, size_t Size>
Matrix<T, Size> operator*(const T &a, const Matrix<T, Size> &b)
{
    Matrix<T, Size> c;
    for (size_t i = 0; i < Size; ++i)
        for (size_t j = 0; j < Size; ++j)
            c.m[j][i] = a * b.m[j][i];
    return c;
}

template <typename T, size_t Size>
Matrix<T, Size> operator*(const Matrix<T, Size> &a, const T &b)
{
    Matrix<T, Size> c;
    for (size_t i = 0; i < Size; ++i)
        for (size_t j = 0; j < Size; ++j)
            c.m[j][i] = a.m[j][i] * b;
    return c;
}

template <typename T, size_t Size>
Matrix<T, Size> operator+(const Matrix<T, Size> &a, const Matrix<T, Size> &b)
{
    Matrix<T, Size> c;
    for (size_t i = 0; i < Size; ++i)
        for (size_t j = 0; j < Size; ++j)
            c.m[j][i] = a.m[j][i] + b.m[j][i];
    return c;
}

inline Matrix4f frustum(float left, float right, float bottom, float top, float nearVal, float farVal)
{
    Matrix4f result{0.f};
    result.m[0][0] = (2.0f * nearVal) / (right - left);
    result.m[1][1] = (2.0f * nearVal) / (top - bottom);
    result.m[2][0] = (right + left) / (right - left);
    result.m[2][1] = (top + bottom) / (top - bottom);
    result.m[2][2] = -(farVal + nearVal) / (farVal - nearVal);
    result.m[2][3] = -1.0f;
    result.m[3][2] = -(2.0f * farVal * nearVal) / (farVal - nearVal);
    return result;
}

//
// Quaternion math
//

template <typename T>
constexpr Array<T, 3> qxdir(const Array<T, 4> &q)
{
    return {q.w() * q.w() + q.x() * q.x() - q.y() * q.y() - q.z() * q.z(), (q.x() * q.y() + q.z() * q.w()) * 2,
            (q.z() * q.x() - q.y() * q.w()) * 2};
}
template <typename T>
constexpr Array<T, 3> qydir(const Array<T, 4> &q)
{
    return {(q.x() * q.y() - q.z() * q.w()) * 2, q.w() * q.w() - q.x() * q.x() + q.y() * q.y() - q.z() * q.z(),
            (q.y() * q.z() + q.x() * q.w()) * 2};
}
template <typename T>
constexpr Array<T, 3> qzdir(const Array<T, 4> &q)
{
    return {(q.z() * q.x() + q.y() * q.w()) * 2, (q.y() * q.z() - q.x() * q.w()) * 2,
            q.w() * q.w() - q.x() * q.x() - q.y() * q.y() + q.z() * q.z()};
}
template <typename T>
constexpr Matrix<T, 4> qmat(const Array<T, 4> &q)
{
    auto        result  = Matrix<T, 4>::scale({1, 1, 1, 1});
    Array<T, 3> qdir[3] = {qxdir(q), qydir(q), qzdir(q)};
    for (size_t i = 0; i < 3; ++i)
        for (size_t j = 0; j < 3; ++j)
            result.m[i][j] = qdir[i][j];
    return result;
}
template <typename T>
constexpr Array<T, 4> qmul(const Array<T, 4> &a, const Array<T, 4> &b)
{
    return {a.x() * b.w() + a.w() * b.x() + a.y() * b.z() - a.z() * b.y(),
            a.y() * b.w() + a.w() * b.y() + a.z() * b.x() - a.x() * b.z(),
            a.z() * b.w() + a.w() * b.z() + a.x() * b.y() - a.y() * b.x(),
            a.w() * b.w() - a.x() * b.x() - a.y() * b.y() - a.z() * b.z()};
}
template <typename T>
Array<T, 4> rotation_quat(const Array<T, 3> &axis, T angle)
{
    auto t = axis * std::sin(angle / 2);
    return {t[0], t[1], t[2], std::cos(angle / 2)};
}

template <typename T, size_t M>
T uangle(const Array<T, M> &a, const Array<T, M> &b)
{
    T d = dot(a, b);
    return d > 1 ? 0 : std::acos(d < -1 ? -1 : d);
}

template <typename T, size_t M>
Array<T, 4> slerp2(const Array<T, M> &a, const Array<T, M> &b, T t)
{
    T th = uangle(a, b);
    return th == 0 ? a : a * (std::sin(th * (1 - t)) / std::sin(th)) + b * (std::sin(th * t) / std::sin(th));
}

template <typename T>
Array<T, 4> qslerp(const Array<T, 4> &a, const Array<T, 4> &b, T t)
{
    return slerp2(a, dot(a, b) < 0 ? -b : b, t);
}

template <typename T>
constexpr Array<T, 3> qrot(const Array<T, 4> &q, const Array<T, 3> &v)
{
    return qxdir(q) * v.x + qydir(q) * v.y + qzdir(q) * v.z;
}

//! Based on the arcball in nanogui 1
struct Arcball
{
    using Quaternionf = Vector4f;

    Arcball(float speedFactor = 2.0f) :
        m_active(false), m_lastPos(0, 0), m_size(0, 0), m_quat(0, 0, 0, 1), m_incr(0, 0, 0, 1),
        m_speedFactor(speedFactor)
    {
    }

    /**
     * \brief The internal rotation of the Arcball.
     *
     * Call \ref Arcball::matrix for drawing loops, this method will not return
     * any updates while \ref mActive is ``true``.
     */
    Quaternionf &state()
    {
        return m_quat;
    }

    /// ``const`` version of \ref Arcball::state.
    const Quaternionf &state() const
    {
        return m_quat;
    }

    /// Sets the rotation of this Arcball.  The Arcball will be marked as **not** active.
    void setState(const Quaternionf &state)
    {
        m_active  = false;
        m_lastPos = {0, 0};
        m_quat    = state;
        m_incr    = {0, 0, 0, 1};
    }

    void setSize(Vector2i size)
    {
        m_size = size;
    }

    const Vector2i &size() const
    {
        return m_size;
    }

    void button(Vector2i pos, bool pressed)
    {
        m_active  = pressed;
        m_lastPos = pos;
        if (!m_active)
            m_quat = normalize(qmul(m_incr, m_quat));
        m_incr = {0, 0, 0, 1};
    }

    bool motion(Vector2i pos)
    {
        if (!m_active)
            return false;

        /* Based on the rotation controller from AntTweakBar */
        float invMinDim = 1.0f / std::min(m_size.x(), m_size.y());
        float w = (float)m_size.x(), h = (float)m_size.y();

        float ox = (m_speedFactor * (2 * m_lastPos.x() - w) + w) - w - 1.0f;
        float tx = (m_speedFactor * (2 * pos.x() - w) + w) - w - 1.0f;
        float oy = (m_speedFactor * (h - 2 * m_lastPos.y()) + h) - h - 1.0f;
        float ty = (m_speedFactor * (h - 2 * pos.y()) + h) - h - 1.0f;

        ox *= invMinDim;
        oy *= invMinDim;
        tx *= invMinDim;
        ty *= invMinDim;

        Vector3f v0(ox, oy, 1.0f), v1(tx, ty, 1.0f);
        if (squared_norm(v0) > 1e-4f && squared_norm(v1) > 1e-4f)
        {
            v0            = normalize(v0);
            v1            = normalize(v1);
            Vector3f axis = cross(v0, v1);
            float    sa = std::sqrt(dot(axis, axis)), ca = dot(v0, v1), angle = std::atan2(sa, ca);
            if (tx * tx + ty * ty > 1.0f)
                angle *= 1.0f + 0.2f * (std::sqrt(tx * tx + ty * ty) - 1.0f);
            m_incr = rotation_quat(normalize(axis), angle);
            if (!std::isfinite(norm(m_incr)))
                m_incr = {0, 0, 0, 1};
        }
        return true;
    }

    Matrix4f matrix() const
    {
        return qmat(qmul(m_incr, m_quat));
    }

private:
    /// Whether or not this Arcball is currently active.
    bool m_active;

    /// The last click position (which triggered the Arcball to be active / non-active).
    Vector2i m_lastPos;

    /// The size of this Arcball.
    Vector2i m_size;

    /**
     * The current stable state.  When this Arcball is active, represents the
     * state of this Arcball when \ref Arcball::button was called with
     * ``down = true``.
     */
    Quaternionf m_quat;

    /// When active, tracks the overall update to the state.  Identity when non-active.
    Quaternionf m_incr;

    /**
     * The speed at which this Arcball rotates.  Smaller values mean it rotates
     * more slowly, higher values mean it rotates more quickly.
     */
    float m_speedFactor;
};

NAMESPACE_END(nanogui)
