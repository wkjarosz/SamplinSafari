//
// Copyright (C) Wojciech Jarosz <wjarosz@gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE.txt file.
//

#pragma once

#if defined(_MSC_VER)
// Make MS cmath define M_PI but not the min/max macros
#define _USE_MATH_DEFINES
#define NOMINMAX
#endif

#include <string>

std::string to_lower(std::string str);
std::string to_upper(std::string str);

int         version_major();
int         version_minor();
int         version_patch();
int         version_combined();
std::string version();
std::string git_hash();
std::string git_describe();
std::string build_timestamp();
std::string backend();