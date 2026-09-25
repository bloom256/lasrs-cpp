// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <compare>
#include <cstdint>
#include <lasrs/error.hpp>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace las
{

struct Version
{
    uint8_t major = 1;
    uint8_t minor = 2;

    constexpr Version() = default;
    constexpr Version(uint8_t major, uint8_t minor) : major(major), minor(minor)
    {
    }

    constexpr bool requires_point_data_start_signature() const
    {
        return *this == Version(1, 0);
    }

    constexpr uint16_t header_size() const
    {
        if (*this <= Version(1, 2))
        {
            return 227;
        }
        if (*this == Version(1, 3))
        {
            return 235;
        }
        return 375;
    }

    constexpr auto operator<=>(const Version &) const = default;
};

struct Transform
{
    double scale = 0.001;
    double offset = 0.0;

    constexpr double direct(int32_t n) const
    {
        return scale * static_cast<double>(n) + offset;
    }

    int32_t inverse(double n) const
    {
        int32_t result = 0;
        detail::check(lasrs_transform_inverse({scale, offset}, n, &result));
        return result;
    }

    constexpr bool operator==(const Transform &) const = default;
};

template <class T> struct Vector
{
    T x{};
    T y{};
    T z{};

    constexpr bool operator==(const Vector &) const = default;
};

struct Color
{
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;

    constexpr Color() = default;
    constexpr Color(uint16_t red, uint16_t green, uint16_t blue) : red(red), green(green), blue(blue)
    {
    }

    constexpr bool operator==(const Color &) const = default;
};

enum class GpsTimeType : uint8_t
{
    Week = 0,
    Standard = 1,
};

constexpr bool is_standard(GpsTimeType type)
{
    return type == GpsTimeType::Standard;
}

enum class LazParallelism : uint8_t
{
    Yes,
    No,
};

namespace point
{

enum class ScanDirection : uint8_t
{
    RightToLeft = 0,
    LeftToRight = 1,
};

// Values 19-63 are reserved and 64-255 user definable; 12 (overlap) is not a
// valid classification and is rejected when a point is handed to las-rs.
enum class Classification : uint8_t
{
    CreatedNeverClassified = 0,
    Unclassified = 1,
    Ground = 2,
    LowVegetation = 3,
    MediumVegetation = 4,
    HighVegetation = 5,
    Building = 6,
    LowPoint = 7,
    ModelKeyPoint = 8,
    Water = 9,
    Rail = 10,
    RoadSurface = 11,
    WireGuard = 13,
    WireConductor = 14,
    TransmissionTower = 15,
    WireStructureConnector = 16,
    BridgeDeck = 17,
    HighNoise = 18,
};

constexpr bool is_reserved(Classification c)
{
    const auto n = static_cast<uint8_t>(c);
    return n >= 19 && n <= 63;
}

constexpr bool is_user_definable(Classification c)
{
    return static_cast<uint8_t>(c) >= 64;
}

struct Format
{
    bool has_gps_time = false;
    bool has_color = false;
    bool is_extended = false;
    bool has_waveform = false;
    bool has_nir = false;
    uint16_t extra_bytes = 0;
    bool is_compressed = false;

    constexpr Format() = default;

    explicit Format(uint8_t n)
    {
        *this = from_c(checked_new(n));
    }

    constexpr void extend()
    {
        has_gps_time = true;
        is_extended = true;
    }

    uint16_t len() const
    {
        return lasrs_format_len(to_c());
    }

    uint8_t to_u8() const
    {
        uint8_t n = 0;
        detail::check(lasrs_format_to_u8(to_c(), &n));
        return n;
    }

    constexpr bool operator==(const Format &) const = default;

    constexpr LasrsFormat to_c() const
    {
        return {has_gps_time, has_color, is_extended, has_waveform, has_nir, extra_bytes, is_compressed};
    }

    static constexpr Format from_c(LasrsFormat f)
    {
        Format format;
        format.has_gps_time = f.has_gps_time;
        format.has_color = f.has_color;
        format.is_extended = f.is_extended;
        format.has_waveform = f.has_waveform;
        format.has_nir = f.has_nir;
        format.extra_bytes = f.extra_bytes;
        format.is_compressed = f.is_compressed;
        return format;
    }

  private:
    static LasrsFormat checked_new(uint8_t n)
    {
        LasrsFormat format{};
        detail::check(lasrs_format_new(n, &format));
        return format;
    }
};

} // namespace point

namespace raw::point
{

struct Waveform
{
    uint8_t wave_packet_descriptor_index = 0;
    uint64_t byte_offset_to_waveform_data = 0;
    uint32_t waveform_packet_size_in_bytes = 0;
    float return_point_waveform_location = 0.0f;
    float x_t = 0.0f;
    float y_t = 0.0f;
    float z_t = 0.0f;

    constexpr bool operator==(const Waveform &) const = default;
};

} // namespace raw::point

struct Point
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    uint16_t intensity = 0;
    uint8_t return_number = 0;
    uint8_t number_of_returns = 0;
    point::ScanDirection scan_direction = point::ScanDirection::RightToLeft;
    bool is_edge_of_flight_line = false;
    point::Classification classification = point::Classification::CreatedNeverClassified;
    bool is_synthetic = false;
    bool is_key_point = false;
    bool is_withheld = false;
    bool is_overlap = false;
    uint8_t scanner_channel = 0;
    float scan_angle = 0.0f;
    uint8_t user_data = 0;
    uint16_t point_source_id = 0;
    std::optional<double> gps_time;
    std::optional<Color> color;
    std::optional<raw::point::Waveform> waveform;
    std::optional<uint16_t> nir;
    std::vector<uint8_t> extra_bytes;

    bool matches(const point::Format &format) const
    {
        return gps_time.has_value() == format.has_gps_time && color.has_value() == format.has_color &&
               waveform.has_value() == format.has_waveform && nir.has_value() == format.has_nir &&
               extra_bytes.size() == format.extra_bytes;
    }

    bool operator==(const Point &) const = default;
};

struct Bounds
{
    Vector<double> min{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::infinity()};
    Vector<double> max{-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
                       -std::numeric_limits<double>::infinity()};

    void grow(const Point &point)
    {
        min = {std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z)};
        max = {std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z)};
    }

    Bounds adapt(const Vector<Transform> &transforms) const;

    constexpr bool intersect(const Bounds &other) const
    {
        return min.x <= other.max.x && max.x >= other.min.x && min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    constexpr bool operator==(const Bounds &) const = default;
};

struct Vlr
{
    std::string user_id;
    uint16_t record_id = 0;
    std::string description;
    std::vector<uint8_t> data;

    size_t len(bool is_extended) const
    {
        return data.size() + (is_extended ? 60 : 54);
    }
    bool is_empty() const
    {
        return data.empty();
    }
    bool has_large_data() const
    {
        return data.size() > std::numeric_limits<uint16_t>::max();
    }
    bool is_crs() const
    {
        return is_projection() && (record_id == 2112 || is_geotiff_record());
    }
    bool is_wkt_crs() const
    {
        return is_projection() && record_id == 2112;
    }
    bool is_geotiff_crs() const
    {
        return is_projection() && is_geotiff_record();
    }

    bool operator==(const Vlr &) const = default;

  private:
    bool is_projection() const
    {
        return std::ranges::equal(user_id, std::string_view("lasf_projection"),
                                  [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
    }

    bool is_geotiff_record() const
    {
        return record_id >= 34735 && record_id <= 34737;
    }
};

struct ReaderOptions
{
    LazParallelism laz_parallelism = LazParallelism::Yes;

    ReaderOptions with_laz_parallelism(LazParallelism value) const
    {
        auto options = *this;
        options.laz_parallelism = value;
        return options;
    }
};

struct WriterOptions
{
    LazParallelism laz_parallelism = LazParallelism::Yes;

    WriterOptions with_laz_parallelism(LazParallelism value) const
    {
        auto options = *this;
        options.laz_parallelism = value;
        return options;
    }
};

namespace copc
{

struct CopcInfoVlr
{
    double center_x = 0.0;
    double center_y = 0.0;
    double center_z = 0.0;
    double halfsize = 0.0;
    double spacing = 0.0;
    double gpstime_minimum = 0.0;
    double gpstime_maximum = 0.0;

    constexpr bool operator==(const CopcInfoVlr &) const = default;
};

struct VoxelKey
{
    int32_t l = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t z = 0;

    static const VoxelKey ROOT;

    VoxelKey child(int32_t direction) const
    {
        if (direction < 0 || direction >= 8)
        {
            throw Error("Invalid direction: " + std::to_string(direction));
        }
        return {l + 1, (x << 1) | (direction & 0x1), (y << 1) | ((direction >> 1) & 0x1),
                (z << 1) | ((direction >> 2) & 0x1)};
    }

    std::array<VoxelKey, 8> children() const
    {
        std::array<VoxelKey, 8> result;
        for (int32_t i = 0; i < 8; ++i)
        {
            result[i] = child(i);
        }
        return result;
    }

    constexpr VoxelKey parent() const
    {
        return {std::max(0, l - 1), x >> 1, y >> 1, z >> 1};
    }

    Bounds bounds(const CopcInfoVlr &info) const
    {
        const Vector<double> root_min{info.center_x - info.halfsize, info.center_y - info.halfsize,
                                      info.center_z - info.halfsize};
        const Vector<double> root_max{info.center_x + info.halfsize, info.center_y + info.halfsize,
                                      info.center_z + info.halfsize};
        const auto divisions = static_cast<double>(1 << l);
        const Vector<double> voxel_size{(root_max.x - root_min.x) / divisions, (root_max.y - root_min.y) / divisions,
                                        (root_max.z - root_min.z) / divisions};
        const Vector<double> min{root_min.x + voxel_size.x * x, root_min.y + voxel_size.y * y,
                                 root_min.z + voxel_size.z * z};
        return {min, {min.x + voxel_size.x, min.y + voxel_size.y, min.z + voxel_size.z}};
    }

    constexpr auto operator<=>(const VoxelKey &) const = default;
};

inline constexpr VoxelKey VoxelKey::ROOT{0, 0, 0, 0};

struct Entry
{
    VoxelKey key;
    uint64_t offset = 0;
    int32_t byte_size = 0;
    int32_t point_count = 0;

    constexpr bool operator==(const Entry &) const = default;
};

} // namespace copc

namespace detail
{

constexpr LasrsTransforms to_c(const Vector<Transform> &t)
{
    return {{t.x.scale, t.x.offset}, {t.y.scale, t.y.offset}, {t.z.scale, t.z.offset}};
}

constexpr Vector<Transform> from_c(const LasrsTransforms &t)
{
    return {{t.x.scale, t.x.offset}, {t.y.scale, t.y.offset}, {t.z.scale, t.z.offset}};
}

constexpr LasrsBounds to_c(const Bounds &b)
{
    return {{b.min.x, b.min.y, b.min.z}, {b.max.x, b.max.y, b.max.z}};
}

constexpr Bounds from_c(const LasrsBounds &b)
{
    return {{b.min.x, b.min.y, b.min.z}, {b.max.x, b.max.y, b.max.z}};
}

constexpr LasrsVersion to_c(Version v)
{
    return {v.major, v.minor};
}

constexpr Version from_c(LasrsVersion v)
{
    return {v.major, v.minor};
}

constexpr LasrsVoxelKey to_c(const copc::VoxelKey &k)
{
    return {k.l, k.x, k.y, k.z};
}

constexpr copc::VoxelKey from_c(const LasrsVoxelKey &k)
{
    return {k.l, k.x, k.y, k.z};
}

constexpr LasrsEntry to_c(const copc::Entry &e)
{
    return {to_c(e.key), e.offset, e.byte_size, e.point_count};
}

constexpr copc::Entry from_c(const LasrsEntry &e)
{
    return {from_c(e.key), e.offset, e.byte_size, e.point_count};
}

constexpr LasrsLazParallelism to_c(LazParallelism p)
{
    return p == LazParallelism::Yes ? LASRS_LAZ_PARALLELISM_YES : LASRS_LAZ_PARALLELISM_NO;
}

inline LasrsVlr to_c(const Vlr &v)
{
    return {to_c(std::string_view(v.user_id)), v.record_id, to_c(std::string_view(v.description)),
            to_c(std::span<const uint8_t>(v.data))};
}

inline Vlr from_c(const LasrsVlr &v)
{
    return {std::string(to_string_view(v.user_id)), v.record_id, std::string(to_string_view(v.description)),
            to_vector(v.data)};
}

inline std::vector<LasrsVlr> to_c(const std::vector<Vlr> &vlrs)
{
    std::vector<LasrsVlr> result;
    result.reserve(vlrs.size());
    std::ranges::transform(vlrs, std::back_inserter(result), [](const Vlr &v) { return to_c(v); });
    return result;
}

inline LasrsPoint to_c(const Point &p)
{
    LasrsPoint c{};
    c.x = p.x;
    c.y = p.y;
    c.z = p.z;
    c.intensity = p.intensity;
    c.return_number = p.return_number;
    c.number_of_returns = p.number_of_returns;
    c.scan_direction = static_cast<uint8_t>(p.scan_direction);
    c.is_edge_of_flight_line = p.is_edge_of_flight_line;
    c.classification = static_cast<uint8_t>(p.classification);
    c.is_synthetic = p.is_synthetic;
    c.is_key_point = p.is_key_point;
    c.is_withheld = p.is_withheld;
    c.is_overlap = p.is_overlap;
    c.scanner_channel = p.scanner_channel;
    c.scan_angle = p.scan_angle;
    c.user_data = p.user_data;
    c.point_source_id = p.point_source_id;
    c.has_gps_time = p.gps_time.has_value();
    c.gps_time = p.gps_time.value_or(0.0);
    c.has_color = p.color.has_value();
    if (p.color)
    {
        c.color = {p.color->red, p.color->green, p.color->blue};
    }
    c.has_waveform = p.waveform.has_value();
    if (p.waveform)
    {
        const auto &w = *p.waveform;
        c.waveform = {w.wave_packet_descriptor_index,
                      w.byte_offset_to_waveform_data,
                      w.waveform_packet_size_in_bytes,
                      w.return_point_waveform_location,
                      w.x_t,
                      w.y_t,
                      w.z_t};
    }
    c.has_nir = p.nir.has_value();
    c.nir = p.nir.value_or(0);
    c.extra_bytes_len = p.extra_bytes.size();
    return c;
}

inline Point from_c(const LasrsPoint &c, std::span<const uint8_t> extra_bytes)
{
    Point p;
    p.x = c.x;
    p.y = c.y;
    p.z = c.z;
    p.intensity = c.intensity;
    p.return_number = c.return_number;
    p.number_of_returns = c.number_of_returns;
    p.scan_direction = static_cast<point::ScanDirection>(c.scan_direction);
    p.is_edge_of_flight_line = c.is_edge_of_flight_line;
    p.classification = static_cast<point::Classification>(c.classification);
    p.is_synthetic = c.is_synthetic;
    p.is_key_point = c.is_key_point;
    p.is_withheld = c.is_withheld;
    p.is_overlap = c.is_overlap;
    p.scanner_channel = c.scanner_channel;
    p.scan_angle = c.scan_angle;
    p.user_data = c.user_data;
    p.point_source_id = c.point_source_id;
    if (c.has_gps_time)
    {
        p.gps_time = c.gps_time;
    }
    if (c.has_color)
    {
        p.color = Color(c.color.red, c.color.green, c.color.blue);
    }
    if (c.has_waveform)
    {
        const auto &w = c.waveform;
        p.waveform = raw::point::Waveform{w.wave_packet_descriptor_index,
                                          w.byte_offset_to_waveform_data,
                                          w.waveform_packet_size_in_bytes,
                                          w.return_point_waveform_location,
                                          w.x_t,
                                          w.y_t,
                                          w.z_t};
    }
    if (c.has_nir)
    {
        p.nir = c.nir;
    }
    p.extra_bytes.assign(extra_bytes.begin(), extra_bytes.end());
    return p;
}

struct PointsForC
{
    std::vector<LasrsPoint> points;
    std::vector<uint8_t> extra_bytes;
};

inline PointsForC to_c(const std::vector<Point> &points)
{
    PointsForC result;
    result.points.reserve(points.size());
    for (const auto &p : points)
    {
        result.points.push_back(to_c(p));
        result.extra_bytes.insert(result.extra_bytes.end(), p.extra_bytes.begin(), p.extra_bytes.end());
    }
    return result;
}

} // namespace detail

inline Bounds Bounds::adapt(const Vector<Transform> &transforms) const
{
    LasrsBounds result{};
    detail::check(lasrs_bounds_adapt(detail::to_c(*this), detail::to_c(transforms), &result));
    return detail::from_c(result);
}

} // namespace las
