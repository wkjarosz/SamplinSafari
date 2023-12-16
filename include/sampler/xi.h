//
// The below code is taken from Abdalla Ahmed's supplemental code available on his publication project page (no license was provided).
// Modified by Wojciech Jarosz, 17.12.2023:
// - Morton function reimplemented to avoid x86-specific intrinsics <immintrin.h>, allowing the code to work also on ARM-based macs.
//
/*
 * Generate xi sequence.
 *
 * Points are retrieved as 32-bit unsigned integers,
 *  interpreted as fixed-point fractions.
 *
 * 2021-03-06: Created by Abdalla Ahmed from earlier versions.
 * 2021-03-07: Rewritten as a class.
 * 2021-03-09: Retired the nest-randomized version, in favor of the elegant
 *  and neat original single-point version. Variants are found in archive.
 * 2021-03-22: Moved the code to a header file for reuse.
 * 2021-03-28: Restored the p0 choice, and replace p1 by a.
 */

#ifndef XI_H
#define XI_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
// #include <immintrin.h>



inline uint32_t bitShuffle(uint32_t x) {                                        // From Hacker's delight
    uint32_t t;
    t = (x ^ (x >> 8)) & 0x0000FF00; x = x ^ t ^ (t << 8);
    t = (x ^ (x >> 4)) & 0x00F000F0; x = x ^ t ^ (t << 4);
    t = (x ^ (x >> 2)) & 0x0C0C0C0C; x = x ^ t ^ (t << 2);
    t = (x ^ (x >> 1)) & 0x22222222; x = x ^ t ^ (t << 1);
    return x;
}

inline uint32_t morton(uint32_t x, uint32_t y) {                                // Return a Morton (z) ordering index by interleaving the bits of y and x, each is a 16-bit unsigned int
    // return _pdep_u32(x, 0x55555555) | _pdep_u32(y, 0xAAAAAAAA);

    // modified 17.12.2023 by Wojciech Jarosz.
    // Avoiding x86 intrinsics using the Binary Magic Numbers morton method from
    // https://graphics.stanford.edu/~seander/bithacks.html  
    static const unsigned int B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF};
    static const unsigned int S[] = {1, 2, 4, 8};

    // Interleave lower 16 bits of x and y, so the bits of x
    // are in the odd positions and bits from y in the even;
    // z gets the resulting 32-bit Morton Number.  
    // x and y must initially be less than 65536.
    uint32_t z; 
                

    y = (y | (y << S[3])) & B[3];
    y = (y | (y << S[2])) & B[2];
    y = (y | (y << S[1])) & B[1];
    y = (y | (y << S[0])) & B[0];

    x = (x | (x << S[3])) & B[3];
    x = (x | (x << S[2])) & B[2];
    x = (x | (x << S[1])) & B[1];
    x = (x | (x << S[0])) & B[0];

    z = y | (x << 1);
    return z;
}

inline uint32_t bitReverse(uint32_t x) {
    x = (x << 16) | (x >> 16);
    x = ((x & 0x00ff00ff) << 8) | ((x & 0xff00ff00) >> 8);
    x = ((x & 0x0f0f0f0f) << 4) | ((x & 0xf0f0f0f0) >> 4);
    x = ((x & 0x33333333) << 2) | ((x & 0xcccccccc) >> 2);
    x = ((x & 0x55555555) << 1) | ((x & 0xaaaaaaaa) >> 1);
    return x;
}

inline unsigned byteUnshuffle(uint32_t x) {                                     // AaBbCcDd --> ABCDabcd
    x = ((x & 0x22) << 1) | ((x >> 1) & 0x22) | (x & 0x99);                     // .B...D.. | ..a...c. | A..bC..d = ABabCDcd
    x = ((x & 0x0C) << 2) | ((x >> 2) & 0x0C) | (x & 0xC3);                     // ..CD.... | ....ab.. | AB....cd = ABCDabcd
    return x;
}

struct Vector {
    uint32_t x, y;
    inline uint32_t& operator[](uint32_t i) { return ((uint32_t *)this)[i]; };
    inline Vector operator^(const Vector   &v) { return {x ^ v.x, y ^ v.y}; };
    inline Vector operator&(const Vector   &v) { return {x & v.x, y & v.y}; };
    inline Vector operator|(const Vector   &v) { return {x | v.x, y | v.y}; };
    inline Vector operator^(const uint32_t &v) { return {x ^ v  , y ^ v  }; };
    inline Vector operator&(const uint32_t &v) { return {x & v  , y & v  }; };
    inline Vector operator|(const uint32_t &v) { return {x | v  , y | v  }; };
    inline Vector operator>>(const int &i)     { return {x >> i , y >> i }; };
    inline Vector operator<<(const int &i)     { return {x << i , y << i }; };
    inline void operator^= (const Vector &v) { x ^= v.x; y ^= v.y; };
    inline void operator&= (const Vector &v) { x &= v.x; y &= v.y; };
    inline void operator|= (const Vector &v) { x |= v.x; y |= v.y; };
    inline void operator^= (const uint32_t &v) { x ^= v; y ^= v; };
    inline void operator&= (const uint32_t &v) { x &= v; y &= v; };
    inline void operator|= (const uint32_t &v) { x |= v; y |= v; };
    inline void operator>>=(const int &i) { x >>= i; y >>= i; };
    inline void operator<<=(const int &i) { x <<= i; y <<= i; };
    inline bool operator==(Vector v) { return x == v.x && y == v.y; }
    inline bool operator!=(Vector v) { return x != v.x || y != v.y; }
    inline unsigned getQuadrant() {                                             // Return the Morton-ordering quadrant to which this vector takes
        return ((y >> 30) & 2) | (x >> 31);
    }
};

typedef Vector Point;                                                           // An alias

class Xi {
protected:
    Vector u[4];                                                                // Bases vectors the define the sequence; u[depth][quadrant]
    Point p0;                                                                   // Needed in inversion.
    uint32_t U[4];                                                              // Inversion vectors from Morton to seqNo
    uint32_t u_z[4];                                                            // Bases for computing Morton codes
    uint32_t id;                                                             // For debugging
public:
    static uint32_t ximul(uint32_t x);                                          // Offered publicly as a utility. It actually is multiplying by 2xi
    static const uint32_t xi = 0x68808000;                                      // Offered as a public constant
    Xi(
        Vector a = {0x80000000, 0x80000000},
        Point p0 = {0, 0},
        uint32_t id = 0
    );                                                                          // Creates a xi sequence using the given vector a and first point p0.
    Point operator[](uint32_t seqNo);                                           // Retrieve a point given its index
    uint32_t getSeqNo(uint32_t X, uint32_t Y, int m);                           // Return the sequence number of the first point in stratum (X, Y) at resolution 2^m, and return the exact point coordinates in p
    uint32_t getSeqNo(uint32_t x, uint32_t y);                                  // Return the sequence number of the sample whose coordinates are (x, y). Only the top 16 bits matter.
    uint32_t getSeqNo(uint32_t z);
    uint32_t getMorton(uint32_t seqNo);                                         // Return the Morton index of the given seqNo
    void printMatrix(FILE *file);                                               // Print the underlying production matrix.
    uint32_t getID() { return id; };
    void printData(FILE *file);
};

/*
 * Obtaining u0 from p0:
 *   p0         = sum_{i= 0}^{15} (u0 >> i)   = u_0 + sum_{i=1}^{15} (u0 >> i)
 * + p0 >> 1    = sum_{i= 1}^{16} (u0 >> i)   =       sum_{i=1}^{15} (u0 >> i) + (u0 >> 16)
 * + p0 >> 16   = sum_{i=16}^{31} (u0 >> i)   =                                  (u0 >> 16) + sum_{i=17}^{31} (u0 >> i)
 * + p0 >> 17   = sum_{i=17}^{32} (u0 >> i)   =                                               sum_{i=17}^{31} (u0 >> i) + (u0 >> 32)
 *   -------------------------------------------------------------------------------------------------------------------------------
 *   p0 + (p0 >> 1) + (p0 >> 16) + (p0 >> 17) = u0                                                                      + (u0 >> 32)
 * The last term is 0 in 32-bit resolution.
 */

Xi::Xi (Vector a, Point p0, uint32_t id) : p0(p0), id(id) {                     // Creates a self-similar xi sequence using the given vector p1-p0 and point p0.
    a |= {0x80000000, 0x80000000};                                              // Evenly indexed columns. Just in case p1 is generated randomly or by enumeration.
    Vector b = {ximul(a.x), ximul(a.y) ^ a.y};                                  // Oddly indexed columns; note adding a in y, equivalent to multiplying by 1 + 2xi
    u[0] = p0 ^ (p0 >> 1) ^ (p0 >> 16) ^ (p0 >> 17);                            // Obtained by solving {p0 = sum_{i=0}^{15} (u0 >> i)} for u0
    u[1] = u[0] ^ a;
    u[2] = u[0] ^ b;
    u[3] = u[1] ^ b;
    uint32_t A(0x40000000), B(0x80000000);
    uint32_t a_z = morton(a.y >> 16, a.x >> 16) & 0x7FFFFFFF;                   // Note that we make an outer shuffle: x-bit first in each pair,
    uint32_t b_z = morton(b.y >> 16, b.x >> 16) & 0x3FFFFFFF;                   //  to account for the preliminary row swap
    for (int i = 0; i < 32; i += 2) {
        if (A & (1 << (31 - i))) A ^= a_z >> i;
        if (B & (1 << (31 - i))) B ^= a_z >> i;
        if (A & (1 << (30 - i))) A ^= b_z >> i;
        if (B & (1 << (30 - i))) B ^= b_z >> i;
    }
    A = bitReverse(A); B = bitReverse(B);                                       // The bit ordering is reversed between Morton index and sequence number.
    U[0] = 0;
    U[1] = B;
    U[2] = A;
    U[3] = A ^ B;
}

inline uint32_t Xi::ximul(uint32_t x) {
    return (x >> 1) ^ (x >> 2) ^ (x >> 4) ^ (x >> 8) ^ (x >> 16);
}

inline Point Xi::operator[](uint32_t seqNo) {
    Point p = {0, 0};
    for (int depth = 0; depth < 16; depth++) {
        p ^= u[seqNo & 3] >> depth;                                             // Index the appropriate vector using the quadrant, seqNo & 3
        seqNo >>= 2;                                                            // Recursively subdivide the quadrant
    }
    return p;
}

inline uint32_t Xi::getSeqNo(uint32_t X, uint32_t Y, int depth) {
    uint32_t mask((1ull << (2 * depth)) - 1);
    return getSeqNo(X << (32 - depth), Y << (32 - depth)) & mask;
}

inline uint32_t Xi::getSeqNo(uint32_t x, uint32_t y) {
    x ^= p0.x; y ^= p0.y;                                                       // Undo xor-scrambling, if any.
    //uint32_t z = bitShuffle((y & 0xFFFF0000) | (x >> 16));
    uint32_t z = morton(x >> 16, y >> 16);
    return getSeqNo(z);
}

inline uint32_t Xi::getSeqNo(uint32_t z) {
    uint32_t seqNo(0);
    for (int bit = 0; bit < 32; bit += 2) {
        seqNo ^= U[(z >> (30 - bit)) & 3] << bit;
    }
    return seqNo;
}

void Xi::printData(FILE *file) {
    fprintf(
        file,
        "ID = %d, "
        "u = {(%08X, %08X), (%08X, %08X), (%08X, %08X), (%08X, %08X)}, "
        "p0 = (%08X, %08X)\n"
        , id, u[0].x, u[0].y, u[1].x, u[1].y, u[2].x, u[2].y, u[3].x, u[3].y
        , p0.x, p0.y
    );
}

void Xi::printMatrix(FILE *file) {
    uint32_t ax = u[0].x ^ u[1].x;
    uint32_t ay = u[0].y ^ u[1].y;
    uint32_t bx = u[0].x ^ u[2].x;
    uint32_t by = u[0].y ^ u[2].y;
    fprintf(stderr, "ax = %08X, bx = %08X\n", ax, bx);
    fprintf(file, "C_x:%32sC_y:%32sC_Morton:\n", "", "");
    char digits[] = {'.', '1'};
    for (int row = 0; row < 32; row++) {
        for (int digit = 0; digit < 16; digit++) {
            if (digit > row) {
                fprintf(file, "..");
            }
            else {
                fprintf(
                    file, "%c%c",
                    digits[(ax >> (31 + digit - row)) & 1],
                    digits[(bx >> (31 + digit - row)) & 1]
                );
            }
        }
        fprintf(file, "    ");
        for (int digit = 0; digit < 16; digit++) {
            if (digit > row) {
                fprintf(file, "..");
            }
            else {
                fprintf(
                    file, "%c%c",
                    digits[(ay >> (31 + digit - row)) & 1],
                    digits[(by >> (31 + digit - row)) & 1]
                );
            }
        }
        fprintf(file, "    ");
        int rowXOrY = row / 2;                                                  // Row in the y or x matrix.
        for (int digit = 0; digit < 16; digit++) {
            if (digit > rowXOrY) {
                fprintf(file, "..");
            }
            else {
                if (row & 1) {                                                  // Odd row? use x
                    fprintf(
                        file, "%c%c",
                        digits[(ax >> (31 + digit - rowXOrY)) & 1],
                        digits[(bx >> (31 + digit - rowXOrY)) & 1]
                    );
                }
                else {
                    fprintf(
                        file, "%c%c",
                        digits[(ay >> (31 + digit - rowXOrY)) & 1],
                        digits[(by >> (31 + digit - rowXOrY)) & 1]
                    );
                }
            }
        }
        fprintf(file, "\n");
    }
}

class Xi256 : public Xi {
protected:
    Vector u8[265];                                                             // Byte-long lookup table.
    uint32_t z2s_11[256];                                                       // Inversion table.
    uint32_t z2s_44[256];                                                       // An inversion table with bits interleaved up to 4-bits level only.
    uint32_t z2sr_44[256];                                                      // This will produce s in a bit-reversed order.
public:
    Xi256(
        Vector a = {0x80000000, 0x80000000},
        Point p0 = {0, 0},
        uint32_t id = 0
    );                                                                          // Creates a xi sequence using the given vector a and first point p0.
    Point operator[](uint32_t seqNo);
    uint32_t getSeqNo(uint32_t z);                                              // Given a Morton index.
    uint32_t getSeqNo(uint32_t X, uint32_t Y, int m);                           // Given stratum coordinates and resolution.
    uint32_t getSeqNo(uint32_t x, uint32_t y);                                  // Given stratum coordinates, using 32-bit resolution.
    uint32_t getSeqNoReversed(uint32_t x, uint32_t y);                          // Bit-reversed sequence number
    void printData(FILE *file);
};

Xi256::Xi256(Vector a, Point p0, uint32_t id) : Xi(a, p0, id) {
    for (unsigned i = 0; i < 256; i++) {
        u8[i] = (
            (u[ i       & 3]     ) ^
            (u[(i >> 2) & 3] >> 1) ^
            (u[(i >> 4) & 3] >> 2) ^
            (u[(i >> 6) & 3] >> 3)
        );
        z2s_11[i] = (
            (U[(i >> 6) & 3]     ) ^ (U[(i >> 4) & 3] << 2) ^
            (U[(i >> 2) & 3] << 4) ^ (U[(i     ) & 3] << 6)
        );
        z2s_44[ byteUnshuffle(i) ] = z2s_11[i];
    }
    for (unsigned i = 0; i < 256; i++) {
        z2sr_44[i] = bitReverse(z2s_44[i]);
    }
}

inline Point Xi256::operator[](uint32_t seqNo) {
    return {
        (u8[(seqNo      ) & 0xff]      ) ^
        (u8[(seqNo >>  8) & 0xff] >>  4) ^
        (u8[(seqNo >> 16) & 0xff] >>  8) ^
        (u8[(seqNo >> 24)       ] >> 12)
    };
}

inline uint32_t Xi256::getSeqNo(uint32_t z) {
    return (
        (z2s_11[(z >> 24)       ]      ) ^
        (z2s_11[(z >> 16) & 0xff] <<  8) ^
        (z2s_11[(z >>  8) & 0xff] << 16) ^
        (z2s_11[ z        & 0xff] << 24)
    );
}

inline uint32_t Xi256::getSeqNo(uint32_t X, uint32_t Y, int depth) {
    uint32_t mask((1 << (2 * depth)) - 1);
    return getSeqNo(X << (32 - depth), Y << (32 - depth)) & mask;
}


// inline uint32_t Xi256::getSeqNo(uint32_t x, uint32_t y) {
//     x ^= p0.x; y ^= p0.y;                                                       // Undo xor-scrambling, if any.
//     x >>= 4;                                                                    // Align x properly with y to use the same shifts
//     return (
//         (z2s_44[ ((y >> 24) & 0xf0) | ((x >> 24)       ) ]      ) ^
//         (z2s_44[ ((y >> 20) & 0xf0) | ((x >> 20) & 0x0f) ] <<  8) ^
//         (z2s_44[ ((y >> 16) & 0xf0) | ((x >> 16) & 0x0f) ] << 16) ^
//         (z2s_44[ ((y >> 12) & 0xf0) | ((x >> 12) & 0x0f) ] << 24)
//     );
// }

inline uint32_t Xi256::getSeqNo(uint32_t x, uint32_t y) {
    x ^= p0.x; y ^= p0.y;                                                       // Undo xor-scrambling, if any.
    uint32_t z = morton(x >> 16, y >> 16);
    return getSeqNo(z);
}

inline uint32_t Xi256::getSeqNoReversed(uint32_t x, uint32_t y) {
    x ^= p0.x; y ^= p0.y;                                                       // Undo xor-scrambling, if any.
    y <<= 4;                                                                    // Align y properly with x to use the same shifts
    return (
        (z2sr_44[ ((y >> 12) & 0xf0) | ((x >> 12) & 0x0f) ]      ) ^
        (z2sr_44[ ((y >>  8) & 0xf0) | ((x >>  8) & 0x0f) ] >>  8) ^
        (z2sr_44[ ((y >>  4) & 0xf0) | ((x >>  4) & 0x0f) ] >> 16) ^
        (z2sr_44[ ((y      ) & 0xf0) | ((x      ) & 0x0f) ] >> 24)
    );
}

void Xi256::printData(FILE *file) {
    Xi::printData(file);
    for (int i = 0; i < 256; i++) {
        fprintf(file, "u[%3d] = (%08X, %08X)\n", i, u8[i].x, u8[i].y);
    }
}

#endif
