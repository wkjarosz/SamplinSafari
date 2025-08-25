/** \file Jittered.cpp
    \author Wojciech Jarosz
*/

#include <sampler/Jittered.h>
#include <sampler/Misc.h> // for permute

Jittered::Jittered(unsigned x, unsigned y, float jitter) : m_maxJit(jitter) { setNumSamples(x, y); }

void Jittered::sample(float r[], unsigned i)
{
    i %= m_numSamples;

    if (i == 0)
        m_rand.seed(m_seed);

    for (unsigned d = 0; d < dimensions(); d += 2)
    {
        int s = permute(i, m_numSamples, m_permutation * 0x51633e2d * (d + 1));

        // horizontal and vertical indices of the stratum in the jittered grid
        int x = s % m_resX;
        int y = s / m_resX;

        // jitter in the d and d+1 dimensions
        float jx = 0.5f + (m_seed != 0) * m_maxJit * (m_rand.nextFloat() - 0.5f);
        float jy = 0.5f + (m_seed != 0) * m_maxJit * (m_rand.nextFloat() - 0.5f);

        r[d] = (x + jx) * m_xScale;
        if (d + 1 < dimensions())
            r[d + 1] = (y + jy) * m_yScale;
    }
}
