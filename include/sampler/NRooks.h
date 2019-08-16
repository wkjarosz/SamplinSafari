/*! \file SamplerNRooks.h
    \brief
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/Sampler.h>
#include <sampler/RandomPermutation.h>
#include <pcg32.h>
#include <vector>

//! Encapsulate an N-rooks (Latin-Hypercube) sampling set.
class NRooks : public TSamplerMinMaxDim<1,(unsigned)-1>
{
public:
    NRooks(unsigned dimensions, unsigned numSamples, bool randomize = true, float jitter = 1.0f);
    ~NRooks() override;

    void reset() override;
    void sample(float[], unsigned i) override;

    unsigned dimensions() const override     {return m_numDimensions;}
    void setDimensions(unsigned d) override  {m_numDimensions = d; reset();}

    bool randomized() const override         {return m_randomize;}
    void setRandomized(bool r) override      {m_randomize = r; reset();}

    std::string name() const override              {return "N-Rooks";}

    int numSamples() const override          {return m_numSamples;}
    int setNumSamples(unsigned n) override
    {
        if (n == m_numSamples)
            return m_numSamples;
        m_numSamples = n;
        reset();
        return m_numSamples;
    }

protected:
    unsigned m_numSamples;
    unsigned m_numDimensions;
    bool m_randomize;
    float m_maxJit;

    float m_scale;

    std::vector<RandomPermutation> m_permutations;
    pcg32 m_rand;
};


//! An in-place version of correlated multi-jittered point sets.
/*!
    Based on method described in the tech report:

    Andrew Kensler. "Correlated Multi-Jittered Sampling",
    Pixar Technical Memo 13-01.
*/
class NRooksInPlace : public TSamplerMinMaxDim<1,(unsigned)-1>
{
public:
    NRooksInPlace(unsigned dimensions, unsigned numSamples, bool randomize = true, float jitter = 0.0f);
    ~NRooksInPlace() override;

    void reset() override;
    void sample(float[], unsigned i) override;

    unsigned dimensions() const override     {return m_numDimensions;}
    void setDimensions(unsigned d) override  {m_numDimensions = d; reset();}

    std::string name() const override              {return "N-Rooks In-Place";}

    int numSamples() const override          {return m_numSamples;}
    int setNumSamples(unsigned n) override
    {
        if (n == m_numSamples)
            return m_numSamples;
        m_numSamples = n;
        reset();
        return m_numSamples;
    }

    bool randomized() const override {return m_randomize;}
    void setRandomized(bool r = true) override
    {
        if ((m_randomize = r))
            m_seed++;
        reset();
    }

    //! Gets/sets how much the points are jittered
    float jitter() const override {return m_maxJit;}
    float setJitter(float j = 1.0f) override {return m_maxJit = j;}

protected:
    unsigned m_numDimensions, m_numSamples;
    float m_maxJit;
    bool m_randomize;
    pcg32 m_rand;
    unsigned m_seed = 13;
    std::vector<unsigned> m_scrambles;
};
