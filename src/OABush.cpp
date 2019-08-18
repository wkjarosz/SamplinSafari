/** \file OABush.cpp
    \brief Implementation of orthogonal array constructions due to Bush (1952).
    \author Wojciech Jarosz
*/

#include <sampler/OABush.h>
#include <sampler/Misc.h>
#include <galois++/element.h>
#include <galois++/primes.h>

using namespace std;

// local functions
namespace
{

float bushLHOffset(int i, int N, int s, int numSS, int p, unsigned type)
{
    switch (type)
    {
    case CENTERED:
        return numSS / 2.0f;
    case J_STYLE:
        return permute((i / s) % numSS, numSS, (i + 1) * p);
    case MJ_STYLE:
        return permute((i / s) % numSS, numSS, p);

    // the following is still a work in progress, seems to work for strength 3,
    // but not others.
    default:
    case CMJ_STYLE:
        return (permute((i / s) % s, s, p) + permute(i % s, s, p * 2) * (numSS / s)) % numSS;
    }
}

} // namespace


BushOAInPlace::BushOAInPlace(unsigned x, unsigned strength, OffsetType ot,
                             bool randomize, float jitter, unsigned dimensions)
    : BoseOAInPlace(x, ot, randomize, jitter, dimensions)
{
    m_t = strength;
}

string BushOAInPlace::name() const
{
    return "Bush OA In-Place";
}

int BushOAInPlace::setNumSamples(unsigned n)
{
    int rtVal = (n == 0) ? 1 : (int)(pow((float)n, 1.f / m_t) + 0.5f);
    setNumSamples(rtVal, rtVal);
    return m_numSamples;
}

void BushOAInPlace::setNumSamples(unsigned x, unsigned)
{
    m_s = primeGE(x);
    m_numSamples = pow(m_s, m_t);
    reset();
}

void BushOAInPlace::sample(float r[], unsigned i)
{
    if (i == 0)
        m_rand.seed(m_seed);

    // compute polynomial coefficients
    auto coeffs = iToPolyCoeffs(i, m_s, m_t);

    unsigned numSubStrata = m_numSamples / m_s;
    unsigned add = m_ot == CMJ_STYLE ? 1 : 0;
    unsigned maxDim = min(dimensions(), m_s - add);
    unsigned s = m_s;
    for (unsigned d = 0; d < maxDim; ++d)
    {
        int phi = polyEval(coeffs, d + add);
        int stratum = permute(phi % m_s, m_s, m_seed * (d+1));

        float subStratum = bushLHOffset(i, m_numSamples, m_s, numSubStrata, m_seed * (d+1) * 0x02e5be93, m_ot);

        float jitter = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
        r[d] = (stratum + (subStratum + jitter) / numSubStrata) / s;
    }

    for (unsigned d = maxDim; d < dimensions(); ++d)
        r[d] = 0.5f;
}


////



BushGaloisOAInPlace::BushGaloisOAInPlace(unsigned x, unsigned strength, OffsetType ot,
                             bool randomize, float jitter, unsigned dimensions)
    : BushOAInPlace(x, strength, ot, randomize, jitter, dimensions)
{
    setNumSamples(x);
    reset();
}

string BushGaloisOAInPlace::name() const
{
    return "Bush-Galois OA In-Place";
}

int BushGaloisOAInPlace::setNumSamples(unsigned n)
{
    int rtVal = (n == 0) ? 1 : (int)(pow((float)n, 1.f / m_t) + 0.5f);
    setNumSamples(rtVal, rtVal);
    return m_numSamples;
}

void BushGaloisOAInPlace::setNumSamples(unsigned x, unsigned)
{
    m_s = primePowerGE(x);
    m_numSamples = pow(m_s, m_t);
    m_gf.resize(m_s);
    reset();
}

void BushGaloisOAInPlace::sample(float r[], unsigned i)
{
    if (i == 0)
        m_rand.seed(m_seed);

    // compute polynomial coefficients
    auto coeffs = iToPolyCoeffs(i, m_s, m_t);

    unsigned numSubStrata = m_numSamples / m_s;
    unsigned add = m_ot == CMJ_STYLE ? 1 : 0;
    unsigned maxDim = min(dimensions(), m_s - add);
    unsigned s = m_s;
    for (unsigned d = 0; d < maxDim; ++d)
    {
        int phi = polyEval(&m_gf, coeffs, d + add);
        int stratum = permute(phi, m_s, m_seed * (d+1));

        float subStratum = bushLHOffset(i, m_numSamples, m_s, numSubStrata, m_seed * (d+1) * 0x02e5be93, m_ot);

        float jitter = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
        r[d] = (stratum + (subStratum + jitter) / numSubStrata) / s;
    }

    for (unsigned d = maxDim; d < dimensions(); ++d)
        r[d] = 0.5f;
}