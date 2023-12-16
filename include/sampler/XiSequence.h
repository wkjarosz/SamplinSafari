/** \file XiSequence.h
    \author Wojciech Jarosz
*/
#pragma once

#include <memory>
#include <pcg32.h>
#include <sampler/Misc.h>
#include <sampler/Sampler.h>

/**
    Implements a 2D Xi-sequence as described in:

    Abdalla G. M. Ahmed, Mikhail Skopenkov, Markus Hadwiger, and Peter Wonka.
    "Analysis and Synthesis of Digital Dyadic Sequences."
    In ACM Trans. Graph. (Proceedings of SIGGRAPH Asia 2023), 42(6), doi:10.1145/3618308.
*/
class XiSequence : public TSamplerDim<2>
{
public:
    XiSequence(unsigned n = 1);

    void sample(float[], unsigned i) override;

    std::string name() const override
    {
        return "Xi (0,m,2)-sequence";
    }

    int numSamples() const override
    {
        return m_numSamples;
    }
    int setNumSamples(unsigned n) override
    {
        m_numSamples = roundUpPow2(n);
        return m_numSamples;
    }

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r = true) override;

private:
    unsigned m_numSamples;
    bool     m_randomize = true;
    pcg32    m_rand;

    std::unique_ptr<class Xi> m_xi;
};
