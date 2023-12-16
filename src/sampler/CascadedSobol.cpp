/** \file CascadedSobol.cpp
    \author Wojciech Jarosz
*/

#include "Samplers/OwenScrambling.h"
#include "Samplers/SobolGenerator1D.h"
#include <assert.h>
#include <fstream>
#include <iostream>
#include <sampler/CascadedSobol.h>
#include <sampler/Misc.h>

using std::string;
using std::vector;

CascadedSobol::CascadedSobol(const string &data_file, unsigned dimensions, unsigned numSamples) :
    m_numSamples(numSamples), m_numDimensions(dimensions)
{
    // Read sobol matrices value from file
    std::ifstream tableFile(data_file);
    if (!tableFile.is_open())
        throw std::runtime_error(string("File \"") + data_file + "\" cannot be read.");

    // initialize with the max dimensions we will use
    load_init_table(tableFile, d, s, a, m, MAX_DIMENSION);

    sobols = std::make_unique<SobolGenerator1D[]>(m.size());
    for (size_t i = 0; i < m.size(); ++i)
        sobols[i].init1D(d[i], s[i], a[i], m[i]);
}

void CascadedSobol::setRandomized(bool b)
{
    owen_permut_flag = b;

    // init owen scrambling seeds
    realSeeds.resize(MAX_DIMENSION);
    for (auto &s : realSeeds)
        s = m_rand.nextUInt();
}

void CascadedSobol::setDimensions(unsigned n)
{
    m_numDimensions = clamp(n, (unsigned)MIN_DIMENSION, (unsigned)MAX_DIMENSION);
}

int CascadedSobol::setNumSamples(unsigned n)
{
    m_numSamples = (n == 0) ? 1 : n;
    nbits        = uint32_t(log(m_numSamples) / log(2.0));
    return m_numSamples;
}

void CascadedSobol::sample(float r[], unsigned i)
{
    assert(i < m_numSamples);

    // adapted from Paulin's cascadedSobol.cpp
    const uint32_t owen_tree_depth = 32;
    uint32_t       index           = i; // dim 0: take n-th point as index -> into 32-bit integer
    for (unsigned idim = 0; idim < m_numDimensions; idim++)
    {
        index           = sobols[idim].getSobolInt(index); // radix-inversion + sobol
        uint32_t result = index;
        if (owen_permut_flag) // we need to apply OwenScrambling only when this flag is set
            result = OwenScrambling(result, realSeeds[idim], owen_tree_depth);
        r[idim] = float(result) / float(std::numeric_limits<uint32_t>::max()); // final value (float) for this dimension
        index   = index >> (32 - nbits); // this will be used as new index for the next dimension
    }
}
