// SPDX-License-Identifier: MIT OR Apache-2.0
#pragma once

#include <lasrs/point_data.hpp>
#include <lasrs/stream.hpp>

namespace las {

class LodSelection {
public:
    static LodSelection All() { return LodSelection({LASRS_LOD_SELECTION_KIND_ALL, 0.0, 0, 0}); }
    static LodSelection Resolution(double resolution) {
        return LodSelection({LASRS_LOD_SELECTION_KIND_RESOLUTION, resolution, 0, 0});
    }
    static LodSelection Level(int32_t level) { return LodSelection({LASRS_LOD_SELECTION_KIND_LEVEL, 0.0, level, 0}); }
    static LodSelection LevelMinMax(int32_t min, int32_t max) {
        return LodSelection({LASRS_LOD_SELECTION_KIND_LEVEL_MIN_MAX, 0.0, min, max});
    }

    LasrsLodSelection to_c() const { return selection_; }

private:
    explicit LodSelection(LasrsLodSelection selection) : selection_(selection) {}

    LasrsLodSelection selection_;
};

class BoundsSelection {
public:
    static BoundsSelection All() { return BoundsSelection({false, {}}); }
    static BoundsSelection Within(const Bounds& bounds) { return BoundsSelection({true, detail::to_c(bounds)}); }

    LasrsBoundsSelection to_c() const { return selection_; }

private:
    explicit BoundsSelection(LasrsBoundsSelection selection) : selection_(selection) {}

    LasrsBoundsSelection selection_;
};

// Stream-based readers keep a reference to the stream: it must outlive the
// reader.
class CopcReader {
public:
    explicit CopcReader(std::istream& stream)
        : CopcReader(open(std::make_unique<detail::InputStream>(stream),
                          [](LasrsInputStream in, LasrsCopcReader** out) { return lasrs_copc_reader_new(in, out); })) {}

    static CopcReader from_path(const std::filesystem::path& path) {
        const auto utf8 = detail::path_to_utf8(path);
        return open(nullptr, [&](LasrsInputStream, LasrsCopcReader** out) {
            return lasrs_copc_reader_from_path(detail::to_c(std::string_view(utf8)), out);
        });
    }

    CopcReader(CopcReader&&) noexcept = default;
    CopcReader& operator=(CopcReader&&) noexcept = default;
    CopcReader(const CopcReader&) = delete;
    CopcReader& operator=(const CopcReader&) = delete;
    ~CopcReader() = default;

    const Header& header() const { return header_; }

    std::vector<copc::Entry> hierarchy_entries() const {
        std::vector<LasrsEntry> entries(lasrs_copc_reader_hierarchy_entries_len(handle_.get()));
        entries.resize(lasrs_copc_reader_hierarchy_entries(handle_.get(), entries.data(), entries.size()));
        std::vector<copc::Entry> result;
        result.reserve(entries.size());
        for (const auto& entry : entries) {
            result.push_back(detail::from_c(entry));
        }
        return result;
    }

    std::optional<copc::Entry> hierarchy_entry(const copc::VoxelKey& key) const {
        LasrsEntry entry{};
        if (!lasrs_copc_reader_hierarchy_entry(handle_.get(), detail::to_c(key), &entry)) {
            return std::nullopt;
        }
        return detail::from_c(entry);
    }

    PointData read_entry(const copc::Entry& entry) {
        LasrsPointData* points = nullptr;
        detail::check(lasrs_copc_reader_read_entry(handle_.get(), detail::to_c(entry), &points));
        return PointData::from_c(points);
    }

    PointData query(const LodSelection& levels, const BoundsSelection& bounds) {
        LasrsPointData* points = nullptr;
        detail::check(lasrs_copc_reader_query(handle_.get(), levels.to_c(), bounds.to_c(), &points));
        return PointData::from_c(points);
    }

private:
    CopcReader(std::unique_ptr<detail::InputStream> input, LasrsCopcReader* reader)
        : input_(std::move(input)), handle_(reader), header_(Header::Borrowed{}, lasrs_copc_reader_header(reader)) {}

    template <class Open>
    static CopcReader open(std::unique_ptr<detail::InputStream> input, Open open_reader) {
        LasrsCopcReader* reader = nullptr;
        detail::check(open_reader(input ? input->to_c() : LasrsInputStream{}, &reader));
        return CopcReader(std::move(input), reader);
    }

    std::unique_ptr<detail::InputStream> input_;
    detail::Handle<LasrsCopcReader, lasrs_copc_reader_free> handle_;
    Header header_;
};

}  // namespace las
