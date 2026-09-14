#pragma once

#include <JuceHeader.h>

using OnPopupMenuCallback = std::function<void ()>;

class RoundedSlideSwitch : public juce::Button,
                                  juce::Timer
{
public:
    RoundedSlideSwitch ();

    // roundedBar is the original half width thumb; circle is a round knob inset
    // in a pill track
    enum class ThumbShape { roundedBar, circle };
    void setThumbShape (ThumbShape newThumbShape) { thumbShape = newThumbShape; repaint (); }

    OnPopupMenuCallback onPopupMenuCallback;

private:
    ThumbShape thumbShape { ThumbShape::roundedBar };
    float position { 0.0f };
    bool wasPopupMenuClick { false };

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void buttonStateChanged () override;
    void paintButton (juce::Graphics& g, bool, bool) override;
    void timerCallback () override;
};
