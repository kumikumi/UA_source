#include <cstring>

#include "playercontrols.h"
#include "../system/inpt.h"

namespace World
{

// Which of the input's sliders and buttons they are, in their order
static const std::array<uint8_t, PlayerControls::SliderCount> SliderIds = {{0, 1, 2, 3, 4, 5, 15}};
static const std::array<uint8_t, PlayerControls::ButtonCount> ButtonIds = {{0, 1, 2, 3, 31}};

PlayerControls PlayerControls::Of(const TInputState &input)
{
    PlayerControls controls;

    for ( size_t i = 0; i < SliderIds.size(); i++ )
        controls.Sliders[i] = input.Sliders[SliderIds[i]];

    for ( size_t i = 0; i < ButtonIds.size(); i++ )
    {
        if ( input.Buttons.Is(ButtonIds[i]) )
            controls.Buttons |= 1 << i;
    }

    return controls;
}

void PlayerControls::To(TInputState &input) const
{
    for ( size_t i = 0; i < SliderIds.size(); i++ )
        input.Sliders[SliderIds[i]] = Sliders[i];

    for ( size_t i = 0; i < ButtonIds.size(); i++ )
    {
        if ( Buttons & (1 << i) )
            input.Buttons.Set(ButtonIds[i]);
        else
            input.Buttons.UnSet(ButtonIds[i]);
    }
}

std::string PlayerControls::Write() const
{
    std::string out;
    out.append((const char *)&Number, sizeof(Number));
    out.append((const char *)Sliders.data(), sizeof(float) * Sliders.size());
    out.append((const char *)&Buttons, sizeof(Buttons));
    return out;
}

bool PlayerControls::Read(const uint8_t *data, size_t size)
{
    if ( size != sizeof(Number) + sizeof(float) * Sliders.size() + sizeof(Buttons) )
        return false;

    memcpy(&Number, data, sizeof(Number));
    data += sizeof(Number);
    memcpy(Sliders.data(), data, sizeof(float) * Sliders.size());
    data += sizeof(float) * Sliders.size();
    memcpy(&Buttons, data, sizeof(Buttons));
    return true;
}

}
