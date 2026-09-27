// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <algorithm>
#include <array>
#include <istream>
#include <lasrs/error.hpp>
#include <limits>
#include <optional>
#include <ostream>

// Adapters that let las-rs read and write std::iostreams. They work on the
// stream buffer, so the stream's state flags and exceptions() setting do not
// interfere, and positions are relative to where the stream was when the
// adapter was created, so LAS data may start anywhere inside a stream. The
// callbacks run inside Rust and never let an exception escape.
namespace las::detail
{

inline const std::streampos invalid_position = std::streampos(std::streamoff(-1));

// Absolute target of a seek relative to `start`, `current` or `end`, or
// nothing if it overflows or is negative.
inline std::optional<std::streamoff> seek_target(int64_t offset, LasrsSeekOrigin origin, std::streamoff current,
                                                 std::streamoff end)
{
    const std::streamoff base = origin == LASRS_SEEK_ORIGIN_CURRENT ? current
                                : origin == LASRS_SEEK_ORIGIN_END   ? end
                                                                    : 0;
    const auto max = std::numeric_limits<std::streamoff>::max();
    if ((offset > 0 && base > max - offset) || base + offset < 0)
    {
        return std::nullopt;
    }
    return base + offset;
}

inline std::streambuf &checked_buffer(std::ios &stream, std::ios_base::openmode mode, const char *what)
{
    std::streambuf *buffer = stream.rdbuf();
    if (!stream.good() || buffer == nullptr || buffer->pubseekoff(0, std::ios_base::cur, mode) == invalid_position)
    {
        throw Error(std::string(what) + " stream is not usable: it must be open, in a good state and seekable");
    }
    return *buffer;
}

class InputStream
{
  public:
    explicit InputStream(std::istream &stream)
        : buffer_(checked_buffer(stream, std::ios_base::in, "input")),
          start_(buffer_.pubseekoff(0, std::ios_base::cur, std::ios_base::in))
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
            *read = self.past_end_ ? 0
                                   : static_cast<size_t>(self.buffer_.sgetn(reinterpret_cast<char *>(buf),
                                                                            static_cast<std::streamsize>(len)));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    // Seeking past the end is allowed and later reads return nothing, as with
    // files; the parallel LAZ reader relies on it, but a stream buffer cannot
    // be positioned there, so that position is tracked here.
    static bool seek(void *context, int64_t offset, LasrsSeekOrigin origin, uint64_t *position) noexcept
    {
        try
        {
            auto &self = *static_cast<InputStream *>(context);
            auto &buffer = self.buffer_;
            const auto now = buffer.pubseekoff(0, std::ios_base::cur, std::ios_base::in);
            const auto last = buffer.pubseekoff(0, std::ios_base::end, std::ios_base::in);
            if (now == invalid_position || last == invalid_position)
            {
                return false;
            }
            const auto current = self.past_end_ ? *self.past_end_ : std::streamoff(now - self.start_);
            const auto end = std::streamoff(last - self.start_);
            const auto target = seek_target(offset, origin, current, end);
            if (!target)
            {
                return false;
            }
            if (*target > end)
            {
                self.past_end_ = *target;
            }
            else
            {
                self.past_end_.reset();
                if (buffer.pubseekpos(self.start_ + *target, std::ios_base::in) == invalid_position)
                {
                    return false;
                }
            }
            *position = static_cast<uint64_t>(*target);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::streambuf &buffer_;
    std::streampos start_;
    std::optional<std::streamoff> past_end_;
};

class OutputStream
{
  public:
    explicit OutputStream(std::ostream &stream)
        : buffer_(checked_buffer(stream, std::ios_base::out, "output")),
          start_(buffer_.pubseekoff(0, std::ios_base::cur, std::ios_base::out))
    {
    }

    LasrsOutputStream to_c()
    {
        return {this, write, seek, flush};
    }

  private:
    static bool write(void *context, const uint8_t *buf, size_t len) noexcept
    {
        try
        {
            auto &buffer = static_cast<OutputStream *>(context)->buffer_;
            const auto size = static_cast<std::streamsize>(len);
            return buffer.sputn(reinterpret_cast<const char *>(buf), size) == size;
        }
        catch (...)
        {
            return false;
        }
    }

    // Seeking past the end zero-fills the gap, as files do; many buffers
    // (e.g. std::stringbuf) would fail instead, and the LAZ writer relies on it.
    static bool seek(void *context, int64_t offset, LasrsSeekOrigin origin, uint64_t *position) noexcept
    {
        try
        {
            auto &self = *static_cast<OutputStream *>(context);
            auto &buffer = self.buffer_;
            const auto now = buffer.pubseekoff(0, std::ios_base::cur, std::ios_base::out);
            const auto last = buffer.pubseekoff(0, std::ios_base::end, std::ios_base::out);
            if (now == invalid_position || last == invalid_position)
            {
                return false;
            }
            const auto current = std::streamoff(now - self.start_);
            const auto end = std::streamoff(last - self.start_);
            const auto target = seek_target(offset, origin, current, end);
            if (!target)
            {
                return false;
            }
            if (*target > end)
            {
                static constexpr std::array<char, 4096> zeros{};
                for (auto gap = *target - end; gap > 0;)
                {
                    const auto chunk = std::min<std::streamoff>(gap, zeros.size());
                    if (buffer.sputn(zeros.data(), chunk) != chunk)
                    {
                        return false;
                    }
                    gap -= chunk;
                }
            }
            else if (buffer.pubseekpos(self.start_ + *target, std::ios_base::out) == invalid_position)
            {
                return false;
            }
            *position = static_cast<uint64_t>(*target);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    static bool flush(void *context) noexcept
    {
        try
        {
            return static_cast<OutputStream *>(context)->buffer_.pubsync() == 0;
        }
        catch (...)
        {
            return false;
        }
    }

    std::streambuf &buffer_;
    std::streampos start_;
};

} // namespace las::detail
