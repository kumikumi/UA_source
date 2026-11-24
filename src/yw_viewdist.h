#ifndef YW_VIEWDIST_H_INCLUDED
#define YW_VIEWDIST_H_INCLUDED

#include <array>
#include <string>
#include <vector>

#include "lstvw.h"

struct TViewDistance;
struct TInputState;

// Sliders for how far the world is drawn, opened with F10 during a game.
// The first slider moves all the others together; each of the others sets
// one of the limits on its own.
class ViewDistanceWindow : public GuiList
{
public:
    void Create(NC_STACK_ypaworld *yw);

    // The first time it goes to the right edge of the screen, top pixels
    // down; later it opens where the user left it.
    void Open(NC_STACK_ypaworld *yw, int top);
    void Close(NC_STACK_ypaworld *yw);

    void HandleInput(NC_STACK_ypaworld *yw, TInputState *inpt);

protected:
    enum
    {
        ROW_VIEW,
        ROW_SECTORS,
        ROW_CLIP,
        ROW_FOGSTART,
        ROW_FOGEND,
        ROW_FADESTART,
        ROW_FADEEND,
        ROW_MAX
    };

    enum
    {
        BTN_SLIDERS = 8, // One per row; GuiList uses the ones before for itself
        BTN_DEFAULT = BTN_SLIDERS + ROW_MAX,
        BTN_UNLIMITED,
        BTN_MAX
    };

    void UpdateSliders(NC_STACK_ypaworld *yw);
    void ApplySlider(NC_STACK_ypaworld *yw, int row, int pos);
    void StopDragging(NC_STACK_ypaworld *yw);

    void LayoutItems(NC_STACK_ypaworld *yw, int pressedBtn);
    void LayoutPadLine(NC_STACK_ypaworld *yw);
    void LayoutButton(NC_STACK_ypaworld *yw, int btn, bool pressed, const std::string &label);
    void LayoutSlider(NC_STACK_ypaworld *yw, int row);

    int KnobLeft(int row) const;
    int SliderPosAt(int row, int knobCenter) const;
    int DistancePos(float distance) const;
    std::string ValueText(NC_STACK_ypaworld *yw, int row, const TViewDistance &view) const;

protected:
    std::array<int, ROW_MAX> _sliderPos {};
    std::array<int, ROW_MAX> _sliderCount {};

    // The distances the sliders step through, the last one unlimited.
    std::vector<float> _distances;

    int  _dragRow = -1;    // The slider whose knob follows the mouse
    int  _grabOffset = 0;  // From the knob's centre to where it was grabbed
    bool _placed = false;

    int _padding = 0;      // Above and below the contents of each row
    int _labelWidth = 0;
    int _sliderWidth = 0;
    int _valueWidth = 0;
    int _knobWidth = 0;
    int _buttonWidth = 0;
};

#endif
