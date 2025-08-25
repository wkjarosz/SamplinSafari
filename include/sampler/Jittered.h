/** \file Jittered.h
    \author Wojciech Jarosz
*/
#pragma once

#include <pcg32.h>
#include <sampler/Sampler.h>

/// Encapsulate a 2D stratified or "jittered" point set.
class Jittered : public TSamplerMinMaxDim<1, 1024>
{
public:
    Jittered(unsigned resX, unsigned resY, float jitter = 1.0f);

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned n) override { m_numDimensions = n; }

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override
    {
        if (n == m_numSamples)
            return m_numSamples;
        int sqrtVal = (n == 0) ? 1 : (int)(std::sqrt((float)n) + 0.5f);
        setNumSamples(sqrtVal, sqrtVal);
        return m_numSamples;
    }
    void setNumSamples(unsigned x, unsigned y)
    {
        m_resX       = x == 0 ? 1 : x;
        m_resY       = y == 0 ? 1 : y;
        m_numSamples = m_resX * m_resY;
        m_xScale     = 1.0f / m_resX;
        m_yScale     = 1.0f / m_resY;
    }

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override
    {
        m_seed = seed;
        m_rand.seed(m_seed);
        m_permutation = seed ? m_rand.nextUInt() : 0;
    }

    /// Gets/sets how much the points are jittered
    float jitter() const override { return m_maxJit; }
    float setJitter(float j = 1.0f) override { return m_maxJit = j; }

    std::string name() const override { return "Jittered"; }

private:
    unsigned m_resX, m_resY, m_numSamples, m_numDimensions;
    float    m_maxJit;
    pcg32    m_rand;
    uint32_t m_seed        = 13;
    uint32_t m_permutation = 13;

    float m_xScale, m_yScale;
};
