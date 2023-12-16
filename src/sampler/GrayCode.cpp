/** \file GrayCode.cpp
    \author Wojciech Jarosz
*/

#include <sampler/GrayCode.h>

GrayCode::GrayCode(unsigned n)
{
    setNumSamples(n);
    regenerate();
}

void GrayCode::regenerate()
{
    // compute all points, and store them
    m_samples.clear();
    m_samples.resize(N);

    double res = 1.0 / N;
    for (unsigned V = 0; V < n; V++)
        m_samples[V].x = m_samples[V * n].y = res * (V * n); // Sample in first stratum of each row and column

    uint32_t u = 0;
    for (unsigned U = 1; U < n; U++)
    {                                             // Iterate through strata up the column
        uint32_t newBitsCount = __builtin_ctz(U); // This number of bits have to be replaced on the left
        uint32_t reservedBitsCount =
            log2n - 1 - newBitsCount; // Number of reserved bits from previous sample. 1 is the octave toggling bit.

        // Generate a random xor mask for new bits, leading to Faure-Tezuka scrambling.
        uint32_t randomBits = m_randomize ? (m_rand.nextUInt() & ((1 << newBitsCount) - 1)) >> 1 : 0;

        // This generates a Gray code ordering of van der Corput sequences.
        u ^= ((randomBits << 1) | 1)
             << reservedBitsCount; // Apply xor mask: random bits, a single 1, and zeros for reserved bits

        for (uint32_t V = 0; V < n; V++)
            // Iterate through columns
            m_samples[U * n + V].x = m_samples[V * n + U].y = res * (V * n + u);
    }
}

void GrayCode::sample(float r[], unsigned i)
{
    assert(i < N);
    r[0] = m_samples[i].x;
    r[1] = m_samples[i].y;
}

void GrayCode::setRandomized(bool r)
{
    m_randomize = r;
    regenerate();
}