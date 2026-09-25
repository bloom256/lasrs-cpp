// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/header.hpp>
#include <tuple>

namespace las {

class PointData {
public:
    PointData(const PointData& other) : handle_(lasrs_point_data_clone(other.get())) {}
    PointData(PointData&&) noexcept = default;

    PointData& operator=(const PointData& other) {
        if (this != &other) {
            handle_.reset(lasrs_point_data_clone(other.get()));
        }
        return *this;
    }

    PointData& operator=(PointData&&) noexcept = default;
    ~PointData() = default;

    size_t len() const { return lasrs_point_data_len(get()); }

    bool is_empty() const { return len() == 0; }

    point::Format format() const { return point::Format::from_c(lasrs_point_data_format(get())); }

    Vector<Transform> transforms() const { return detail::from_c(lasrs_point_data_transforms(get())); }

    std::span<const uint8_t> raw_bytes() const { return detail::to_span(lasrs_point_data_raw_bytes(get())); }

    size_t record_len() const { return lasrs_point_data_record_len(get()); }

    std::vector<Point> points() const {
        const size_t n = len();
        const size_t extra_len = format().extra_bytes;
        std::vector<LasrsPoint> c_points(n);
        std::vector<uint8_t> extra(n * extra_len);
        detail::check(lasrs_point_data_points(get(), c_points.data(), extra.data()));
        std::vector<Point> result;
        result.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            result.push_back(detail::from_c(c_points[i], std::span(extra).subspan(i * extra_len, extra_len)));
        }
        return result;
    }

    std::span<uint8_t> resize_for(size_t n) {
        uint8_t* bytes = lasrs_point_data_resize_for(handle_.get(), n);
        return {bytes, n * record_len()};
    }

    std::vector<int32_t> x_raw() const { return column<int32_t>(lasrs_point_data_x_raw); }
    std::vector<int32_t> y_raw() const { return column<int32_t>(lasrs_point_data_y_raw); }
    std::vector<int32_t> z_raw() const { return column<int32_t>(lasrs_point_data_z_raw); }
    std::vector<double> x() const { return column<double>(lasrs_point_data_x); }
    std::vector<double> y() const { return column<double>(lasrs_point_data_y); }
    std::vector<double> z() const { return column<double>(lasrs_point_data_z); }
    std::vector<uint16_t> intensity() const { return column<uint16_t>(lasrs_point_data_intensity); }
    std::vector<uint8_t> classification() const { return column<uint8_t>(lasrs_point_data_classification); }
    std::vector<uint8_t> return_number() const { return column<uint8_t>(lasrs_point_data_return_number); }
    std::vector<uint8_t> number_of_returns() const { return column<uint8_t>(lasrs_point_data_number_of_returns); }
    std::vector<float> scan_angle_degrees() const { return column<float>(lasrs_point_data_scan_angle_degrees); }
    std::vector<uint8_t> user_data() const { return column<uint8_t>(lasrs_point_data_user_data); }
    std::vector<uint16_t> point_source_id() const { return column<uint16_t>(lasrs_point_data_point_source_id); }

    std::optional<std::vector<double>> gps_time() const { return optional_column<double>(lasrs_point_data_gps_time); }

    std::optional<std::vector<std::tuple<uint16_t, uint16_t, uint16_t>>> rgb() const {
        const auto colors = optional_column<LasrsColor>(lasrs_point_data_rgb);
        if (!colors) {
            return std::nullopt;
        }
        std::vector<std::tuple<uint16_t, uint16_t, uint16_t>> result;
        result.reserve(colors->size());
        for (const auto& c : *colors) {
            result.emplace_back(c.red, c.green, c.blue);
        }
        return result;
    }

    std::optional<std::vector<uint16_t>> nir() const { return optional_column<uint16_t>(lasrs_point_data_nir); }

    const LasrsPointData* c_ptr() const { return handle_.get(); }
    LasrsPointData* c_ptr() { return handle_.get(); }

    static PointData from_c(LasrsPointData* owned) { return PointData(owned); }

private:
    explicit PointData(LasrsPointData* owned) : handle_(owned) {}

    const LasrsPointData* get() const { return handle_.get(); }

    template <class T>
    std::vector<T> column(void (*fill)(const LasrsPointData*, T*)) const {
        std::vector<T> values(len());
        fill(get(), values.data());
        return values;
    }

    template <class T>
    std::optional<std::vector<T>> optional_column(bool (*fill)(const LasrsPointData*, T*)) const {
        std::vector<T> values(len());
        if (!fill(get(), values.data())) {
            return std::nullopt;
        }
        return values;
    }

    detail::Handle<LasrsPointData, lasrs_point_data_free> handle_;
};

class PointDataBuilder {
public:
    PointDataBuilder() = default;

    PointDataBuilder with_format(const point::Format& format) const {
        auto builder = *this;
        builder.format_ = format;
        return builder;
    }

    PointDataBuilder with_transforms(const Vector<Transform>& transforms) const {
        auto builder = *this;
        builder.transforms_ = transforms;
        return builder;
    }

    PointDataBuilder for_header(const Header& header) const {
        return with_format(header.point_format()).with_transforms(header.transforms());
    }

    PointData build() const {
        return PointData::from_c(lasrs_point_data_build(format_.to_c(), detail::to_c(transforms_)));
    }

    PointData build_from_bytes(std::span<const uint8_t> bytes) const {
        LasrsPointData* points = nullptr;
        detail::check(
            lasrs_point_data_build_from_bytes(format_.to_c(), detail::to_c(transforms_), detail::to_c(bytes), &points));
        return PointData::from_c(points);
    }

    PointData build_from_points(const std::vector<Point>& points) const {
        const auto c = detail::to_c(points);
        LasrsPointData* result = nullptr;
        detail::check(lasrs_point_data_build_from_points(format_.to_c(), detail::to_c(transforms_), c.points.data(),
                                                         c.points.size(), c.extra_bytes.data(), &result));
        return PointData::from_c(result);
    }

private:
    point::Format format_;
    Vector<Transform> transforms_;
};

inline void Header::add_point_data(const PointData& points) { lasrs_header_add_point_data(mut_ptr(), points.c_ptr()); }

}  // namespace las
