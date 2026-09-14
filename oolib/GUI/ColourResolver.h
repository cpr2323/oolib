#pragma once

#include <JuceHeader.h>

/*
    Lets an oolib component take its colours from the host app without knowing
    anything about themes, and without breaking any app that offers none.

    juce::LookAndFeel::findColour asserts and returns black for an ID it has not
    been given, so a shared component cannot simply call findColour with an ID of
    its own - in an app that never registers it, the component would paint black.

    resolveColour tries three sources in order:

      1. a colour set on this instance with Component::setColour
      2. the LookAndFeel in scope, but only if it actually defines the ID
      3. the fallback the component was written with

    So a component keeps its existing appearance everywhere by default, and picks
    up an app's palette only where one is offered.
*/
namespace oolib
{
    inline juce::Colour resolveColour (const juce::Component& component, int colourId, juce::Colour fallback)
    {
        if (component.isColourSpecified (colourId))
            return component.findColour (colourId);

        const auto& lookAndFeel { component.getLookAndFeel () };
        if (lookAndFeel.isColourSpecified (colourId))
            return lookAndFeel.findColour (colourId);

        return fallback;
    }

    /*
        Colour IDs for oolib's own components. The base is picked to sit clear of
        JUCE's IDs (around 0x1000000) and of any app's.
    */
    namespace ColourIds
    {
        enum
        {
            // SplitWindowComponent
            splitterBackground = 0x6f6f0001,
            splitterHandle,
            splitterHandleOutline,

            // RoundedSlideSwitch
            slideSwitchTrackOff,
            slideSwitchTrackOn,
            slideSwitchThumbOff,
            slideSwitchThumbOn,

            // CustomTextEditor
            customTextEditorText,

            // Optional extras. Each falls back to transparent, and a transparent
            // result means the extra is simply not drawn, so an app that does not
            // define them keeps the original look.

            // SplitWindowComponent - draw the bar as a hairline in this colour,
            // instead of a filled handle
            splitterDivider,
            // RoundedSlideSwitch - a rim around the track, and a halo on the thumb
            slideSwitchTrackOutlineOff,
            slideSwitchTrackOutlineOn,
            slideSwitchThumbGlow
        };
    }
}
