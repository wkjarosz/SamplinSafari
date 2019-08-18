/** \file OABoseBush.h
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/OABose.h>

/// Produces OA samples based on the construction by Bose and Bush (1952).
/**
    The OAs are of the form: \f$ OA(2q^2, k, q, 2) \f$,
    where \f$ k \le 2q+1 \f$, for powers of 2, \f$ q=2^r \f$.

    This version precomputes and stores all the samples.

    > R.C. Bose and K.A. Bush (1952) Annals of Mathematical Statistics,
    > Vol 23 pp 508-524.
 */
class BoseBushOA : public BoseGaloisOAInPlace
{
public:
    BoseBushOA(unsigned x, OffsetType ot = CENTERED,
                       bool randomize = false, float jitter = 0.0f,
                       unsigned dimensions = 2);
    ~BoseBushOA() override {}

    void reset() override;

    unsigned setStrength(unsigned) override { return 2; }

    int coarseGridRes(int samples) const override
    {
        return int(std::sqrt(0.5f * samples));
    }

    void sample(float[], unsigned i) override;

    std::string name() const override;
    int setNumSamples(unsigned n) override;

protected:
    Array2d<float> m_B;
};


/// Produces OA samples based on the construction by Bose and Bush (1952).
/**
    The OAs are of the form: \f$ OA(2q^2, k, q, 2) \f$,
    where \f$ k \le 2q+1 \f$, for powers of 2, \f$ q=2^r \f$.

    This version constructs the samples "in-place", one sample at a time.

    > R.C. Bose and K.A. Bush (1952) Annals of Mathematical Statistics,
    > Vol 23 pp 508-524.
 */
class BoseBushOAInPlace : public BoseGaloisOAInPlace
{
public:
    BoseBushOAInPlace(unsigned x, OffsetType ot = CENTERED,
                       bool randomize = false, float jitter = 0.0f,
                       unsigned dimensions = 2);
    ~BoseBushOAInPlace() override {}

    unsigned setStrength(unsigned) override { return 2; }

    int coarseGridRes(int samples) const override
    {
        return int(std::sqrt(0.5f * samples));
    }

    void sample(float[], unsigned i) override;

    std::string name() const override;
    int setNumSamples(unsigned n) override;
};