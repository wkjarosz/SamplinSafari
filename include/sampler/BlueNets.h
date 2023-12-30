/** \file BlueNets.h
    \author Wojciech Jarosz
*/
#pragma once

#include <memory>
#include <pcg32.h>
#include <sampler/Sampler.h>
#include <vector>

/**
    Generates optimized (0, m, 2)-nets, as described in the paper:

        Ahmed and Wonka: "Optimizing Dyadic Nets"

    Based on the `net-optimize-pointers.cpp` code by Ahmed, which contains no license.
*/
class BlueNets : public TSamplerDim<2>
{
public:
    BlueNets(unsigned n = 2);

    void sample(float[], unsigned i) override;

    std::string name() const override
    {
        return "Blue nets";
    }

    int numSamples() const override
    {
        return pointCount;
    }
    int setNumSamples(unsigned num) override;

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r = true) override;

private:
    void regenerate();

    std::vector<float> xs, ys;

    bool        m_randomize          = true;
    int         iterations           = 1;
    double      sigma                = 0.5;
    std::string optimizationSequence = "v";
    double      rf                   = 0.75;
    int         range                = 0;
    uint32_t    pointCount           = 1;

    pcg32 m_rand;
};
