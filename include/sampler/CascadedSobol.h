/** \file CascadedSobol.h
    \author Wojciech Jarosz
*/
#pragma once

#include <memory>
#include <pcg32.h> // for pcg32
#include <sampler/Sampler.h>
#include <string>
#include <vector>

/**
    Implements Cascaded Sobol samples from the paper:

        Cascaded Sobol' Sampling, Loïs Paulin, David Coeurjolly, Jean-Claude Iehl, Nicolas Bonneel, Alexander Keller,
        Victor Ostromoukhov, ACM Transactions on Graphics (Proceedings of SIGGRAPH Asia), 40(6), pp. 274:1–274:13,
        December 2021.
*/
class CascadedSobol : public TSamplerMinMaxDim<1, 10>
{
public:
    CascadedSobol(const std::string &data_file, unsigned dimensions = 2, unsigned numSamples = 1);

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned n) override;

    uint32_t seed() const override { return owen_permut_flag; }
    void     setSeed(uint32_t seed = 0) override;

    std::string name() const override { return "Cascaded Sobol"; }

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override;

protected:
    unsigned m_numSamples;
    unsigned m_numDimensions;

    uint32_t              owen_permut_flag = 0;
    pcg32                 m_rand;
    std::vector<uint32_t> realSeeds;
    uint32_t              nbits;

    // Sobol matrices data
    std::unique_ptr<class SobolGenerator1D[]> sobols; // array of sobol data per dim
    std::vector<uint32_t>                     d;
    std::vector<uint32_t>                     s;
    std::vector<uint32_t>                     a;
    std::vector<std::vector<uint32_t>>        m;

    std::vector<double> m_samples;
};
