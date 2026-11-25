#ifndef WORLD_CONSTS_H
#define WORLD_CONSTS_H

#include <cstddef>
#include <cstdint>

namespace World {
// Constant values
constexpr float CVSectorLength = 1200.0;
constexpr float CVSectorHalfLength = CVSectorLength / 2.0;
constexpr size_t CVLocaleStringsNumber = 2700;
constexpr size_t CVFractionsCount = 8;
constexpr size_t CVMaxMap = 256;
constexpr size_t CVMaxNetPlayers = 4;

/* How long will construct buildings */
constexpr size_t CVBuildConstructTime = 3000;

/* Target unit too far from sector target distance*/
constexpr float CVUnitFarSecDist = 3600.0;

/* The id the first unit of a level gets */
constexpr uint32_t CVFirstUnitId = 0x10000;
}

#endif