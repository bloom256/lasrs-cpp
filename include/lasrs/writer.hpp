// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/point_data.hpp>
#include <lasrs/stream.hpp>

namespace las
{

// Destroying an open writer closes it but cannot report errors; call close()
// to observe them. Stream-based writers keep a reference to the stream: it
// must outlive the writer.
class Writer
{
  public:
    Writer(std::ostream &stream, const Header &header)
        : Writer(open([&](LasrsWriter **out) { return lasrs_writer_new(detail::to_c(stream), header.c_ptr(), out); }))
    {
    }

    static Writer with_options(std::ostream &stream, const Header &header, WriterOptions options)
    {
        return open([&](LasrsWriter **out) {
            return lasrs_writer_with_options(detail::to_c(stream), header.c_ptr(),
                                             detail::to_c(options.laz_parallelism), out);
        });
    }

    static Writer from_path(const std::filesystem::path &path, const Header &header)
    {
        const auto utf8 = detail::path_to_utf8(path);
        return open([&](LasrsWriter **out) {
            return lasrs_writer_from_path(detail::to_c(std::string_view(utf8)), header.c_ptr(), out);
        });
    }

    Writer(Writer &&) noexcept = default;
    Writer &operator=(Writer &&) noexcept = default;
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;
    ~Writer() = default;

    void close()
    {
        detail::check(lasrs_writer_close(handle_.get()));
    }

    Header header() const
    {
        return Header::from_c(lasrs_writer_header(handle_.get()));
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
    explicit Writer(LasrsWriter *writer) : handle_(writer)
    {
    }

    template <class Open> static Writer open(Open open_writer)
    {
        LasrsWriter *writer = nullptr;
        detail::check(open_writer(&writer));
        return Writer(writer);
    }

    detail::Handle<LasrsWriter, lasrs_writer_free> handle_;
};

} // namespace las
