
#ifndef DataFormats_TICL_interface_FillAssociator_h
#define DataFormats_TICL_interface_FillAssociator_h

#include "DataFormats/TICL/interface/AssociationMap.h"
#include "DataFormats/TICL/interface/detail/FillAssociator.h"
#include <aV2P/alpaka.hpp>
#include <span>

namespace ticl::associator {

  template <aV2P::concepts::Acc TAcc, typename TQueue, std::integral TKey, concepts::trivially_copyable TMapped>
    requires aV2P::concepts::Queue<TQueue>
  AV2P_FN_HOST auto fill(TQueue& queue,
                           ticl::AssociationMapView<TKey, TMapped>& map,
                           std::span<const TKey> keys,
                           std::span<const TMapped> values) {
    detail::fill<TAcc>(queue, map, keys, values);
  }

}  // namespace ticl::associator

#endif
