#ifndef DataFormats_VertexSoA_ZVertexHost_H
#define DataFormats_VertexSoA_ZVertexHost_H

#include <cstdint>

#include <aV2P/alpaka.hpp>

#include "DataFormats/Portable/interface/PortableHostCollection.h"
#include "DataFormats/VertexSoA/interface/ZVertexSoA.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

namespace reco {
  using ZVertexHost = PortableHostCollection<reco::ZVertexBlocks>;
}  // namespace reco

#endif  // DataFormats_VertexSoA_ZVertexHost_H
