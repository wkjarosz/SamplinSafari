/** \file Jittered.h
    \author Wojciech Jarosz
*/
#pragma once

#include <pcg32.h>
#include <sampler/Sampler.h>

/// Encapsulate a 2D stratified or "jittered" point set.
class Jittered : public TSamplerDim<2>
{
public:
    Jittered(unsigned resX, unsigned resY, float jitter = 1.0f);
    ~Jittered() override;

    void reset() override;
    void sample(float[], unsigned i) override;

    std::string name() const override;

    int numSamples() const override
    {
        return m_resX * m_resY;
    }
    int setNumSamples(unsigned n) override
    {
        int sqrtVal = (n == 0) ? 1 : (int)(std::sqrt((float)n) + 0.5f);
        m_resX = m_resY = sqrtVal;
        reset();
        return m_resX * m_resY;
    }
    void setNumSamples(unsigned x, unsigned y)
    {
        m_resX = x;
        m_resY = y;
        reset();
    }

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r = true) override
    {
        if ((m_randomize = r))
            m_seed++;
    }

    /// Gets/sets how much the points are jittered
    float jitter() const override
    {
        return m_maxJit;
    }
    float setJitter(float j = 1.0f) override
    {
        return m_maxJit = j;
    }

private:
    unsigned m_resX, m_resY;
    float    m_maxJit;

    float    m_xScale, m_yScale;
    bool     m_randomize = true;
    pcg32    m_rand;
    unsigned m_seed = 13;
};
