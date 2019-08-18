/** \file Misc.cpp
    \author Wojciech Jarosz
*/

#include <sampler/Misc.h>
#include <sampler/RandomPermutation.h>
#include <galois++/element.h>
#include <vector>
#include <algorithm>
#include <stdexcept>

using namespace std;


// Compute the polynomial coefficients by taking the digits
// of the sample index i expressed in the base
vector<int> iToPolyCoeffs(unsigned i, unsigned base, unsigned degree)
{
    if (base == 0)
        throw invalid_argument("`base` cannot be 0");

    vector<int> coeffs(degree);
    for (unsigned ii = 0; ii < degree; i /= base, ++ii)
        coeffs[ii] = i % base;
    return coeffs;
}


// Evaluate the polynomial with coefficients coeffs at argument location arg
unsigned polyEval(const vector<int> & coeffs, unsigned arg)
{
    unsigned ans = 0;
    for (size_t l = coeffs.size(); l--; )
        ans = (ans * arg) + coeffs.at(l);        // Horner's rule
    return ans;
}

// Galois field version of above
unsigned polyEval(const Galois::Field * gf, vector<int> & coeffs, int arg)
{
    Galois::Element ans(gf, 0);
    for (size_t l = coeffs.size(); l--; ) // Horner's rule
        ans = (ans * arg) + coeffs.at(l);
    return unsigned(ans.value());
}

unsigned polyEvalExceptOne(const vector<int> &coeffs, int base, size_t
                           ignoreIdx)
{
    size_t i = 0;
    unsigned res = 0;

    for (size_t j = 0; j < coeffs.size(); j++)
    {
        if (j == ignoreIdx) continue;
        res += coeffs[i] * unsigned(pow(base, i));
        i++;
    }
    return res;
}
