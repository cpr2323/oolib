#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    WaveformView - a reusable JUCE audio waveform display.

    Design goals (this first pass is DRAWING ONLY - no mouse handling / markers):

      * Visual consistency across all zoom levels. The waveform keeps a
        'similar' shape whether zoomed all the way out or well in, because the
        zoomed-out view is drawn from the true min/max (peak) of every sample
        under each pixel column - NOT by point-subsampling one sample per pixel
        (which is what makes naive displays alias and 'jump around').

      * Audacity-style behaviour:
          - > 1 sample per pixel  -> min/max peak envelope + lighter RMS body.
          - <= 1 sample per pixel -> real per-sample polyline.
          - a few pixels per sample -> individual sample dots on the line.

      * A single authoritative coordinate transform (sample <-> x) in double
        precision sample space, shared by drawing and (later) mouse mapping, so
        the two can never disagree.

      * Fast at any zoom on large files, via a precomputed min/max/RMS summary
        pyramid so a repaint is ~O(width) regardless of file length.

    The component does NOT own the audio - you pass a non-owning pointer that
    must outlive the view (or call setAudioBuffer(nullptr) before it dies).

    Everything else (mouse zoom/scroll, marker editing, smooth animation) is
    intended to be layered on top later using the public view/transform API.
*/
class WaveformView : public juce::Component
{
public:
    WaveformView ();
    ~WaveformView () override = default;

    //==============================================================================
    // Data. Non-owning: the buffer must outlive the view. Passing nullptr clears.
    void setAudioBuffer (const juce::AudioBuffer<float>* newBuffer);

    // Which channel to display, or -1 to show a downmix (average) of all channels.
    void setDisplayChannel (int channel);

    juce::int64 getNumSamples () const noexcept { return numSamples; }

    //==============================================================================
    // View / zoom / scroll. All positions are in SAMPLE units, doubles, so that
    // sub-sample scroll positions (needed for smooth scrolling later) are exact.

    // Show 'numSamplesVisible' samples starting at 'startSample' (left edge).
    void setVisibleRange (double startSample, double numSamplesVisible);

    // Fit the whole file across the current width.
    void zoomToFit ();

    // Change zoom while keeping 'centreSample' pinned to its current pixel.
    void zoomAroundSample (double centreSample, double newSamplesPerPixel);

    // Multiply the current zoom (factor < 1 zooms in) around a focus sample / x.
    void zoomBy (double factor, double focusSample);
    void zoomByAroundX (double factor, float focusX);

    void scrollBySamples (double deltaSamples);

    // Scale the vertical (amplitude) zoom so the loudest sample in the currently
    // visible range just fills the height.
    void zoomVerticalToFit ();

    double getSamplesPerPixel   () const noexcept { return samplesPerPixel; }
    double getVisibleStartSample() const noexcept { return viewStartSample; }

    // Smallest allowed samples-per-pixel (i.e. the deepest zoom-in). Default 1/64
    // meaning up to 64 pixels per sample.
    void setMinSamplesPerPixel (double newMin);

    //==============================================================================
    // Authoritative coordinate transforms. Use these for overlays and (later)
    // mouse mapping so everything stays in lock-step with what is drawn.
    double xToSample (float x)      const noexcept;
    float  sampleToX (double sample) const noexcept;
    float  amplitudeToY (float amplitude) const noexcept;

    //==============================================================================
    // Appearance.
    struct ColourScheme
    {
        juce::Colour background   { juce::Colour (0xff2b2b2b) };
        juce::Colour centreLine   { juce::Colour (0xff404040) };
        juce::Colour peak         { juce::Colour (0xff4aa3df) }; // min/max envelope
        juce::Colour rms          { juce::Colour (0xffa6d8ff) }; // brighter RMS body
        juce::Colour sampleLine   { juce::Colour (0xff8fd0ff) }; // per-sample polyline
        juce::Colour sampleDot    { juce::Colour (0xffffffff) };
    };

    // Individually addressable colour slots, in a stable order so UIs can
    // iterate over them (see getColourName).
    enum class ColourId
    {
        background = 0,
        centreLine,
        peak,
        rms,
        sampleLine,
        sampleDot,
        numColours
    };

    // Human-readable label for a slot, e.g. for a colour-picker list.
    static const char* getColourName (ColourId id) noexcept;

    void setColourScheme (const ColourScheme& newScheme);
    const ColourScheme& getColourScheme () const noexcept { return colours; }

    // Get / set a single colour slot. setColour repaints.
    juce::Colour getColour (ColourId id) const noexcept;
    void         setColour (ColourId id, juce::Colour newColour);

    // Derive the four waveform stroke colours (peak, rms, sample line, sample
    // dot) from a single base colour, reproducing the hue/saturation/brightness
    // relationship of the default scheme: peak == base, and the rms / line / dot
    // become progressively less saturated and brighter (dot -> white). The
    // background and centre line (the backdrop, not the waveform) are untouched.
    // The pure transform is exposed as a static helper for reuse/testing.
    static void  deriveWaveformColours (juce::Colour base, ColourScheme& schemeToFill);
    void         setWaveformColoursFromBase (juce::Colour base);

    // Vertical amplitude gain (1.0 = -1..1 fills the height, minus a small margin).
    void  setVerticalZoom (float gain);
    float getVerticalZoom () const noexcept { return verticalGain; }

    // Show / hide the brighter RMS body drawn inside the peak envelope (only
    // visible in the zoomed-out peak view).
    void setRmsVisible (bool shouldBeVisible);
    bool isRmsVisible () const noexcept { return rmsVisible; }

    //==============================================================================
    // How the audio is rendered. The two representations are:
    //
    //   * peakEnvelope - one bar per pixel column spanning the true min..max of
    //     every sample under it (plus the RMS body). Because the top and bottom
    //     of each bar are independent real extremes it isn't a mirror image, it
    //     just looks like one for audio that is roughly symmetric about zero.
    //   * sampleLine   - the per-sample polyline through the real sample values,
    //     with dots once samples are sparse enough on screen.
    //
    // In 'automatic' the view picks between them by zoom (see
    // setPeakEnvelopeThreshold); the other two pin it to one representation at
    // every zoom level.
    enum class DrawStyle
    {
        automatic = 0,
        peakEnvelope,
        sampleLine,
        numStyles
    };

    static const char* getDrawStyleName (DrawStyle style) noexcept;

    void      setDrawStyle (DrawStyle newStyle);
    DrawStyle getDrawStyle () const noexcept { return drawStyle; }

    // In 'automatic', the zoom at which the view flips to the peak envelope:
    // the envelope is used once samplesPerPixel exceeds this. The default of 1.0
    // switches exactly where one pixel starts covering more than one sample,
    // which is where the two representations coincide - raise it to keep the
    // real sample line for a few samples per pixel, lower it to get the envelope
    // even when zoomed past one sample per pixel. Ignored by the pinned styles.
    void   setPeakEnvelopeThreshold (double samplesPerPixel);
    double getPeakEnvelopeThreshold () const noexcept { return peakEnvelopeThreshold; }

    // What the CURRENT view actually draws, i.e. the style and threshold
    // resolved against the current zoom. Use this rather than re-deriving the
    // rule (a status readout, say), so host and view can never disagree.
    bool isDrawingPeakEnvelope () const noexcept;

    // Optional hook to draw over the finished waveform (markers, selection, etc.).
    // Use sampleToX()/amplitudeToY() inside to position things by sample value.
    std::function<void (juce::Graphics&, WaveformView&)> onPaintOverlay;

private:
    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized () override;

    // --- rendering ---
    void paintPeaks   (juce::Graphics&); // > 1 sample per pixel
    void paintSamples (juce::Graphics&); // <= 1 sample per pixel

    // --- view maths ---
    void clampView ();
    double maxSamplesPerPixel () const noexcept; // whole file across the width

    // --- data access (selected channel or downmix) ---
    float getDisplaySample (juce::int64 index) const noexcept;

    // --- summary pyramid (min/max/RMS) for fast, consistent zoomed-out drawing ---
    struct Bin
    {
        float  mn    { 0.0f };
        float  mx    { 0.0f };
        double sumSq { 0.0 };
        int    count { 0 };
    };
    struct Level
    {
        juce::int64      binSize { 0 };
        std::vector<Bin> bins;
    };

    struct Summary { float mn, mx, rms; };

    void buildPeakCache ();
    Summary querySummary (juce::int64 s0, juce::int64 s1) const; // over [s0, s1)
    void accumulateRaw  (juce::int64 a, juce::int64 b, Bin& acc) const;

    std::vector<Level> levels;

    //==============================================================================
    const juce::AudioBuffer<float>* audioBuffer { nullptr };
    juce::int64 numSamples  { 0 };
    int         displayChannel { -1 }; // -1 == downmix

    // View state, sample space, double precision.
    double viewStartSample  { 0.0 };
    double samplesPerPixel  { 1.0 };
    bool   viewInitialised  { false };

    double minSPP { 1.0 / 64.0 };

    float verticalGain { 1.0f };
    bool  rmsVisible   { true };

    DrawStyle drawStyle             { DrawStyle::automatic };
    double    peakEnvelopeThreshold { 1.0 };

    ColourScheme colours;

    // Scratch buffer reused across repaints to avoid per-frame allocation.
    std::vector<Summary> columnCache;

    static constexpr juce::int64 kBaseBinSize   { 256 };
    static constexpr juce::int64 kRawThreshold  { 2048 }; // read raw for spans <= this
    static constexpr juce::int64 kMaxBinsPerCol { 512 };  // bound work per column
    static constexpr float       kDotPixelsPerSample { 8.0f }; // dots once this sparse
    static constexpr float       kVerticalMargin     { 2.0f };
    static constexpr int         kMaxLinePointsPerPixel { 4 }; // sample-line decimation budget

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};
