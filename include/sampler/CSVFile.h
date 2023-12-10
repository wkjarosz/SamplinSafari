/** \file NRooks.h
    \author Wojciech Jarosz
*/
#pragma once

#include <galois++/array2d.h>
#include <sampler/Sampler.h>
#include <string_view>

/// A sampler that reads its data from a comma separated value (CSV) file
class CSVFile : public TSamplerMinMaxDim<1, (unsigned)-1>
{
public:
    CSVFile(const std::string &filename = "");

    bool read(const std::string &filename, const std::string_view &data = std::string_view());

    void sample(float[], unsigned i) override;

    unsigned dimensions() const override
    {
        return m_numDimensions;
    }

    int numSamples() const override
    {
        return m_numSamples;
    }
    int setNumSamples(unsigned n) override
    {
        return m_numSamples;
    }

    std::string name() const override;

protected:
    std::vector<float> m_values;
    unsigned           m_numDimensions = 0;
    unsigned           m_numSamples    = 0;
    std::string        m_filename;
};
