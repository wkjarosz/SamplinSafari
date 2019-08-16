/*! \file SamplerSobol.h
    \brief
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/Sampler.h>
#include <pcg32.h>
#include <vector>


//! A Sobol quasi-random number sequence.
/*!
    A wrapper for L. Gruenschloss's fast Sobol sampler.
*/
class Sobol : public TSamplerMinMaxDim<1,1024>
{
public:
    Sobol(unsigned dimensions = 2);

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override    {return m_numDimensions;}
    void setDimensions(unsigned n) override {m_numDimensions = n;}

    bool randomized() const override        {return m_scrambles.size();}
    void setRandomized(bool b = true) override;

    std::string name() const override             {return "Sobol";}

protected:
    unsigned m_numDimensions;
    pcg32 m_rand;
    std::vector<unsigned> m_scrambles;
};



//! A (0,2) sequence created by padding the first two dimensions of Sobol
class ZeroTwo : public TSamplerMinMaxDim<1,1024>
{
public:
    ZeroTwo(unsigned n = 64, unsigned dimensions = 2, bool shuffle = false);

    void reset() override;
    void sample(float[], unsigned i) override;

    unsigned dimensions() const override     {return m_numDimensions;}
    void setDimensions(unsigned d) override  {m_numDimensions = d; reset();}

    bool randomized() const override         {return m_randomize;}
    void setRandomized(bool r) override      {m_randomize = r; reset();}

    int numSamples() const  override         {return m_numSamples;}
    int setNumSamples(unsigned n) override;

    std::string name() const override              {return m_shuffle ? "Shuffled+XORed (0,2)" : "XORed (0,2)";}

protected:
    unsigned m_numSamples, m_numDimensions;
    bool m_randomize, m_shuffle;
    pcg32 m_rand;
    std::vector<unsigned> m_scrambles;
    std::vector<unsigned> m_permutes;
};


// Copyright (c) 2012 Leonhard Gruenschloss (leonhard@gruenschloss.org)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


struct SobolMatrices
{
    static const unsigned numDimensions = 1024;
    static const unsigned size = 52;
    static const unsigned matrices[];
};

// Compute one component of the Sobol'-sequence, where the component
// corresponds to the dimension parameter, and the index specifies
// the point inside the sequence. The scramble parameter can be used
// to permute elementary intervals, and might be chosen randomly to
// generate a randomized QMC sequence.
inline float sobol(unsigned long long index, unsigned dimension,
                   unsigned scramble = 0U)
{
    assert(dimension < SobolMatrices::numDimensions);

    unsigned result = scramble;
    for (unsigned i = dimension * SobolMatrices::size; index; index >>= 1, ++i)
    {
        if (index & 1)
            result ^= SobolMatrices::matrices[i];
    }

    return result * (1.f / (1ULL << 32));
}
