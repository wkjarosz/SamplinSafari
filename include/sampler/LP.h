/** \file LP.h
    \author Wojciech Jarosz
*/
#pragma once

#include <pcg32.h>           // for pcg32
#include <sampler/Sampler.h> // for TSamplerMinMaxDim
#include <string>            // for basic_string, string

/// The Larcher-Pillichshammer Gruenschloss-Keller (0,3) sequence
/**
    This is a (0,3) sequence in base 2. The first two dimensions are
    the Larcher-Pillichshammer (0,2) sequence:

    G. Larcher and F. Pillichshammer.
    "Walsh series analysis of the L2-discrepancy of symmetrisized point sets."
    Monatsheft Mathematik 132, 1–18, 2001.

    and the third dimension completes the (0,3) sequence using the construction
    by Gruenschloss & Keller from:

    L. Gruenschloss and A. Keller.
    "(t, m, s)-Nets and Maximized Minimum Distance, Part II",
    in P. L'Ecuyer and A. Owen (eds.),
    Monte Carlo and Quasi-Monte Carlo Methods 2008, Springer-Verlag, 2009.
*/
class LarcherPillichshammerGK : public TSamplerMinMaxDim<1, 3>
{
public:
    LarcherPillichshammerGK(unsigned dimensions = 2, unsigned numSamples = 64, bool randomize = false);
    ~LarcherPillichshammerGK() override;

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override
    {
        return m_numDimensions;
    }

    std::string name() const override
    {
        return "LP-GK";
    }

    int numSamples() const override
    {
        return m_numSamples;
    }
    int setNumSamples(unsigned n) override
    {
        m_numSamples = (n == 0) ? 1 : n;
        m_inv        = 1.0f / m_numSamples;
        return m_numSamples;
    }

    bool randomized() const override
    {
        return m_scramble1 + m_scramble2 + m_scramble3 != 0;
    }
    void setRandomized(bool b = true) override
    {
        if (!b)
            m_scramble1 = m_scramble2 = m_scramble3 = 0;
        else
        {
            m_scramble1 = m_rand.nextUInt();
            m_scramble2 = m_rand.nextUInt();
            m_scramble3 = m_rand.nextUInt();
        }
    }

protected:
    unsigned m_numSamples, m_numDimensions;
    float    m_inv;
    pcg32    m_rand;
    unsigned m_scramble1, m_scramble2, m_scramble3;
};
