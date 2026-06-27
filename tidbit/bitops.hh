// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Rok Zitko

#pragma once

#include <type_traits>

namespace bitops_detail {
template <typename Target, typename Source>
constexpr Target as_target(Source value) {
  if constexpr (std::is_same_v<std::remove_cv_t<Target>, std::remove_cv_t<Source>>)
    return value;
  else
    return static_cast<Target>(value);
}
}

/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (decltype(a)(1) << (b)))
#define BIT_CLEAR(a,b) ((a) &= ~(decltype(a)(1) << (b)))
#define BIT_FLIP(a,b) ((a) ^= (decltype(a)(1) << (b)))
#define BIT_CHECK(a,b) (!!((a) & (decltype(a)(1) << (b))))        // '!!' to make sure this returns 0 or 1

/* x=target variable, y=mask */
#define BITMASK_SET(x,y) ((x) |= bitops_detail::as_target<decltype(x)>(y))
#define BITMASK_CLEAR(x,y) ((x) &= ~(bitops_detail::as_target<decltype(x)>(y)))
#define BITMASK_FLIP(x,y) ((x) ^= bitops_detail::as_target<decltype(x)>(y))
#define BITMASK_CHECK_ALL(x,y) (((x) & bitops_detail::as_target<decltype(x)>(y)) == bitops_detail::as_target<decltype(x)>(y))   // warning: evaluates y twice
#define BITMASK_CHECK_ANY(x,y) ((x) & bitops_detail::as_target<decltype(x)>(y))
