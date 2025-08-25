/** \file OABoseBush.cpp
    \brief Implementation of orthogonal array constructions due to
           Bose and Bush (1952).
    \author Wojciech Jarosz
*/

#include <sampler/Misc.h>
#include <sampler/OABoseBush.h>
#define GF_SIZE_DEBUG
#define GF_RANGE_DEBUG
#include <galois++/element.h>

using namespace std;

BoseBushOA::BoseBushOA(unsigned x, OffsetType ot, uint32_t seed, float jitter, unsigned dimensions) :
    BoseGaloisOAInPlace(x, ot, seed, jitter, dimensions)
{
    setNumSamples(2 * x * x);
    reset();
}

string BoseBushOA::name() const { return "Bose-Bush OA"; }

int BoseBushOA::setNumSamples(unsigned n)
{
    m_s          = max(2, (int)pow(2.f, ceil(log2(sqrt(n / 2)))));
    m_numSamples = 2 * m_s * m_s;
    m_gf.resize(2 * m_s);
    reset();
    return m_numSamples;
}

void BoseBushOA::reset()
{
    BoseOAInPlace::reset();

    int      q = m_gf.q;
    unsigned s = q / 2; /* number of levels in design */

    Array2d<int> A(s, q);

    m_B.resize(2 * s * s, dimensions());
    int irow = 0;
    for (int i = 0; i < q; i++)
    {
        const Galois::Element gi(&m_gf, i);
        for (int j = 0; j < q; j++)
        {
            const Galois::Element mul((gi * j) % s);
            for (unsigned k = 0; k < s; k++) A(k, j) = (mul + k).value();
        }

        for (unsigned k = 0; k < s; k++)
        {
            for (unsigned j = 0; j < dimensions() && j < 2 * s; j++) m_B(irow, j) = A(k, j);

            if (dimensions() >= 2 * s + 1)
                m_B(irow, 2 * s) = i % s;
            irow++;
        }
    }
}

void BoseBushOA::sample(float r[], unsigned row)
{
    int      q = m_gf.q;
    unsigned s = q / 2; /* number of levels in design */

    for (unsigned dim = 0; dim < dimensions() && dim < 2 * s + 1; ++dim)
    {
        int Acol     = m_B(row, dim);
        int stratumJ = permute(Acol, m_s, m_seed * (dim + 1));

        float jitterJ = 0.5f + int(m_seed != 0) * m_maxJit * (m_rand.nextFloat() - 0.5f);
        r[dim]        = (stratumJ + jitterJ) / m_s;
    }
}

////

BoseBushOAInPlace::BoseBushOAInPlace(unsigned x, OffsetType ot, uint32_t seed, float jitter, unsigned dimensions) :
    BoseGaloisOAInPlace(x, ot, seed, jitter, dimensions)
{
    setNumSamples(2 * x * x);
    reset();
}

string BoseBushOAInPlace::name() const { return "Bose-Bush OA In-Place"; }

int BoseBushOAInPlace::setNumSamples(unsigned n)
{
    m_s          = max(2, (int)pow(2.f, ceil(log2(sqrt(n / 2)))));
    m_numSamples = 2 * m_s * m_s;
    m_gf.resize(2 * m_s);
    reset();
    return m_numSamples;
}

void BoseBushOAInPlace::sample(float r[], unsigned row)
{
    int      q = m_gf.q;
    unsigned s = q / 2; /* number of levels in design */

    unsigned              i = row / s;
    const Galois::Element gi(&m_gf, i);

    for (unsigned dim = 0; dim < dimensions() && dim < 2 * s + 1; ++dim)
    {
        int A = (dim < 2 * s) ? (((gi * dim) % s) + (row % s)).value() : i % s;

        int stratumJ = permute(A, m_s, m_seed * (dim + 1));

        float jitterJ = 0.5f + int(m_seed != 0) * m_maxJit * (m_rand.nextFloat() - 0.5f);
        r[dim]        = (stratumJ + jitterJ) / m_s;
    }
}