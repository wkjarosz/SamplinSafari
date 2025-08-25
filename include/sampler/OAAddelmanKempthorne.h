/**
   \file OAAddelmanKempthorne.h
   \author Wojciech Jarosz
   \date 2019-08-12

   @copyright Copyright (c) 2019
 */
#pragma once

#include <sampler/OABose.h>

/// Produces OA samples based on the construction by Addelman and Kempthorne (1961)
/**
    This version uses Galois field arithmetic, so the OAs are of the form:
    \f$ OA(2q^2, k, q, 2), \f$ where \f$ k \le 2q+1 \f$, and \f$ q \f$ is an
    odd prime power.

    This version constructs the samples "in-place", one sample at a time.

    > S. Addelman and O. Kempthorne (1961). Annals of Mathematical Statistics,
    > Vol 32 pp 1167-1176.
 */
class AddelmanKempthorneOAInPlace : public BoseGaloisOAInPlace
{
public:
    AddelmanKempthorneOAInPlace(unsigned n, OffsetType ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f,
                                unsigned dimensions = 2);
    ~AddelmanKempthorneOAInPlace() override {}

    int coarseGridRes(int samples) const override { return int(std::sqrt(0.5f * samples)); }

    void sample(float[], unsigned i) override;

    std::string name() const override;
    int         setNumSamples(unsigned n) override;
};