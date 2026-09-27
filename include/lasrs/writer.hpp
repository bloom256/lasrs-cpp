// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/point_data.hpp>
#include <lasrs/stream.hpp>
#include <memory>
#include <optional>

namespace las
{

// Destroying an open writer closes it but cannot report errors; call close()
// to observe them. Stream-based writers keep a reference to the stream: it
// must outlive the writer.
class Writer
{
  public:
    Writer(std::ostream &stream, const Header &header)
        : Writer(open(std::make_unique<detail::OutputStream>(stream), [&](LasrsOutputStream out, LasrsWriter **writer) {
              return lasrs_writer_new(out, header.c_ptr(), writer);
          }))
    {
    }

    static Writer with_options(std::ostream &stream, const Header &header, WriterOptions options)
    {
        return open(std::make_unique<detail::OutputStream>(stream), [&](LasrsOutputStream out, LasrsWriter **writer) {
            return lasrs_writer_with_options(out, header.c_ptr(), detail::to_c(options.laz_parallelism), writer);
        });
    }

    static Writer from_path(const std::filesystem::path &path, const Header &header)
    {
        const auto utf8 = detail::path_to_utf8(path);
        return open(nullptr, [&](LasrsOutputStream, LasrsWriter **writer) {
            return lasrs_writer_from_path(detail::to_c(std::string_view(utf8)), header.c_ptr(), writer);
        });
    }

    Writer(Writer &&) noexcept = default;
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    ~Writer() = default;

    // The Rust writer is released (and closed) before the stream adapter it
    // writes through.
    Writer &operator=(Writer &&other) noexcept
    {
        handle_ = std::move(other.handle_);
        output_ = std::move(other.output_);
        header_ = std::move(other.header_);
        return *this;
    }

    void close()
    {
        detail::check(lasrs_writer_close(handle_.get()));
    }

    // The header as it is now; the returned reference and views into it stay
    // valid until the next call of header() or the writer's destruction.
    const Header &header() const
    {
        header_ = Header::from_c(lasrs_writer_header(handle_.get()));
        return *header_;
    }

    void write_point(const Point &point)
    {
        const auto c = detail::to_c(point);
        detail::check(lasrs_writer_write_point(handle_.get(), &c, point.extra_bytes.data()));
    }

    void write_points(const PointData &points)
    {
        detail::check(lasrs_writer_write_points(handle_.get(), points.c_ptr()));
    }

  private:
    Writer(std::unique_ptr<detail::OutputStream> output, LasrsWriter *writer)
        : output_(std::move(output)), handle_(writer)
    {
    }

    template <class Open> static Writer open(std::unique_ptr<detail::OutputStream> output, Open open_writer)
    {
        LasrsWriter *writer = nullptr;
        detail::check(open_writer(output ? output->to_c() : LasrsOutputStream{}, &writer));
        return Writer(std::move(output), writer);
    }

    // Declared before handle_ so it outlives the Rust writer, which still
    // writes through it while closing on destruction.
    std::unique_ptr<detail::OutputStream> output_;
    detail::Handle<LasrsWriter, lasrs_writer_free> handle_;
    mutable std::optional<Header> header_;
};

} // namespace las
