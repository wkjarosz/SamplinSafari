/*!
   \file SOA.cpp
   \author Afnan Enayet
 */

#include <sampler/SOA.h>
#include <sampler/Misc.h>
#include <galois++/primes.h>

StrongOrthogonalArray::StrongOrthogonalArray(unsigned, unsigned strength,
                                             bool randomize, float jitter)
{
    m_t = strength;
    m_randomize = randomize;
    m_maxJit = jitter;
}

StrongOrthogonalArray::~StrongOrthogonalArray() { }

void StrongOrthogonalArray::clear()
{
    m_pts.clear();
}

void StrongOrthogonalArray::reset()
{
    // set the number of dimensions to have a max of p + 1
    m_numDimensions = std::min(m_numDimensions, m_resX + 1);

    // initialize m_pts for the number of samples and dimensions
    m_pts.resize(m_numSamples);

    for (auto &point : m_pts) {
        point = std::vector<float>();
        point.resize(m_numDimensions);
    }

    // first, we need to determine the new m' value
    // this equation is given by He and Tang (2012)
    // we derive it by solving for m, instead of m'
    // our m' is the number of dimensions, and m is
    // the dimensionality of the original orthogonal array
    unsigned m_prime = m_numDimensions;

    // m gives us the dimensionality of the OA we need to generate
    unsigned m;

    if (m_t % 2 == 0)
        m = unsigned(std::floor(m_t * m_prime / 2));
    else
        m = unsigned(std::floor((m_prime * m_t - 2) / 2));

    auto oa = bushOA(m);

    // construct the column selection vector that will generate the ordered
    // orthogonal array
    // this column will yield which column of the original OA corresponds to
    // the same column in the ordered orthogonal array
    std::vector<int> ooaCols(m_prime * m_t);

    // TODO set up the column selection vector for each strength, using He and Tang's method
    switch(m_t) {
        case 2:
            // determine
            break;
        case 3:
            break;
        case 4:
            break;
        case 5:
            break;
    }
}

void StrongOrthogonalArray::sample(float point[], unsigned i)
{
    if (i > m_numSamples)
        i = 0;

    for (unsigned d = 0; d < m_numDimensions; d++) {
        point[d] = m_pts[i][d];
    }
}

std::string StrongOrthogonalArray::name() const
{
    return "Strong Orthogonal Array";
}

int StrongOrthogonalArray::setNumSamples(unsigned n)
{
    // We want to set the number of samples closest to some power of a prime
    int rtVal = (n == 0) ? 1 : (int) (std::pow((float) n, 1.f/m_t) + 0.5f);
    setNumSamples(rtVal, rtVal);
    return m_numSamples;
}

void StrongOrthogonalArray::setNumSamples(unsigned x, unsigned)
{
    m_resX = primeGE(x);
    m_numSamples = pow(m_resX, m_t);
    reset();
}

std::vector<std::vector<int>> StrongOrthogonalArray::bushOA(unsigned numDimensions) {
    std::vector<std::vector<int>> oa;
    oa.resize(m_numSamples);

    for (unsigned i = 0; i < m_numSamples; i++) {
        oa[i].resize(numDimensions);
        auto coeffs = iToPolyCoeffs(i, m_resX, m_t);

        // The Bush construction technique uses a particular construction for
        // columns 0...p, and another procedure for p + 1, so that will be
        // handled after the loop, if necessary
        auto upperDim = std::min(numDimensions, m_resX);

        for (unsigned j = 0; j < upperDim; j++) {
            oa[i][j] = polyEval(coeffs, j) % m_resX;
        }

        if (numDimensions >= m_resX + 1)
            oa[i][m_resX] = i % m_resX;
    }
    return oa;
}
