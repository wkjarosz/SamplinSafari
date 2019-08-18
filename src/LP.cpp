/** \file LP.cpp
    \author Wojciech Jarosz
*/

#include <sampler/LP.h>
#include <sampler/Misc.h>
#include <iostream>

LarcherPillichshammerGK::LarcherPillichshammerGK(unsigned dimension,
                                                       unsigned numSamples,
                                                       bool randomize) :
    m_numSamples(numSamples), m_numDimensions(dimension),
    m_inv(1.0f/m_numSamples)
{
    setRandomized(randomize);
}

LarcherPillichshammerGK::~LarcherPillichshammerGK()
{

}

void
LarcherPillichshammerGK::sample(float r[], unsigned i)
{
    r[0] = randomDigitScramble(i*m_inv, m_scramble1);
    if (m_numDimensions == 1) return;

    r[1] = LarcherPillichshammerRI(i, m_scramble2);
    if (m_numDimensions == 2) return;

    r[2] = GruenschlossKellerRI(i, m_scramble3);
}
