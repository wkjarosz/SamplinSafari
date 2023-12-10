/** \file Random.cpp
    \author Wojciech Jarosz
*/

#include <assert.h>
#include <fstream>
#include <iostream>
#include <sampler/CSVFile.h>
#include <stdio.h>

using std::string;
using std::string_view;
using std::vector;

namespace
{

// from https://stackoverflow.com/a/116220 (CC BY-SA 4.0)
string read_file_to_string(string_view path)
{
    constexpr auto read_size = std::size_t(4096);
    auto           stream    = std::ifstream(path.data());
    stream.exceptions(std::ios_base::badbit);

    if (not stream)
        throw std::ios_base::failure("file does not exist");

    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size))
        out.append(buf, 0, stream.gcount());
    out.append(buf, 0, stream.gcount());
    return out;
}

// -------------------------------------
// The below functions are adapted from:
// -------------------------------------
// Dear ImGui Test Engine License (v1.03)
// Copyright (c) 2018-2023 Omar Cornut

// This document is a legal agreement ("License") between you ("Licensee") and
// DISCO HELLO ("Licensor") that governs your use of Dear ImGui Test Engine ("Software").

// 1. LICENSE MODELS

// 1.1. Free license

// The Licensor grants you a free license ("Free License") if you meet ANY of the following
// criterion:

// - You are a natural person;
// - You are not a legal entity, or you are a not-for-profit legal entity;
// - You are using this Software for educational purposes;
// - You are using this Software to create Derivative Software released publicly and under
//   an Open Source license, as defined by the Open Source Initiative;
// - You are a legal entity with a turnover inferior to 2 million USD (or equivalent) during
//   your last fiscal year.

// 1.2. Paid license

// If you do not meet any criterion of Article 1.1, Licensor grants you a trial period of a
// maximum of 45 days, at no charge. Following this trial period, you must subscribe to a paid
// license ("Paid License") with the Licensor to continue using the Software.
// Paid Licenses are exclusively sold by DISCO HELLO. Paid Licenses and the associated
// information are available at the following URL: http://www.dearimgui.com/licenses

// 2. GRANT OF LICENSE

// 2.1. License scope

// A limited and non-exclusive license is hereby granted, to the Licensee, to reproduce,
// execute, publicly perform, and display, use, copy, modify, merge, distribute, or create
// derivative works based on and/or derived from the Software ("Derivative Software").

// 2.2. Right of distribution

// License holders may also publish and/or distribute the Software or any Derivative
// Software. The above copyright notice and this license shall be included in all copies
// or substantial portions of the Software and/or Derivative Software.

// License holders may add their own attribution notices within the Derivative Software
// that they distribute. Such attribution notices must not directly or indirectly imply a
// modification of the License. License holders may provide to their modifications their
// own copyright and/or additional or different terms and conditions, providing such
// conditions complies with this License.

// 3. DISCLAIMER

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
// OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//-----------------------------------------------------------------------------
// Helper: Simple/dumb CSV parser
//-----------------------------------------------------------------------------

struct CSVParser
{
    // Public fields
    int Columns = 0; // Number of columns in CSV file.
    int Rows    = 0; // Number of rows in CSV file.

    // Internal fields
    string               _Filename; // The filename this was loaded from
    vector<char>         _Data;     // CSV file data.
    vector<const char *> _Index;    // CSV table: _Index[row * _Columns + col].

    // Functions
    CSVParser(int columns = -1) : Columns(columns)
    {
    }
    ~CSVParser()
    {
        clear();
    }
    bool        load_from(const string &filename, string_view _Data); // parse CSV file stored in the string_view
    void        clear();                                              // Free allocated buffers.
    const char *cell(int row, int col)
    {
        assert(0 <= row && row < Rows && 0 <= col && col < Columns);
        return _Index[row * Columns + col];
    }
};

void CSVParser::clear()
{
    Rows = Columns = 0;
    _Filename      = "";
    _Data.clear();
    _Index.clear();
}

bool CSVParser::load_from(const string &filename, string_view csv_data)
{
    _Filename = filename;
    _Data     = vector<char>{csv_data.begin(), csv_data.end()};
    // pad with one extra zero
    _Data.push_back('\0');

    int columns = 1;
    if (Columns > 0)
    {
        columns = Columns; // User-provided expected column count.
    }
    else
    {
        for (const char *c = _Data.data(); *c != '\n' && *c != '\0';
             c++) // Count columns. Quoted columns with commas are not supported.
            if (*c == ',')
                columns++;
    }

    // Count rows. Extra new lines anywhere in the file are ignored.
    int max_rows = 0;
    for (const char *c = _Data.data(), *end = c + _Data.size() - 1; c < end; c++)
        if ((*c == '\n' && c[1] != '\r' && c[1] != '\n') || *c == '\0')
            max_rows++;

    if (columns == 0 || max_rows == 0)
        return false;

    // Create index
    _Index.resize(columns * max_rows);

    int         col      = 0;
    const char *col_data = _Data.data();
    for (char *c = _Data.data(); *c != '\0'; c++)
    {
        const bool is_comma = (*c == ',');
        const bool is_eol   = (*c == '\n' || *c == '\r');
        const bool is_eof   = (*c == '\0');
        if (is_comma || is_eol || is_eof)
        {
            _Index[Rows * columns + col] = col_data;
            col_data                     = c + 1;
            if (is_comma)
            {
                col++;
            }
            else
            {
                if (col + 1 == columns)
                    Rows++;
                else
                    fprintf(stderr, "%s: Unexpected number of columns on line %d, ignoring.\n", _Filename.c_str(),
                            Rows + 1); // FIXME
                col = 0;
            }
            *c = 0;
            if (is_eol)
                while (c[1] == '\r' || c[1] == '\n')
                    c++;
        }
    }

    Columns = columns;
    return true;
}

// -------------------------------------
// end Dear ImGui Test Engine code
// -------------------------------------

} // namespace

CSVFile::CSVFile(const string &filename)
{
    if (filename.empty())
        return;

    if (!read(filename))
        std::cerr << "Error reading file: " << filename << std::endl;
}

bool CSVFile::read(const string &filename, const string_view &csv_data)
{
    m_filename = filename;

    CSVParser parser;
    if (!parser.load_from(filename, csv_data.empty() ? read_file_to_string(filename) : csv_data))
        return false;

    m_numDimensions = parser.Columns;
    m_numSamples    = parser.Rows;

    m_values.resize(m_numDimensions * m_numSamples);

    // Read point data from CSV
    int i = 0;
    for (int row = 0; row < parser.Rows; ++row)
    {
        for (int col = 0; col < parser.Columns; ++col, ++i)
        {
            assert(i < (int)m_values.size());
            sscanf(parser.cell(row, col), "%f", &m_values[i]);
        }
    }

    return true;
}

string CSVFile::name() const
{
    return "CSV file: " + (m_filename.empty() ? string("<choose a file>") : m_filename);
}

void CSVFile::sample(float r[], unsigned i)
{
    assert(i < m_numSamples);
    for (unsigned d = 0; d < dimensions(); ++d)
    {
        assert(i * m_numDimensions + d < m_values.size());
        r[d] = m_values[i * m_numDimensions + d];
    }
}
