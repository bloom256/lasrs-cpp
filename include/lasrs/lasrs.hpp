// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/copc.hpp>
#include <lasrs/header.hpp>
#include <lasrs/point_data.hpp>
#include <lasrs/reader.hpp>
#include <lasrs/types.hpp>
#include <lasrs/writer.hpp>

namespace las {

namespace header {
using las::Builder;
}

// True if the linked library matches these headers.
inline bool abi_compatible() { return lasrs_abi_version() == LASRS_ABI_VERSION; }

}  // namespace las
