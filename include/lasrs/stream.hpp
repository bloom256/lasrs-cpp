// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/lasrs.h>

#include <istream>
#include <optional>
#include <ostream>

// Adapters that let las-rs read and write std::iostreams. They run inside
// Rust, so they must never let an exception escape.
namespace las::detail
{

inline std::ios_base::seekdir to_seekdir(LasrsSeekOrigin origin)
{
    switch (origin)
    {
    case LASRS_SEEK_ORIGIN_START:
        return std::ios_base::beg;
    case LASRS_SEEK_ORIGIN_CURRENT:
        return std::ios_base::cur;
    case LASRS_SEEK_ORIGIN_END:
        return std::ios_base::end;
    }
    return std::ios_base::beg;
}

// Seeking past the end is allowed and later reads return nothing, as with
// files; the parallel LAZ reader relies on it but e.g. std::istringstream
// cannot position itself there, so that position is tracked here.
class InputStream
{
  public:
    explicit InputStream(std::istream &stream) : stream_(stream)
    {
    }

    LasrsInputStream to_c()
    {
        return {this, read, seek};
    }

  private:
    static bool read(void *context, uint8_t *buf, size_t len, size_t *read) noexcept
    {
        try
        {
            auto &self = *static_cast<InputStream *>(context);
            if (self.past_end_)
            {
                *read = 0;
                return true;
            }
            auto &in = self.stream_;
            in.read(reinterpret_cast<char *>(buf), static_cast<std::streamsize>(len));
            *read = static_cast<size_t>(in.gcount());
            if (in.bad())
            {
                return false;
            }
            in.clear();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    static bool seek(void *context, int64_t offset, LasrsSeekOrigin origin, uint64_t *position) noexcept
    {
        try
        {
            auto &self = *static_cast<InputStream *>(context);
            auto &in = self.stream_;
            in.clear();
            const std::streamoff current = self.past_end_ ? *self.past_end_ : std::streamoff(in.tellg());
            const std::streamoff end = in.seekg(0, std::ios_base::end).tellg();
            std::streamoff target = offset;
            if (origin == LASRS_SEEK_ORIGIN_CURRENT)
            {
                target += current;
            }
            else if (origin == LASRS_SEEK_ORIGIN_END)
            {
                target += end;
            }
            if (in.fail() || current < 0 || target < 0)
            {
                return false;
            }
            if (target > end)
            {
                self.past_end_ = target;
            }
            else
            {
                self.past_end_.reset();
                in.seekg(target);
            }
            *position = static_cast<uint64_t>(target);
            return !in.fail();
        }
        catch (...)
        {
            return false;
        }
    }

    std::istream &stream_;
    std::optional<std::streamoff> past_end_;
};

inline bool ostream_write(void *context, const uint8_t *buf, size_t len) noexcept
{
    try
    {
        auto &out = *static_cast<std::ostream *>(context);
        out.write(reinterpret_cast<const char *>(buf), static_cast<std::streamsize>(len));
        return out.good();
    }
    catch (...)
    {
        return false;
    }
}

// Seeking past the end zero-fills the gap, as files do; many streams (e.g.
// std::stringstream) would fail instead, and the LAZ writer relies on it.
inline bool ostream_seek(void *context, int64_t offset, LasrsSeekOrigin origin, uint64_t *position) noexcept
{
    try
    {
        auto &out = *static_cast<std::ostream *>(context);
        const std::streamoff current = out.tellp();
        const std::streamoff end = out.seekp(0, std::ios_base::end).tellp();
        std::streamoff target = offset;
        if (origin == LASRS_SEEK_ORIGIN_CURRENT)
        {
            target += current;
        }
        else if (origin == LASRS_SEEK_ORIGIN_END)
        {
            target += end;
        }
        if (out.fail() || target < 0)
        {
            return false;
        }
        if (target > end)
        {
            for (std::streamoff i = end; i < target; ++i)
            {
                out.put('\0');
            }
        }
        else
        {
            out.seekp(target);
        }
        *position = static_cast<uint64_t>(target);
        return out.good();
    }
    catch (...)
    {
        return false;
    }
}

inline bool ostream_flush(void *context) noexcept
{
    try
    {
        return static_cast<std::ostream *>(context)->flush().good();
    }
    catch (...)
    {
        return false;
    }
}

inline LasrsOutputStream to_c(std::ostream &stream)
{
    return {&stream, ostream_write, ostream_seek, ostream_flush};
}

} // namespace las::detail
