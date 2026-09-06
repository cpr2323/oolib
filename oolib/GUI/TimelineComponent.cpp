#include "oolib/GUI/TimelineComponent.h"

namespace
{
    // Round to a '1/2/5 x 10^k' value near x. roundUp=false picks the nearest
    // nice value at or below x's scale; useful for choosing tick steps.
    double niceNumber (double x, bool roundToNearest)
    {
        if (x <= 0.0)
            return 1.0;
        const auto expo = std::floor (std::log10 (x));
        const auto frac = x / std::pow (10.0, expo); // 1..10
        double niceFrac;
        if (roundToNearest)
            niceFrac = frac < 1.5 ? 1.0 : frac < 3.0 ? 2.0 : frac < 7.0 ? 5.0 : 10.0;
        else
            niceFrac = frac <= 1.0 ? 1.0 : frac <= 2.0 ? 2.0 : frac <= 5.0 ? 5.0 : 10.0;
        return niceFrac * std::pow (10.0, expo);
    }

    // Smallest ladder value >= target (or the largest if none qualifies).
    double pickFromLadder (double target, const std::vector<double>& ladder)
    {
        for (auto v : ladder)
            if (v >= target)
                return v;
        return ladder.back ();
    }

    juce::String pad2 (int v)
    {
        return juce::String (v).paddedLeft ('0', 2);
    }
}

//==============================================================================
TimelineComponent::TimelineComponent ()
{
    availableUnits = getAllUnits ();
    setInterceptsMouseClicks (true, false);
}

void TimelineComponent::setSampleRate (double newSampleRate)
{
    sampleRate = juce::jmax (1.0, newSampleRate);
    repaint ();
}

void TimelineComponent::setTempo (double newBpm, int newBeatsPerBar)
{
    bpm         = juce::jmax (1.0, newBpm);
    beatsPerBar = juce::jmax (1, newBeatsPerBar);
    repaint ();
}

void TimelineComponent::setView (double startSample, double newSamplesPerPixel)
{
    viewStartSample = startSample;
    samplesPerPixel = juce::jmax (1.0e-9, newSamplesPerPixel);
    repaint ();
}

void TimelineComponent::setUnit (Unit newUnit)
{
    if (unit == newUnit || ! isUnitAvailable (newUnit))
        return;
    unit = newUnit;
    repaint ();
}

juce::Array<TimelineComponent::Unit> TimelineComponent::getAllUnits ()
{
    return { Unit::timeMinutesSeconds, Unit::beatsAndBars, Unit::samples };
}

void TimelineComponent::setAvailableUnits (const juce::Array<Unit>& units)
{
    availableUnits = units.isEmpty () ? getAllUnits () : units;

    // Keep what's on display legal. Everything the host shows in these units
    // (cursor readouts, marker tags) follows the timeline, so a forced fallback
    // has to be announced - unlike setUnit(), where the host already knows.
    if (! availableUnits.contains (unit))
    {
        unit = availableUnits.getFirst ();
        repaint ();
        if (onUnitChanged)
            onUnitChanged (unit);
    }
}

juce::String TimelineComponent::getUnitName (Unit u)
{
    switch (u)
    {
        case Unit::timeMinutesSeconds: return "Minutes:Seconds";
        case Unit::beatsAndBars:       return "Beats:Bars";
        case Unit::samples:            return "Samples";
    }
    return {};
}

void TimelineComponent::setColourScheme (const ColourScheme& scheme)
{
    colours = scheme;
    repaint ();
}

//==============================================================================
double TimelineComponent::xToSample (float x) const noexcept
{
    return viewStartSample + (double) x * samplesPerPixel;
}

float TimelineComponent::sampleToX (double sample) const noexcept
{
    return (float) ((sample - viewStartSample) / samplesPerPixel);
}

juce::String TimelineComponent::formatSamplePosition (double sample) const
{
    switch (unit)
    {
        case Unit::samples:
            return juce::String ((juce::int64) std::llround (sample));

        case Unit::timeMinutesSeconds:
        {
            double s = sample / sampleRate;
            const bool neg = s < 0;
            s = std::abs (s);
            const int whole = (int) std::floor (s);
            const int hours = whole / 3600;
            const int mins  = (whole % 3600) / 60;
            const double secs = s - hours * 3600 - mins * 60;
            juce::String secStr (secs, 3);
            if (secs < 10.0) secStr = "0" + secStr;
            juce::String out;
            if (hours > 0) out << hours << ":" << pad2 (mins) << ":" << secStr;
            else           out << mins << ":" << secStr;
            return (neg ? "-" : "") + out;
        }

        case Unit::beatsAndBars:
        {
            const double samplesPerBeat = sampleRate * 60.0 / bpm;
            const double beat = sample / samplesPerBeat; // 0-based
            const int measure = (int) std::floor (beat / beatsPerBar) + 1;
            const double beatInBar = beat - (double) (measure - 1) * beatsPerBar;
            const int beatWhole = (int) std::floor (beatInBar) + 1;
            const int hundredths = (int) std::round ((beatInBar - std::floor (beatInBar)) * 100.0);
            return juce::String (measure) + "." + juce::String (beatWhole) + "." + pad2 (hundredths);
        }
    }
    return {};
}

//==============================================================================
void TimelineComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu ())
        showUnitMenu (e.getScreenPosition ());
}

void TimelineComponent::showUnitMenu (juce::Point<int> screenPos)
{
    // With one (or no) unit configured there is nothing to pick, so don't put a
    // menu in the user's way at all.
    if (availableUnits.size () <= 1)
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader ("Timeline Units");
    for (int i = 0; i < availableUnits.size (); ++i)
        menu.addItem (i + 1, getUnitName (availableUnits[i]), true, unit == availableUnits[i]);

    // The menu is async: capture the list by value and re-check availability on
    // the way out, in case the host reconfigured us while it was open.
    const auto offered = availableUnits;

    menu.showMenuAsync (juce::PopupMenu::Options ()
                            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safeThis = juce::Component::SafePointer<TimelineComponent> (this), offered] (int result)
                        {
                            auto* self = safeThis.getComponent ();
                            if (self == nullptr || result <= 0 || result > offered.size ())
                                return;

                            const auto chosen = offered[result - 1];
                            if (chosen == self->unit || ! self->isUnitAvailable (chosen))
                                return;

                            self->setUnit (chosen);
                            if (self->onUnitChanged)
                                self->onUnitChanged (chosen);
                        });
}

//==============================================================================
void TimelineComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours.background);

    const auto h = (float) getHeight ();
    g.setColour (colours.minorTick);
    g.drawHorizontalLine (getHeight () - 1, 0.0f, (float) getWidth ());

    if (getWidth () <= 0)
        return;

    std::vector<Tick> ticks;
    buildTicks (ticks);

    // Labels occupy a band at the top; tick marks live below it so a line
    // never runs through its own number.
    const float bottom     = h - 1.0f;
    const float labelBandH = juce::jmin (14.0f, h * 0.55f);
    const float majorTop   = labelBandH;                            // major ticks below the text
    const float minorTop   = labelBandH + (bottom - labelBandH) * 0.45f; // minor ticks shorter

    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    for (const auto& t : ticks)
    {
        const auto x = sampleToX (t.sample);
        if (x < -1.0f || x > (float) getWidth () + 1.0f)
            continue;

        if (t.major)
        {
            g.setColour (colours.majorTick);
            g.drawVerticalLine ((int) std::round (x), majorTop, bottom);

            g.setColour (colours.text);
            const int labelW = 90;
            const juce::Rectangle<int> labelBounds ((int) std::round (x) - labelW / 2, 0,
                                                    labelW, (int) labelBandH);
            g.drawText (t.label, labelBounds, juce::Justification::centred, false);
        }
        else
        {
            g.setColour (colours.minorTick);
            g.drawVerticalLine ((int) std::round (x), minorTop, bottom);
        }
    }
}

//==============================================================================
void TimelineComponent::buildTicks (std::vector<Tick>& ticks) const
{
    ticks.clear ();
    const auto first = viewStartSample;
    const auto last  = viewStartSample + (double) getWidth () * samplesPerPixel;
    if (last <= first)
        return;

    switch (unit)
    {
        case Unit::timeMinutesSeconds: buildTimeTicks   (ticks, first, last); break;
        case Unit::beatsAndBars:       buildBeatTicks   (ticks, first, last); break;
        case Unit::samples:            buildSampleTicks (ticks, first, last); break;
    }
}

//==============================================================================
void TimelineComponent::buildTimeTicks (std::vector<Tick>& ticks, double first, double last) const
{
    const double targetSeconds = kTargetMajorPixels * samplesPerPixel / sampleRate;
    static const std::vector<double> ladder {
        0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05,
        0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0,
        60.0, 120.0, 300.0, 600.0, 900.0, 1800.0, 3600.0
    };
    const double stepSec     = pickFromLadder (targetSeconds, ladder);
    const double stepSamples = stepSec * sampleRate;
    const double minorSamples = stepSamples / 5.0;
    const bool   drawMinors  = (minorSamples / samplesPerPixel) >= kMinMinorPixels;

    const int decimals = stepSec < 1.0
                       ? juce::jlimit (1, 4, (int) std::ceil (-std::log10 (stepSec)))
                       : 0;

    auto format = [this, decimals] (double sample)
    {
        double s = sample / sampleRate;
        const bool neg = s < 0;
        s = std::abs (s);
        const int whole = (int) std::floor (s);
        const int hours = whole / 3600;
        const int mins  = (whole % 3600) / 60;
        const double secs = s - hours * 3600 - mins * 60;

        juce::String out;
        if (hours > 0)      out << hours << ":" << pad2 (mins) << ":";
        else if (mins > 0)  out << mins << ":";

        if (decimals > 0)
        {
            juce::String secStr (secs, decimals);
            if (out.isNotEmpty () && secs < 10.0)
                secStr = "0" + secStr;
            out << secStr;
        }
        else
        {
            if (out.isNotEmpty ()) out << pad2 ((int) std::round (secs));
            else                   out << (int) std::round (secs);
        }
        return (neg ? "-" : "") + out;
    };

    const double startK = std::floor (first / minorSamples) - 1.0;
    const double endK   = std::ceil  (last  / minorSamples) + 1.0;
    for (double k = startK; k <= endK; k += 1.0)
    {
        const double sample = k * minorSamples;
        // A tick is major when it lands on a multiple of the major step.
        const double majorK = sample / stepSamples;
        const bool   isMajor = std::abs (majorK - std::round (majorK)) < 1.0e-6;
        if (! isMajor && ! drawMinors)
            continue;
        ticks.push_back ({ sample, isMajor, isMajor ? format (sample) : juce::String () });
    }
}

//==============================================================================
void TimelineComponent::buildBeatTicks (std::vector<Tick>& ticks, double first, double last) const
{
    const double samplesPerBeat = sampleRate * 60.0 / bpm;
    const double pixelsPerBeat   = samplesPerBeat / samplesPerPixel;

    // Major step in whole beats so labels stay clean (bar.beat or bar).
    static const std::vector<double> beatLadder {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024
    };
    double majorBeats = beatLadder.back ();
    for (auto cand : beatLadder)
        if (cand * pixelsPerBeat >= kTargetMajorPixels) { majorBeats = cand; break; }

    // Minor step: a fraction of the major that is still readable.
    double minorBeats = 0.0;
    for (double frac : { 0.25, 0.5 })
    {
        const double cand = majorBeats * frac;
        if (cand < majorBeats && cand * pixelsPerBeat >= kMinMinorPixels)
        {
            minorBeats = cand;
            break;
        }
    }

    const double majorSamples = majorBeats * samplesPerBeat;
    const double gridBeats     = (minorBeats > 0.0 ? minorBeats : majorBeats);
    const double gridSamples   = gridBeats * samplesPerBeat;

    const bool labelMeasures = (majorBeats >= beatsPerBar)
                             && (std::fmod (majorBeats, (double) beatsPerBar) < 1.0e-6);

    auto format = [this, labelMeasures] (double sample)
    {
        const double beat = sample / (sampleRate * 60.0 / bpm); // 0-based
        const int measure = (int) std::floor (beat / beatsPerBar) + 1;
        if (labelMeasures)
            return juce::String (measure);
        const double beatInBar = beat - (double) (measure - 1) * beatsPerBar;
        const int b = (int) std::round (beatInBar) + 1;
        return juce::String (measure) + "." + juce::String (b);
    };

    const double startK = std::floor (first / gridSamples) - 1.0;
    const double endK   = std::ceil  (last  / gridSamples) + 1.0;
    for (double k = startK; k <= endK; k += 1.0)
    {
        const double sample = k * gridSamples;
        const double majorK = sample / majorSamples;
        const bool   isMajor = std::abs (majorK - std::round (majorK)) < 1.0e-6;
        ticks.push_back ({ sample, isMajor, isMajor ? format (sample) : juce::String () });
    }
}

//==============================================================================
void TimelineComponent::buildSampleTicks (std::vector<Tick>& ticks, double first, double last) const
{
    const double targetSamples = kTargetMajorPixels * samplesPerPixel;
    double stepSamples = niceNumber (targetSamples, true);
    stepSamples = juce::jmax (1.0, std::round (stepSamples));

    const double minorSamples = juce::jmax (1.0, stepSamples / 5.0);
    const bool   drawMinors   = (minorSamples / samplesPerPixel) >= kMinMinorPixels
                             && minorSamples < stepSamples;

    auto format = [] (double sample)
    {
        return juce::String ((juce::int64) std::llround (sample));
    };

    const double grid = drawMinors ? minorSamples : stepSamples;
    const double startK = std::floor (first / grid) - 1.0;
    const double endK   = std::ceil  (last  / grid) + 1.0;
    for (double k = startK; k <= endK; k += 1.0)
    {
        const double sample = k * grid;
        if (sample < 0.0)
            continue;
        const double majorK = sample / stepSamples;
        const bool   isMajor = std::abs (majorK - std::round (majorK)) < 1.0e-6;
        if (! isMajor && ! drawMinors)
            continue;
        ticks.push_back ({ sample, isMajor, isMajor ? format (sample) : juce::String () });
    }
}
