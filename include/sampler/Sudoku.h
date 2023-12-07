/** \file Sudoku.h
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/MultiJittered.h>

/// An in-place version of Sudoku point set with both jittered and n-rooks stratification.
/*!
    This produces MxN nested NxM MultiJittered patterns, filling in a complete (MxN)^2 sudoku board
*/
class SudokuInPlace : public CorrelatedMultiJitteredInPlace
{
public:
    SudokuInPlace(unsigned x, unsigned y, unsigned dimensions = 2, bool randomize = false, float jitter = 0.0f,
                  bool correlated = false);

    int coarseGridRes(int samples) const override
    {
        return std::pow(samples, 0.25f);
    }

    void sample(float[], unsigned i) override;
    int  numSamples() const override
    {
        return m_numSamples;
    }
    int setNumSamples(unsigned n) override
    {
        if (n == m_numSamples)
            return m_numSamples;
        int   v  = std::max(1, (int)round(std::pow((float)n, 0.5f)));
        float sv = std::sqrt(v);
        setNumSamples((int)ceil(sv), std::max(1, (int)floor(sv)));
        return m_numSamples;
    }
    void setNumSamples(unsigned x, unsigned y)
    {
        m_resX       = x;
        m_resY       = y;
        m_numDigits  = m_resX * m_resY;
        m_numSamples = m_numDigits * m_numDigits;
        reset();
    }

    std::string name() const override
    {
        return m_decorrelate ? "Sudoku In-Place" : "Correlated Sudoku In-Place";
    }

protected:
    unsigned m_numDigits = 1;
};
