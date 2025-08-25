/** \file Faure.h
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/Sampler.h>
#include <vector>

/// Stochastic Faure quasi-random number sequence.
/**
    A wrapper for Helmer's Owen-scrambled stochastic (0,s) Faure sampler with s=3,5,7,11
*/
class Faure : public TSamplerMinMaxDim<1, 11>
{
public:
    Faure(unsigned dimensions = 2, unsigned numSamples = 1);

    void sample(float[], unsigned i) override;

    /// Returns an appropriate grid resolution to help visualize stratification
    int coarseGridRes(int samples) const override
    {
        return (int)pow(s(), floor(log(std::pow(samples, 1.f / s())) / log(s())));
    }

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned n) override;

    uint32_t seed() const override { return m_owen; }
    void     setSeed(uint32_t seed = 0) override
    {
        m_owen = seed;
        regenerate();
    }

    std::string name() const override;

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override;

protected:
    void     regenerate();
    unsigned s() const;

    unsigned            m_numSamples;
    unsigned            m_numDimensions;
    uint32_t            m_owen;
    std::vector<double> m_samples;
};
