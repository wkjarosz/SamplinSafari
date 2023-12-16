/** \file Jittered.cpp
    \author Wojciech Jarosz
*/

#include <sampler/Jittered.h>

Jittered::Jittered(unsigned x, unsigned y, float jitter) : m_resX(x), m_resY(y), m_maxJit(jitter)
{
    reset();
}

void Jittered::reset()
{
    if (m_resX == 0)
        m_resX = 1;
    if (m_resY == 0)
        m_resY = 1;
    m_xScale = 1.0f / m_resX;
    m_yScale = 1.0f / m_resY;
    // m_rand.seed(m_seed);
}

void Jittered::sample(float r[], unsigned i)
{
    if (i == 0)
        m_rand.seed(m_seed);
    float jx = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
    float jy = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
    r[0]     = ((i % m_resX) + jx) * m_xScale;
    r[1]     = ((i / m_resY) + jy) * m_yScale;
}
