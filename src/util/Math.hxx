// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef MATH_HXX
#define MATH_HXX

#include <cmath>

/*
 * C99 math can be optionally omitted with gcc's libstdc++.
 * Use boost if unavailable.
 */
#if (defined(__GLIBCPP__) || defined(__GLIBCXX__)) && !defined(_GLIBCXX_USE_C99_MATH_TR1)
#include <boost/math/special_functions/round.hpp>
using boost::math::lround;
#else
using std::lround;
#endif

#endif
