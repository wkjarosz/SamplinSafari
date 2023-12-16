/** \file GrayCode.h
    \author Wojciech Jarosz
*/
#pragma once

#include <memory>
#include <pcg32.h>
#include <sampler/Sampler.h>

/**
    A (0,m,2) dyadic net that use Gray-code-ordered van der Corput sequences as described in:

        Ahmed and Wonka: "Optimizing Dyadic Nets".

    Based on the `gray-code-nets.cpp` code by Ahmed, which contains no license.
*/
class GrayCode : public TSamplerDim<2>
{
public:
    GrayCode(unsigned n = 3);

    void sample(float[], unsigned i) override;

    std::string name() const override
    {
        return "Gray code nets";
    }

    int numSamples() const override
    {
        return N;
    }
    int setNumSamples(unsigned num) override
    {
        int log2N = std::round(std::log2(num));
        if (log2N & 1)
            log2N++;        // Only allow even powers of 2.
        N     = 1 << log2N; // Make N a power of 4, if not.
        log2n = log2N / 2;
        n     = 1 << log2n;
        regenerate();
        return N;
    }

    bool randomized() const override
    {
        return m_randomize;
    }
    void setRandomized(bool r = true) override;

private:
    void regenerate();

    struct Point
    {
        double x, y;
    };

    std::vector<Point> m_samples;

    unsigned N;
    unsigned n, log2n;
    bool     m_randomize = true;
    pcg32    m_rand;
};
