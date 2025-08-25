/** \file Halton.h
    \author Wojciech Jarosz
*/
#pragma once

#include <pcg32.h>                  // for pcg32
#include <sampler/Sampler.h>        // for TSamplerMinMaxDim
#include <sampler/halton_sampler.h> // for Halton_sampler
#include <string>                   // for basic_string, string

/// A Halton quasi-random number sequence.
/**
    A wrapper for L. Gruenschloss's fast Halton sampler.
*/
class Halton : public TSamplerMinMaxDim<1, 256>
{
public:
    Halton(unsigned dimensions = 2);
    ~Halton() override {}

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned) override;

    // Init the permutation arrays using Faure-permutations.
    void initFaure() { m_halton.init_faure(); }

    // Init the permutation arrays using randomized permutations.
    void initRandom();

    /// Gets/sets whether the point set is randomized
    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override;

    std::string name() const override { return "Halton"; }

protected:
    unsigned       m_numDimensions;
    uint32_t       m_seed;
    pcg32          m_rand;
    Halton_sampler m_halton;
};

/// Encapsulate a Halton-Zaremba sequence quasi-random number generator.
class HaltonZaremba : public TSamplerMinMaxDim<1, (unsigned)(-1) - 1>
{
public:
    HaltonZaremba(unsigned dimensions = 2);
    ~HaltonZaremba() override {}

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned) override;

    std::string name() const override { return "Halton-Zaremba"; }

protected:
    unsigned m_numDimensions;
};
