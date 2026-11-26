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

/* The game advances in fixed steps, so how units behave does not depend on
   the frame rate, and a recording keeps each step: 60 a second, the rate the
   port ran at while it was capped to 60 fps. The original game had no fixed
   step: each frame advanced it by the time the frame took, plus one tick, at
   whatever frame rate the machine managed. A step's length is in timer ticks
   (1/1024 s): 17 at 60 steps a second. */
constexpr int32_t CVStepsPerSecond = 60;
constexpr int32_t CVStepTicks = 1024 / CVStepsPerSecond;
}

#endif