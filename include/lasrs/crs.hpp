// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/error.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace las::crs
{

using GeoTiffData = std::variant<uint16_t, std::string, std::vector<double>>;

struct GeoTiffKeyEntry
{
    uint16_t id = 0;
    GeoTiffData data;

    bool is_gt_model_type_geo_key() const
    {
        return id == 1024;
    }
    bool is_geodetic_crs_geo_key() const
    {
        return id == 2048;
    }
    bool is_projected_crs_geo_key() const
    {
        return id == 3072;
    }
    bool is_vertical_crs_geo_key() const
    {
        return id == 4096;
    }

    bool operator==(const GeoTiffKeyEntry &) const = default;
};

struct GeoTiffCrs
{
    std::vector<GeoTiffKeyEntry> entries;

    std::optional<uint16_t> get_gt_model_type_geo_key_value() const
    {
        return u16_value(&GeoTiffKeyEntry::is_gt_model_type_geo_key);
    }
    std::optional<uint16_t> get_geodetic_crs_geo_key_value() const
    {
        return u16_value(&GeoTiffKeyEntry::is_geodetic_crs_geo_key);
    }
    std::optional<uint16_t> get_projected_crs_geo_key_value() const
    {
        return u16_value(&GeoTiffKeyEntry::is_projected_crs_geo_key);
    }
    std::optional<uint16_t> get_vertical_crs_geo_key_value() const
    {
        return u16_value(&GeoTiffKeyEntry::is_vertical_crs_geo_key);
    }

    bool operator==(const GeoTiffCrs &) const = default;

  private:
    // Like las-rs: the first entry with the key, if its data is a u16.
    std::optional<uint16_t> u16_value(bool (GeoTiffKeyEntry::*is_key)() const) const
    {
        const auto entry = std::ranges::find_if(entries, [&](const GeoTiffKeyEntry &e) { return (e.*is_key)(); });
        if (entry == entries.end() || !std::holds_alternative<uint16_t>(entry->data))
        {
            return std::nullopt;
        }
        return std::get<uint16_t>(entry->data);
    }
};

namespace detail
{

inline GeoTiffKeyEntry from_c(const LasrsGeoTiffKeyEntry &entry)
{
    switch (entry.kind)
    {
    case LASRS_GEO_TIFF_DATA_KIND_STRING:
        return {entry.id, std::string(las::detail::to_string_view(entry.string))};
    case LASRS_GEO_TIFF_DATA_KIND_DOUBLES:
        return {entry.id, std::vector<double>(entry.doubles, entry.doubles + entry.doubles_len)};
    case LASRS_GEO_TIFF_DATA_KIND_U16:
        break;
    }
    return {entry.id, entry.u16_value};
}

} // namespace detail
} // namespace las::crs
