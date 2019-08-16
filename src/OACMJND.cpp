/*! \file OACMJND.cpp
    \brief 
    \author Wojciech Jarosz
*/

#include <sampler/OACMJND.h>
#include <sampler/Misc.h>

using namespace std;


CMJNDInPlace::CMJNDInPlace(unsigned samples, unsigned dimensions, OffsetType ot,
                           bool randomize, float jitter)
    : OrthogonalArray(dimensions, ot, randomize, jitter),
      m_numDimensions(dimensions)
{
    setNumSamples(samples);
    reset();
}

CMJNDInPlace::~CMJNDInPlace() {}

std::string CMJNDInPlace::name() const
{
    return "CMJND In-Place";
}

void CMJNDInPlace::reset()
{
    // If randomizing, populate random seeds, otherwise set all seeds to 0
    m_strataPermute = m_randomize ? m_rand.nextUInt() : 0;
}

void CMJNDInPlace::sample(float point[], unsigned i)
{
    if (i == 0)
        m_rand.seed(m_seed);

    // i = permute(i, m_numSamples, m_permutation);

    // determine which strata the element is in based off of the base `N`
    // representation of the number
    auto coeffs = iToPolyCoeffs(i, m_base, dimensions());

    int period = m_numSamples / m_base;
    for (unsigned d = 0; d < dimensions(); d++)
    {
        int stratum = permute(coeffs.at(d), m_base, m_strataPermute);
        float jitter = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);

        // Create "permuted" coefficients, excluding the current dimension
        std::vector<int> permutedCoeffs(dimensions() - 1);
        std::copy(coeffs.begin(), coeffs.begin() + d, permutedCoeffs.begin());
        std::copy(coeffs.begin() + d + 1, coeffs.end(),
                  permutedCoeffs.begin() + d);

        if (m_ot == CMJ_STYLE || m_ot == CENTERED)
        {
            //
            // Wojciech's multi-tier permutation approach
            //

            // first do Andrew's permutations for the biggest substratum offset
            int offset = 0;
            for (unsigned digit = 0; digit < permutedCoeffs.size(); ++digit)
                offset += permute(permutedCoeffs[digit], m_base,
                                  m_strataPermute * 0x51633e2d * (digit + 1) *
                                      (d + 1));
            offset %= m_base;

            if (m_ot == CENTERED)
            {
                // if just doing Andrew's version which doesn't have latin
                // hypercube projections, then adjust the scale of offset and
                // jitter to account for the fact that we divide by period below
                // and not m_base
                offset *= period / m_base;
                jitter *= m_base;
            }
            else
            {
                // Otherwise, do additional offsets to enforce latin hypercubes
                for (unsigned digit = 1; digit < permutedCoeffs.size(); ++digit)
                {
                    int subOffset =
                        permute(permutedCoeffs[digit], m_base, offset);
                    offset *= m_base;
                    offset += subOffset;
                }
            }

            point[d] = (stratum + (offset + jitter) / period) / m_base;

            //
            // End Wojciech's new version
            //
        }
        else if (m_ot == MJ_STYLE)
        {
            //
            // Old version which enforces latin hypercubes, but each slice is
            // not quite a proper CMJ2D

            int subStratum = polyEval(permutedCoeffs, m_base);
            subStratum = permute(subStratum, period,
                                 m_strataPermute * 0x51633e2d * (d + 1));
            point[d] = (stratum + (subStratum + jitter) / period) / m_base;

            //
            // end old version
            //
        }
    }
}

int CMJNDInPlace::coarseGridRes(int samples) const
{
    // get the n-root of samples
    int baseCandidate = int(std::ceil(std::pow(samples, 1.0 / dimensions())));
    return baseCandidate;
}

int CMJNDInPlace::setNumSamples(unsigned n)
{
    m_base = unsigned(std::ceil(std::pow(n, 1.0 / dimensions())));
    m_numSamples = unsigned(std::pow(m_base, dimensions()));
    return m_numSamples;
}
