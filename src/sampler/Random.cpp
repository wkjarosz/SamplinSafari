/** \file Random.cpp
    \author Wojciech Jarosz
*/

#include <sampler/Random.h>

Random::Random(unsigned dimensions, uint64_t seed) : m_numDimensions(dimensions), m_rand(seed)
{
    setDimensions(dimensions);
}

void Random::sample(float r[], unsigned)
{
    for (unsigned d = 0; d < dimensions(); d++)
        r[d] = m_rand.nextFloat();
}

void Random::setDimensions(unsigned n)
{
    if (n < 1)
        n = 1;

    m_numDimensions = n;
}
