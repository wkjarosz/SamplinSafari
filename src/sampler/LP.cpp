/** \file LP.cpp
    \author Wojciech Jarosz
*/

#include <sampler/LP.h>
#include <sampler/Misc.h>

LarcherPillichshammerGK::LarcherPillichshammerGK(unsigned dimension, unsigned numSamples, uint32_t seed) :
    m_numSamples(numSamples), m_numDimensions(dimension), m_inv(1.0f / m_numSamples)
{
    setSeed(seed);
}

LarcherPillichshammerGK::~LarcherPillichshammerGK() {}

void LarcherPillichshammerGK::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < dimensions(); d += 3)
    {
        int      s  = permute(i, m_numSamples, d);
        unsigned ds = 0x68bc21eb * (d + 1);
        if (d < dimensions())
            r[d] = randomDigitScramble(s * m_inv, m_scramble1 * ds);

        if (d + 1 < dimensions())
            r[d + 1] = LarcherPillichshammerRI(s, m_scramble2 * ds);

        if (d + 2 < dimensions())
            r[d + 2] = GruenschlossKellerRI(s, m_scramble3 * ds);
    }
}
