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

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned n) override
    {
        m_numDimensions = n;
        setSeed(m_seed);
    }

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override;

    std::string name() const override { return "Sobol"; }

protected:
    unsigned              m_numDimensions;
    uint32_t              m_seed = 13;
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

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned d) override
    {
        m_numDimensions = d;
        reset();
    }

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override
    {
        m_seed = seed;
        reset();
    }

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override;

    std::string name() const override { return m_shuffle ? "Shuffled+XORed (0,2)" : "XORed (0,2)"; }

protected:
    unsigned              m_numSamples, m_numDimensions;
    bool                  m_shuffle;
    uint32_t              m_seed = 13;
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

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned n) override
    {
        m_numDimensions = n;
        setSeed(m_seed);
    }

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override { m_seed = seed; }

    std::string name() const override { return "Stochastic Sobol"; }

protected:
    unsigned m_numDimensions;
    uint32_t m_seed = 13;
    pcg32    m_rand;
};

/**
    Blue-noise Sobol sampler.

    This sampler uses padded 1D and 2D Sobol’ samples, but with a randomized Morton (Z) sample order across the image,
    which leads to a blue noise distribution. This tends to push error to higher frequencies, which in turn makes it
    appear more visually pleasing.

    This is based on PBRT's implementation of Ahmed and Wonka's blue noise Sobol' sampler.
    http://abdallagafar.com/publications/zsampler/
*/
class ZSobol : public SSobol
{
public:
    ZSobol(unsigned dimensions = 2);

    void sample(float[], unsigned i) override;

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override
    {
        if (n == m_numSamples)
            return m_numSamples;
        m_numSamples = n;
        reset();
        return m_numSamples;
    }

    void reset() override;

    std::string name() const override { return "Blue-noise Sobol"; }

protected:
    uint64_t shuffled_morton_index(uint32_t morton_index, uint32_t num_base_4_digits, uint32_t dimension);

    uint32_t m_numSamples;

    int m_num_base_4_digits, m_log2_res;
};

/**
    Low-discrepancy sampler based on padding 2D Sobol sequences with guaranteed 2D projections.

    Based on the paper:
        Nicolas Bonneel, David Coeurjolly, Jean-Claude Iehl, and Victor Ostromoukhov.
        "Sobol’ Sequences with Guaranteed-Quality 2D Projections."
        ACM Trans. Graph. 44, 4, Article 97 (August 2025), 16 pages.
        https://doi.org/10.1145/3730821

    and the supplemental code at: https://github.com/liris-origami/OneTwoSobolSequences

    \ingroup Samplers
*/
class OneTwo : public TSamplerMinMaxDim<1, 1024>
{
public:
    OneTwo(unsigned n = 64, unsigned dimensions = 2, uint32_t seed = 0);

    void reset() override;
    void sample(float[], unsigned i) override;

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned d) override
    {
        m_numDimensions = d;
        reset();
    }

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override
    {
        m_seed = seed;
        reset();
    }

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override;

    std::string name() const override { return "(1,2) Sobol"; }

protected:
    float sample12(const uint64_t index, const int dim) const;

    unsigned m_numSamples, m_numDimensions;
    uint32_t m_seed = 13;
    pcg32    m_rand;
    // std::vector<unsigned> m_scrambles;
    std::vector<unsigned> m_permutes;
};
