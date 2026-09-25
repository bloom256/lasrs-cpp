// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <chrono>
#include <lasrs/types.hpp>
#include <utility>

namespace las
{

class PointData;
class Reader;
class Writer;
class CopcReader;

namespace detail
{

inline LasrsDate to_c(const std::chrono::year_month_day &date)
{
    return {static_cast<int>(date.year()), static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day())};
}

inline std::chrono::year_month_day from_c(const LasrsDate &date)
{
    return {std::chrono::year{date.year}, std::chrono::month{date.month}, std::chrono::day{date.day}};
}

} // namespace detail

class Header
{
  public:
    Header() : Header(lasrs_header_default())
    {
    }

    explicit Header(Version version) : Header(lasrs_header_from_version(detail::to_c(version)))
    {
    }

    Header(const Header &other) : Header(lasrs_header_clone(other.ptr_))
    {
    }

    Header(Header &&other) noexcept : owned_(std::move(other.owned_)), ptr_(std::exchange(other.ptr_, nullptr))
    {
    }

    Header &operator=(const Header &other)
    {
        if (this != &other)
        {
            *this = Header(other);
        }
        return *this;
    }

    Header &operator=(Header &&other) noexcept
    {
        owned_ = std::move(other.owned_);
        ptr_ = std::exchange(other.ptr_, nullptr);
        return *this;
    }

    ~Header() = default;

    uint16_t file_source_id() const
    {
        return lasrs_header_file_source_id(ptr_);
    }

    GpsTimeType gps_time_type() const
    {
        return static_cast<GpsTimeType>(lasrs_header_gps_time_type(ptr_));
    }

    bool has_synthetic_return_numbers() const
    {
        return lasrs_header_has_synthetic_return_numbers(ptr_);
    }

    bool has_wkt_crs() const
    {
        return lasrs_header_has_wkt_crs(ptr_);
    }

    std::array<uint8_t, 16> guid() const
    {
        std::array<uint8_t, 16> guid{};
        lasrs_header_guid(ptr_, reinterpret_cast<uint8_t (*)[16]>(guid.data()));
        return guid;
    }

    Version version() const
    {
        return detail::from_c(lasrs_header_version(ptr_));
    }

    std::string_view system_identifier() const
    {
        return detail::to_string_view(lasrs_header_system_identifier(ptr_));
    }

    std::string_view generating_software() const
    {
        return detail::to_string_view(lasrs_header_generating_software(ptr_));
    }

    std::optional<std::chrono::year_month_day> date() const
    {
        LasrsDate date{};
        if (!lasrs_header_date(ptr_, &date))
        {
            return std::nullopt;
        }
        return detail::from_c(date);
    }

    std::span<const uint8_t> padding() const
    {
        return detail::to_span(lasrs_header_padding(ptr_));
    }

    point::Format point_format() const
    {
        return point::Format::from_c(lasrs_header_point_format(ptr_));
    }

    Vector<Transform> transforms() const
    {
        return detail::from_c(lasrs_header_transforms(ptr_));
    }

    Bounds bounds() const
    {
        return detail::from_c(lasrs_header_bounds(ptr_));
    }

    uint64_t number_of_points() const
    {
        return lasrs_header_number_of_points(ptr_);
    }

    std::optional<uint64_t> number_of_points_by_return(uint8_t n) const
    {
        uint64_t count = 0;
        if (!lasrs_header_number_of_points_by_return(ptr_, n, &count))
        {
            return std::nullopt;
        }
        return count;
    }

    std::span<const uint8_t> vlr_padding() const
    {
        return detail::to_span(lasrs_header_vlr_padding(ptr_));
    }

    std::span<const uint8_t> point_padding() const
    {
        return detail::to_span(lasrs_header_point_padding(ptr_));
    }

    std::vector<Vlr> vlrs() const
    {
        std::vector<Vlr> result(lasrs_header_vlrs_len(ptr_));
        for (size_t i = 0; i < result.size(); ++i)
        {
            result[i] = detail::from_c(lasrs_header_vlr(ptr_, i));
        }
        return result;
    }

    std::vector<Vlr> evlrs() const
    {
        std::vector<Vlr> result(lasrs_header_evlrs_len(ptr_));
        for (size_t i = 0; i < result.size(); ++i)
        {
            result[i] = detail::from_c(lasrs_header_evlr(ptr_, i));
        }
        return result;
    }

    std::vector<Vlr> all_vlrs() const
    {
        auto result = vlrs();
        auto extended = evlrs();
        result.insert(result.end(), std::make_move_iterator(extended.begin()), std::make_move_iterator(extended.end()));
        return result;
    }

    bool has_crs_vlrs() const
    {
        return lasrs_header_has_crs_vlrs(ptr_);
    }

    void clear()
    {
        lasrs_header_clear(mut_ptr());
    }

    void add_point(const Point &point)
    {
        const auto c = detail::to_c(point);
        detail::check(lasrs_header_add_point(mut_ptr(), &c, point.extra_bytes.data()));
    }

    void add_point_data(const PointData &points);

    void remove_crs_vlrs()
    {
        lasrs_header_remove_crs_vlrs(mut_ptr());
    }

    void set_wkt_crs(std::span<const uint8_t> wkt_crs_bytes)
    {
        detail::check(lasrs_header_set_wkt_crs(mut_ptr(), detail::to_c(wkt_crs_bytes)));
    }

    std::optional<std::span<const uint8_t>> get_wkt_crs_bytes() const
    {
        LasrsBytes bytes{};
        if (!lasrs_header_get_wkt_crs_bytes(ptr_, &bytes))
        {
            return std::nullopt;
        }
        return detail::to_span(bytes);
    }

    std::optional<copc::CopcInfoVlr> copc_info_vlr() const
    {
        LasrsCopcInfoVlr info{};
        if (!lasrs_header_copc_info_vlr(ptr_, &info))
        {
            return std::nullopt;
        }
        return copc::CopcInfoVlr{info.center_x, info.center_y,        info.center_z,       info.halfsize,
                                 info.spacing,  info.gpstime_minimum, info.gpstime_maximum};
    }

    bool operator==(const Header &other) const
    {
        return lasrs_header_eq(ptr_, other.ptr_);
    }

    const LasrsHeader *c_ptr() const
    {
        return ptr_;
    }

    static Header from_c(LasrsHeader *owned)
    {
        return Header(owned);
    }

  private:
    friend class Reader;
    friend class CopcReader;

    struct Borrowed
    {
    };

    explicit Header(LasrsHeader *owned) : owned_(owned), ptr_(owned)
    {
    }

    Header(Borrowed, const LasrsHeader *borrowed) : ptr_(borrowed)
    {
    }

    LasrsHeader *mut_ptr()
    {
        return owned_.get();
    }

    detail::Handle<LasrsHeader, lasrs_header_free> owned_;
    const LasrsHeader *ptr_ = nullptr;
};

struct Builder
{
    std::optional<std::chrono::year_month_day> date;
    uint16_t file_source_id = 0;
    std::string generating_software;
    GpsTimeType gps_time_type = GpsTimeType::Week;
    std::array<uint8_t, 16> guid{};
    bool has_synthetic_return_numbers = false;
    bool has_wkt_crs = false;
    std::vector<uint8_t> padding;
    point::Format point_format;
    std::vector<uint8_t> point_padding;
    std::string system_identifier;
    Vector<Transform> transforms;
    Version version;
    std::vector<uint8_t> vlr_padding;
    std::vector<Vlr> vlrs;
    std::vector<Vlr> evlrs;

    Builder() : Builder(Version{})
    {
    }

    explicit Builder(Version version)
    {
        assign(make_handle(lasrs_builder_from_version(detail::to_c(version))));
    }

    explicit Builder(const Header &header)
    {
        base_ = make_handle(lasrs_builder_from_header(header.c_ptr()));
        assign(base_);
    }

    Header into_header() const
    {
        const auto c = to_c();
        LasrsHeader *header = nullptr;
        detail::check(lasrs_builder_into_header(base_.get(), &c.fields, c.vlrs.data(), c.vlrs.size(), c.evlrs.data(),
                                                c.evlrs.size(), &header));
        return Header::from_c(header);
    }

    std::optional<Version> minimum_supported_version() const
    {
        const auto c = to_c();
        bool has_version = false;
        LasrsVersion version{};
        detail::check(lasrs_builder_minimum_supported_version(base_.get(), &c.fields, c.vlrs.data(), c.vlrs.size(),
                                                              c.evlrs.data(), c.evlrs.size(), &has_version, &version));
        if (!has_version)
        {
            return std::nullopt;
        }
        return detail::from_c(version);
    }

  private:
    using BuilderHandle = std::shared_ptr<const LasrsBuilder>;

    struct CBuilder
    {
        LasrsBuilderFields fields;
        std::vector<LasrsVlr> vlrs;
        std::vector<LasrsVlr> evlrs;
    };

    static BuilderHandle make_handle(LasrsBuilder *builder)
    {
        return {builder, [](const LasrsBuilder *b) { lasrs_builder_free(const_cast<LasrsBuilder *>(b)); }};
    }

    void assign(const BuilderHandle &builder)
    {
        const auto f = lasrs_builder_fields(builder.get());
        if (f.has_date)
        {
            date = detail::from_c(f.date);
        }
        file_source_id = f.file_source_id;
        generating_software = detail::to_string_view(f.generating_software);
        gps_time_type = static_cast<GpsTimeType>(f.gps_time_type);
        std::copy(std::begin(f.guid), std::end(f.guid), guid.begin());
        has_synthetic_return_numbers = f.has_synthetic_return_numbers;
        has_wkt_crs = f.has_wkt_crs;
        padding = detail::to_vector(f.padding);
        point_format = point::Format::from_c(f.point_format);
        point_padding = detail::to_vector(f.point_padding);
        system_identifier = detail::to_string_view(f.system_identifier);
        transforms = detail::from_c(f.transforms);
        version = detail::from_c(f.version);
        vlr_padding = detail::to_vector(f.vlr_padding);
        vlrs.resize(lasrs_builder_vlrs_len(builder.get()));
        for (size_t i = 0; i < vlrs.size(); ++i)
        {
            vlrs[i] = detail::from_c(lasrs_builder_vlr(builder.get(), i));
        }
        evlrs.resize(lasrs_builder_evlrs_len(builder.get()));
        for (size_t i = 0; i < evlrs.size(); ++i)
        {
            evlrs[i] = detail::from_c(lasrs_builder_evlr(builder.get(), i));
        }
    }

    CBuilder to_c() const
    {
        LasrsBuilderFields f{};
        f.has_date = date.has_value();
        if (date)
        {
            f.date = detail::to_c(*date);
        }
        f.file_source_id = file_source_id;
        f.generating_software = detail::to_c(std::string_view(generating_software));
        f.gps_time_type = static_cast<uint8_t>(gps_time_type);
        std::copy(guid.begin(), guid.end(), std::begin(f.guid));
        f.has_synthetic_return_numbers = has_synthetic_return_numbers;
        f.has_wkt_crs = has_wkt_crs;
        f.padding = detail::to_c(std::span<const uint8_t>(padding));
        f.point_format = point_format.to_c();
        f.point_padding = detail::to_c(std::span<const uint8_t>(point_padding));
        f.system_identifier = detail::to_c(std::string_view(system_identifier));
        f.transforms = detail::to_c(transforms);
        f.version = detail::to_c(version);
        f.vlr_padding = detail::to_c(std::span<const uint8_t>(vlr_padding));
        return {f, detail::to_c(vlrs), detail::to_c(evlrs)};
    }

    BuilderHandle base_;
};

} // namespace las
