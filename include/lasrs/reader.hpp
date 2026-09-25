// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/point_data.hpp>
#include <lasrs/stream.hpp>

namespace las
{

// Stream-based readers keep a reference to the stream: it must outlive the
// reader and must not be used by anyone else meanwhile.
class Reader
{
  public:
    explicit Reader(std::istream &stream)
        : Reader(open(std::make_unique<detail::InputStream>(stream),
                      [](LasrsInputStream in, LasrsReader **out) { return lasrs_reader_new(in, out); }))
    {
    }

    static Reader with_options(std::istream &stream, ReaderOptions options)
    {
        return open(std::make_unique<detail::InputStream>(stream), [&](LasrsInputStream in, LasrsReader **out) {
            return lasrs_reader_with_options(in, detail::to_c(options.laz_parallelism), out);
        });
    }

    static Reader from_path(const std::filesystem::path &path)
    {
        const auto utf8 = detail::path_to_utf8(path);
        return open(nullptr, [&](LasrsInputStream, LasrsReader **out) {
            return lasrs_reader_from_path(detail::to_c(std::string_view(utf8)), out);
        });
    }

    Reader(Reader &&) noexcept = default;
    Reader &operator=(Reader &&) noexcept = default;
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;
    ~Reader() = default;

    const Header &header() const
    {
        return header_;
    }

    PointData read_points(uint64_t n)
    {
        LasrsPointData *points = nullptr;
        detail::check(lasrs_reader_read_points(handle_.get(), n, &points));
        return PointData::from_c(points);
    }

    PointData read_all()
    {
        LasrsPointData *points = nullptr;
        detail::check(lasrs_reader_read_all(handle_.get(), &points));
        return PointData::from_c(points);
    }

    uint64_t fill_points(uint64_t n, PointData &target)
    {
        uint64_t read = 0;
        detail::check(lasrs_reader_fill_points(handle_.get(), n, target.c_ptr(), &read));
        return read;
    }

    void seek(uint64_t position)
    {
        detail::check(lasrs_reader_seek(handle_.get(), position));
    }

  private:
    Reader(std::unique_ptr<detail::InputStream> input, LasrsReader *reader)
        : input_(std::move(input)), handle_(reader), header_(Header::Borrowed{}, lasrs_reader_header(reader))
    {
    }

    template <class Open> static Reader open(std::unique_ptr<detail::InputStream> input, Open open_reader)
    {
        LasrsReader *reader = nullptr;
        detail::check(open_reader(input ? input->to_c() : LasrsInputStream{}, &reader));
        return Reader(std::move(input), reader);
    }

    std::unique_ptr<detail::InputStream> input_;
    detail::Handle<LasrsReader, lasrs_reader_free> handle_;
    Header header_;
};

} // namespace las
