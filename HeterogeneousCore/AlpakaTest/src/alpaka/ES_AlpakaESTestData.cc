#include "HeterogeneousCore/AlpakaCore/interface/alpaka/typelookup.h"

// PortableCollection-based model
#include "HeterogeneousCore/AlpakaTest/interface/alpaka/AlpakaESTestData.h"
TYPELOOKUP_AV2P_DATA_REG(AlpakaESTestDataADevice);
TYPELOOKUP_AV2P_DATA_REG(AlpakaESTestDataCDevice);
TYPELOOKUP_AV2P_DATA_REG(AlpakaESTestDataDDevice);
TYPELOOKUP_AV2P_DATA_REG(AlpakaESTestDataACBlocksDevice);

// Template-over-device model
#include "HeterogeneousCore/AlpakaTest/interface/AlpakaESTestData.h"
TYPELOOKUP_AV2P_TEMPLATED_DATA_REG(cms::alpakatest::AlpakaESTestDataB);

// Template-over-device model with PortableCollection members
TYPELOOKUP_AV2P_TEMPLATED_DATA_REG(cms::alpakatest::AlpakaESTestDataE);
