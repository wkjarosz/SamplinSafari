/** \file NRooks.cpp
    \author Wojciech Jarosz
*/

#include <sampler/Misc.h>
#include <sampler/NRooks.h>

using namespace std;

NRooks::NRooks(unsigned dims, unsigned samples, bool randomize, float jitter) :
    m_numSamples(samples), m_numDimensions(dims), m_randomize(randomize), m_maxJit(jitter)
{
    m_permutations.resize(dimensions());
    for (unsigned d = 0; d < dimensions(); d++)
        m_permutations[d].resize(m_numSamples);

    reset();
}

NRooks::~NRooks()
{
}

void NRooks::reset()
{
    if (m_numSamples == 0)
        m_numSamples = 1;
    m_scale = 1.0f / m_numSamples;

    m_permutations.resize(dimensions());
    // set up the random permutations for each dimension
    for (unsigned d = 0; d < dimensions(); d++)
    {
        m_permutations[d].resize(m_numSamples);
        m_permutations[d].identity();
        if (m_randomize)
            m_permutations[d].shuffle(m_rand);
    }
}

void NRooks::sample(float r[], unsigned i)
{
    if (i >= m_numSamples)
        i = 0;

    float jitter = m_maxJit * m_randomize;
    for (unsigned d = 0; d < dimensions(); d++)
        r[d] = (m_permutations[d][i] + 0.5f + jitter * (m_rand.nextFloat() - 0.5f)) * m_scale;
}

NRooksInPlace::NRooksInPlace(unsigned dim, unsigned n, bool r, float j) :
    m_numDimensions(dim), m_numSamples(n), m_maxJit(j), m_randomize(r)
{
    m_scrambles.resize(dim);
    setRandomized(r);
}

NRooksInPlace::~NRooksInPlace()
{
    // empty
}

void NRooksInPlace::reset()
{
    m_scrambles.resize(dimensions());
    m_rand.seed(m_seed);
    // compute new permutation seeds
    for (unsigned d = 0; d < dimensions(); ++d)
        m_scrambles[d] = m_randomize ? m_rand.nextUInt() : 0;
}

void NRooksInPlace::sample(float r[], unsigned i)
{
    if (i >= m_numSamples)
        i = 0;

    if (i == 0)
        m_rand.seed(m_seed);

    float jitter = m_maxJit * m_randomize;
    for (unsigned d = 0; d < dimensions(); d++)
        r[d] = (permute(i, m_numSamples, m_scrambles[d]) + 0.5f + jitter * (m_rand.nextFloat() - 0.5f)) / m_numSamples;
}
