#ifndef HeterogeneousCore_AlpakaInterface_interface_concepts_h
#define HeterogeneousCore_AlpakaInterface_interface_concepts_h

#include <type_traits>

#include <aV2P/alpaka.hpp>

namespace cms::alpakatools {
  template <typename T>
  concept NonCPUQueue = aV2P::concepts::Queue<T> and not std::is_same_v<aV2P::Dev<T>, aV2P::DevCpu>;
}  // namespace cms::alpakatools

#endif
