/** \file NRooks.h
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/Sampler.h>
#include <pcg32.h>

/// A wrapper for a pseudo-random number generator.
class Random : public TSamplerMinMaxDim<1,(unsigned)-1>
{
public:
    Random(unsigned dimensions = 2, uint64_t seed = PCG32_DEFAULT_STATE);
    ~Random() override;

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override     {return m_numDimensions;}
    void setDimensions(unsigned) override;

    std::string name() const override              {return "Random";}

protected:
    unsigned m_numDimensions;
    pcg32 m_rand;
};
