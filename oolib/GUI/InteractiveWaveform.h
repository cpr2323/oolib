#pragma once

#include <JuceHeader.h>
#include "oolib/GUI/WaveformView.h"

//==============================================================================
/**
    InteractiveWaveform - wheel / drag navigation layered on top of WaveformView.

    WaveformView is deliberately free of mouse code: it owns the sample <-> pixel
    transform and exposes it publicly, leaving the interaction layer to be chosen
    per app. This is the standard one:

      * wheel                -> zoom time, around the pointer
      * ctrl + wheel         -> zoom amplitude
      * horizontal wheel     -> scroll
      * left drag            -> scroll
      * right drag           -> zoom time (horizontally) and amplitude (vertically),
                                around the point where the drag started

    It only ever calls WaveformView's public view/transform API, so it adds no
    coupling of its own. Apps that want different gestures can subclass
    WaveformView directly instead and use this as the reference.

    The two rate constants are settable so the gesture feel can be tuned without
    subclassing (see setWheelZoomStep / setDragZoomRate).
*/
class InteractiveWaveform : public WaveformView
{
public:
    InteractiveWaveform () = default;
    ~InteractiveWaveform () override = default;

    //==============================================================================
    // Called after any gesture that changed the view, so hosts can re-publish it
    // to a ruler / overlay / status bar.
    std::function<void ()>       onViewChanged;
    std::function<void (double)> onCursorSample; // sample under the cursor
    std::function<void ()>       onCursorExit;

    //==============================================================================
    // Gesture tuning.

    // Zoom multiplier applied per wheel notch. Must be > 0 and != 1; values below
    // 1 mean a notch of wheel-up zooms in by that factor. Default 0.85.
    void setWheelZoomStep (double newStep) noexcept
    {
        jassert (newStep > 0.0 && newStep != 1.0);
        wheelZoomStep = newStep;
    }

    double getWheelZoomStep () const noexcept { return wheelZoomStep; }

    // Zoom rate per pixel of right-button drag, applied exponentially. Must be
    // > 0; larger values zoom faster. Default 0.01.
    void setDragZoomRate (double newRate) noexcept
    {
        jassert (newRate > 0.0);
        dragZoomRate = newRate;
    }

    double getDragZoomRate () const noexcept { return dragZoomRate; }

    //==============================================================================
    static constexpr double defaultWheelZoomStep { 0.85 };  // per wheel notch
    static constexpr double defaultDragZoomRate  { 0.01 };  // per pixel of drag

    //==============================================================================
    void mouseMove (const juce::MouseEvent& e) override
    {
        if (getNumSamples () > 0 && onCursorSample)
            onCursorSample (xToSample (e.position.x));
    }

    void mouseEnter (const juce::MouseEvent& e) override
    {
        mouseMove (e);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (onCursorExit)
            onCursorExit ();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (getNumSamples () <= 0)
            return;

        if (e.mods.isCtrlDown () && wheel.deltaY != 0.0f)
        {
            // Ctrl + wheel: vertical (amplitude) zoom. Wheel up zooms in.
            const auto factor = wheel.deltaY > 0.0f ? 1.0 / wheelZoomStep : wheelZoomStep;
            setVerticalZoom (getVerticalZoom () * (float) factor);
        }
        else if (wheel.deltaY != 0.0f)
        {
            // Zoom toward the cursor. deltaY > 0 (wheel up) zooms in.
            const auto factor = wheel.deltaY > 0.0f ? wheelZoomStep : 1.0 / wheelZoomStep;
            zoomByAroundX (factor, e.position.x);
        }
        else if (wheel.deltaX != 0.0f)
        {
            scrollBySamples (-wheel.deltaX * getSamplesPerPixel () * 40.0);
        }
        if (onViewChanged) onViewChanged ();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        lastDragX   = e.position.x;
        lastDragY   = e.position.y;
        dragAnchorX = e.position.x;
        dragZoom    = e.mods.isRightButtonDown ();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto dx = e.position.x - lastDragX;
        const auto dy = e.position.y - lastDragY;
        lastDragX = e.position.x;
        lastDragY = e.position.y;

        if (dragZoom)
        {
            // Right-button drag: horizontal movement zooms time (around the
            // initial click), vertical movement zooms amplitude. Move right to
            // zoom in horizontally, up to zoom in vertically.
            if (dx != 0.0f)
                zoomByAroundX (std::exp (-dx * dragZoomRate), dragAnchorX);
            if (dy != 0.0f)
                setVerticalZoom (getVerticalZoom () * (float) std::exp (-dy * dragZoomRate));
        }
        else
        {
            scrollBySamples (-dx * getSamplesPerPixel ());
        }

        if (onViewChanged) onViewChanged ();
        if (getNumSamples () > 0 && onCursorSample)
            onCursorSample (xToSample (e.position.x));
    }

private:
    double wheelZoomStep { defaultWheelZoomStep };
    double dragZoomRate  { defaultDragZoomRate };

    float lastDragX   { 0.0f };
    float lastDragY   { 0.0f };
    float dragAnchorX { 0.0f };
    bool  dragZoom    { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InteractiveWaveform)
};
