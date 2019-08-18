/**
   \file OACommon.cpp
   \author Wojciech Jarosz
 */

#include <vector>
#include <cmath>

using namespace std;

// local functions
namespace
{

enum OffsetType : unsigned
{
    CENTERED = 0,
    J_STYLE,
    MJ_STYLE,
    CMJ_STYLE,
    NUM_OFFSET_TYPES
};

unsigned permute(unsigned i, unsigned l, unsigned p)
{
    if (p == 0)
        return i;

    unsigned w = l - 1;
    w |= w >> 1;
    w |= w >> 2;
    w |= w >> 4;
    w |= w >> 8;
    w |= w >> 16;
    do
    {
        i ^= p;             i *= 0xe170893d;
        i ^= p >> 16;
        i ^= (i & w) >> 4;
        i ^= p >> 8;        i *= 0x0929eb3f;
        i ^= p >> 23;
        i ^= (i & w) >> 1;  i *= 1 | p >> 27;
        i *= 0x6935fa69;
        i ^= (i & w) >> 11; i *= 0x74dcb303;
        i ^= (i & w) >> 2;  i *= 0x9e501cc3;
        i ^= (i & w) >> 2;  i *= 0xc860a3df;
        i &= w;
        i ^= i >> 5;
    } while (i >= l);

    return (i + p) % l;
}


float randf(unsigned i, unsigned p)
{
    if (p == 0) return 0.5f;
    i ^= p;
    i ^= i >> 17;
    i ^= i >> 10;       i *= 0xb36534e5;
    i ^= i >> 12;
    i ^= i >> 21;       i *= 0x93fc4795;
    i ^= 0xdf6e307f;
    i ^= i >> 17;       i *= 1 | p >> 18;
    return i * (1.0f / 4294967808.0f);
}

/// Compute substrata offsets for Bose OA samples
inline float boseOffset(unsigned sx, unsigned sy, unsigned s, unsigned p,
                        unsigned ot)
{
    if (ot == CENTERED)  return 0.5f * (s - 1);
    if (ot == J_STYLE)   return permute(sy, s, (sy * s + sx + 1) * p);
    if (ot == MJ_STYLE)  return permute(sy, s, (sx + 1) * p);
                         return permute(sy, s, p);   // default: CMJ
}

/// Compute substrata offsets for Bush OA samples
inline unsigned bushOffset(unsigned i, unsigned N, unsigned s,
                           unsigned stm, unsigned p, unsigned ot)
{
    switch (ot)
    {
        case CENTERED: return (stm - 1) / 2.0f;
        case J_STYLE:  return permute((i / s) % stm, stm, (i + 1) * p);
        // CMJ is still a work in progrestm, seems to work for strength 3,
        // but not others.
        case CMJ_STYLE:
            return (permute((i / s) % s, s, p) + permute(i % s, s, p * 2) * (stm / s)) % stm;
        default:
        case MJ_STYLE: return permute((i / s) % stm, stm, p);
    }
}

/// Compute the digits of decimal value `i` expressed in base `b`
inline vector<unsigned> toBaseS(unsigned i, unsigned b, unsigned t)
{
    vector<unsigned> digits(t);
    for (unsigned ii = 0; ii < t; i /= b, ++ii)
        digits[ii] = i % b;
    return digits;
}

/// Evaluate polynomial with coefficients a at location arg
inline unsigned evalPoly(const vector<unsigned>& a, unsigned arg)
{
    unsigned ans = 0;
    for (unsigned l = a.size(); l--; )
        ans = (ans * arg) + a[l];        // Horner's rule
    return ans;
}

/// Copy all but the j-th element of vector in
inline vector<unsigned> allButJ(const vector<unsigned>& in, unsigned j)
{
    vector<unsigned> out(in.size() - 1);
    copy(in.begin(), in.begin() + j, out.begin());
    copy(in.begin() + j + 1, in.end(), out.begin() + j);
    return out;
}

} // namespace


float boseOA(unsigned i,  ///< sample index
             unsigned j,  ///< dimension (< s+1)
             unsigned s,  ///< number of levels/strata
             unsigned p,  ///< pseudo-random permutation seed
             unsigned ot) ///< CENTERED, J_STYLE, MJ_STYLE, or CMJ_STYLE
{
    unsigned Aij, Aik;
    // i = permute(i % (s * s), s * s, p);
    unsigned Ai0 = i / s;
    unsigned Ai1 = i % s;
    if (j == 0)
    {
        Aij = Ai0;
        Aik = Ai1;
    }
    else if (j == 1)
    {
        Aij = Ai1;
        Aik = Ai0;
    }
    else
    {
        unsigned k = (j % 2) ? j - 1 : j + 1;
        Aij = (Ai0 + (j - 1) * Ai1) % s;
        Aik = (Ai0 + (k - 1) * Ai1) % s;
    }
    unsigned stratum  = permute(Aij, s,         p * (j+1) * 0x51633e2d);
    float subStratum  = boseOffset(Aij, Aik, s, p * (j+1) * 0x68bc21eb, ot);
    float    jitter   = randf(i,                p * (j+1) * 0x02e5be93);
    return (stratum + (subStratum + jitter) / s) / s;
}

void boseOA(float Xi[],   ///< pointer to float array for output
            unsigned i,   ///< sample index
            unsigned d,   ///< number of dimensions (<= s+1)
            unsigned s,   ///< number of levels/strata
            float maxJit, ///< amount to jitter within substrata
            unsigned p,   ///< pseudo-random permutation seed
            unsigned ot)  ///< CENTERED, J_STYLE, MJ_STYLE, or CMJ_STYLE
{
    // // to index into the sample set in XY stratum order, use the following
    // unsigned stratumX = i / s;
    // unsigned stratumY = i % s;
    // unsigned Ai0 = permute(stratumX, s, p * 1 * 0x51633e2d);
    // unsigned Ai1 = permute(stratumY, s, p * 2 * 0x51633e2d);
    // 
    unsigned Ai0      = i % s;
    unsigned Ai1      = i / s;
    unsigned stratumX = permute(Ai0, s,           p * 1 * 0x51633e2d);
    unsigned stratumY = permute(Ai1, s,           p * 2 * 0x51633e2d);
    float sstratX     = boseOffset(Ai0, Ai1, s,   p * 1 * 0x68bc21eb, ot);
    float sstratY     = boseOffset(Ai1, Ai0, s,   p * 2 * 0x68bc21eb, ot);
    float jitterX     = 0.5f + maxJit * (randf(i, p * 1 * 0x02e5be93) - 0.5f);
    float jitterY     = 0.5f + maxJit * (randf(i, p * 2 * 0x02e5be93) - 0.5f);
    Xi[0]             = (stratumX + (sstratX + jitterX) / float(s)) / float(s);
    Xi[1]             = (stratumY + (sstratY + jitterY) / float(s)) / float(s);

    for (unsigned j = 2; j < d; ++j)
    {
        unsigned Aij      = (Ai0 + (j-1) * Ai1) % s;
        unsigned j2       = (j % 2) ? j-1 : j+1;
        unsigned Aij2     = (Ai0 + (j2-1) * Ai1) % s;
        unsigned stratumJ = permute(Aij, s,           p * (j+1) * 0x51633e2d);
        float sstratJ     = boseOffset(Aij, Aij2, s,  p * (j+1) * 0x68bc21eb, ot);
        float jitterJ     = 0.5f + maxJit * (randf(i, p * (j+1) * 0x02e5be93) - 0.5f);
        Xi[j]             = (stratumJ + (sstratJ + jitterJ) / s) / s;
    }
}

float bushOA(unsigned i,   ///< sample index
             unsigned j,   ///< dimension (< s)
             unsigned s,   ///< number of levels/strata
             unsigned t,   ///< strength of OA (0 < t <= d)
             float maxJit, ///< amount to jitter within substrata
             unsigned p,   ///< pseudo-random permutation seed
             unsigned ot)  ///< J or MJ
{
    unsigned N          = pow(s, t);
    unsigned stm        = N / s;     // pow(s, t-1)
    auto iDigits        = toBaseS(i, s, t);
    unsigned phi        = evalPoly(iDigits, j);
    unsigned stratum    = permute(phi % s, s,       p * (j+1) * 0x51633e2d);
    unsigned subStratum = bushOffset(i, N, s, stm,  p * (j+1) * 0x68bc21eb, ot);
    float jitter        = 0.5f + maxJit * (randf(i, p * (j+1) * 0x02e5be93) - 0.5f);
    return (stratum + (subStratum + jitter) / stm) / s;
}

void bushOA(float Xi[],   ///< pointer to float array for output
            unsigned i,   ///< sample index
            unsigned d,   ///< number of dimensions (<= s+1)
            unsigned s,   ///< number of levels/strata
            unsigned t,   ///< strength of OA (0 < t <= d)
            float maxJit, ///< amount to jitter within substrata
            unsigned p,   ///< pseudo-random permutation seed
            unsigned ot)  ///< J or MJ
{
    unsigned N = pow(s, t);
    unsigned stm = N / s;   // pow(s, t-1)

    // get digits of `i` in base `s`
    auto iDigits = toBaseS(i, s, t);

    for (unsigned j = 0; j < d; ++j)
    {
        unsigned phi      = evalPoly(iDigits, j);
        unsigned stratum  = permute(phi % s, s,       p * (j+1) * 0x51633e2d);
        unsigned sstratum = bushOffset(i, N, s, stm,  p * (j+1) * 0x68bc21eb, ot);
        float jitter      = 0.5f + maxJit * (randf(i, p * (j+1) * 0x02e5be93) - 0.5f);
        Xi[j]             = (stratum + (sstratum + jitter) / stm) / s;
    }
}

/// Generalization of Kensler's CMJ pattern to higher dimensions.
float cmjdD(unsigned i,   ///< sample index
            unsigned j,   ///< dimension (<= s+1)
            unsigned s,   ///< number of levels/strata
            unsigned t,   ///< strength of OA (t=d)
            float maxJit, ///< amount to jitter within substrata
            unsigned p)   ///< pseudo-random permutation seed
{
    unsigned N        = pow(s, t);
    unsigned stm1     = N / s;     // pow(s, t-1)
    auto     iDigits  = toBaseS(i, s, t);
    unsigned stratum  = permute(iDigits[j], s,    p * (j+1) * 0x51633e2d);
    auto     pDigits  = allButJ(iDigits, j);
    unsigned sStratum = evalPoly(pDigits, s);
             sStratum = permute(sStratum, stm1,   p * (j+1) * 0x68bc21eb);
    float    jitter   = 0.5f + maxJit * (randf(i, p * (j+1) * 0x02e5be93) - 0.5f);
    return (stratum + (sStratum + jitter) / stm1) / s;
}

/// Generalization of Kensler's CMJ pattern to higher dimensions.
void cmjdD(float Xi[],   ///< pointer to float array for output
           unsigned i,   ///< sample index
           unsigned s,   ///< number of levels/strata
           unsigned t,   ///< strength of OA (t=d)
           float maxJit, ///< amount to jitter within substrata
           unsigned p)   ///< pseudo-random seed
{
    unsigned N = pow(s, t);
    unsigned stm1 = N / s;  // pow(s, t-1)

    // get digits of `i` in base `s`
    auto iDigits = toBaseS(i, s, t);

    for (unsigned j = 0; j < t; ++j)
    {
        unsigned stratum  = permute(iDigits[j], s,    p * (j+1) * 0x51633e2d);
        auto pDigits      = allButJ(iDigits, j);
        unsigned sstratum = evalPoly(pDigits, s);
        sstratum          = permute(sstratum, stm1,   p * (j+1) * 0x68bc21eb);
        float jitter      = 0.5f + maxJit * (randf(i, p * (j+1) * 0x02e5be93) - 0.5f);
        Xi[j]             = (stratum + (sstratum + jitter) / stm1) / s;
    }
}