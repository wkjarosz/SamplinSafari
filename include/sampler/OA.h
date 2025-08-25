/** \file OA.h
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/Sampler.h> // for TSamplerMinMaxDim
#include <string>            // for basic_string, string
#include <vector>            // for vector

enum OffsetType : unsigned
{
    CENTERED = 0,
    J_STYLE,
    MJ_STYLE,
    CMJ_STYLE,
    NUM_OFFSET_TYPES
};

/**
    Base class for all Orthogonal Array-based samplers.

    Provides member variables and functions for strength, randomization, sub-strata jitter, and style of substrata
    offsets.

    In the literature, an orthogonal array OA(N, k, n, t) denotes a (k × N) matrix whose entries are the numbers 1...n
    with the property that in any subset of t rows of the matrix, each of the n^t possible t-tuples of these numbers
    appears precisely λ times.

    These parameters are typically given the following names (with interpretation for Monte Carlo sampling in
    parenthases):

        N is the number of experimental runs (number of points),
        k is the number of factors (dimensions),
        n is the number of levels (number of strata per dimension),
        t is the strength (number of simultaneous dimensions that are stratified), and
        λ is the index (number of points per stratum).

    The definition of strength leads to the parameter relation
        N = λnt.

    In many constructions, n is a prime or a power of a prime.

    Sometimes an OA is defined transposed to the above.
 */
class OrthogonalArray : public TSamplerMinMaxDim<2, (unsigned)-1>
{
public:
    OrthogonalArray(unsigned t = 2, unsigned ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f) :
        m_t(t), m_ot(ot), m_seed(seed), m_maxJit(jitter)
    {
    }

    ~OrthogonalArray() override {}

    virtual unsigned strength() const { return m_t; }
    virtual unsigned setStrength(unsigned t) { return m_t = (t > 1) ? t : m_t; }

    virtual unsigned                 offsetType() const { return m_ot; }
    virtual unsigned                 setOffsetType(unsigned ot);
    virtual std::vector<std::string> offsetTypeNames() const;

    uint32_t seed() const override { return m_seed; }
    void     setSeed(uint32_t seed = 0) override { m_seed = seed; }

    float jitter() const override { return m_maxJit; }
    float setJitter(float j = 1.0f) override
    {
        m_maxJit = j;
        reset();
        return m_maxJit;
    }

    std::string name() const override { return "Abstract Orthogonal Array"; }

protected:
    uint32_t m_t;      ///< strength
    uint32_t m_ot;     ///< offset type
    uint32_t m_seed;   ///< randomization/permutation seed
    float    m_maxJit; ///< amount to jitter within each substratum
};