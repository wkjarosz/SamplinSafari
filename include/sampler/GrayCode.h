/** \file GrayCode.h
    \author Wojciech Jarosz
*/
#pragma once

#include <pcg32.h>
#include <sampler/Sampler.h>
#include <vector>

/**
    A (0,m,2) dyadic net that use Gray-code-ordered van der Corput sequences as described in:

        Ahmed and Wonka: "Optimizing Dyadic Nets".

    Based on the `gray-code-nets.cpp` code by Ahmed, which contains no license.
*/
class GrayCode : public TSamplerDim<2>
{
public:
    GrayCode(unsigned n = 2);

    void sample(float[], unsigned i) override;

    std::string name() const override { return "Gray code nets"; }

    int numSamples() const override { return N; }
    int setNumSamples(unsigned num) override;

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override
    {
        m_seed = seed;
        regenerate();
    }

private:
    void regenerate();

    struct Point
    {
        double x, y;
    };

    std::vector<Point> m_samples;

    unsigned N;
    unsigned n, log2n;
    uint32_t m_seed = 0;
    pcg32    m_rand;
};
