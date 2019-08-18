/** \file OAAddelmanKempthorne.cpp
    \brief Implementation of orthogonal array constructions due to Addelman and
        Kempthorne (1961).
    \author Wojciech Jarosz
*/

#include <sampler/OAAddelmanKempthorne.h>
#include <sampler/Misc.h>
#include <galois++/element.h>
#include <galois++/primes.h>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

// local functions
namespace
{

int akodd(Galois::Element* kay, vector<int>& b, vector<int>& c, vector<int>& k)
{
    Galois::Element num(kay->field()), den(kay->field()), four(kay->field());

    int q = kay->field()->q;
    int p = kay->field()->p;

    four = (p != 3) ? 4 : 1;

    *kay = 0;
    for (int i = 2; i < q; i++)
        if (kay->field()->root[i] == -1)
            *kay = i;

    if (kay->value() == 0)
    {
        ostringstream s;
        s << "Problem: no rootless element in GF(" << kay->field()->n << ").\n";
        const string ss = s.str();
        throw runtime_error(ss.c_str());
    }

    b[0] = k[0] = c[0] = 0;
    for (int i = 1; i < q; i++)
    {
        num = *kay + (p - 1); /* -1 = +(p-1) */
        den = (*kay * four) * i;
        b[i] = (num / den).value();
        k[i] = (*kay * i).value();
        c[i] = ((num * i) * i / four).value();
    }

    return 0;
}

int akeven(Galois::Element* kay, vector<int>& b, vector<int>& c, vector<int>& k)
{
    int q = kay->field()->q;
    *kay = 1;

    if (q == 2)
        b[1] = c[1] = k[1] = 1;
    if (q == 4)
    {
        b[1] = c[1] = 2;
        b[2] = c[2] = 1;
        b[3] = c[3] = 3;
        k[1] = 1;
        k[2] = 2;
        k[3] = 3;
    }

    for (int i = 1; i < q; ++i)
        k[i] = i;

    if (q > 4)
        throw domain_error("Addelman Kempthorne designs not yet available "
                           "for even q > 4\n");

    return 0;
}

} // namespace

AddelmanKempthorneOAInPlace::AddelmanKempthorneOAInPlace(unsigned x, OffsetType ot,
                                       bool randomize, float jitter,
                                       unsigned dimensions)
    : BoseGaloisOAInPlace(x, ot, randomize, jitter, dimensions)
{
    setNumSamples(2 * x * x);
    reset();
}

string AddelmanKempthorneOAInPlace::name() const
{
    return "Addel-Kemp OA In-Place";
}

int AddelmanKempthorneOAInPlace::setNumSamples(unsigned n)
{
    int base = (int)round(sqrt(n * 0.5f));
    base = (base < 2) ? 2 : base;
    m_s = primeGE(base);
    m_numSamples = 2 * m_s * m_s;
    m_gf.resize(m_s);
    reset();
    return m_numSamples;
}

void AddelmanKempthorneOAInPlace::sample(float r[], unsigned row)
{
    if (row == 0)
        m_rand.seed(m_seed);

    const Galois::Element i(&m_gf, (row / m_s) % m_s);
    const Galois::Element j(&m_gf, row % m_s);
    const Galois::Element square = i * i;

    if (2 * row < m_numSamples)
    {
        // First q*q rows
        auto Adim = [this, i, j, square](unsigned dim) {
            // re-ordered the dimensions so that i is the first one
            // mathematically the i.value case is just a special case of the
            // (i+m*j) when m == 0, but putting i in the first column allows us
            // to more easily create a nested/sliced OA
            if (dim == 0)
                return i.value();
            if (dim == 1)
                return j.value();
            else if (dim > 1 && dim <= m_s + 1)
            {
                unsigned m = dim - 1;
                return (i + m * j).value();
            }
            else if (dim > m_s + 1 && dim <= 2 * m_s + 1)
            {
                unsigned m = dim - (m_s + 2);
                return (m * i + j + square).value();
            }
            else
                throw domain_error("Out to bounds dimension");
        };

        for (unsigned dim = 0; dim < 2 * m_s + 1 && dim < dimensions();
             ++dim)
        {
            int Acol = Adim(dim);
            int k = (dim % 2) ? dim - 1 : (dim + 1) % (2 * m_s + 1);
            int Aik = Adim(k);
            int stratumJ = permute(Acol, m_s, m_seed * (dim+1));
            // int sstratJ = boseLHOffset(Acol, 2*Aik, 2*m_s,
            // m_seed * (dim+1) * 0x68bc21eb, m_ot); LHS with 2*m_s
            // fine strata int sstratJ = permute(2*Aik, 2*m_s, (Acol + 1) *
            // m_seed * (dim+1) * 0x68bc21eb); centered float sstratJ = m_s -
            // 0.5f;
            // // antithetic j
            // int sstratJ = permute(Aik, m_s, (Aik * m_s + Acol + 1) *
            // m_seed * (dim+1) * 0x68bc21eb);
            // // randomize which quandrant the samples go into
            // if (permute(Aik % 2, 2, (Acol * m_s + Aik + 1) *
            // m_seed * (dim+1)))
            //     sstratJ = 2*m_s - 1 - sstratJ;
            // antithetic mj
            int sstratJ = permute(Aik, m_s, (Acol + 1) * m_seed * (dim+1) * 0x68bc21eb);
            // randomize which quandrant the samples go into
            if (permute(Aik % 2, 2, (Acol * m_s + Aik + 1) * m_seed * (dim+1)))
                sstratJ = 2 * m_s - 1 - sstratJ;
            // // antithetic cmj
            // int sstratJ = permute(Aik, m_s, m_seed * (dim+1) *
            // 0x68bc21eb);
            // // randomize whether cmj points get pushed to upper or lower half
            // of slice if (permute(Aik % 2, 2, m_seed * (dim+1)))
            //     sstratJ = 2*m_s - 1 - sstratJ;
            float jitterJ = 0.5f + int(m_randomize) * m_maxJit *
                                       (m_rand.nextFloat() - 0.5f);
            r[dim] = (stratumJ + (sstratJ + jitterJ) / float(2 * m_s)) /
                     float(m_s);
        }
    }
    else
    {
        // Second q*q rows
        for (size_t dim = 0; dim < dimensions(); ++dim)
            r[dim] = 0.0f;

        vector<int> b(m_s);
        vector<int> c(m_s);
        vector<int> k(m_s);
        Galois::Element kay(&m_gf);

        if (m_gf.p != 2)
            akodd(&kay, b, c, k);
        else
            akeven(&kay, b, c, k);

        const Galois::Element ksquare = kay * square;

        auto Adim = [this, &i, &j, &ksquare, &b, &c, &k](unsigned dim) {
            if (dim == 0)
                return i.value();
            if (dim == 1)
                return j.value();
            else if (dim > 1 && dim <= m_s + 1)
            {
                unsigned m = dim - 1;
                return (i + m * j + b[m]).value();
            }
            else if (dim > m_s + 1 && dim <= 2 * m_s + 1)
            {
                unsigned m = dim - (m_s + 2);
                return (k[m] * i + j + ksquare + c[m]).value();
            }
            else
                throw domain_error("Out to bounds dimension");
        };

        for (unsigned dim = 0; dim < 2 * m_s + 1 && dim < dimensions();
             ++dim)
        {
            int Acol = Adim(dim);
            int k = (dim % 2) ? dim - 1 : (dim + 1) % (2 * m_s + 1);
            int Aik = Adim(k);
            int stratumJ = permute(Acol, m_s, m_seed * (dim+1));
            // int sstratJ = boseLHOffset(Acol, 2*Aik+1, 2*m_s,
            // m_seed * (dim+1) * 0x63bc21eb, m_ot); LHS with 2*m_s
            // fine strata int sstratJ = permute(2*Aik+1, 2*m_s, (Acol + 1) *
            // m_seed * (dim+1) * 0x68bc21eb); centered float sstratJ = m_s -
            // 0.5f;
            // // antithetic j
            // int sstratJ = permute(Aik, m_s, (Aik * m_s + Acol + 1) *
            // m_seed * (dim+1) * 0x68bc21eb);
            // // randomize which quandrant the samples go into
            // if (!permute(Aik % 2, 2, (Acol * m_s + Aik + 1) *
            // m_seed * (dim+1)))
            //     sstratJ = 2*m_s - 1 - sstratJ;
            // antithetic mj
            int sstratJ = permute(Aik, m_s, (Acol + 1) * m_seed * (dim+1) * 0x68bc21eb);
            // randomize which quandrant the samples go into
            if (!permute(Aik % 2, 2, (Acol * m_s + Aik + 1) * m_seed * (dim+1)))
                sstratJ = 2 * m_s - 1 - sstratJ;
            // // antithetic cmj
            // int sstratJ = permute(Aik, m_s, m_seed * (dim+1) *
            // 0x68bc21eb);
            // // randomize whether cmj points get pushed to upper or lower half
            // of slice if (!permute(Aik % 2, 2, m_seed * (dim+1)))
            //     sstratJ = 2*m_s - 1 - sstratJ;
            float jitterJ = 0.5f + int(m_randomize) * m_maxJit * (m_rand.nextFloat() - 0.5f);
            r[dim] = (stratumJ + (sstratJ + jitterJ) / (2 * m_s)) / m_s;
        }
    }
}


////