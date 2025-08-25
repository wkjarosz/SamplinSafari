/** \file Halton.cpp
    \author Wojciech Jarosz
*/

#include <galois++/primes.h> // for nthPrime
#include <sampler/Halton.h>
#include <sampler/Misc.h> // for foldedRadicalInverse

Halton::Halton(unsigned dimensions) : m_numDimensions(0), m_seed(13)
{
    setDimensions(dimensions);
    setSeed(m_seed);
}

void Halton::setDimensions(unsigned n)
{
    if (n < 1)
        n = 1;

    m_numDimensions = n;
}

void Halton::initRandom() { m_halton.init_random(m_rand); }

void Halton::setSeed(uint32_t seed)
{
    m_seed = seed;
    m_rand.seed(m_seed);
    if (m_seed)
        initRandom();
    else
        initFaure();
}

void Halton::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < m_numDimensions; d++) r[d] = m_halton.sample(d, i);
}

HaltonZaremba::HaltonZaremba(unsigned dimensions) : m_numDimensions(0) { setDimensions(dimensions); }

void HaltonZaremba::setDimensions(unsigned n)
{
    if (n < 1)
        n = 1;

    m_numDimensions = n;
}

void HaltonZaremba::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < m_numDimensions; d++) r[d] = foldedRadicalInverse(i, nthPrime(d + 1));
}
