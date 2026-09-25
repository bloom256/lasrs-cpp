// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/lasrs.h>

#include <filesystem>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace las
{

class Error : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

namespace detail
{

inline std::string_view to_string_view(LasrsStr s) noexcept
{
    return {reinterpret_cast<const char *>(s.ptr), s.len};
}

inline LasrsStr to_c(std::string_view s) noexcept
{
    return {reinterpret_cast<const uint8_t *>(s.data()), s.size()};
}

inline std::span<const uint8_t> to_span(LasrsBytes b) noexcept
{
    return {b.ptr, b.len};
}

inline std::vector<uint8_t> to_vector(LasrsBytes b)
{
    return {b.ptr, b.ptr + b.len};
}

inline LasrsBytes to_c(std::span<const uint8_t> b) noexcept
{
    return {b.data(), b.size()};
}

inline void check(LasrsStatus status)
{
    if (status != LASRS_STATUS_OK)
    {
        throw Error(std::string(to_string_view(lasrs_last_error())));
    }
}

inline std::string path_to_utf8(const std::filesystem::path &path)
{
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char *>(utf8.data()), utf8.size()};
}

template <auto Free> struct Deleter
{
    template <class T> void operator()(T *handle) const noexcept
    {
        Free(handle);
    }
};

template <class T, auto Free> using Handle = std::unique_ptr<T, Deleter<Free>>;

} // namespace detail
} // namespace las
