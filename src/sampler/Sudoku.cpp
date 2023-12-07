/** \file Sudoku.cpp
    \author Wojciech Jarosz
*/

#include <algorithm>
#include <sampler/Misc.h> // for permute
#include <sampler/Sudoku.h>
#include <vector>

using namespace std;

SudokuInPlace::SudokuInPlace(unsigned x, unsigned y, unsigned dimensions, bool randomize, float jitter,
                             bool correlated) :
    CorrelatedMultiJitteredInPlace(x, y, dimensions, randomize, jitter, correlated)
{
    setNumSamples(x, y);
}

void SudokuInPlace::sample(float r[], unsigned i)
{
    if (i >= m_numSamples)
        i = 0;

    if (i == 0)
        m_rand.seed(m_seed);

    // which digit of the sudoku puzzle we are considering
    unsigned digit = permute(i / m_numDigits, m_numDigits, m_permutation * 0x1fc195a7);

    // 2D indices of the digit we are considering
    int px = digit % m_resX;
    int py = digit / m_resX;

    for (unsigned d = 0; d < dimensions(); d += 2)
    {
        // make i specify the (possibly permuted) sample index within the digit
        int s = permute(i % m_numDigits, m_numDigits, m_permutation * (0x51633e2d * (d + 1) * (digit + 1)));

        // x and y indices of the big stratum in the sudoku puzzle we are considering
        int x = s % m_resX;
        int y = s / m_resX;

        // x and y offset within the big stratum based on the digit and index within the digit
        int sx = permute((y + py) % m_resY, m_resY, m_permutation * (m_decorrelate * x + 0x02e5be93) * (d + 1));
        int sy = permute((x + px) % m_resX, m_resX, m_permutation * (m_decorrelate * y + 0x68bc21eb) * (d + 1));

        // compute the offsets to make the collective sudoku digit a Latin square
        int ssy = permute(sx + x * m_resY, m_numDigits,
                          m_permutation * (m_decorrelate * (y * m_resX + sy) + 0xeb12cb86) * (d + 1));
        int ssx = permute(sy + y * m_resX, m_numDigits,
                          m_permutation * (m_decorrelate * (x * m_resY + sx) + 0x39eb5e20) * (d + 1));

        // jitter in the x and y dimensions
        float jx = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);
        float jy = 0.5f + m_maxJit * (m_rand.nextFloat() - 0.5f);

        r[d]     = (x + (sx + (ssx + jx) / m_numDigits) / m_resY) / m_resX;
        r[d + 1] = (y + (sy + (ssy + jy) / m_numDigits) / m_resX) / m_resY;
    }
}
