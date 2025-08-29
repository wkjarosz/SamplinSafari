/** \file Sobol.cpp
    \author Wojciech Jarosz
*/

#include <iostream>
#include <sampler/Misc.h>
#include <sampler/Sobol.h>

#include "sampler/onetwo_matrices.h"
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
        for (unsigned d = 0; d < dimensions(); ++d) r[d] = sobol::sample(i, d, m_scrambles[d]);
    }
    else
    {
        for (unsigned d = 0; d < dimensions(); ++d) r[d] = sobol::sample(i, d);
    }
}

void Sobol::setSeed(uint32_t seed)
{
    m_seed = seed;
    m_rand.seed(m_seed);
    if (seed == 0)
        m_scrambles.resize(0);
    else
    {
        m_scrambles.resize(dimensions());
        for (unsigned d = 0; d < dimensions(); ++d) m_scrambles[d] = m_rand.nextUInt();
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
        m_scrambles[d] = m_seed ? m_rand.nextUInt() : 0;
        m_permutes[d]  = m_shuffle ? m_rand.nextUInt() : 0;
    }
}

int ZeroTwo::setNumSamples(unsigned n) { return m_numSamples = (n == 0) ? 1 : n; }

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
        r[d] = float(sampling::GetSobolStatelessIter(i, d % MAX_DIMENSION, mix_bits(m_seed + d / 2), 2));
}

ZSobol::ZSobol(unsigned dimensions) : SSobol(dimensions)
{
    // empty
}

void ZSobol::reset()
{
    int      sqrtVal    = (m_numSamples == 0) ? 1 : (int)(std::sqrt((float)m_numSamples) + 0.5f);
    uint32_t res        = roundUpPow2(sqrtVal);
    m_numSamples        = res * res;
    m_log2_res          = iLog2(res);
    m_num_base_4_digits = m_log2_res;
}

uint64_t ZSobol::shuffled_morton_index(uint32_t morton_index, uint32_t num_base_4_digits, uint32_t dimension)
{
    // Define the full set of 4-way permutations in _permutations_
    static constexpr uint8_t permutations[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 2, 1}, {0, 3, 1, 2}, {1, 0, 2, 3}, {1, 0, 3, 2},
        {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 2, 0}, {1, 3, 0, 2}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 0, 1, 3}, {2, 0, 3, 1},
        {2, 3, 0, 1}, {2, 3, 1, 0}, {3, 1, 2, 0}, {3, 1, 0, 2}, {3, 2, 1, 0}, {3, 2, 0, 1}, {3, 0, 2, 1}, {3, 0, 1, 2}};

    uint64_t sample_index = 0;
    // Apply random permutations to full base-4 digits
    bool pow2_samples = 0 & 1;
    int  last_digit   = pow2_samples ? 1 : 0;
    for (int i = num_base_4_digits - 1; i >= last_digit; --i)
    {
        // Randomly permute $i$th base-4 digit in _mortonIndex_
        int digit_shift = 2 * i - (pow2_samples ? 1 : 0);
        int digit       = (morton_index >> digit_shift) & 3;
        // Choose permutation _p_ to use for _digit_
        uint64_t higher_digits = morton_index >> (digit_shift + 2);
        int      p             = (mix_bits(higher_digits ^ (0x55555555u * dimension)) >> 24) % 24;

        digit = permutations[p][digit];
        sample_index |= uint64_t(digit) << digit_shift;
    }

    // Handle power-of-2 (but not 4) sample count
    if (pow2_samples)
    {
        int digit = morton_index & 1;
        sample_index |= digit ^ (mix_bits((morton_index >> 1) ^ (0x55555555u * dimension)) & 1);
    }

    return sample_index;
}

void ZSobol::sample(float r[], unsigned i)
{
    uint32_t pixel_x      = i / (1 << m_log2_res);
    uint32_t pixel_y      = i % (1 << m_log2_res);
    uint64_t morton_index = encode_Morton2(pixel_x, pixel_y);

    for (unsigned d = 0; d < dimensions(); ++d)
    {
        auto j = shuffled_morton_index(morton_index, m_num_base_4_digits, (d / 2) * 2);
        r[d]   = float(sampling::GetSobolStatelessIter(j, d % MAX_DIMENSION, mix_bits(m_seed + d / 2), 2));
        if (d == 0)
            r[0] = (pixel_x + r[0]) / (1 << m_log2_res);
        else if (d == 1)
            r[1] = (pixel_y + r[1]) / (1 << m_log2_res);
    }
}

OneTwo::OneTwo(unsigned n, unsigned dimensions, uint32_t seed) :
    m_numSamples(n), m_numDimensions(dimensions), m_seed(seed)
{
    reset();
}

void OneTwo::reset()
{
    m_rand.seed(m_seed);
    // m_scrambles.resize(dimensions());
    m_permutes.resize(dimensions());
    for (unsigned d = 0; d < dimensions(); ++d) { m_permutes[d] = m_seed ? m_rand.nextUInt() : 0; }
}

int OneTwo::setNumSamples(unsigned n) { return m_numSamples = (n == 0) ? 1 : n; }

void OneTwo::sample(float r[], unsigned i)
{
    for (unsigned d = 0; d < dimensions(); ++d)
    {
        int pi = permute(i, m_numSamples, m_permutes[d / 2]);
        r[d]   = sample12(pi, d);
    }
}

float OneTwo::sample12(const uint64_t index, const int dim) const
{
    // auto     h = hash(m_seed, dim);
    uint32_t v = onetwo_sample(dim % onetwo_matrices_size, index);
    // switch (m_randomize)
    // {
    // case RandomizeStrategy::None: break;
    // case RandomizeStrategy::PermuteDigits: v = BinaryPermuteScrambler(h)(v); break;
    // case RandomizeStrategy::FastOwen: v = FastOwenScrambler(h)(v); break;
    // case RandomizeStrategy::Owen: v = OwenScrambler(h)(v); break;
    // }
    return std::min(v * 0x1p-32f, k_float_one_minus_epsilon);
}
