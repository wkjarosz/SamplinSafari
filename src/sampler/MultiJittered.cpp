/** \file MultiJittered.cpp
    \author Wojciech Jarosz
*/

#include <sampler/Misc.h> // for permute
#include <sampler/MultiJittered.h>
#include <utility> // for swap

using namespace std;

MultiJittered::MultiJittered(unsigned x, unsigned y, bool randomize, float jitter) :
    m_resX(x), m_resY(y), m_numSamples(m_resX * m_resY), m_randomize(randomize), m_maxJit(jitter)
{
    reset();
}

MultiJittered::~MultiJittered()
{
    clear();
}

void MultiJittered::clear()
{
    if (m_samples)
        for (unsigned d = 0; d < dimensions(); d++)
            if (m_samples[d])
                delete[] m_samples[d];
    delete[] m_samples;
    m_samples = nullptr;
}

void MultiJittered::reset()
{
    if (m_resX == 0)
        m_resX = 1;
    if (m_resY == 0)
        m_resY = 1;
    m_scale = 1.0f / m_numSamples;

    clear();

    m_samples = new float *[dimensions()];
    for (unsigned d = 0; d < dimensions(); d++)
        m_samples[d] = new float[m_numSamples];

    float jitter = m_randomize ? m_maxJit : 0.0f;
    m_rand.seed(m_seed);
    for (unsigned i = 0; i < m_resX; i++)
        for (unsigned j = 0; j < m_resY; j++)
        {
            float jitterx                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            float jittery                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            m_samples[0][j * m_resX + i] = i * m_resY + j + jitterx;
            m_samples[1][j * m_resX + i] = j * m_resX + i + jittery;
        }

    if (m_randomize)
    {
        // shuffle x coordinates within each column of cells
        for (unsigned i = 0; i < m_resX; i++)
            for (unsigned j = m_resY - 1; j >= 1; j--)
            {
                unsigned k = m_rand.nextUInt(j);
                std::swap(m_samples[0][j * m_resX + i], m_samples[0][k * m_resX + i]);
            }

        // shuffle y coordinates within each row of cells
        for (unsigned j = 0; j < m_resY; j++)
            for (unsigned i = m_resX - 1; i >= 1; i--)
            {
                unsigned k = m_rand.nextUInt(i);
                std::swap(m_samples[1][j * m_resX + i], m_samples[1][j * m_resX + k]);
            }
    }
}

void MultiJittered::sample(float r[], unsigned i)
{
    if (i >= m_numSamples)
        i = 0;

    for (unsigned d = 0; d < dimensions(); d++)
        r[d] = (m_samples[d][i]) * m_scale;
}

MultiJitteredInPlace::MultiJitteredInPlace(unsigned x, unsigned y, bool randomize, float jitter) :
    m_resX(x), m_resY(y), m_numSamples(m_resX * m_resY), m_randomize(randomize), m_maxJit(jitter)
{
    reset();
}

MultiJitteredInPlace::~MultiJitteredInPlace()
{
    clear();
}

std::string MultiJitteredInPlace::name() const
{
    return "MultiJittered In-Place";
}

void MultiJitteredInPlace::setRandomized(bool r)
{
    m_randomize = r;

    m_permutation = r ? m_rand.nextUInt() : 0;

    reset();
}

void MultiJitteredInPlace::clear()
{
}

void MultiJitteredInPlace::reset()
{
    if (m_resX == 0)
        m_resX = 1;
    m_scale = 1.0f / m_numSamples;
}

void MultiJitteredInPlace::sample(float r[], unsigned i)
{
    i = i % m_numSamples;

    if (i == 0)
        m_rand.seed(m_seed);

    int   s  = permute(i, m_numSamples, m_permutation * 0x51633e2d);
    int   x  = s % m_resX;
    int   y  = s / m_resX;
    int   sx = permute(x, m_resX, (y + m_permutation) * 0x68bc21eb);
    int   sy = permute(y, m_resY, (x + m_permutation) * 0x02e5be93);
    float jx = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);
    float jy = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);
    r[0]     = (x + (sy + jx) / m_resY) / m_resX;
    r[1]     = (y + (sx + jy) / m_resX) / m_resY;
}

CorrelatedMultiJittered::CorrelatedMultiJittered(unsigned x, unsigned y, bool randomize, float jitter) :
    MultiJittered(x, y, randomize, jitter)
{
    // empty
}

CorrelatedMultiJittered::~CorrelatedMultiJittered()
{
    clear();
}

void CorrelatedMultiJittered::reset()
{
    if (m_resX == 0)
        m_resX = 1;
    if (m_resY == 0)
        m_resY = 1;
    m_scale = 1.0f / m_numSamples;

    clear();

    m_samples = new float *[dimensions()];
    for (unsigned d = 0; d < dimensions(); d++)
        m_samples[d] = new float[m_numSamples];

    float jitter = m_randomize ? m_maxJit : 0.0f;

    for (unsigned i = 0; i < m_resX; i++)
        for (unsigned j = 0; j < m_resY; j++)
        {
            float jitterx                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            float jittery                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            m_samples[0][j * m_resX + i] = i * m_resY + j + jitterx;
            m_samples[1][j * m_resX + i] = j * m_resX + i + jittery;
        }

    if (m_randomize)
    {
        // shuffle x coordinates within each column of cells
        for (unsigned j = m_resY - 1; j >= 1; j--)
        {
            unsigned k = m_rand.nextUInt(j);
            for (unsigned i = 0; i < m_resX; i++)
                std::swap(m_samples[0][j * m_resX + i], m_samples[0][k * m_resX + i]);
        }

        // shuffle y coordinates within each row of cells
        for (unsigned i = m_resX - 1; i >= 1; i--)
        {
            unsigned k = m_rand.nextUInt(i);
            for (unsigned j = 0; j < m_resY; j++)
                std::swap(m_samples[1][j * m_resX + i], m_samples[1][j * m_resX + k]);
        }
    }
}

CorrelatedMultiJitteredInPlace::CorrelatedMultiJitteredInPlace(unsigned x, unsigned y, unsigned dims, bool r, float j) :
    m_resX(x), m_resY(y), m_numSamples(m_resX * m_resY), m_numDimensions(dims), m_maxJit(j), m_randomize(r)
{
    setRandomized(r);
}

CorrelatedMultiJitteredInPlace::~CorrelatedMultiJitteredInPlace()
{
    // empty
}

void CorrelatedMultiJitteredInPlace::sample(float r[], unsigned i)
{
    if (i >= m_numSamples)
        i = 0;

    if (i == 0)
        m_rand.seed(m_seed);

    for (unsigned d = 0; d < dimensions(); d += 2)
    {
        int   s  = permute(i, m_numSamples, m_permutation * 0x51633e2d * (d + 1));
        int   sx = permute(s % m_resX, m_resX, m_permutation * 0x68bc21eb * (d + 1));
        int   sy = permute(s / m_resX, m_resY, m_permutation * 0x02e5be93 * (d + 1));
        float jx = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);
        float jy = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);
        r[d]     = (sx + (sy + jx) / m_resY) / m_resX;
        if (d + 1 < dimensions())
            r[d + 1] = (s + jy) / m_numSamples;
    }
}
