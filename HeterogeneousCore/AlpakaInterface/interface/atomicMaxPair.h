#ifndef HeterogeneousCore_AlpakaCore_interface_atomicMaxPair_h
#define HeterogeneousCore_AlpakaCore_interface_atomicMaxPair_h
#include <aV2P/alpaka.hpp>

#include "FWCore/Utilities/interface/bit_cast.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

// Note: Does not compile with AV2P_FN_ACC on ROCm
template <aV2P::concepts::Acc TAcc, typename F>
AV2P_FN_HOST_ACC AV2P_FN_INLINE void atomicMaxPair(const TAcc& acc,
                                                       unsigned long long int* address,
                                                       std::pair<unsigned int, float> value,
                                                       F comparator) {
#if defined(__CUDA_ARCH__) or defined(__HIP_DEVICE_COMPILE__)
  unsigned long long int val = (static_cast<unsigned long long int>(value.first) << 32) + __float_as_uint(value.second);
  unsigned long long int ret = *address;
  while (comparator(value,
                    std::pair<unsigned int, float>{static_cast<unsigned int>(ret >> 32 & 0xffffffff),
                                                   __uint_as_float(ret & 0xffffffff)})) {
    unsigned long long int old = ret;
    if ((ret = atomicCAS(address, old, val)) == old)
      break;
  }
#else
  unsigned long long int val =
      (static_cast<unsigned long long int>(value.first) << 32) + edm::bit_cast<unsigned int>(value.second);
  unsigned long long int ret = *address;
  while (comparator(value,
                    std::pair{static_cast<unsigned int>(ret >> 32 & 0xffffffff),
                              edm::bit_cast<float>(static_cast<unsigned int>(ret & 0xffffffff))})) {
    unsigned long long int old = ret;
    if ((ret = aV2P::atomicCas(acc, address, old, val)) == old)
      break;
  }
#endif  // __CUDA_ARCH__ or __HIP_DEVICE_COMPILE__
}

#endif  // HeterogeneousCore_AlpakaCore_interface_atomicMaxPair_h
