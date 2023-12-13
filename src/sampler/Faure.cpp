/** \file Faure.cpp
    \author Wojciech Jarosz
*/

#include <assert.h>
#include <iostream>
#include <sampler/Faure.h>
#include <sampler/Misc.h>

#include "sampling/sfaure.h"

Faure::Faure(unsigned dimensions, unsigned numSamples) : m_numSamples(numSamples), m_numDimensions(dimensions)
{
    regenerate();
}

unsigned Faure::s() const
{
    if (m_numDimensions <= 3)
        return 3;
    else if (m_numDimensions <= 5)
        return 5;
    else if (m_numDimensions <= 7)
        return 7;
    else // if (m_numDimensions <= 11)
        return 11;
}

std::string Faure::name() const
{
    return "Stochastic Faure (0," + std::to_string(s()) + ")";
}

void Faure::regenerate()
{
    // compute all points, and store them
    m_samples.clear();
    m_samples.resize(m_numSamples * s());

    if (m_numSamples > 0)
    {
        if (m_numDimensions <= 3)
            sampling::GetStochasticFaure03Samples(m_numSamples, s(), false, 1, m_owen, &m_samples[0]);
        else if (m_numDimensions <= 5)
            sampling::GetStochasticFaure05Samples(m_numSamples, s(), false, 1, m_owen, &m_samples[0]);
        else if (m_numDimensions <= 7)
            sampling::GetStochasticFaure07Samples(m_numSamples, s(), false, 1, m_owen, &m_samples[0]);
        else if (m_numDimensions <= 11)
            sampling::GetStochasticFaure011Samples(m_numSamples, s(), false, 1, m_owen, &m_samples[0]);
    }
}

void Faure::setDimensions(unsigned n)
{
    auto newD = clamp(n, (unsigned)MIN_DIMENSION, (unsigned)MAX_DIMENSION);
    if (newD != m_numDimensions)
    {
        m_numDimensions = newD;
        regenerate();
    }
}

int Faure::setNumSamples(unsigned n)
{
    auto oldN    = m_numSamples;
    m_numSamples = (n == 0) ? 1 : n;
    if (oldN < m_numSamples)
        regenerate();

    return m_numSamples;
}

void Faure::sample(float r[], unsigned i)
{
    assert(i < m_numSamples);
    for (unsigned d = 0; d < dimensions(); ++d)
    {
        assert(s() * i + d < m_samples.size());
        r[d] = m_samples[s() * i + d];
    }
}
