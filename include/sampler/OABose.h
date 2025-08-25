/** \file OABose.h
    \author Wojciech Jarosz
*/
#pragma once

#include <galois++/field.h>
#include <pcg32.h>
#include <sampler/OA.h>

/// Produces OA samples based on the construction by Bose (1938).
/**
    The OAs are of the form: \f$ OA(p^2, k, p, 2), \f$
    where \f$ k \le p+1 \f$, and \f$ p \f$ is a prime.

    This version constructs and stores the entire OA in memory.

    > R.C. Bose (1938) Sankhya Vol 3, pp 323-338.
 */
class BoseOA : public OrthogonalArray
{
public:
    BoseOA(unsigned, OffsetType ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f, unsigned dimensions = 2);
    ~BoseOA() override;
    void clear();

    unsigned setOffsetType(unsigned ot) override;
    unsigned setStrength(unsigned) override { return 2; }

    void reset() override;
    void sample(float[], unsigned i) override;

    unsigned dimensions() const override { return m_numDimensions; }
    void     setDimensions(unsigned d) override
    {
        m_numDimensions = d;
        reset();
    }

    std::string name() const override;

    int  numSamples() const override { return m_numSamples; }
    int  setNumSamples(unsigned n) override;
    void setNumSamples(unsigned x, unsigned);

protected:
    unsigned m_s, m_numSamples;
    unsigned m_numDimensions;

    float m_scale;

    unsigned **m_samples = nullptr;

    pcg32 m_rand;
};

/// Produces OA samples based on the construction by Bose (1938).
/**
    The OAs are of the form: \f$ OA(p^2, k, p, 2), \f$
    where \f$ k \le p+1 \f$, and \f$ p \f$ is a prime.

    This version constructs the samples "in-place", one sample at a time.

    > R.C. Bose (1938) Sankhya Vol 3, pp 323-338.
 */
class BoseOAInPlace : public OrthogonalArray
{
public:
    BoseOAInPlace(unsigned n, OffsetType ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f,
                  unsigned dimensions = 2);
    virtual ~BoseOAInPlace() {}

    virtual unsigned setStrength(unsigned) { return 2; }

    virtual void reset();
    virtual void sample(float[], unsigned i);

    virtual unsigned dimensions() const { return m_numDimensions; }
    virtual void     setDimensions(unsigned d)
    {
        m_numDimensions = d;
        reset();
    }

    virtual std::string name() const;

    virtual int  numSamples() const { return m_numSamples; }
    virtual int  setNumSamples(unsigned n);
    virtual void setNumSamples(unsigned x, unsigned y);

protected:
    unsigned m_s, m_numSamples, m_numDimensions;
    pcg32    m_rand;
};

// This is an attempt at doing nested sudoku patterns in arbitrary dimensions, but it doesn't work yet
class BoseSudokuInPlace : public BoseOAInPlace
{
public:
    BoseSudokuInPlace(unsigned n, OffsetType ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f,
                      unsigned dimensions = 2);

    void        sample(float[], unsigned i) override;
    std::string name() const override;
    int         setNumSamples(unsigned n) override;
    void        setNumSamples(unsigned x, unsigned y) override;

protected:
    unsigned m_numDigits = 1;
};

/// Produces OA samples based on the construction by Bose (1938).
/**
    The OAs are of the form: \f$ OA(q^2, k, q, 2), \f$
    where \f$ k \le q+1 \f$, and \f$ q \f$ is a prime power:
    \f$ q = p^r \f$ where \f$ p \f$ is a prime and \f$ r \f$ is a positive
    integer.

    This version constructs the samples "in-place", one sample at a time.

    > R.C. Bose (1938) Sankhya Vol 3, pp 323-338.
 */
class BoseGaloisOAInPlace : public BoseOAInPlace
{
public:
    BoseGaloisOAInPlace(unsigned n, OffsetType ot = CENTERED, uint32_t seed = 0, float jitter = 0.0f,
                        unsigned dimensions = 2);
    ~BoseGaloisOAInPlace() override {}

    unsigned setStrength(unsigned) override { return 2; }

    void sample(float[], unsigned i) override;

    std::string name() const override;
    int         setNumSamples(unsigned n) override;

protected:
    Galois::Field m_gf;
};