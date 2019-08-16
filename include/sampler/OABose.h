/*! \file OABose.h
    \brief
    \author Wojciech Jarosz
*/
#pragma once

#include <sampler/OA.h>
#include <pcg32.h>
#include <galois++/field.h>

/**
   Produces OA samples based on construction by:
   
      R.C. Bose (1938) Sankhya Vol 3, pp 323-338.
   
   The OAs are of the form:
   
      OA(p^2, k, p, 2),
   
   where k <= p+1, and p is a prime.
   
   This version constructs and stores the entire OA in memory.
 */
class BoseOA : public OrthogonalArray
{
public:
    BoseOA(unsigned, OffsetType ot = CENTERED, bool randomize = false,
           float jitter = 0.0f, unsigned dimensions = 2);
    ~BoseOA() override;
    void clear();

    unsigned setOffsetType(unsigned ot) override;
    unsigned setStrength(unsigned) override { return 2; }

    void reset() override;
    void sample(float[], unsigned i) override;

    unsigned dimensions() const override { return m_numDimensions; }
    void setDimensions(unsigned d) override
    {
        m_numDimensions = d;
        reset();
    }

    std::string name() const override;

    int numSamples() const override { return m_numSamples; }
    int setNumSamples(unsigned n) override;
    void setNumSamples(unsigned x, unsigned);

protected:
    unsigned m_s, m_numSamples;
    unsigned m_numDimensions;

    float m_scale;

    unsigned** m_samples = nullptr;

    pcg32 m_rand;
};


/**
   Produces OA samples based on construction by:
   
      R.C. Bose (1938) Sankhya Vol 3, pp 323-338.
   
   The OAs are of the form:
   
      OA(p^2, k, p, 2),
   
   where k <= p+1, and p is a prime.
   
   This version constructs the samples "in-place", one sample at a time.
 */
class BoseOAInPlace : public OrthogonalArray
{
public:
    BoseOAInPlace(unsigned n, OffsetType ot = CENTERED, bool randomize = false,
                  float jitter = 0.0f, unsigned dimensions = 2);
    virtual ~BoseOAInPlace() {}

    virtual unsigned setStrength(unsigned) { return 2; }

    virtual void reset();
    virtual void sample(float[], unsigned i);

    virtual unsigned dimensions() const { return m_numDimensions; }
    virtual void setDimensions(unsigned d)
    {
        m_numDimensions = d;
        reset();
    }

    virtual std::string name() const;

    virtual int numSamples() const { return m_numSamples; }
    virtual int setNumSamples(unsigned n);
    virtual void setNumSamples(unsigned x, unsigned y);

protected:
    unsigned m_s, m_numSamples, m_numDimensions;
    pcg32 m_rand;
};


/**
   Produces OA samples based on construction by:
   
      R.C. Bose (1938) Sankhya Vol 3, pp 323-338.
   
   The OAs are of the form:
   
      OA(q^2, k, q, 2),
   
   where k <= q+1, and q is a prime power: q = p^r where
   p is a prime and r is a positive integer.
   
   This version constructs the samples "in-place", one sample at a time.
 */
class BoseGaloisOAInPlace : public BoseOAInPlace
{
public:
    BoseGaloisOAInPlace(unsigned n, OffsetType ot = CENTERED,
                        bool randomize = false, float jitter = 0.0f,
                        unsigned dimensions = 2);
    ~BoseGaloisOAInPlace() override {}

    unsigned setStrength(unsigned) override { return 2; }

    void sample(float[], unsigned i) override;

    std::string name() const override;
    int setNumSamples(unsigned n) override;

protected:
    Galois::Field m_gf;
};