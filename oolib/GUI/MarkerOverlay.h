#pragma once

#include <JuceHeader.h>
#include "oolib/GUI/WaveformView.h"

//==============================================================================
/**
    MarkerOverlay - a transparent overlay for editing sample markers on top of a
    WaveformView.

    A marker is one position in the audio, in samples, with a name and a look.
    That is the whole model: the overlay holds a flat list of them and never
    assumes that any two are related. Apps that DO have relationships between
    their markers - a sample start that must not pass its end, a loop point that
    lives between the two, a length that travels with its start - express them
    through constrainPosition (where a marker may go) and onMarkerMoved (what
    else has to move once it got there), so those rules stay with the app that
    owns them instead of being baked in here:

        * A8Manager   : sample start/end, plus loop start/length.
        * SquidSalmple: a cue set's start, loop and end.

    The overlay's own only rule is that a marker stays on a whole sample inside
    the audio, and it is applied last - so a constrainPosition that hands back
    something out of range still cannot put a marker off the end of the file. It
    maps sample<->pixel through the WaveformView it sits over, so markers stay
    locked to the waveform at every zoom/scroll.

    Every marker carries its own Style, so one app can draw its markers as flat
    white lines and another as coloured flags without either having to subclass.

    Mouse handling only claims the small handle rectangles (see hitTest); clicks
    anywhere else fall through to the waveform beneath for panning/zooming.
*/
class MarkerOverlay : public juce::Component
{
public:
    // Which edge the handle sits against.
    enum class HandlePlacement { top, bottom };

    // How the handle is drawn. 'none' leaves the marker line on its own, and
    // makes that marker read-only - there is nothing left to grab.
    enum class HandleShape { rectangle, roundedRectangle, triangle, none };

    // Which side of the marker line the handle hangs off. Markers that bound a
    // span usually point inwards (start rightOfLine, end leftOfLine) so both
    // stay legible when the span is narrow.
    enum class HandleAlignment { centred, leftOfLine, rightOfLine };

    // When the marker's name and position are shown beside it.
    enum class LabelVisibility { never, whileDragging, always };

    struct Style
    {
        juce::Colour    colour        { juce::Colours::yellow };
        bool            dashed        { false };
        float           lineThickness { 1.5f };
        HandlePlacement placement     { HandlePlacement::top };
        HandleShape     shape         { HandleShape::roundedRectangle };
        HandleAlignment alignment     { HandleAlignment::centred };
        float           handleWidth   { 10.0f };
        float           handleHeight  { 14.0f };
        LabelVisibility label         { LabelVisibility::whileDragging };
    };

    struct Marker
    {
        juce::String name;
        double       position { 0.0 };  // sample offset
        Style        style;
    };

    MarkerOverlay ()
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }

    // The waveform we overlay - used for the sample<->pixel transform and the
    // audio length that bounds every marker.
    void setWaveformView (WaveformView* view) { waveform = view; }

    //==============================================================================
    // Marker list. Indices are stable for the life of the list, and markers are
    // drawn in the order they were added, so add the ones that should sit on top
    // last.
    int addMarker (const Marker& marker)
    {
        markers.add (marker);
        clampMarker (markers.getReference (markers.size () - 1));
        repaint ();
        return markers.size () - 1;
    }

    void clearMarkers ()
    {
        markers.clear ();
        drag = {};
        repaint ();
    }

    int getNumMarkers () const noexcept { return markers.size (); }

    // -1 when nothing has that name. Names are the overlay's only concession to
    // meaning: it never interprets them, they are there so a host can find its
    // markers without tracking indices, and so labels can say what they are.
    int indexOfMarker (const juce::String& name) const
    {
        for (auto markerIndex { 0 }; markerIndex < markers.size (); ++markerIndex)
            if (markers.getReference (markerIndex).name == name)
                return markerIndex;
        return -1;
    }

    const juce::String& getName (int markerIndex) const { return markers.getReference (markerIndex).name; }

    double getPosition (int markerIndex) const { return markers.getReference (markerIndex).position; }

    // Moves a marker without consulting constrainPosition - the caller is the
    // one deciding where it goes - and without reporting it back through
    // onMarkerMoved. Only the audio bounds still apply.
    void setPosition (int markerIndex, double sample)
    {
        auto& marker { markers.getReference (markerIndex) };
        marker.position = sample;
        clampMarker (marker);
        repaint ();
    }

    const Style& getStyle (int markerIndex) const { return markers.getReference (markerIndex).style; }

    void setStyle (int markerIndex, const Style& newStyle)
    {
        markers.getReference (markerIndex).style = newStyle;
        repaint ();
    }

    // The marker currently being dragged, or -1.
    int getDraggedMarker () const noexcept { return drag.markerIndex; }

    //==============================================================================
    // Where the marker being dragged is allowed to go: given its index and the
    // position the drag is asking for, return the position it may actually take.
    // This is where an app puts the relationships between its own markers. When
    // unset, the audio bounds are the only limit.
    std::function<double (int markerIndex, double proposedPosition)> constrainPosition;

    // A drag moved a marker; its new position is already in place. Move any
    // markers that have to follow it from here, with setPosition.
    std::function<void (int markerIndex)> onMarkerMoved;

    // How a sample offset is rendered in labels. Set this to the timeline's
    // formatter so markers follow the units on display. When unset, falls back
    // to a raw sample count.
    std::function<juce::String (double)> formatPosition;

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        if (! haveAudio ())
            return;

        for (auto markerIndex { 0 }; markerIndex < markers.size (); ++markerIndex)
            paintMarker (g, markers.getReference (markerIndex), markerIndex == drag.markerIndex);
    }

    // Only the handle rectangles belong to us; everything else falls through to
    // the waveform below so panning / zooming still works over the markers.
    bool hitTest (int x, int y) override
    {
        return findHandleAt ((float) x, (float) y) >= 0;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag.markerIndex = findHandleAt (e.position.x, e.position.y);
        if (drag.markerIndex < 0)
            return;

        // Keep the point that was grabbed under the pointer for the whole drag.
        drag.grabOffset = getPosition (drag.markerIndex) - xToSample (e.position.x);
        repaint ();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag.markerIndex < 0)
            return;

        moveMarker (drag.markerIndex, xToSample (e.position.x) + drag.grabOffset);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        drag = {};
        repaint (); // clear the drag labels
    }

private:
    //==============================================================================
    struct DragState
    {
        int    markerIndex { -1 };
        double grabOffset  { 0.0 };
    };

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
    // The overlay's own rule, applied after the host's constraint so that a
    // constrainPosition which returns something out of range still cannot put a
    // marker off the audio.
    void clampMarker (Marker& marker) const
    {
        marker.position = juce::jlimit (0.0, totalSamples (), std::round (marker.position));
    }

    void moveMarker (int markerIndex, double proposedPosition)
    {
        auto& marker { markers.getReference (markerIndex) };
        proposedPosition = std::round (proposedPosition);
        marker.position = constrainPosition ? constrainPosition (markerIndex, proposedPosition)
                                            : proposedPosition;
        clampMarker (marker);

        if (onMarkerMoved)
            onMarkerMoved (markerIndex);
        repaint ();
    }

    //==============================================================================
    juce::Rectangle<float> handleRect (const Marker& marker) const
    {
        const auto& style { marker.style };
        const auto x { sampleToX (marker.position) };
        const auto handleX = [&style, x] ()
        {
            switch (style.alignment)
            {
                case HandleAlignment::leftOfLine:  return x - style.handleWidth;
                case HandleAlignment::rightOfLine: return x;
                case HandleAlignment::centred:
                default:                           return x - (style.handleWidth * 0.5f);
            }
        } ();
        const auto handleY { style.placement == HandlePlacement::top
                               ? 0.0f
                               : (float) getHeight () - style.handleHeight };
        return { handleX, handleY, style.handleWidth, style.handleHeight };
    }

    int findHandleAt (float px, float py) const
    {
        if (! haveAudio ())
            return -1;

        auto best { -1 };
        auto bestDistance { std::numeric_limits<float>::max () };

        // Later markers are drawn over earlier ones, so when two handles land in
        // the same place the one the user can actually see wins the click.
        for (auto markerIndex { 0 }; markerIndex < markers.size (); ++markerIndex)
        {
            const auto& marker { markers.getReference (markerIndex) };
            if (marker.style.shape == HandleShape::none)
                continue;

            const auto rect { handleRect (marker) };
            if (rect.expanded (kHitPad, kHitPad).contains (px, py))
            {
                if (const auto distance { std::abs (px - rect.getCentreX ()) }; distance <= bestDistance)
                {
                    bestDistance = distance;
                    best = markerIndex;
                }
            }
        }
        return best;
    }

    //==============================================================================
    void paintMarker (juce::Graphics& g, const Marker& marker, bool isBeingDragged) const
    {
        const auto& style { marker.style };
        const auto x { sampleToX (marker.position) };
        const auto height { (float) getHeight () };

        g.setColour (style.colour);
        if (style.dashed)
        {
            const float dashes[] { 5.0f, 4.0f };
            g.drawDashedLine (juce::Line<float> (x, 0.0f, x, height), dashes, 2, style.lineThickness);
        }
        else
        {
            g.drawLine (x, 0.0f, x, height, style.lineThickness);
        }

        paintHandle (g, marker);

        if (style.label == LabelVisibility::always
            || (isBeingDragged && style.label == LabelVisibility::whileDragging))
            paintLabel (g, marker);
    }

    void paintHandle (juce::Graphics& g, const Marker& marker) const
    {
        const auto& style { marker.style };
        if (style.shape == HandleShape::none)
            return;

        const auto rect { handleRect (marker) };
        const auto outline { juce::Colours::black.withAlpha (0.45f) };

        switch (style.shape)
        {
            case HandleShape::rectangle:
            {
                g.setColour (style.colour);
                g.fillRect (rect);
                g.setColour (outline);
                g.drawRect (rect, 1.0f);
            }
            break;

            case HandleShape::roundedRectangle:
            {
                g.setColour (style.colour);
                g.fillRoundedRectangle (rect, 2.0f);
                g.setColour (outline);
                g.drawRoundedRectangle (rect, 2.0f, 1.0f);
            }
            break;

            case HandleShape::triangle:
            {
                const auto path { handlePath (marker, rect) };
                g.setColour (style.colour);
                g.fillPath (path);
                g.setColour (outline);
                g.strokePath (path, juce::PathStrokeType (1.0f));
            }
            break;

            case HandleShape::none:
            default:
            break;
        }
    }

    // A centred triangle tapers to a point away from its edge; an off-centre one
    // squares off against its marker line and slopes away from it, so which line
    // a handle belongs to stays obvious when two of them are close together.
    static juce::Path handlePath (const Marker& marker, juce::Rectangle<float> rect)
    {
        const auto& style { marker.style };
        const auto atTop { style.placement == HandlePlacement::top };
        const auto baseY { atTop ? rect.getY () : rect.getBottom () };
        const auto tipY  { atTop ? rect.getBottom () : rect.getY () };

        juce::Path path;
        if (style.alignment == HandleAlignment::centred)
        {
            path.addTriangle (rect.getX (), baseY, rect.getRight (), baseY, rect.getCentreX (), tipY);
        }
        else
        {
            const auto lineX { style.alignment == HandleAlignment::leftOfLine ? rect.getRight () : rect.getX () };
            const auto farX  { style.alignment == HandleAlignment::leftOfLine ? rect.getX () : rect.getRight () };
            path.addTriangle (lineX, baseY, farX, baseY, lineX, tipY);
        }
        return path;
    }

    juce::String labelText (const Marker& marker) const
    {
        const auto position { formatPosition ? formatPosition (marker.position)
                                             : juce::String ((juce::int64) marker.position) };
        return marker.name.isEmpty () ? position : marker.name + " " + position;
    }

    void paintLabel (juce::Graphics& g, const Marker& marker) const
    {
        const auto& style { marker.style };
        const auto text { labelText (marker) };

        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        const auto width { juce::GlyphArrangement::getStringWidth (g.getCurrentFont (), text) + 8.0f };
        constexpr auto height { 16.0f };

        // Sits on the same side of the line as the handle, and is kept inside
        // the component so it stays readable at either edge.
        const auto x { sampleToX (marker.position) };
        const auto preferredX { style.alignment == HandleAlignment::leftOfLine ? x - width - 2.0f : x + 2.0f };
        const auto labelX { juce::jlimit (0.0f, juce::jmax (0.0f, (float) getWidth () - width), preferredX) };
        const auto labelY { style.placement == HandlePlacement::top
                              ? style.handleHeight + 3.0f
                              : (float) getHeight () - style.handleHeight - height - 3.0f };

        const juce::Rectangle<float> box { labelX, labelY, width, height };
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (box, 2.0f);
        g.setColour (style.colour);
        g.drawText (text, box, juce::Justification::centred);
    }

    //==============================================================================
    static constexpr float kHitPad { 3.0f };

    WaveformView*       waveform { nullptr };
    juce::Array<Marker> markers;
    DragState           drag;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerOverlay)
};
