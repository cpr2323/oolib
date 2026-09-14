#include "oolib/GUI/RoundedSlideSwitch.h"
#include "oolib/GUI/ColourResolver.h"
#include "oolib/Debug/DebugLog.h"

RoundedSlideSwitch::RoundedSlideSwitch () : Button ({})
{
    setClickingTogglesState (true);
}

void RoundedSlideSwitch::mouseDown (const juce::MouseEvent& mouseEvent)
{
    if (mouseEvent.mods.isPopupMenu ())
    {
        if (onPopupMenuCallback != nullptr)
            onPopupMenuCallback ();
        wasPopupMenuClick = true;
    }
    else
    {
        Button::mouseDown (mouseEvent);
        wasPopupMenuClick = false;
    }
}

void RoundedSlideSwitch::mouseUp (const juce::MouseEvent& mouseEvent)
{
    if (wasPopupMenuClick)
        wasPopupMenuClick = false;
    else
        Button::mouseUp (mouseEvent);
}

void RoundedSlideSwitch::buttonStateChanged ()
{
    startTimer (10);
}

void RoundedSlideSwitch::timerCallback ()
{
    auto rate = 0.1f;
    rate *= getToggleState () ? 1.0f : -1.0f;

    position = juce::jlimit (0.0f, 1.0f, position + rate);

    if (position == 0.0f || position == 1.0f)
        stopTimer ();

    repaint ();
}

void RoundedSlideSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    auto area = getLocalBounds ().toFloat ();

    using namespace oolib;
    const auto trackOff { resolveColour (*this, ColourIds::slideSwitchTrackOff, juce::Colours::darkgrey) };
    const auto trackOn  { resolveColour (*this, ColourIds::slideSwitchTrackOn,  juce::Colours::lightgrey) };
    const auto thumbOff { resolveColour (*this, ColourIds::slideSwitchThumbOff, juce::Colours::lightgrey) };
    const auto thumbOn  { resolveColour (*this, ColourIds::slideSwitchThumbOn,  juce::Colours::darkgrey) };

    if (thumbShape == ThumbShape::circle)
    {
        const auto outlineOff { resolveColour (*this, ColourIds::slideSwitchTrackOutlineOff, juce::Colours::transparentBlack) };
        const auto outlineOn  { resolveColour (*this, ColourIds::slideSwitchTrackOutlineOn,  juce::Colours::transparentBlack) };
        const auto glow       { resolveColour (*this, ColourIds::slideSwitchThumbGlow,       juce::Colours::transparentBlack) };

        const auto trackBounds { area.reduced (0.5f) };
        const auto cornerSize { trackBounds.getHeight () * 0.5f };
        g.setColour (trackOff.interpolatedWith (trackOn, position));
        g.fillRoundedRectangle (trackBounds, cornerSize);
        if (const auto outline { outlineOff.interpolatedWith (outlineOn, position) }; ! outline.isTransparent ())
        {
            g.setColour (outline);
            g.drawRoundedRectangle (trackBounds, cornerSize, 1.0f);
        }

        const auto diameter { area.getHeight () - 4.0f };
        const auto travel { area.getWidth () - 4.0f - diameter };
        const juce::Rectangle<float> thumbBounds { 2.0f + (travel * position), 2.0f, diameter, diameter };
        if (! glow.isTransparent () && position > 0.0f)
        {
            juce::Path thumbPath;
            thumbPath.addEllipse (thumbBounds);
            juce::DropShadow (glow.withMultipliedAlpha (position), juce::roundToInt (diameter * 0.55f), {}).drawForPath (g, thumbPath);
        }
        g.setColour (thumbOff.interpolatedWith (thumbOn, position));
        g.fillEllipse (thumbBounds);
        return;
    }

    g.setColour (trackOff.interpolatedWith (trackOn, position));
    g.fillRoundedRectangle (area, 8.0f);

    g.setColour (thumbOff.interpolatedWith (thumbOn, position));
    auto buttonWidth { area.getWidth () / 2.0f };
    g.fillRoundedRectangle (2.0f + (buttonWidth * position), 2.0f, buttonWidth - 4.0f, area.getHeight () - 4.0f, 6.0f);
}
