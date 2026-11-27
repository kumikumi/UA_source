#ifndef WORLD_PLAYERCONTROLS_H_INCLUDED
#define WORLD_PLAYERCONTROLS_H_INCLUDED

#include <array>
#include <cstdint>
#include <string>

struct TInputState;

namespace World
{

// What a player steers the unit they control with in a step: the part of
// their input that units read (User_layer), as the game's interface leaves
// it (NC_STACK_ypaworld::PrepareUserInput). A player at another machine
// sends theirs every step, and the game goes by them step by step in the
// order they were made (Net::ViewServer::TakeControls).
struct PlayerControls
{
    // The input's sliders and buttons that units read: sliders 0-5 and 15,
    // buttons 0-3 and 31
    static constexpr size_t SliderCount = 7;
    static constexpr size_t ButtonCount = 5;

    uint32_t Number = 0; // Counts them as they're made, from 1
    std::array<float, SliderCount> Sliders = {};
    uint8_t Buttons = 0; // A bit for each of the buttons held

    // Of an input state; into one, leaving the rest of it as it is
    static PlayerControls Of(const TInputState &input);
    void To(TInputState &input) const;

    // As bytes; false when they don't read as controls
    std::string Write() const;
    bool Read(const uint8_t *data, size_t size);
};

}

#endif
