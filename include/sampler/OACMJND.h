/*! \file OACMJND.h
    \brief
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/OA.h>
#include <pcg32.h>


/// Produces full factorial OA samples based on the construction by Jarosz et al. (2019)
/**
    The OAs are of the form: \f$ OA(s^t, t, s, t) \f$, where \f$ t \ge 2 \f$, and
    \f$ s \f$ is any positive integer.
  
    This version constructs the samples "in-place", one sample at a time.

    > Jarosz et al. 2019. Orthogonal Array Sampling for Monte Carlo Rendering.
 */
class CMJNDInPlace : public OrthogonalArray
{
public:
    CMJNDInPlace(unsigned n, unsigned dimensions = 2, OffsetType ot = CENTERED,
                 bool randomize = true, float jitter = 0.0);
    ~CMJNDInPlace() override;

    void sample(float point[], unsigned i) override;

    /// Resets the permutation seeds
    void reset() override;

    std::string name() const override;

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override;

    int coarseGridRes(int samples) const override;

    unsigned dimensions() const override { return m_numDimensions; }
    void setDimensions(unsigned d) override
    {
        m_numDimensions = d;
        m_t = d;
        reset();
    }

    unsigned setStrength(unsigned t) override
    {
        OrthogonalArray::setStrength(t);
        setDimensions(m_t);
        return m_t;
    }

protected:
    /// The base to use for sampling (otherwise known as `N`)
    unsigned m_base;
    pcg32 m_rand;

    /// Permutation seed for the strata
    unsigned m_strataPermute;

    /// The total sample count being generated
    unsigned m_numSamples;

    /// The number of dimensions to generate
    unsigned m_numDimensions;
};
