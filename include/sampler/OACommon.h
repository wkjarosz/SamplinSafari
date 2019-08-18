/*!
   \file OACommon.h
   \author Wojciech Jarosz
   \date 2019-08-15   

   Self-contained versions of Bose, Bush and CMJ constructions. Slightly
   improved/corrected versions of the pseudo-code in the EGSR 2019 publication:

        W. Jarosz, A. Enayet, A. Kensler, C. Kilpatrick, P. Christensen.
            "Orthogonal Array Sampling for Monte Carlo Rendering."
            Computer Graphics Forum (Proceedings of EGSR), 38(4), July 2019.
 */
#pragma once

/**
    Strength-2 orthogonal arrays using the Bose construction.
    
    This version computes a single dimension `j` of sample `i`.
 */
float boseOA(unsigned i,   ///< sample index
             unsigned j,   ///< dimension (< s+1)
             unsigned s,   ///< number of levels/strata
             unsigned p,   ///< pseudo-random permutation seed
             unsigned ot   ///< CENTERED = 0, J_STYLE = 1, MJ_STYLE = 2, or CMJ_STYLE = 3
             );

/**
    Strength-t orthogonal arrays using the Bush construction.
    
    This version computes all `d` dimensions of sample `i` at once.
 */
void boseOA(float Xi[],   ///< pointer to float array for output
            unsigned i,   ///< sample index
            unsigned d,   ///< number of dimensions (<= s+1)
            unsigned s,   ///< number of levels/strata
            float maxJit, ///< amount to jitter within substrata
            unsigned p,   ///< pseudo-random permutation seed
            unsigned ot   ///< CENTERED = 0, J_STYLE = 1, MJ_STYLE = 2, or CMJ_STYLE = 3
            );

/**
    Strength-t orthogonal arrays using the Bush construction.
    
    This version computes a single dimension `j` of sample `i`.
 */
float bushOA(unsigned i,   ///< sample index
             unsigned j,   ///< dimension (< s)
             unsigned s,   ///< number of levels/strata
             unsigned t,   ///< strength of OA (0 < t <= d)
             float maxJit, ///< amount to jitter within substrata
             unsigned p,   ///< pseudo-random permutation seed
             unsigned ot   ///< J = 1 or MJ = 2
             );

/**
    Strength-t orthogonal arrays using the Bush construction.
    
    This version computes all `d` dimensions of sample `i` at once.
 */
void bushOA(float Xi[],   ///< pointer to float array for output
            unsigned i,   ///< sample index
            unsigned d,   ///< number of dimensions (<= s+1)
            unsigned s,   ///< number of levels/strata
            unsigned t,   ///< strength of OA (0 < t <= d)
            float maxJit, ///< amount to jitter within substrata
            unsigned p,   ///< pseudo-random permutation seed
            unsigned ot   ///< J = 1 or MJ = 2
            );

/// Generalization of Kensler's CMJ pattern to higher dimensions.
float cmjdD(unsigned i,   ///< sample index
            unsigned j,   ///< dimension (<= s+1)
            unsigned s,   ///< number of levels/strata
            unsigned t,   ///< strength of OA (t=d)
            float maxJit, ///< amount to jitter within substrata
            unsigned p);  ///< pseudo-random permutation seed

/// Generalization of Kensler's CMJ pattern to higher dimensions.
void cmjdD(float Xi[],   ///< pointer to float array for output
           unsigned i,   ///< sample index
           unsigned s,   ///< number of levels/strata
           unsigned t,   ///< strength of OA (t=d)
           float maxJit, ///< amount to jitter within substrata
           unsigned p);  ///< pseudo-random seed