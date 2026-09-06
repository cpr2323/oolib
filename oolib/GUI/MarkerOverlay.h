#pragma once

#include <JuceHeader.h>
#include "oolib/GUI/WaveformView.h"

//==============================================================================
/**
    MarkerOverlay - a transparent overlay for editing sample markers on top of a
    WaveformView.

    The overlay is deliberately app-agnostic. Every app models its markers as
    sample offsets, and (across the apps we care about) they always come in
    start/end PAIRS - a "region". What differs is how many regions there are and
    what each app *calls* them:

        * A8Manager   : sample start/end, plus loop start/length (2 regions).
        * SquidSalmple: up to 64 cue sets (64 regions).

    So this component only knows about regions and the universal constraints:

        * A region's start can never move past its end, nor its end before its
          start.
        * Neither marker can leave the audio: both stay within [0, length].
        * A region's end can be treated as a LENGTH instead of a position
          (EndMode::length). In that mode the length is held constant when the
          start moves (so the end moves with it), and - as a consequence - once
          the end reaches the end of the audio the start can no longer move right
          (doing so would push the end out of bounds).

    Interpreting a region as "sample" vs "loop", or its end as "length", is left
    to the host; the overlay just enforces the geometry and reports edits through
    onRegionChanged. It maps sample<->pixel through the WaveformView it sits over,
    so markers stay locked to the waveform at every zoom/scroll.

    Mouse handling only claims the small handle rectangles (see hitTest); clicks
    anywhere else fall through to the waveform beneath for panning/zooming.
*/
class MarkerOverlay : public juce::Component
{
public:
    enum class EndMode        { position, length };
    enum class HandlePlacement { top, bottom };

    struct Region
    {
        juce::String    name;
        double          start   { 0.0 };  // sample offset
        double          end     { 0.0 };  // sample offset of the end marker
        EndMode         endMode { EndMode::position };
        HandlePlacement placement { HandlePlacement::top };
        juce::Colour    colour  { juce::Colours::yellow };
        bool            dashed  { false };
    };

    MarkerOverlay ()
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }

    // The waveform we overlay - used for the sample<->pixel transform and the
    // audio length that bounds every marker.
    void setWaveformView (WaveformView* view) { waveform = view; }

    //==============================================================================
    // Region setup / access.
    int addRegion (const Region& r)
    {
        regions.add (r);
        clampRegion (regions.getReference (regions.size () - 1));
        repaint ();
        return regions.size () - 1;
    }

    void clearRegions () { regions.clear (); repaint (); }
    int  getNumRegions () const noexcept { return regions.size (); }

    double  getStart   (int region) const { return regions[region].start; }
    double  getEnd     (int region) const { return regions[region].end; }
    double  getLength  (int region) const { return regions[region].end - regions[region].start; }
    EndMode getEndMode (int region) const { return regions[region].endMode; }

    // Set both markers of a region at once (clamped to the geometry rules).
    void setRegionPositions (int region, double start, double end)
    {
        auto& r = regions.getReference (region);
        r.start = start;
        r.end   = end;
        clampRegion (r);
        repaint ();
    }

    // Switching modes never moves the markers - it only changes how the end is
    // interpreted and how future start-drags behave. (In length mode the current
    // end-start becomes the held length.)
    void setEndMode (int region, EndMode mode)
    {
        regions.getReference (region).endMode = mode;
        repaint ();
    }

    // Notified (with the region index) whenever a drag edits a region.
    std::function<void (int)> onRegionChanged;

    // How a sample offset is rendered in the UI (drag tags, future readouts).
    // Set this to the timeline's formatter so markers follow the selected units.
    // When unset, falls back to a raw sample count.
    std::function<juce::String (double)> formatPosition;

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        if (! haveAudio ())
            return;

        for (const auto& r : regions)
            paintRegion (g, r);

        if (drag.region >= 0)
            paintDragTags (g, regions[drag.region]);
    }

    // Only the handle rectangles belong to us; everything else falls through to
    // the waveform below so panning / zooming still works over the markers.
    bool hitTest (int x, int y) override
    {
        return findHandleAt ((float) x, (float) y).region >= 0;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = findHandleAt (e.position.x, e.position.y);
        if (drag.region < 0)
            return;

        const auto& r = regions[drag.region];
        const auto handleSample = drag.isEnd ? r.end : r.start;
        grabOffset = handleSample - xToSample (e.position.x); // keep grab point stable
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.region < 0)
            return;

        moveHandle (drag.region, drag.isEnd, xToSample (e.position.x) + grabOffset);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        drag = {};
        repaint (); // clear the drag value tags
    }

private:
    //==============================================================================
    struct HandleRef { int region { -1 }; bool isEnd { false }; };

    bool haveAudio () const noexcept
    {
        return waveform != nullptr && waveform->getNumSamples () > 0 && getHeight () > 0;
    }

    double totalSamples () const noexcept
    {
        return waveform != nullptr ? (double) waveform->getNumSamples () : 0.0;
    }

    float  sampleToX (double sample) const { return waveform->sampleToX (sample); }
    double xToSample (float x)       const { return waveform->xToSample (x); }

    //==============================================================================
    // Geometry rules. All marker positions are snapped to whole samples.
    void clampRegion (Region& r) const
    {
        const auto N = totalSamples ();
        r.start = juce::jlimit (0.0, N, std::round (r.start));
        r.end   = juce::jlimit (r.start, N, std::round (r.end));
    }

    void moveHandle (int region, bool isEnd, double newPos)
    {
        auto& r = regions.getReference (region);
        const auto N = totalSamples ();
        newPos = std::round (newPos);

        if (isEnd)
        {
            // End (in either mode) is bounded by the start and the audio end.
            r.end = juce::jlimit (r.start, N, newPos);
        }
        else if (r.endMode == EndMode::length)
        {
            // Length is held constant, so the end travels with the start. That
            // also caps how far right the start can go: start <= length - end.
            const auto length = r.end - r.start;
            const auto maxStart = juce::jmax (0.0, N - length);
            r.start = juce::jlimit (0.0, maxStart, newPos);
            r.end   = r.start + length;
        }
        else
        {
            // End is a fixed position: the start just can't cross it.
            r.start = juce::jlimit (0.0, r.end, newPos);
        }

        if (onRegionChanged)
            onRegionChanged (region);
        repaint ();
    }

    //==============================================================================
    juce::Rectangle<float> handleRect (const Region& r, bool isEnd) const
    {
        // Handles are centred on their marker line and sit flush with the top
        // edge (sample-style) or bottom edge (loop-style).
        const auto x  = sampleToX (isEnd ? r.end : r.start);
        const auto rx = x - kHandleW * 0.5f;
        const auto ry = r.placement == HandlePlacement::top
                          ? 0.0f
                          : (float) getHeight () - kHandleH;
        return { rx, ry, kHandleW, kHandleH };
    }

    HandleRef findHandleAt (float px, float py) const
    {
        if (! haveAudio ())
            return {};

        HandleRef best;
        float bestDist = std::numeric_limits<float>::max ();

        for (int i = 0; i < regions.size (); ++i)
        {
            for (bool isEnd : { false, true })
            {
                const auto rect = handleRect (regions[i], isEnd);
                if (rect.expanded (kHitPad, kHitPad).contains (px, py))
                {
                    const auto d = std::abs (px - rect.getCentreX ());
                    if (d < bestDist) { bestDist = d; best = { i, isEnd }; }
                }
            }
        }
        return best;
    }

    //==============================================================================
    void paintRegion (juce::Graphics& g, const Region& r) const
    {
        const auto sx = sampleToX (r.start);
        const auto ex = sampleToX (r.end);
        const auto H  = (float) getHeight ();

        g.setColour (r.colour);
        drawVertical (g, sx, H, r.dashed);
        drawVertical (g, ex, H, r.dashed);

        paintHandle (g, r, false);
        paintHandle (g, r, true);
    }

    static void drawVertical (juce::Graphics& g, float x, float height, bool dashed)
    {
        if (dashed)
        {
            const float dashes[] = { 5.0f, 4.0f };
            g.drawDashedLine (juce::Line<float> (x, 0.0f, x, height), dashes, 2, 1.5f);
        }
        else
        {
            g.drawLine (x, 0.0f, x, height, 1.5f);
        }
    }

    void paintHandle (juce::Graphics& g, const Region& r, bool isEnd) const
    {
        const auto rect = handleRect (r, isEnd);
        g.setColour (r.colour);
        g.fillRoundedRectangle (rect, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (rect, 2.0f, 1.0f);
    }

    // While dragging, show the region's two values (the end as a length when in
    // length mode) so the constrained behaviour is easy to see.
    juce::String formatValue (double sample) const
    {
        return formatPosition ? formatPosition (sample)
                              : juce::String ((juce::int64) sample);
    }

    void paintDragTags (juce::Graphics& g, const Region& r) const
    {
        drawTag (g, r, false, "S " + formatValue (r.start));

        // In length mode the end reads as a length; formatting the (end - start)
        // span through the same unit formatter gives that duration in-unit.
        const auto endText = r.endMode == EndMode::length
                               ? "L " + formatValue (r.end - r.start)
                               : "E " + formatValue (r.end);
        drawTag (g, r, true, endText);
    }

    void drawTag (juce::Graphics& g, const Region& r, bool isEnd, const juce::String& text) const
    {
        g.setFont (12.0f);
        const auto w = (float) g.getCurrentFont ().getStringWidth (text) + 8.0f;
        const auto h = 16.0f;
        const auto x = sampleToX (isEnd ? r.end : r.start);
        const auto tx = isEnd ? x - w - 2.0f : x + 2.0f;                  // inside the region
        const auto ty = r.placement == HandlePlacement::top
                          ? kHandleH + 3.0f
                          : (float) getHeight () - kHandleH - h - 3.0f;

        juce::Rectangle<float> box (tx, ty, w, h);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (box, 2.0f);
        g.setColour (r.colour);
        g.drawText (text, box, juce::Justification::centred);
    }

    //==============================================================================
    static constexpr float kHandleW { 10.0f };
    static constexpr float kHandleH { 14.0f };
    static constexpr float kHitPad  { 3.0f };

    WaveformView*        waveform { nullptr };
    juce::Array<Region>  regions;
    HandleRef            drag;
    double               grabOffset { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerOverlay)
};
