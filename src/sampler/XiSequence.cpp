/** \file XiSequence.cpp
    \author Wojciech Jarosz
*/

#include <sampler/XiSequence.h>
#include <sampler/xi.h>

XiSequence::XiSequence(unsigned n) : m_numSamples(n) { m_xi = std::make_unique<Xi>(); }

void XiSequence::sample(float r[], unsigned i)
{
    static constexpr float inv = 1.0f / (1ULL << 32);

    r[0] = (*m_xi)[i].x * inv;
    r[1] = (*m_xi)[i].y * inv;
}

void XiSequence::setSeed(uint32_t seed)
{
    m_rand.seed(seed);
    if (m_seed)
        m_xi = std::make_unique<Xi>(Vector{m_rand.nextUInt() | 0x80000000, m_rand.nextUInt() | 0x80000000},
                                    Point{m_rand.nextUInt(), m_rand.nextUInt()});
    else
        m_xi = std::make_unique<Xi>();
}