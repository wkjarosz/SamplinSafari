/** \file GrayCode.cpp
    \author Wojciech Jarosz
*/

#include <bitcount.h>
#include <sampler/GrayCode.h>
#include <sampler/Misc.h>

GrayCode::GrayCode(unsigned n)
{
    setNumSamples(n);
    regenerate();
}

void GrayCode::regenerate()
{
    m_rand.seed(m_seed);

    // compute all points, and store them
    m_samples.clear();
    m_samples.resize(N);

    double offset = m_seed ? 0. : 0.5;
    double res    = 1.0 / N;
    // Sample in first stratum of each row and column
    for (unsigned V = 0; V < n; V++) m_samples[V].x = m_samples[V * n].y = res * (V * n + offset);

    uint32_t u = 0;
    // Iterate through strata up the column
    for (unsigned U = 1; U < n; U++)
    {
        // This number of bits have to be replaced on the left
        uint32_t newBitsCount = bit_ctz(U);
        // Number of reserved bits from previous sample. 1 is the octave toggling bit.
        uint32_t reservedBitsCount = log2n - 1 - newBitsCount;

        // Generate a random xor mask for new bits, leading to Faure-Tezuka scrambling.
        uint32_t randomBits = m_seed ? (m_rand.nextUInt() & ((1 << newBitsCount) - 1)) >> 1 : 0;

        // Apply xor mask: random bits, a single 1, and zeros for reserved bits
        // This generates a Gray code ordering of van der Corput sequences.
        u ^= ((randomBits << 1) | 1) << reservedBitsCount;

        // Iterate through columns
        for (uint32_t V = 0; V < n; V++) m_samples[U * n + V].x = m_samples[V * n + U].y = res * (V * n + u + offset);
    }

    if (m_seed)
    {
        // perform additional xor scrambling
        auto s1 = m_rand.nextUInt();
        auto s2 = m_rand.nextUInt();
        for (unsigned i = 0; i < N; ++i)
            m_samples[i] = Point{randomDigitScramble(m_samples[i].x, s1), randomDigitScramble(m_samples[i].y, s2)};
    }
}

int GrayCode::setNumSamples(unsigned num)
{
    int log2N = std::round(std::log2(num));
    if (log2N & 1)
        log2N++;        // Only allow even powers of 2.
    N     = 1 << log2N; // Make N a power of 4, if not.
    log2n = log2N / 2;
    n     = 1 << log2n;
    regenerate();
    return N;
}

void GrayCode::sample(float r[], unsigned i)
{
    assert(i < N);
    r[0] = m_samples[i].x;
    r[1] = m_samples[i].y;
}