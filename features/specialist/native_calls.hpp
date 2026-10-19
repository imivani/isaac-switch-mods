#pragma once
#include <cstdint>
namespace isaac::specialist::native {
#ifdef ISAAC_SPECIALIST_ADAPTER_TEST
std::uintptr_t ResolveTestFunction(std::uintptr_t module, std::uintptr_t offset);
#endif
template<class Signature> inline Signature Function(std::uintptr_t module, std::uintptr_t offset) {
#ifdef ISAAC_SPECIALIST_ADAPTER_TEST
    return reinterpret_cast<Signature>(ResolveTestFunction(module, offset));
#else
    return reinterpret_cast<Signature>(module + offset);
#endif
}
}
