#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    TimelineComponent - a reusable horizontal ruler for a waveform / timeline view.

    It is deliberately decoupled from any audio data. You drive it with the SAME
    view mapping the waveform uses:

        setView (startSample, samplesPerPixel);

    so that, as long as this component shares the waveform's horizontal bounds
    (same x and width), a given pixel column maps to the same sample in both.

    Supported display units:
        * Minutes:Seconds  (needs sample rate)
        * Beats:Bars       (needs sample rate + tempo/metre)
        * Samples

    Right-click brings up a unit-selection popup. Which units that popup offers
    is configurable (setAvailableUnits) so a host that only ever deals in, say,
    samples can restrict it - and when only one unit is available there is
    nothing to choose, so the popup is suppressed entirely.

    The host can also change the unit programmatically (e.g. from a
    View > Timeline menu) and is notified of user changes via onUnitChanged.
*/
class TimelineComponent : public juce::Component
{
public:
    enum class Unit { timeMinutesSeconds, beatsAndBars, samples };

    TimelineComponent ();
    ~TimelineComponent () override = default;

    // Context needed to label the ticks.
    void setSampleRate (double newSampleRate);
    void setTempo (double bpm, int beatsPerBar);

    // The shared view mapping - call whenever the waveform's view changes.
    void setView (double startSample, double samplesPerPixel);

    // Requests to switch to a unit that isn't currently available are ignored.
    void setUnit (Unit newUnit);
    Unit getUnit () const noexcept { return unit; }

    //==============================================================================
    // Which units the right-click popup offers, in the order they appear.
    // Defaults to all of them. With a single unit the popup is not shown at all.
    // An empty list is ignored (all units stay available). If the displayed unit
    // is dropped from the list, the timeline falls back to the first available
    // one and reports that through onUnitChanged.
    void setAvailableUnits (const juce::Array<Unit>& units);

    const juce::Array<Unit>& getAvailableUnits () const noexcept { return availableUnits; }
    bool isUnitAvailable (Unit u) const noexcept                 { return availableUnits.contains (u); }

    static juce::Array<Unit> getAllUnits ();

    double getBpm         () const noexcept { return bpm; }
    int    getBeatsPerBar () const noexcept { return beatsPerBar; }

    static juce::String getUnitName (Unit u);

    // High-precision label for a single sample position in the current unit
    // (for a cursor readout). Distinct from the ruler's rounded tick labels.
    juce::String formatSamplePosition (double sample) const;

    struct ColourScheme
    {
        juce::Colour background { juce::Colour (0xff1e1e1e) };
        juce::Colour majorTick  { juce::Colour (0xffcfcfcf) };
        juce::Colour minorTick  { juce::Colour (0xff777777) };
        juce::Colour text       { juce::Colour (0xffd0d0d0) };
    };
    void setColourScheme (const ColourScheme& scheme);

    // Same transforms as the waveform (this component's local coordinates).
    double xToSample (float x)      const noexcept;
    float  sampleToX (double sample) const noexcept;

    // Notified when the unit changes by any route other than an explicit
    // setUnit () call: the right-click popup, or setAvailableUnits () dropping
    // the unit that was on display.
    std::function<void (Unit)> onUnitChanged;

private:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void showUnitMenu (juce::Point<int> screenPos);

    struct Tick
    {
        double       sample;
        bool         major;
        juce::String label; // only for major ticks
    };
    void buildTicks (std::vector<Tick>& ticks) const;
    void buildTimeTicks    (std::vector<Tick>&, double first, double last) const;
    void buildBeatTicks    (std::vector<Tick>&, double first, double last) const;
    void buildSampleTicks  (std::vector<Tick>&, double first, double last) const;

    double viewStartSample { 0.0 };
    double samplesPerPixel { 1.0 };
    double sampleRate      { 44100.0 };
    double bpm             { 120.0 };
    int    beatsPerBar     { 4 };

    Unit unit { Unit::timeMinutesSeconds };
    juce::Array<Unit> availableUnits;
    ColourScheme colours;

    // Approximate target spacing between labelled (major) ticks, in pixels.
    static constexpr double kTargetMajorPixels { 84.0 };
    static constexpr double kMinMinorPixels    { 7.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
};
