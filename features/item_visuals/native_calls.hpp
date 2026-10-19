#pragma once
#include <cstdint>

namespace isaac_item_visuals {
#ifdef ISAAC_ITEM_VISUALS_ADAPTER_TEST
namespace test { std::uintptr_t Resolve(std::uintptr_t module, std::uintptr_t offset); }
#endif
template<class Function> Function NativeCall(std::uintptr_t module, std::uintptr_t offset) {
#ifdef ISAAC_ITEM_VISUALS_ADAPTER_TEST
    return reinterpret_cast<Function>(test::Resolve(module, offset));
#else
    return reinterpret_cast<Function>(module + offset);
#endif
}
}
