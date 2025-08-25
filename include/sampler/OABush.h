/** \file OABush.h
    \author Wojciech Jarosz
*/
#pragma once

#include <galois++/field.h>
#include <sampler/OABose.h>

/// Produces OA samples based on the construction by Bush (1952).
/**
    This version uses modulo arithmetic, so the OAs are of the form:
    \f$ OA(p^t, k, p, t), \f$ where \f$ k \le p \f$, \f$ t \ge 3 \f$, and
    \f$ p \f$ is a prime.

    Bush's original designs allowed for one more dimension \f$( p+1 )\f$, but we
    sacrifice this dimension so that OffsetType == CMJ_STYLE and MJ_STYLE produce
    "sliced" a.k.a "nested" designs where each of the \f$ p \f$ consecutive sets
    of \f$ p^{t-1} \f$ points forms an \f$ OA(p^{t-1}, k-1, p, t-1) \f$. When
    OffsetType == CMJ_STYLE, we sacrifice yet another dimension to achieve this,
    so the designs then have \f$ k \le p-1 \f$. CMJ_STYLE currently only works
    properly for \f$ t=3 \f$.

    This version constructs the samples "in-place", one sample at a time.

    > K.A. Bush (1952) Annals of Mathematical Statistics, Vol 23 pp 426-434.
 */
class BushOAInPlace : public BoseOAInPlace
{
public:
    BushOAInPlace(unsigned n, unsigned strength, OffsetType ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f,
                  unsigned dimensions = 2);
    ~BushOAInPlace() override {}

    unsigned setStrength(unsigned t) override { return OrthogonalArray::setStrength(t); }

    int coarseGridRes(int samples) const override { return int(std::pow(samples, 1.f / m_t)); }

    void sample(float[], unsigned i) override;

    std::string name() const override;

    int  setNumSamples(unsigned n) override;
    void setNumSamples(unsigned x, unsigned y) override;
};

/// Produces OA samples based on the construction by Bush (1952).
/**
    This version uses Galois Field arithmetic, so the OAs are of the form:
    \f$ OA(q^t, k, q, t), \f$ where \f$ k \le q \f$, \f$ t \ge 3 \f$, and
    prime power \f$ q \f$.

    Bush's original designs allowed for one more dimension \f$( q+1 )\f$, but we
    sacrifice this dimension so that OffsetType == CMJ_STYLE and MJ_STYLE produce
    "sliced" a.k.a "nested" designs where each of the \f$ q \f$ consecutive sets
    of \f$ q^{t-1} \f$ points forms an \f$ OA(q^{t-1}, k-1, q, t-1) \f$. When
    OffsetType == CMJ_STYLE, we sacrifice yet another dimension to achieve this,
    so the designs then have \f$ k \le q-1 \f$. CMJ_STYLE currently only works
    properly for \f$ t=3 \f$.

    This version constructs the samples "in-place", one sample at a time.

    > K.A. Bush (1952) Annals of Mathematical Statistics, Vol 23 pp 426-434.
 */
class BushGaloisOAInPlace : public BushOAInPlace
{
public:
    BushGaloisOAInPlace(unsigned n, unsigned strength, OffsetType ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f,
                        unsigned dimensions = 2);
    ~BushGaloisOAInPlace() override {}

    void sample(float[], unsigned i) override;

    std::string name() const override;

    int  setNumSamples(unsigned n) override;
    void setNumSamples(unsigned x, unsigned y) override;

protected:
    Galois::Field m_gf;
};
