/*! \file SamplerOA.h
    \brief
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/Sampler.h>
#include <vector>
#include <sampler/OACommon.h>

enum OffsetType : unsigned
{
    CENTERED = 0,
    J_STYLE,
    MJ_STYLE,
    CMJ_STYLE,
    NUM_OFFSET_TYPES
};

/**
   \brief Base class for all Orthogonal Array-based samplers, providing member
   variables and functions for strength, randomization, sub-strata jitter, and
   style of substrata offsets.
 */
class OrthogonalArray : public TSamplerMinMaxDim<2, (unsigned)-1>
{
public:
    OrthogonalArray(unsigned t = 2, unsigned ot = CENTERED,
                    bool randomize = false, float jitter = 0.0f)
        : m_t(t), m_ot(ot), m_randomize(randomize),
          m_maxJit(jitter)
    {
    }

    ~OrthogonalArray() override {}

    virtual unsigned strength() const { return m_t; }
    virtual unsigned setStrength(unsigned t)
    {
        return m_t = (t > 1) ? t : m_t;
    }

    virtual unsigned offsetType() const { return m_ot; }
    virtual unsigned setOffsetType(unsigned ot);
    virtual std::vector<std::string> offsetTypeNames() const;

    bool randomized() const override { return m_randomize; }
    void setRandomized(bool r) override
    {
        if ((m_randomize = r))
            m_seed++;
        else
            m_seed = 0;
    }

    float jitter() const override { return m_maxJit; }
    float setJitter(float j = 1.0f) override
    {
        m_maxJit = j;
        reset();
        return m_maxJit;
    }

    std::string name() const override { return "Abstract Orthogonal Array"; }

protected:
    unsigned m_t;       ///< strength
    unsigned m_ot;      ///< offset type
    bool m_randomize;
    unsigned m_seed;    ///< randomization/permutation seed
    float m_maxJit;     ///< amount to jitter within each substratum
};