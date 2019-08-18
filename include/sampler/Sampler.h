/** \file Sampler.h
    \author Wojciech Jarosz
*/
#pragma once

#include <cmath>
#include <string>

/// Abstract class providing a common API to multi-dimensional (quasi-)random
/// number generators.
class Sampler
{
public:
    virtual ~Sampler() {}

    /// Reset/initialize the point set
    virtual void reset() { ; }

    /// Returns an appropriate grid resolution to help visualize stratification
    virtual int coarseGridRes(int samples) const { return int(std::sqrt(samples)); }

    /// Return the number of samples for finite point sets, or 0 for infinite
    /// sequences
    virtual int numSamples() const { return -1; }
    
    /// Set the number of samples for finite sequences
    /**
        Tries to set the total number of samples to `n`. For Samplers that
        require a certain number of smaples (e.g. jitter must be
        \f$ n \times m \f$), find the closest usable number of samples.

        Returns the resulting number of samples, or -1 for infinite sequences.
    */
    virtual int setNumSamples(unsigned n)
    {
        reset();
        return -1;
    }

    ///@{ \name Get/set the point set dimensionality
    virtual unsigned dimensions() const = 0;
    virtual void setDimensions(unsigned) { ; }
    virtual unsigned minDimensions() const = 0;
    virtual unsigned maxDimensions() const = 0;
    ///@}

    ///@{ \name Get/set whether the point set is randomized
    virtual bool randomized() const { return true; }
    virtual void setRandomized(bool r = true) { ; }
    ///@}


    ///@{ \name Get/set how much the points are jittered
    virtual float jitter() const { return 1.0f; }
    virtual float setJitter(float j = 1.0f) { return 1.0f; }
    ///@}

    /// Compute the `i`-th sample in the sequence and store in the `point` array
    virtual void sample(float point[], unsigned i) = 0;

    /// Return a human-readible name for the sampler
    virtual std::string name() const { return "Abstract Sampler"; }
};

/// Convenience template base class that defines the min and max dimensions
/**
    Inherit from this instead of Sampler if all the minimum and maximum
    dimensions for a point set are fixed at compile time. This is true for
    most of the samplers.
*/
template <unsigned MIN_D, unsigned MAX_D>
class TSamplerMinMaxDim : public Sampler
{
public:
    enum : unsigned
    {
        MIN_DIMENSION = MIN_D,
        MAX_DIMENSION = MAX_D
    };

    ~TSamplerMinMaxDim() override {}

    unsigned minDimensions() const override { return MIN_D; }
    unsigned maxDimensions() const override { return MAX_D; }

    std::string name() const override { return "Abstract TSamplerMinMaxDim"; }
};

/// Convenience template base class that defines the min, max, and current
/// dimensions
/**
    Inherit from this instead of Sampler if all the dimensions of the point set
    are known at compile time. For instance, Multi-jittered sampling only works
    for dimension = 2, so it inherits from TSamplerDim<2u>
*/
template <unsigned DIM> class TSamplerDim : public TSamplerMinMaxDim<DIM, DIM>
{
public:
    enum : unsigned
    {
        DIMENSION = DIM,
    };

    ~TSamplerDim() override {}

    /// Getters and setters for the point set dimension
    unsigned dimensions() const override { return DIM; }

    std::string name() const override { return "Abstract TSamplerDim"; }
};
