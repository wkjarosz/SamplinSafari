/** \file OABose.cpp
    \brief Implementation of orthogonal array constructions due to Bose (1939).
    \author Wojciech Jarosz
*/

#include <sampler/OABose.h>
#include <sampler/Misc.h>
#include <sampler/RandomPermutation.h>
#include <galois++/element.h>
#include <galois++/primes.h>
#include <iostream>

using namespace std;

// local functions
namespace
{

float boseLHOffset(int sx, int sy, int s, int p, unsigned type)
{
    switch (type)
    {
    case CENTERED:
        return s / 2.0f;
    case J_STYLE:
        return permute(0, s, (sy * s + sx + 1) * p);
    case MJ_STYLE:
        return permute(sy, s, (sx + 1) * p);
    default:
    case CMJ_STYLE:
        return permute(sy, s, p);
    }
}

} // namespace

BoseOA::BoseOA(unsigned x, OffsetType ot, bool randomize, float jitter,
               unsigned dimensions)
    : OrthogonalArray(2, ot, randomize, jitter), m_s(x),
      m_numSamples(m_s * m_s), m_numDimensions(dimensions)
{
    reset();
}

BoseOA::~BoseOA()
{
    clear();
}

string BoseOA::name() const
{
    return "Bose OA";
}

int BoseOA::setNumSamples(unsigned n)
{
    int sqrtVal = (n == 0) ? 1 : (int)(sqrt((float)n) + 0.5f);
    setNumSamples(sqrtVal, sqrtVal);
    return m_numSamples;
}

void BoseOA::setNumSamples(unsigned x, unsigned)
{
    m_s = primeGE(x);
    m_numSamples = m_s * m_s;

    reset();
}

void BoseOA::clear()
{
    if (m_samples)
        for (unsigned d = 0; d < dimensions(); d++)
            if (m_samples[d])
                delete[] m_samples[d];
    delete[] m_samples;
    m_samples = nullptr;
}

unsigned BoseOA::setOffsetType(unsigned ot)
{
    auto ret = OrthogonalArray::setOffsetType(ot);
    reset();
    return ret;
}

void BoseOA::reset()
{
    if (m_s == 0)
        m_s = 1;
    m_scale = 1.0f / m_numSamples;

    clear();

    m_samples = new unsigned*[dimensions()];
    for (unsigned d = 0; d < dimensions(); d++)
        m_samples[d] = new unsigned[m_numSamples];

    // initialize permutation arrays
    vector<RandomPermutation> perm(dimensions());
    perm[0] = RandomPermutation(m_s);
    for (unsigned d = 1; d < dimensions(); d++)
        perm[d] = RandomPermutation(m_s);

    if (m_randomize)
        for (unsigned d = 0; d < dimensions(); d++)
            perm[d].shuffle(m_rand);

    for (unsigned i = 0; i < m_numSamples; ++i)
    {
        int Ai1 = i / m_s;
        int Ai2 = i % m_s;

        switch (m_ot)
        {
        case CENTERED:
        case J_STYLE:
        {
            m_samples[0][i] = (perm[0][Ai1]) * m_s;
            m_samples[1][i] = (perm[1][Ai2]) * m_s;
            for (unsigned d = 2; d < dimensions(); ++d)
            {
                int Aid = (Ai1 + (d - 1) * Ai2) % m_s;
                m_samples[d][i] = (perm[d][Aid]) * m_s;
            }
        }
        break;

        case MJ_STYLE:
        case CMJ_STYLE:
        default:
        {
            // the multiplication by m_s creates m_s^2 thin
            // (latin-hypercube) strata along each dimension Adding Ai* offsets
            // the samples within a thick stratum to form latin-hypercube
            // samples
            // For dimension D, it seems that adding any Ai* where *!= D would
            // work
            m_samples[0][i] = perm[0][Ai1] * m_s + Ai2;
            m_samples[1][i] = perm[1][Ai2] * m_s + Ai1;
            for (unsigned d = 2; d < dimensions(); ++d)
            {
                int Aid = (Ai1 + (d - 1) * Ai2) % m_s;
                m_samples[d][i] = perm[d][Aid] * m_s + Ai2;
            }
        }
        break;
        }
    }

    // printf("The canonical OA with %d points is:\n", m_numSamples);
    // // print out the OA
    // for (unsigned i = 0; i < m_numSamples; ++i)
    // {
    //     for (unsigned d = 0; d < dimensions(); ++d)
    //         printf("%5d ", m_samples[d][i]);
    //     printf("\n");
    // }
    // printf("\n");

    // if we do (not-correlated) multi-jittered style offsets, then do true
    // shuffles
    if (m_randomize && m_ot == MJ_STYLE)
    {
        // X and Y are easy since we can directly enumerate the points within
        // the x or y strata,

        // shuffle y coordinates within each row of cells
        for (unsigned i = 0; i < m_s; i++)
            for (unsigned j = m_s - 1; j >= 1; j--)
            {
                unsigned k = m_rand.nextUInt(j);
                swap(m_samples[1][j * m_s + i],
                     m_samples[1][k * m_s + i]);
            }

        // shuffle x coordinates within each column of cells
        for (unsigned j = 0; j < m_s; j++)
            for (unsigned i = m_s - 1; i >= 1; i--)
            {
                unsigned k = m_rand.nextUInt(i);
                swap(m_samples[0][j * m_s + i],
                     m_samples[0][j * m_s + k]);
            }

        // shuffle the strata in each remaining dimension d
        // remaining dimensions are tougher since we cannot directly enumerate,
        // and therefore need exhaustively search for points in each d-stratum
        for (unsigned d = 2; d < dimensions(); ++d)
        {
            vector<vector<int>> pointsInStratum;
            pointsInStratum.resize(m_s);

            // determine mapping from strata to all points falling in that
            // stratum
            for (unsigned i = 0; i < m_numSamples; i++)
            {
                int x = m_samples[d][i] / m_s;
                pointsInStratum[x].push_back(i);
            }

            // shuffle the d-coordinates within each stratum s
            for (unsigned s = 0; s < m_s; s++)
            {
                for (unsigned i = pointsInStratum[s].size() - 1; i >= 1; i--)
                    swap(m_samples[d][pointsInStratum[s][i]],
                         m_samples[d][pointsInStratum[s][m_rand.nextUInt(i)]]);
            }
        }
    }
}

void BoseOA::sample(float r[], unsigned i)
{
    if (i >= m_numSamples)
        i = 0;

    if (i == 0)
        m_rand.seed(m_seed);

    float jitter = m_randomize * m_maxJit;
    for (unsigned d = 0; d < dimensions(); d++)
    {
        switch (m_ot)
        {
        case CENTERED:
        case J_STYLE:
            r[d] = (m_samples[d][i] / m_s +
                    (0.5f + jitter * (m_rand.nextFloat() - 0.5f))) /
                   m_s;
            break;

        case MJ_STYLE:
        case CMJ_STYLE:
        default:
            r[d] = (m_samples[d][i] +
                    (0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f))) *
                   m_scale;
            break;
        }
    }
}

BoseOAInPlace::BoseOAInPlace(unsigned x, OffsetType ot, bool randomize,
                             float jitter, unsigned dimensions)
    : OrthogonalArray(2, ot, randomize, jitter), m_s(x),
      m_numSamples(m_s * m_s), m_numDimensions(dimensions)
{
    reset();
}

string BoseOAInPlace::name() const
{
    return "Bose OA In-Place";
}

int BoseOAInPlace::setNumSamples(unsigned n)
{
    int sqrtVal = (n == 0) ? 1 : (int)(sqrt((float)n) + 0.5f);
    setNumSamples(sqrtVal, sqrtVal);
    return m_numSamples;
}

void BoseOAInPlace::setNumSamples(unsigned x, unsigned)
{
    m_s = primeGE(x);
    m_numSamples = m_s * m_s;
    reset();
}

void BoseOAInPlace::reset()
{
    m_rand.seed(m_seed);
}

void BoseOAInPlace::sample(float r[], unsigned i)
{
    if (i == 0)
        m_rand.seed(m_seed);

    unsigned maxDim = min(dimensions(), m_s + 1);

    int stratumX = i / m_s;
    int stratumY = i % m_s;
    int Ai0 = permute(stratumX, m_s, m_seed * 1);
    int Ai1 = permute(stratumY, m_s, m_seed * 2);
    float sstratX = boseLHOffset(Ai0, Ai1, m_s, m_seed * 1 * 0x68bc21eb, m_ot);
    float sstratY = boseLHOffset(Ai1, Ai0, m_s, m_seed * 2 * 0x68bc21eb, m_ot);
    float jitterX = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
    float jitterY = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
    r[0] = (stratumX + (sstratX + jitterX) / m_s) / m_s;
    r[1] = (stratumY + (sstratY + jitterY) / m_s) / m_s;

    for (unsigned j = 2; j < maxDim; ++j)
    {
        int Aij = (Ai0 + (j - 1) * Ai1) % m_s;
        int k = (j % 2) ? j - 1 : j + 1;
        int Aik = (Ai0 + (k - 1) * Ai1) % m_s;
        int stratumJ = permute(Aij, m_s, m_seed * (j+1));
        float sstratJ = boseLHOffset(Aij, Aik, m_s, m_seed * (j+1) * 0x68bc21eb, m_ot);
        float jitterJ = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
        r[j] = (stratumJ + (sstratJ + jitterJ) / m_s) / m_s;
    }

    for (unsigned j = maxDim; j < dimensions(); ++j)
        r[j] = 0.5f;
}

//
//

BoseGaloisOAInPlace::BoseGaloisOAInPlace(unsigned x, OffsetType ot,
                                         bool randomize, float jitter,
                                         unsigned dimensions)
    : BoseOAInPlace(x, ot, randomize, jitter, dimensions)
{
    setNumSamples(x * x);
    reset();
}

string BoseGaloisOAInPlace::name() const
{
    return "Bose-Galois OA In-Place";
}

int BoseGaloisOAInPlace::setNumSamples(unsigned n)
{
    int base = (int)round(sqrt(n));
    base = (base < 2) ? 2 : base;
    m_s = primePowerGE(base);
    m_numSamples = m_s * m_s;
    m_gf.resize(m_s);
    reset();
    return m_numSamples;
}

void BoseGaloisOAInPlace::sample(float r[], unsigned i)
{
    if (i == 0)
        m_rand.seed(m_seed);

    int stratumX = i / m_s;
    int stratumY = i % m_s;
    Galois::Element Ai0(&m_gf, permute(stratumX, m_s, m_seed * 1));
    Galois::Element Ai1(&m_gf, permute(stratumY, m_s, m_seed * 2));
    float sstratX = boseLHOffset(Ai0.value(), Ai1.value(), m_s, m_seed * 1 * 0x68bc21eb, m_ot);
    float sstratY = boseLHOffset(Ai1.value(), Ai0.value(), m_s, m_seed * 2 * 0x68bc21eb, m_ot);
    float jitterX = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
    float jitterY = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
    r[0] = (stratumX + (sstratX + jitterX) / m_s) / m_s;
    r[1] = (stratumY + (sstratY + jitterY) / m_s) / m_s;

    unsigned maxDim = min(dimensions(), m_s + 1);
    for (unsigned j = 2; j < maxDim; ++j)
    {
        int km1 = (j % 2) ? j - 2 : j % m_s;
        int Aij = (Ai0 + (j - 1) * Ai1).value();
        int Aik = (Ai0 + km1*Ai1).value();
        int stratumJ = permute(Aij, m_s, m_seed * (j+1));
        float sstratJ = boseLHOffset(Aij, Aik, m_s, m_seed * (j+1) * 0x68bc21eb, m_ot);
        float jitterJ = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
        r[j] = (stratumJ + (sstratJ + jitterJ) / m_s) / m_s;
    }

    for (unsigned j = maxDim; j < dimensions(); ++j)
        r[j] = 0.5f;
}

