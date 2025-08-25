/** \file MultiJittered.cpp
    \author Wojciech Jarosz
*/

#include <sampler/Misc.h> // for permute
#include <sampler/MultiJittered.h>
#include <utility> // for swap

using namespace std;

MultiJittered::MultiJittered(unsigned x, unsigned y, uint32_t seed, float jitter) :
    m_resX(x), m_resY(y), m_numSamples(m_resX * m_resY), m_maxJit(jitter), m_seed(seed)
{
    reset();
}

MultiJittered::~MultiJittered() { clear(); }

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
    m_scale      = 1.0f / m_numSamples;
    m_numSamples = m_resX * m_resY;

    clear();

    m_samples = new float *[dimensions()];
    for (unsigned d = 0; d < dimensions(); d++) m_samples[d] = new float[m_numSamples];

    float jitter = m_seed ? m_maxJit : 0.0f;
    m_rand.seed(m_seed);
    for (unsigned i = 0; i < m_resX; i++)
        for (unsigned j = 0; j < m_resY; j++)
        {
            float jitterx                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            float jittery                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            m_samples[0][j * m_resX + i] = i * m_resY + j + jitterx;
            m_samples[1][j * m_resX + i] = j * m_resX + i + jittery;
        }

    if (m_seed)
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

    for (unsigned d = 0; d < dimensions(); d++) r[d] = (m_samples[d][i]) * m_scale;
}

MultiJitteredInPlace::MultiJitteredInPlace(unsigned x, unsigned y, uint32_t seed, float jitter) :
    m_resX(x), m_resY(y), m_numSamples(m_resX * m_resY), m_maxJit(jitter), m_seed(seed)
{
    reset();
}

std::string MultiJitteredInPlace::name() const { return "MultiJittered In-Place"; }

void MultiJitteredInPlace::reset()
{
    if (m_resX == 0)
        m_resX = 1;
    if (m_resY == 0)
        m_resY = 1;

    m_numSamples = m_resX * m_resY;
}

void MultiJitteredInPlace::sample(float r[], unsigned i)
{
    i = i % m_numSamples;

    if (i == 0)
        m_rand.seed(m_seed);

    // i is the (possibly permuted) sample index
    i = permute(i, m_numSamples, m_permutation * 0x51633e2d);

    // x and y indices of the big stratum in the multi-jittered grid
    int x = i % m_resX;
    int y = i / m_resX;

    // x and y offset within the big stratum based on the sample index
    int sx = permute(y, m_resY, m_permutation * (x + 0x02e5be93));
    int sy = permute(x, m_resX, m_permutation * (y + 0x68bc21eb));

    // jitter in the x and y directions
    float jx = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);
    float jy = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);

    r[0] = (x + (sx + jx) / m_resY) / m_resX;
    r[1] = (y + (sy + jy) / m_resX) / m_resY;
}

CorrelatedMultiJittered::CorrelatedMultiJittered(unsigned x, unsigned y, uint32_t seed, float jitter) :
    MultiJittered(x, y, seed, jitter)
{
    // empty
}

CorrelatedMultiJittered::~CorrelatedMultiJittered() { clear(); }

void CorrelatedMultiJittered::reset()
{
    if (m_resX == 0)
        m_resX = 1;
    if (m_resY == 0)
        m_resY = 1;
    m_scale      = 1.0f / m_numSamples;
    m_numSamples = m_resX * m_resY;

    clear();

    m_samples = new float *[dimensions()];
    for (unsigned d = 0; d < dimensions(); d++) m_samples[d] = new float[m_numSamples];

    float jitter = m_seed ? m_maxJit : 0.0f;

    for (unsigned i = 0; i < m_resX; i++)
        for (unsigned j = 0; j < m_resY; j++)
        {
            float jitterx                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            float jittery                = 0.5f + jitter * (m_rand.nextFloat() - 0.5f);
            m_samples[0][j * m_resX + i] = i * m_resY + j + jitterx;
            m_samples[1][j * m_resX + i] = j * m_resX + i + jittery;
        }

    if (m_seed)
    {
        // shuffle x coordinates within each column of cells
        for (unsigned j = m_resY - 1; j >= 1; j--)
        {
            unsigned k = m_rand.nextUInt(j);
            for (unsigned i = 0; i < m_resX; i++) std::swap(m_samples[0][j * m_resX + i], m_samples[0][k * m_resX + i]);
        }

        // shuffle y coordinates within each row of cells
        for (unsigned i = m_resX - 1; i >= 1; i--)
        {
            unsigned k = m_rand.nextUInt(i);
            for (unsigned j = 0; j < m_resY; j++) std::swap(m_samples[1][j * m_resX + i], m_samples[1][j * m_resX + k]);
        }
    }
}

CorrelatedMultiJitteredInPlace::CorrelatedMultiJitteredInPlace(unsigned x, unsigned y, unsigned dims, uint32_t seed,
                                                               float j, bool correlated) :
    m_resX(x), m_resY(y), m_numSamples(m_resX * m_resY), m_numDimensions(dims), m_maxJit(j), m_seed(seed),
    m_decorrelate(correlated ? 0 : 1)
{
    setSeed(seed);
}

void CorrelatedMultiJitteredInPlace::sample(float r[], unsigned i)
{
    i %= m_numSamples;

    if (i == 0)
        m_rand.seed(m_seed);

    for (unsigned d = 0; d < dimensions(); d += 2)
    {
        int s = permute(i, m_numSamples, m_permutation * 0x51633e2d * (d + 1));

        // horizontal and vertical indices of the big stratum in the multi-jittered grid
        int x = s % m_resX;
        int y = s / m_resX;

        // offsets in the d and d+1 dimensions within the big stratum
        int sx = permute(y, m_resY, m_permutation * (m_decorrelate * x + 0x02e5be93) * (d + 1));
        int sy = permute(x, m_resX, m_permutation * (m_decorrelate * y + 0x68bc21eb) * (d + 1));

        // jitter in the d and d+1 dimensions
        float jx = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);
        float jy = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);

        r[d] = (x + (sx + jx) / m_resY) / m_resX;
        if (d + 1 < dimensions())
            r[d + 1] = (y + (sy + jy) / m_resX) / m_resY;
    }
}
