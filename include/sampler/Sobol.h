/** \file Sobol.h
    \author Wojciech Jarosz
*/
#pragma once

#include <pcg32.h>
#include <sampler/Sampler.h>
#include <vector>

/// A Sobol quasi-random number sequence.
/**
    A wrapper for L. Gruenschloss's fast Sobol sampler.
*/
class Sobol : public TSamplerMinMaxDim<1, 1024>
{
public:
    Sobol(unsigned dimensions = 2);

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override
    {
        return m_numDimensions;
    }
    void setDimensions(unsigned n) override
    {
        m_numDimensions = n;
        setRandomized(randomized());
    }

    bool randomized() const override
    {
        return m_scrambles.size();
    }
    void setRandomized(bool b = true) override;

    std::string name() const override
    {
        return "Sobol";
    }

protected:
    unsigned              m_numDimensions;
    pcg32                 m_rand;
    std::vector<unsigned> m_scrambles;
};

/// A (0,2) sequence created by padding the first two dimensions of Sobol
class ZeroTwo : public TSamplerMinMaxDim<1, 1024>
{
public:
    ZeroTwo(unsigned n = 64, unsigned dimensions = 2, bool shuffle = false);

    void reset() override;
    void sample(float[], unsigned i) override;

    unsigned dimensions() const override
    {
        return m_numDimensions;
    }
    void setDimensions(unsigned d) override
    {
        m_numDimensions = d;
        reset();
    }

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r) override
    {
        m_randomize = r;
        reset();
    }

    int numSamples() const override
    {
        return m_numSamples;
    }
    int setNumSamples(unsigned n) override;

    std::string name() const override
    {
        return m_shuffle ? "Shuffled+XORed (0,2)" : "XORed (0,2)";
    }

protected:
    unsigned              m_numSamples, m_numDimensions;
    bool                  m_randomize, m_shuffle;
    pcg32                 m_rand;
    std::vector<unsigned> m_scrambles;
    std::vector<unsigned> m_permutes;
};

/// Stochastic Sobol quasi-random number sequence.
/**
    A wrapper for Helmer's Owen-scrambled stochastic Sobol sampler.
*/
class SSobol : public TSamplerMinMaxDim<1, 64> // the 64 needs to match sampling::MAX_SOBOL_DIM
{
public:
    SSobol(unsigned dimensions = 2);

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override
    {
        return m_numDimensions;
    }
    void setDimensions(unsigned n) override
    {
        m_numDimensions = n;
        setRandomized(randomized());
    }

    bool randomized() const override
    {
        return m_scramble_seed != 0;
    }
    void setRandomized(bool b = true) override;

    std::string name() const override
    {
        return "Stochastic Sobol";
    }

protected:
    unsigned m_numDimensions;
    pcg32    m_rand;
    uint32_t m_scramble_seed;
};
