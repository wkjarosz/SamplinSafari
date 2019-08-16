/*!
 * \file SamplerSOA.h
 * \author Afnan Enayet
 * \brief Strong orthogonal array point sets
 *
 * This module generates strong orthogonal arrays from regular orthogonal
 * arrays.
 */

#pragma once

#include <vector>

#include <sampler/Sampler.h>
#include <pcg32.h>

/*!
 * \brief A strong orthogonal array sampler class
 *
 * The strong orthogonal array uses the construction techniques used in the
 * regular orthogonal array classes (found in SamplerOrthogonalArray.[ch]),
 * and converts them to ordered orthogonal arrays, then converting them into
 * strong orthogonal arrays.
 *
 * Currently, SOAs up to strength 5 are supported.
 */
class StrongOrthogonalArray : public TSamplerMinMaxDim<3, 5>
{
public:
    StrongOrthogonalArray(unsigned, unsigned strength, bool randomize = false,
                          float jitter = 0.0f);
    ~StrongOrthogonalArray() override;
    void clear();

    void reset() override;
    void sample(float point[], unsigned i) override;

    unsigned dimensions() const override     {return m_numDimensions;}
    void setDimensions(unsigned d) override  {m_numDimensions = d; reset();}

    bool randomized() const override         {return m_randomize;}
    void setRandomized(bool r) override      {m_randomize = r; reset();}

    std::string name() const override;

    int numSamples() const override          {return m_numSamples;}

    /*!
     * \brief Set the number of samples to generate points for
     *
     * This method will find the smallest prime greater than or equal to `n`,
     * and then set the number of samples equal to n raised to the desired 
     * strength.
     *
     * \param n The base for the number of samples desired, where the number
     * of samples is $ n ^ {strength} $
     */
    int setNumSamples(unsigned n) override;
    void setNumSamples(unsigned x, unsigned);

private:
    /*!
     * \brief Construct an OA using the Bush construction
     *
     * This constructs an orthogonal array for the Bush construction technique
     * as described by Art Owen, using the relevant parameters set for the SOA.
     * This OA will not be randomized or converted back to the canonical point
     * set.
     * 
     * \param numDimensions The number of dimensions required for the new
     * orthogonal array
     * \returns An orthogonal array
     */
    std::vector<std::vector<int>> bushOA(unsigned numDimensions);
    unsigned m_resX, m_numSamples, m_t, m_numDimensions;
    bool m_randomize;
    std::vector<std::vector<float>> m_pts;
    float m_maxJit;
};
