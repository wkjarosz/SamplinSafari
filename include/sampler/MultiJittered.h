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
    MultiJittered(unsigned x, unsigned y, uint32_t seed = 13, float jitter = 0.0f);
    ~MultiJittered() override;
    void clear();

    void reset() override;
    void sample(float[], unsigned i) override;

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override
    {
        m_seed = seed;
        m_rand.seed(m_seed);
        reset();
    }

    std::string name() const override { return "Multi-Jittered"; }

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
        m_resX       = x;
        m_resY       = y;
        m_numSamples = m_resX * m_resY;
        reset();
    }

protected:
    unsigned m_resX, m_resY, m_numSamples;
    float    m_maxJit;

    float m_scale;

    float **m_samples = nullptr;

    pcg32    m_rand;
    uint32_t m_seed = 13;
};

/// An in-place version of multi-jittered point set with both jittered and n-rooks stratification.
/**
    Produces standard multi-jittered points, but uses in-place random permutations
    instead of storing and permuting all numSamples points ahead of time.
*/
class MultiJitteredInPlace : public TSamplerDim<2>
{
public:
    MultiJitteredInPlace(unsigned, unsigned, uint32_t seed = 0, float jitter = 0.0f);

    void reset() override;
    void sample(float[], unsigned i) override;

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed) override
    {
        m_seed = seed;
        m_rand.seed(m_seed);
        m_permutation = m_seed ? m_rand.nextUInt() : 0;
    }

    float jitter() const override { return m_maxJit; }
    float setJitter(float j = 1.0f) override
    {
        m_maxJit = j;
        reset();
        return m_maxJit;
    }

    std::string name() const override;

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
        m_resX       = x;
        m_resY       = y;
        m_numSamples = m_resX * m_resY;
        reset();
    }

protected:
    unsigned m_resX, m_resY, m_numSamples;
    float    m_maxJit;
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
    CorrelatedMultiJittered(unsigned, unsigned, uint32_t seed = 0, float jitter = 0.0f);
    ~CorrelatedMultiJittered() override;

    void reset() override;

    std::string name() const override { return "Correlated Multi-Jittered"; }
};

/// An in-place version of (correlated) multi-jittered point sets.
/**
    Based on method described in the tech report:

    > Andrew Kensler. "Correlated Multi-Jittered Sampling",
    > Pixar Technical Memo 13-01.

    Setting correlated to false will produce standard multi-jittered points (subsuming the MultiJitteredInPlace class).
*/
class CorrelatedMultiJitteredInPlace : public TSamplerMinMaxDim<1, 1024>
{
public:
    CorrelatedMultiJitteredInPlace(unsigned x, unsigned y, unsigned dimensions = 2, uint32_t seed = 0,
                                   float jitter = 0.0f, bool correlated = true);

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
        m_resX       = x;
        m_resY       = y;
        m_numSamples = m_resX * m_resY;
    }

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override
    {
        m_seed = seed;
        m_rand.seed(m_seed);
        m_permutation = m_seed ? m_rand.nextUInt() : 0;
    }

    float jitter() const override { return m_maxJit; }
    float setJitter(float j = 1.0f) override { return m_maxJit = j; }

    std::string name() const override
    {
        return m_decorrelate ? "Multi-Jittered In-Place" : "Correlated Multi-Jittered In-Place";
    }

protected:
    unsigned m_resX, m_resY, m_numSamples, m_numDimensions;
    float    m_maxJit;
    pcg32    m_rand;
    uint32_t m_seed        = 13;
    uint32_t m_permutation = 13;
    uint32_t m_decorrelate;
};