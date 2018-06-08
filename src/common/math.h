/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL v2
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   math helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#pragma once

#include "common/common_pch.h"

#if defined(COMP_MSC)
# include <intrin.h>
#endif

#if HAVE_BOOST_INTEGER_COMMON_FACTOR_HPP
# include <boost/integer/common_factor.hpp>
#else
# include <boost/math/common_factor.hpp>
#endif

#include "common/math_fwd.h"

namespace mtx { namespace math {

inline std::size_t
count_1_bits(uint64_t value) {
#if defined(COMP_MSC)
  return __popcnt(value);
#else
  return __builtin_popcountll(value);
#endif
}

uint64_t round_to_nearest_pow2(uint64_t value);
int int_log2(uint64_t value);
double int_to_double(int64_t value);

// Converting unsigned int types to signed ints assuming the
// underlying bits in memory should represent the 2's complement of a
// signed integer. See https://stackoverflow.com/a/13208789/507077

template<typename Tunsigned>
typename std::enable_if<
  std::is_unsigned<Tunsigned>::value,
  typename std::make_signed<Tunsigned>::type
>::type
to_signed(Tunsigned const &u) {
  using Tsigned = typename std::make_signed<Tunsigned>::type;

  if (u <= std::numeric_limits<Tsigned>::max())
    return static_cast<Tsigned>(u);

  return static_cast<Tsigned>(u - std::numeric_limits<Tsigned>::min()) + std::numeric_limits<Tsigned>::min();
}

template<typename Tsigned>
typename std::enable_if<
  std::is_signed<Tsigned>::value,
  Tsigned
>::type
to_signed(Tsigned const &s) {
  return s;
}

}}
