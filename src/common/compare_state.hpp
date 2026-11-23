#pragma once
#include <lv2/atom/atom.h>
#include <lv2/urid/urid.h>

#include <array>
#include <cstdint>

namespace voiceforge {
inline constexpr unsigned compareCapacity = 160;
inline constexpr unsigned compareInputOffset = 122;
inline constexpr unsigned compareOutputOffset = compareInputOffset + 1;
struct CompareState {
    LV2_Atom_Vector_Body vector{sizeof(float), 0};
    // Selection, presence of A/B, then both port-indexed snapshots.
    std::array<float, 3 + 2 * compareCapacity> values{};
};
static_assert(sizeof(CompareState) == sizeof(LV2_Atom_Vector_Body) + sizeof(CompareState::values));

struct CompareUrids {
    LV2_URID sequence, eventTransfer, vector, boolean, floating, property;
    explicit CompareUrids(const LV2_URID_Map* map) noexcept
        : sequence(id(map, LV2_ATOM__Sequence)), eventTransfer(id(map, LV2_ATOM__eventTransfer)),
          vector(id(map, LV2_ATOM__Vector)), boolean(id(map, LV2_ATOM__Bool)),
          floating(id(map, LV2_ATOM__Float)),
          property(id(map, "https://weikiboy-tech.github.io/voiceforge-lv2/compare-state-v1")) {}
private:
    static LV2_URID id(const LV2_URID_Map* m, const char* uri) noexcept {
        return m ? m->map(m->handle, uri) : 0;
    }
};
} // namespace voiceforge
