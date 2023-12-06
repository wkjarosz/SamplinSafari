/** \file MultiJittered.h
    \author Wojciech Jarosz
*/
#pragma once

#include <cmath>             // for sqrt
#include <pcg32.h>           // for pcg32
#include <sampler/Sampler.h> // for TSamplerDim, TSamplerMinMaxDim
#include <string>            // for basic_string, string

/// A multi-jittered point set with both jittered and n-rooks stratification.
/**
    Based on method described in the article:

    K. Chiu, P. Shirley, C. Wang.
    "Multi-Jittered Sampling"
    In Graphics Gems IV, chapter V.4, pages 370–374. Academic Press, May 1994.
    http://www.graphicsgems.org/
*/
class MultiJittered : public TSamplerDim<2>
{
public:
    MultiJittered(unsigned, unsigned, bool randomize = true, float jitter = 0.0f);
    ~MultiJittered() override;
    void clear();

    void reset() override;
    void sample(float[], unsigned i) override;

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r) override
    {
        m_randomize = r;
        reset();
    }

    std::string name() const override
    {
        return "Multi-Jittered";
    }

    int numSamples() const override
    {
        return m_numSamples;
    }
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
        m_resX       = x;
        m_resY       = y;
        m_numSamples = m_resX * m_resY;
        reset();
    }

protected:
    unsigned m_resX, m_resY, m_numSamples;
    bool     m_randomize;
    float    m_maxJit;

    float m_scale;

    float **m_samples = nullptr;

    pcg32    m_rand;
    unsigned m_seed = 13;
};

/// An in-place version of multi-jittered point set with both jittered and n-rooks stratification.
/**
    Produces standard multi-jittered points, but uses 2*sqrt(numSamples) permutation arrays
    (one for each major row and major column), instead of storing all numSamples points.
*/
class MultiJitteredInPlace : public TSamplerDim<2>
{
public:
    MultiJitteredInPlace(unsigned, unsigned, bool randomize = false, float jitter = 0.0f);
    ~MultiJitteredInPlace() override;
    void clear();

    void reset() override;
    void sample(float[], unsigned i) override;

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r) override;

    float jitter() const override
    {
        return m_maxJit;
    }
    float setJitter(float j = 1.0f) override
    {
        m_maxJit = j;
        reset();
        return m_maxJit;
    }

    std::string name() const override;

    int numSamples() const override
    {
        return m_numSamples;
    }
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
        m_resX       = x;
        m_resY       = y;
        m_numSamples = m_resX * m_resY;
        reset();
    }

protected:
    unsigned m_resX, m_resY, m_numSamples;
    bool     m_randomize;
    float    m_maxJit;

    float m_scale;

    pcg32    m_rand;
    unsigned m_seed = 13;
    unsigned m_permutation;
};

/// Correlated multi-jittered point sets
/**
    Based on method described in the tech report:

    Andrew Kensler. "Correlated Multi-Jittered Sampling",
    Pixar Technical Memo 13-01.

*/
class CorrelatedMultiJittered : public MultiJittered
{
public:
    CorrelatedMultiJittered(unsigned, unsigned, bool randomize = true, float jitter = 0.0f);
    ~CorrelatedMultiJittered() override;

    void reset() override;

    std::string name() const override
    {
        return "Correlated Multi-Jittered";
    }
};

/// An in-place version of correlated multi-jittered point sets.
/**
    Based on method described in the tech report:

    > Andrew Kensler. "Correlated Multi-Jittered Sampling",
    > Pixar Technical Memo 13-01.
*/
class CorrelatedMultiJitteredInPlace : public TSamplerMinMaxDim<1, 1024>
{
public:
    CorrelatedMultiJitteredInPlace(unsigned x, unsigned y, unsigned dimensions = 2, bool randomize = true,
                                   float jitter = 0.0f);
    ~CorrelatedMultiJitteredInPlace() override;

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override
    {
        return m_numDimensions;
    }
    void setDimensions(unsigned n) override
    {
        m_numDimensions = n;
    }

    int numSamples() const override
    {
        return m_numSamples;
    }
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
        m_resX       = x;
        m_resY       = y;
        m_numSamples = m_resX * m_resY;
    }

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r) override
    {
        m_randomize   = r;
        m_permutation = r ? m_rand.nextUInt() : 0;
    }

    float jitter() const override
    {
        return m_maxJit;
    }
    float setJitter(float j = 1.0f) override
    {
        m_maxJit = j;
        reset();
        return m_maxJit;
    }

    std::string name() const override
    {
        return "Correlated Multi-Jittered In-Place";
    }

protected:
    unsigned m_resX, m_resY, m_numSamples, m_numDimensions;
    float    m_maxJit;
    bool     m_randomize;
    pcg32    m_rand;
    unsigned m_seed = 13;
    unsigned m_permutation;
};