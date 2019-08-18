/** \file RandomPermutation.h
    \author Wojciech Jarosz
*/
#pragma once

#include <pcg32.h>
#include <vector>

/// A class that encapsulates generation and access to random permutations
class RandomPermutation
{
public:
   RandomPermutation() {}
   RandomPermutation(uint32_t size) :
       m_X(size)
   {
       identity();
   }

   /// Create the identity permutation
   void identity()
   {
       for (size_t i = 0; i < m_X.size(); ++i)
           m_X[i] = i;
   }

   /// Returns the size of the permutation table
   uint32_t size() const {return m_X.size();}

   /// Resize the permutation table, but leaves it unintialized
   void resize(uint32_t size) {m_X.resize(size);}

   /// Access the i-th element in the permutted array
   int32_t operator[](const uint32_t i) const {return m_X[i];}

   /// Shuffle using the provided RNG
   void shuffle(pcg32 & rand) {rand.shuffle(m_X.begin(), m_X.end());}

protected:
   std::vector<uint32_t> m_X;
};