#pragma once
#include <cstddef>
#include <cstdint>

namespace isaac_eid {

// Numeric values match native pickup variants, except HorsePill, which is an
// internal category. Native horse pills still have variant 70.
enum class Category : std::uint16_t {
    Pill = 70, Collectible = 100, Card = 300, Trinket = 350, HorsePill = 1070
};

struct EntryIndex {
    std::uint16_t category;
    std::uint16_t lookup_id;
    std::uint32_t name_offset;
    std::uint32_t markup_offset;
    std::uint32_t placeholder;
};

struct Description {
    const char* name = nullptr;
    const char* markup = nullptr;
};

struct Catalogue {
    const EntryIndex* entries;
    std::size_t count;
    const char* strings;
    std::size_t string_bytes;

    // IDs follow EID's table keys, NOT always native subtype. Pill IDs are the
    // resolved effect + 1 (gold = 9999). Golden trinkets need their flag removed.
    // Returns false for unknown IDs and unused slots retained by upstream.
    bool Find(Category category, std::uint32_t lookup_id, Description& out) const {
        out = {};
        if (!entries || !strings || lookup_id == 0 || lookup_id > 0xffffu) return false;
        const std::uint32_t wanted = (static_cast<std::uint32_t>(category) << 16) | lookup_id;
        std::size_t lower = 0, upper = count;
        while (lower < upper) {
            const std::size_t mid = lower + (upper - lower) / 2;
            const auto& entry = entries[mid];
            const std::uint32_t key = (static_cast<std::uint32_t>(entry.category) << 16) | entry.lookup_id;
            if (key < wanted) lower = mid + 1;
            else upper = mid;
        }
        if (lower == count) return false;
        const auto& entry = entries[lower];
        if (entry.category != static_cast<std::uint16_t>(category) || entry.lookup_id != lookup_id ||
            entry.placeholder || entry.name_offset >= string_bytes || entry.markup_offset >= string_bytes) return false;
        out = {strings + entry.name_offset, strings + entry.markup_offset};
        return true;
    }
};

Catalogue EnglishRepentance();
const char* SourceCommit();

} // namespace isaac_eid
