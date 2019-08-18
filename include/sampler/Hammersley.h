/** \file Hammersley.h
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/Halton.h>
#include <sampler/Misc.h>
#include <pcg32.h>

/// A Hammersley sequence quasi-random number generator.
/**
    The Hammersley class extends another Sampler by prepending i/N as the first
    dimension. Hammersley inherits from the template parameter BaseHalton,
    which should be either the Halton or HaltonZaremba class.
*/
template <typename BaseHalton>
class Hammersley : public BaseHalton
{
public:
    Hammersley(unsigned dimensions = 2, unsigned numSamples = 64) :
        BaseHalton(dimensions - 1),
        m_numSamples(numSamples), m_inv(1.0f / m_numSamples)
    {

    }
    ~Hammersley() override {}

    void sample(float r[], unsigned i) override
    {
        r[0] = randomDigitScramble(i*m_inv, m_scramble1);
        BaseHalton::sample(r + 1, i);
    }

    unsigned dimensions() const override     {return BaseHalton::dimensions()+1;}
    void setDimensions(unsigned d) override  {BaseHalton::setDimensions(d-1);}
    unsigned minDimensions() const override  {return 1;}
    unsigned maxDimensions() const override  {return BaseHalton::maxDimensions()+1;}

    bool randomized() const override {return m_scramble1 != 0;}
    void setRandomized(bool r = true) override
    {
        BaseHalton::setRandomized(r);
        m_scramble1 = r ? m_rand.nextUInt() : 0;
    }

    std::string name() const override
    {
        return std::string("Hammersley (") + BaseHalton::name() + ")";
    }

    int numSamples() const override          {return m_numSamples;}
    int setNumSamples(unsigned n) override
    {
        m_numSamples = (n == 0) ? 1 : n;
        m_inv = 1.0f / m_numSamples;
        return m_numSamples;
    }

protected:
    unsigned m_numSamples;
    float m_inv;
    unsigned m_scramble1;
    pcg32 m_rand;
};
