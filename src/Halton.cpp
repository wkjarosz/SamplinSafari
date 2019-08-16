/*! \file rBRandSamplerHalton.cpp
    \brief
    \author Wojciech Jarosz
*/

#include <sampler/Halton.h>
#include <sampler/Misc.h>
#include <galois++/primes.h>
#include <iostream>
#include <random>


Halton::Halton(unsigned dimensions) :
    m_numDimensions(0), m_randomized(true)
{
    setDimensions(dimensions);
    setRandomized(true);
}

void Halton::setDimensions(unsigned n)
{
    if (n < 1)
        n = 1;

    m_numDimensions = n;
}

void Halton::initRandom()
{
    m_halton.init_random(m_rand);
}

void Halton::setRandomized(bool r)
{
    m_randomized = r;
    if (m_randomized)
        initRandom();
    else
        initFaure();
}

void Halton::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < m_numDimensions; d++)
        r[d] = m_halton.sample(d, i);
}



HaltonZaremba::HaltonZaremba(unsigned dimensions) :
    m_numDimensions(0)
{
    setDimensions(dimensions);
}

void HaltonZaremba::setDimensions(unsigned n)
{
    if (n < 1)
        n = 1;

    m_numDimensions = n;
}

void HaltonZaremba::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < m_numDimensions; d++)
        r[d] = foldedRadicalInverse(i, nthPrime(d+1));
}
