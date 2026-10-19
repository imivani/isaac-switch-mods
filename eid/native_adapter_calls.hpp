#pragma once
#include <cstdint>

namespace isaac::eid_native {
// KAGE uses NpadId + 1: connected pads are 1..8 and handheld is 33.
// This is a device identifier, not a zero-based player-array index.
// Evidence: Manager::Poll 0x4fbae8 and its table at 0x8c3cc0.
constexpr bool ControllerDevice(unsigned id) {
    return (id >= 1 && id <= 8) || id == 33;
}

#ifdef ISAAC_EID_ADAPTER_TEST
// Desktop tests compile the production adapter and replace only native calls.
// The Switch build does not contain or resolve this testing entry point.
std::uintptr_t ResolveTestFunction(std::uintptr_t module, std::uintptr_t offset);
#endif
template<class Signature>
inline Signature Function(std::uintptr_t module, std::uintptr_t offset) {
#ifdef ISAAC_EID_ADAPTER_TEST
    return reinterpret_cast<Signature>(ResolveTestFunction(module, offset));
#else
    return reinterpret_cast<Signature>(module + offset);
#endif
}
}
