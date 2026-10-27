#ifndef RecoVertex_PixelVertexFinding_plugins_alpaka_PixelVertexWorkSpaceSoADeviceAlpaka_h
#define RecoVertex_PixelVertexFinding_plugins_alpaka_PixelVertexWorkSpaceSoADeviceAlpaka_h

#include <aV2P/alpaka.hpp>

#include "DataFormats/Portable/interface/alpaka/PortableCollection.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "RecoVertex/PixelVertexFinding/interface/PixelVertexWorkSpaceLayout.h"
#include "RecoVertex/PixelVertexFinding/plugins/PixelVertexWorkSpaceSoAHostAlpaka.h"

namespace AV2P_ACCELERATOR_NAMESPACE {

  namespace vertexFinder {

    using PixelVertexWorkSpaceSoADevice = PortableCollection<::vertexFinder::PixelVertexWSSoALayout<>>;
    using PixelVertexWorkSpaceSoAHost = ::vertexFinder::PixelVertexWorkSpaceSoAHost;

  }  // namespace vertexFinder

}  // namespace AV2P_ACCELERATOR_NAMESPACE

#endif  // RecoVertex_PixelVertexFinding_plugins_alpaka_PixelVertexWorkSpaceSoADeviceAlpaka_h
