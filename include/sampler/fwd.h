/**
   \file fwd.h
   \author Wojciech Jarosz (wjarosz@gmail.com)
   \brief Forward declarations
   \date 2019-08-12
   
   @copyright Copyright (c) 2019
 */

#pragma once

/* Forward declarations */
class Sampler;
template <unsigned MIN_D, unsigned MAX_D> class TSamplerMinMaxDim;
template <unsigned DIM> class TSamplerDim;
class RandomPermutation;
class Halton;
class HaltonZaremba;
template <typename BaseHalton> class Hammersley;
class Jittered;
class LarcherPillichshammerGK;
class MultiJittered;
class MultiJitteredInPlace;
class CorrelatedMultiJittered;
class CorrelatedMultiJitteredInPlace;
class NRooks;
class NRooksInPlace;
class OrthogonalArray;
class BoseOA;
class BoseOAInPlace;
class BoseGaloisOAInPlace;
class AddelmanKempthorneOAInPlace;
class BushOAInPlace;
class BoseBushOA;
class CMJNDInPlace;
class Random;
class StrongOrthogonalArray;
class Sobol;
class ZeroTwo;
