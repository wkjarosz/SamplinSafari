/*! \file OABush.h
    \brief
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/OABose.h>
#include <galois++/field.h>

/**
   Produces OA samples based on the construction by
  
        K.A. Bush (1952) Annals of Mathematical Statistics, Vol 23 pp 426-434.
  
   The OAs are of the form:
  
        OA(p^t, k, p, t),
  
   where k <= p, t >= 3, and p is a prime.
  
   Bush's original designs allowed for one more dimension (p+1), but we
   sacrifice this dimension so that OffsetType == CMJ_STYLE and MJ_STYLE produce
   "sliced" a.k.a "nested" designs where each of the p consecutive sets
   of p^(t-1) points forms an OA(p^(t-1), k-1, p, t-1). When OffsetType ==
   CMJ_STYLE, we sacrifice yet another dimension to achieve this, so the designs
   then have k <= p-1. CMJ_STYLE currently only works properly for t=3.
  
   This version constructs the samples "in-place", one sample at a time.
 */
class BushOAInPlace : public BoseOAInPlace
{
public:
    BushOAInPlace(unsigned n, unsigned strength, OffsetType ot = CENTERED,
                  bool randomize = false, float jitter = 0.0f,
                  unsigned dimensions = 2);
    ~BushOAInPlace() override {}

    unsigned setStrength(unsigned t) override
    {
        return OrthogonalArray::setStrength(t);
    }

    int coarseGridRes(int samples) const override
    {
        return int(std::pow(samples, 1.f / m_t));
    }

    void sample(float[], unsigned i) override;

    std::string name() const override;

    int setNumSamples(unsigned n) override;
    void setNumSamples(unsigned x, unsigned y) override;
};


/**
   Produces OA samples based on the construction by
  
        K.A. Bush (1952) Annals of Mathematical Statistics, Vol 23 pp 426-434.
  
   The OAs are of the form:
  
        OA(q^t, k, q, t),
  
   where k <= q, t >= 3, and prime power q.
  
   Bush's original designs allowed for one more dimension (q+1), but we
   sacrifice this dimension so that OffsetType == CMJ_STYLE and MJ_STYLE produce
   "sliced" a.k.a "nested" designs where each of the q consecutive sets
   of q^(t-1) points forms an OA(q^(t-1), k-1, q, t-1). When OffsetType ==
   CMJ_STYLE, we sacrifice yet another dimension to achieve this, so the designs
   then have k <= q-1. CMJ_STYLE currently only works properly for t=3.

   This version constructs the samples "in-place", one sample at a time.
 */
class BushGaloisOAInPlace : public BushOAInPlace
{
public:
    BushGaloisOAInPlace(unsigned n, unsigned strength, OffsetType ot = CENTERED,
                  bool randomize = false, float jitter = 0.0f,
                  unsigned dimensions = 2);
    ~BushGaloisOAInPlace() override {}

    void sample(float[], unsigned i) override;

    std::string name() const override;

    int setNumSamples(unsigned n) override;
    void setNumSamples(unsigned x, unsigned y) override;

protected:
    Galois::Field m_gf;
};
