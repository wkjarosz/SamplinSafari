/** \file Misc.h
    \author Wojciech Jarosz
*/
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <galois++/fwd.h>
#include <vector>

/**
    Evaluate a polynomial.

    Evaluate a polynomial of the form \f$f(x) = c_m x^m + \cdots + c_0 x^0\f$,
    with the given coefficient vector `c` at the specified position `x`.

    \param c     A vector of the coefficient values
    \param x     The position to evaluate the polynomial
    \returns     The value of the polynomial at the given position
 */
unsigned polyEval(const std::vector<int> &f, unsigned x);

/**
    Evaluate a polynomial.

    Evaluate a polynomial of the form \f$f(x) = c_m x^m + \cdots + c_0 x^0\f$,
    with the given coefficient vector `c` at the specified position `x`, where
    all arithmetic takes place in the given Galois field `gf`.

    \param gf    The Galois field over which to perform arithmetic
    \param c     A vector of the coefficient values
    \param x     The position to evaluate the polynomial
    \returns     The value of the polynomial at the given position
 */
unsigned polyEval(const Galois::Field *gf, std::vector<int> &c, int x);

/**
    Evaluate a vector of a number with an arbitrary base representation,
    save for one index

    Given a vector of digits representating some number in an arbitrary base,
    this method will evaluate the number in base 10, while ignoring the digit
    at index `ignoreIdx`.

    \param coeffs       Digits of a number in some arbitrary base representation
    \param base         The base of `coeffs`
    \param ignoreIdx    The digit of the `coeffs` vector to ignore
    \returns            `coeffs` represented in base 10 format (excluding the
                        digit at `ignoreIdx`)
 */
unsigned polyEvalExceptOne(const std::vector<int> &coeffs, int base, size_t ignoreIdx);

/**
    Given some value \f$i\f$, determine the coefficient values for a polynomial
    of the form \f$b^d + \cdots + b^0\f$ where \f$b\f$ is `base` and \f$d\f$
    is `degree`.

    \param i        The value to convert into base `base`
    \param base     The base to use for the polynomial
    \param degree   The limit on how high the exponents can be in the polynomial
    \returns        A vector of coefficient values
 */
std::vector<int> iToPolyCoeffs(unsigned i, unsigned base, unsigned degree);

/// Clamps a double between two bounds.
/**
    \param a    The value to test.
    \param l    The lower bound.
    \param h    The upper bound.
    \return     The value `a` clamped to the lower and upper bounds.

    This function has been specially crafted to prevent NaNs from propagating.
*/
template <typename T>
inline T clamp(T a, T l, T h)
{
    return (a >= l) ? ((a <= h) ? a : h) : l;
}

/// Returns a modulus b.
template <typename T>
inline T mod(T a, T b)
{
    int n = (int)(a / b);
    a -= n * b;
    if (a < 0)
        a += b;
    return a;
}

/// Linearly interpolates between `a` and `b`, using parameter `t`.
/**
    \param a    A value.
    \param b    Another value.
    \param t    A blending factor of `a` and `b`.
    \return     Linear interpolation of `a` and `b`:
                a value between `a` and `b` if `t` is between `0` and `1`.
*/
template <typename T, typename S>
inline T lerp(T a, T b, S t)
{
    return T((S(1) - t) * a + t * b);
}

/// Smoothly interpolates between a and b.
/**
    Performs a smooth s-curve (Hermite) interpolation between two values.

    \tparam T       A floating-point type
    \param a        A value.
    \param b        Another value.
    \param x        A number between `a` and `b`.
    \return         A value between `0.0` and `1.0`.
*/
template <typename T>
inline T smoothStep(T a, T b, T x)
{
    T t = std::min(std::max(T(x - a) / (b - a), T(0)), T(1));
    return t * t * (T(3) - T(2) * t);
}

/// Test if an integer is a power of 2
inline bool isPowerOf2(int v) { return (v & (v - 1)) == 0; }

/// Round up to the next smallest power of two
inline unsigned roundUpPow2(unsigned v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}

/// Round down to the previous largest power of two
inline unsigned roundDownPow2(unsigned v)
{
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v - (v >> 1);
}

/// Interpret the bits of a float as an int
inline int floatAsInt(float f)
{
    union
    {
        int   i;
        float f;
    } u;
    u.f = f;
    return u.i;
}

/// Interpret the bits of an int as a float
inline float intAsFloat(int i)
{
    union
    {
        int   i;
        float f;
    } u;
    u.i = i;
    return u.f;
}

/// Fast base-2 logarithm of a float
inline int iLog2(float f) { return ((floatAsInt(f) & 0x7f800000) >> 23) - 0x7f; }

/// Fast base-2 logarithm of an integer
template <typename T>
inline int iLog2(T f)
{
    return iLog2(float(f));
}

/// In-place pseudo-random number in [0,1)
/**
    Based on method described in the tech report:

    > Andrew Kensler. "Correlated Multi-Jittered Sampling",
    > Pixar Technical Memo 13-01.

    Modified to return 0.5f if p==0.
*/
inline float randf(unsigned i, unsigned p)
{
    if (p == 0)
        return 0.5f;
    i ^= p;
    i ^= i >> 17;
    i ^= i >> 10;
    i *= 0xb36534e5;
    i ^= i >> 12;
    i ^= i >> 21;
    i *= 0x93fc4795;
    i ^= 0xdf6e307f;
    i ^= i >> 17;
    i *= 1 | p >> 18;
    return i * (1.0f / 4294967808.0f);
}

inline uint32_t randu(int index, unsigned seed)
{
    unsigned result = index;
    result ^= seed;
    result ^= result >> 17;
    result ^= result >> 10;
    result *= 0xb36534e5;
    result ^= result >> 12;
    result ^= result >> 21;
    result *= 0x93fc4795;
    result ^= 0xdf6e307f;
    result ^= result >> 17;
    result *= 1 | seed >> 18;
    return result;
}

// https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
// updated to 64 bits.
inline uint64_t left_shift2(uint64_t x)
{
    x &= 0xffffffff;
    x = (x ^ (x << 16)) & 0x0000ffff0000ffff;
    x = (x ^ (x << 8)) & 0x00ff00ff00ff00ff;
    x = (x ^ (x << 4)) & 0x0f0f0f0f0f0f0f0f;
    x = (x ^ (x << 2)) & 0x3333333333333333;
    x = (x ^ (x << 1)) & 0x5555555555555555;
    return x;
}

inline uint64_t encode_Morton2(uint32_t x, uint32_t y) { return (left_shift2(y) << 1) | left_shift2(x); }

// Hashing Inline Functions
// http://zimbry.blogspot.ch/2011/09/better-bit-mixing-improving-on.html
inline uint64_t mix_bits(uint64_t v)
{
    v ^= (v >> 31);
    v *= 0x7fb5d329728ea185;
    v ^= (v >> 27);
    v *= 0x81dadef4bc2dd44d;
    v ^= (v >> 33);
    return v;
}

/// In-place enumeration of random permutations
/**
    Returns the `i`-th element of the `p`-th pseudo-random permutation of the
    numbers `0..(l-1)`.

    Based on method described in the tech report:

    > Andrew Kensler. "Correlated Multi-Jittered Sampling",
    > Pixar Technical Memo 13-01.

    Modified to return the identity permutation if `p==0`.
*/
inline unsigned permute(unsigned i, unsigned l, unsigned p)
{
    if (p == 0)
        return i;

    unsigned w = l - 1;
    w |= w >> 1;
    w |= w >> 2;
    w |= w >> 4;
    w |= w >> 8;
    w |= w >> 16;
    do {
        i ^= p;
        i *= 0xe170893d;
        i ^= p >> 16;
        i ^= (i & w) >> 4;
        i ^= p >> 8;
        i *= 0x0929eb3f;
        i ^= p >> 23;
        i ^= (i & w) >> 1;
        i *= 1 | p >> 27;
        i *= 0x6935fa69;
        i ^= (i & w) >> 11;
        i *= 0x74dcb303;
        i ^= (i & w) >> 2;
        i *= 0x9e501cc3;
        i ^= (i & w) >> 2;
        i *= 0xc860a3df;
        i &= w;
        i ^= i >> 5;
    } while (i >= l);

    return (i + p) % l;
}

inline float randomDigitScramble(float f, unsigned scramble)
{
    return (unsigned(f * 0x100000000LL) ^ scramble) * 2.3283064365386962890625e-10f;
}

inline unsigned LarcherPillichshammerRIU(unsigned n, unsigned scramble = 0)
{
    for (unsigned v = 1U << 31; n; n >>= 1, v |= v >> 1)
        if (n & 1)
            scramble ^= v;

    return scramble;
}

inline unsigned GruenschlossKellerRIU(unsigned n, unsigned scramble = 0)
{
    for (unsigned v2 = 3U << 30; n; n >>= 1, v2 ^= v2 >> 1)
        if (n & 1)
            scramble ^= v2 << 1; // vector addition of matrix column by XOR

    return scramble;
}

inline unsigned VanDerCorputRIU(unsigned n, unsigned scramble = 0)
{
    n = (n << 16) | (n >> 16);
    n = ((n & 0x00ff00ff) << 8) | ((n & 0xff00ff00) >> 8);
    n = ((n & 0x0f0f0f0f) << 4) | ((n & 0xf0f0f0f0) >> 4);
    n = ((n & 0x33333333) << 2) | ((n & 0xcccccccc) >> 2);
    n = ((n & 0x55555555) << 1) | ((n & 0xaaaaaaaa) >> 1);
    n ^= scramble;
    return n;
}

inline unsigned SobolRIU(unsigned n, unsigned scramble = 0)
{
    for (unsigned v = 1U << 31; n; n >>= 1, v ^= v >> 1)
        if (n & 0x1)
            scramble ^= v;
    return scramble;
}

inline float VanDerCorputRI(unsigned n, unsigned scramble = 0)
{
    // map back to [0,1)
    return VanDerCorputRIU(n, scramble) / float(0x100000000LL);
}

inline float SobolRI(unsigned n, unsigned scramble = 0)
{
    // map back to [0,1)
    return SobolRIU(n, scramble) / float(0x100000000LL);
}

inline float LarcherPillichshammerRI(unsigned n, unsigned scramble = 0)
{
    // map back to [0,1)
    return LarcherPillichshammerRIU(n, scramble) / float(0x100000000LL);
}

inline float GruenschlossKellerRI(unsigned n, unsigned scramble = 0)
{
    // map back to [0,1)
    return GruenschlossKellerRIU(n, scramble) / float(0x100000000LL);
}

inline float radicalInverse(int n, int base, float inv, unsigned perm[])
{
    float v = 0.0f;
    for (float p = inv; n != 0; p *= inv, n /= base) v += perm[n % base] * p;
    return v;
}

inline float radicalInverse(int n, int base, unsigned perm[]) { return radicalInverse(n, base, 1.0f / base, perm); }

inline float radicalInverse(int n, int base, float inv)
{
    float v = 0.0f;
    for (float p = inv; n != 0; p *= inv, n /= base) v += (n % base) * p;
    return v;
}

inline float radicalInverse(int n, int base) { return radicalInverse(n, base, 1.0f / base); }

template <typename P>
inline float foldedRadicalInverse(int n, int base, float inv, const P &perm)
{
    float    v         = 0;
    unsigned modOffset = 0;

    for (float p = inv; v + base * p != v; p *= inv, n /= base, ++modOffset) v += perm[(n + modOffset) % base] * p;

    return v;
}

template <typename P>
inline float foldedRadicalInverse(int n, int base, const P &perm)
{
    return foldedRadicalInverse(n, base, 1.0f / base, perm);
}

inline float foldedRadicalInverse(int n, int base, float inv)
{
    float    v         = 0;
    unsigned modOffset = 0;

    for (float p = inv; v + base * p != v; p *= inv, n /= base, ++modOffset) v += ((n + modOffset) % base) * p;

    return v;
}

inline float foldedRadicalInverse(int n, int base) { return foldedRadicalInverse(n, base, 1.0f / base); }
