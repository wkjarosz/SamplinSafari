/** \file Sobol.cpp
    \author Wojciech Jarosz
*/

#include <iostream>
#include <sampler/Misc.h>
#include <sampler/Sobol.h>

#include "sampling/ssobol.h"
#include "sobol.h"

Sobol::Sobol(unsigned dimensions) : m_numDimensions(dimensions)
{
    // empty
}

void Sobol::sample(float r[], unsigned i)
{
    if (m_scrambles.size() == dimensions())
    {
        for (unsigned d = 0; d < dimensions(); ++d)
            r[d] = sobol::sample(i, d, m_scrambles[d]);
    }
    else
    {
        for (unsigned d = 0; d < dimensions(); ++d)
            r[d] = sobol::sample(i, d);
    }
}

void Sobol::setRandomized(bool b)
{
    if (!b)
        m_scrambles.resize(0);
    else
    {
        m_scrambles.resize(dimensions());
        for (unsigned d = 0; d < dimensions(); ++d)
            m_scrambles[d] = m_rand.nextUInt();
    }
}

ZeroTwo::ZeroTwo(unsigned n, unsigned dimensions, bool shuffle) :
    m_numSamples(n), m_numDimensions(dimensions), m_shuffle(shuffle)
{
    reset();
}

void ZeroTwo::reset()
{
    m_scrambles.resize(dimensions());
    m_permutes.resize(dimensions());
    for (unsigned d = 0; d < dimensions(); ++d)
    {
        m_scrambles[d] = m_randomize ? m_rand.nextUInt() : 0;
        m_permutes[d]  = m_shuffle ? m_rand.nextUInt() : 0;
    }
}

int ZeroTwo::setNumSamples(unsigned n)
{
    return m_numSamples = (n == 0) ? 1 : n;
}

void ZeroTwo::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < dimensions(); ++d)
        r[d] = sobol::sample(permute(i, m_numSamples, m_permutes[d / 2]), d % 2, m_scrambles[d]);
}

SSobol::SSobol(unsigned dimensions) : m_numDimensions(dimensions)
{
    // empty
}

void SSobol::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < dimensions(); ++d)
        r[d] = float(sampling::GetSobolStatelessIter(i, d % MAX_DIMENSION, m_scramble_seed, dimensions()));
}

void SSobol::setRandomized(bool b)
{
    if (!b)
        m_scramble_seed = 0u;
    else
        m_scramble_seed = m_rand.nextUInt();
}
