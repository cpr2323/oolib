#include "oolib/GUI/WaveformView.h"

//==============================================================================
WaveformView::WaveformView ()
{
    setOpaque (true);
}

//==============================================================================
void WaveformView::setAudioBuffer (const juce::AudioBuffer<float>* newBuffer)
{
    audioBuffer = newBuffer;
    numSamples  = (audioBuffer != nullptr) ? audioBuffer->getNumSamples () : 0;

    buildPeakCache ();

    viewInitialised = false;
    zoomToFit ();     // resets the view to show the whole (new) file
    repaint ();
}

void WaveformView::setDisplayChannel (int channel)
{
    if (displayChannel == channel)
        return;

    displayChannel = channel;
    buildPeakCache (); // summary depends on which channel(s) we mix
    repaint ();
}

//==============================================================================
void WaveformView::setVisibleRange (double startSample, double numSamplesVisible)
{
    if (getWidth () <= 0)
        return;

    numSamplesVisible = juce::jmax (1.0, numSamplesVisible);
    samplesPerPixel   = numSamplesVisible / (double) getWidth ();
    viewStartSample   = startSample;
    viewInitialised   = true;
    clampView ();
    repaint ();
}

void WaveformView::zoomToFit ()
{
    if (numSamples <= 0 || getWidth () <= 0)
    {
        viewStartSample = 0.0;
        samplesPerPixel = 1.0;
        return;
    }

    samplesPerPixel = maxSamplesPerPixel ();
    viewStartSample = 0.0;
    viewInitialised = true;
    clampView ();
    repaint ();
}

void WaveformView::zoomAroundSample (double centreSample, double newSamplesPerPixel)
{
    if (getWidth () <= 0)
        return;

    // Pixel the focus sample currently sits at - keep it there after the zoom.
    const auto focusX = sampleToX (centreSample);

    samplesPerPixel = juce::jlimit (minSPP, maxSamplesPerPixel (), newSamplesPerPixel);
    viewStartSample = centreSample - (double) focusX * samplesPerPixel;

    viewInitialised = true;
    clampView ();
    repaint ();
}

void WaveformView::zoomBy (double factor, double focusSample)
{
    zoomAroundSample (focusSample, samplesPerPixel * factor);
}

void WaveformView::zoomByAroundX (double factor, float focusX)
{
    zoomAroundSample (xToSample (focusX), samplesPerPixel * factor);
}

void WaveformView::scrollBySamples (double deltaSamples)
{
    viewStartSample += deltaSamples;
    clampView ();
    repaint ();
}

void WaveformView::setMinSamplesPerPixel (double newMin)
{
    minSPP = juce::jmax (1.0e-6, newMin);
    clampView ();
    repaint ();
}

//==============================================================================
double WaveformView::xToSample (float x) const noexcept
{
    return viewStartSample + (double) x * samplesPerPixel;
}

float WaveformView::sampleToX (double sample) const noexcept
{
    return (float) ((sample - viewStartSample) / samplesPerPixel);
}

float WaveformView::amplitudeToY (float amplitude) const noexcept
{
    const auto mid      = (float) getHeight () * 0.5f;
    const auto halfSpan = juce::jmax (1.0f, mid - kVerticalMargin);
    return mid - juce::jlimit (-1.0f, 1.0f, amplitude * verticalGain) * halfSpan;
}

//==============================================================================
const char* WaveformView::getColourName (ColourId id) noexcept
{
    switch (id)
    {
        case ColourId::background: return "Background";
        case ColourId::centreLine: return "Centre line";
        case ColourId::peak:       return "Peak envelope";
        case ColourId::rms:        return "RMS body";
        case ColourId::sampleLine: return "Sample line";
        case ColourId::sampleDot:  return "Sample dot";
        case ColourId::numColours:
        default:                   return "";
    }
}

void WaveformView::setColourScheme (const ColourScheme& newScheme)
{
    colours = newScheme;
    repaint ();
}

juce::Colour WaveformView::getColour (ColourId id) const noexcept
{
    switch (id)
    {
        case ColourId::background: return colours.background;
        case ColourId::centreLine: return colours.centreLine;
        case ColourId::peak:       return colours.peak;
        case ColourId::rms:        return colours.rms;
        case ColourId::sampleLine: return colours.sampleLine;
        case ColourId::sampleDot:  return colours.sampleDot;
        case ColourId::numColours:
        default:                   return {};
    }
}

void WaveformView::setColour (ColourId id, juce::Colour newColour)
{
    switch (id)
    {
        case ColourId::background: colours.background = newColour; break;
        case ColourId::centreLine: colours.centreLine = newColour; break;
        case ColourId::peak:       colours.peak       = newColour; break;
        case ColourId::rms:        colours.rms        = newColour; break;
        case ColourId::sampleLine: colours.sampleLine = newColour; break;
        case ColourId::sampleDot:  colours.sampleDot  = newColour; break;
        case ColourId::numColours:
        default:                   return;
    }
    repaint ();
}

void WaveformView::deriveWaveformColours (juce::Colour base, ColourScheme& scheme)
{
    // The default waveform colours all share one hue (~205 deg) and differ only
    // in saturation/brightness. Measured against the default peak, the family
    // fans from the base toward white as saturation drops and brightness rises:
    //   peak        : saturation x1.00, brightness = base            (== base)
    //   sample line : saturation x0.66, brightness = 1.0   (brighter, less saturated)
    //   rms         : saturation x0.52, brightness = 1.0
    //   sample dot  : saturation x0.15, brightness = 1.0   (near-white, lightly tinted)
    // The default dot was pure white (x0.00); we keep a small saturation instead
    // so the dot still visibly tracks the base colour when derived from it.
    const auto h = base.getHue ();
    const auto s = base.getSaturation ();
    const auto v = base.getBrightness ();
    const auto a = base.getFloatAlpha ();

    auto shade = [h, a] (float sat, float bright)
    {
        return juce::Colour (h, juce::jlimit (0.0f, 1.0f, sat),
                                juce::jlimit (0.0f, 1.0f, bright), a);
    };

    scheme.peak       = shade (s * 1.00f, v);   // == base
    scheme.sampleLine = shade (s * 0.66f, 1.0f);
    scheme.rms        = shade (s * 0.52f, 1.0f);
    scheme.sampleDot  = shade (s * 0.15f, 1.0f);
}

void WaveformView::setWaveformColoursFromBase (juce::Colour base)
{
    deriveWaveformColours (base, colours);
    repaint ();
}

void WaveformView::setVerticalZoom (float gain)
{
    verticalGain = juce::jmax (0.01f, gain);
    repaint ();
}

void WaveformView::setRmsVisible (bool shouldBeVisible)
{
    if (rmsVisible == shouldBeVisible)
        return;
    rmsVisible = shouldBeVisible;
    repaint ();
}

//==============================================================================
const char* WaveformView::getDrawStyleName (DrawStyle style) noexcept
{
    switch (style)
    {
        case DrawStyle::automatic:    return "Automatic (by zoom)";
        case DrawStyle::peakEnvelope: return "Peak envelope";
        case DrawStyle::sampleLine:   return "Sample line";
        case DrawStyle::numStyles:
        default:                      return "";
    }
}

void WaveformView::setDrawStyle (DrawStyle newStyle)
{
    if (drawStyle == newStyle || newStyle == DrawStyle::numStyles)
        return;
    drawStyle = newStyle;
    repaint ();
}

void WaveformView::setPeakEnvelopeThreshold (double newThreshold)
{
    newThreshold = juce::jmax (minSPP, newThreshold);
    if (peakEnvelopeThreshold == newThreshold)
        return;
    peakEnvelopeThreshold = newThreshold;
    repaint ();
}

bool WaveformView::isDrawingPeakEnvelope () const noexcept
{
    switch (drawStyle)
    {
        case DrawStyle::peakEnvelope: return true;
        case DrawStyle::sampleLine:   return false;
        case DrawStyle::automatic:
        case DrawStyle::numStyles:
        default:                      return samplesPerPixel > peakEnvelopeThreshold;
    }
}

void WaveformView::zoomVerticalToFit ()
{
    if (audioBuffer == nullptr || numSamples <= 0 || getWidth () <= 0)
        return;

    auto s0 = (juce::int64) std::floor (viewStartSample);
    auto s1 = (juce::int64) std::ceil  (viewStartSample + samplesPerPixel * (double) getWidth ());
    s0 = juce::jlimit ((juce::int64) 0, numSamples, s0);
    s1 = juce::jlimit ((juce::int64) 0, numSamples, s1);
    if (s1 <= s0)
        return;

    const auto sum  = querySummary (s0, s1);
    const auto peak = juce::jmax (std::abs (sum.mn), std::abs (sum.mx));
    if (peak > 1.0e-6f)
        setVerticalZoom (1.0f / peak);
}

//==============================================================================
void WaveformView::resized ()
{
    if (! viewInitialised)
        zoomToFit ();
    else
        clampView ();
}

double WaveformView::maxSamplesPerPixel () const noexcept
{
    if (getWidth () <= 0)
        return 1.0;
    return juce::jmax (minSPP, (double) numSamples / (double) getWidth ());
}

void WaveformView::clampView ()
{
    if (getWidth () <= 0 || numSamples <= 0)
        return;

    samplesPerPixel = juce::jlimit (minSPP, maxSamplesPerPixel (), samplesPerPixel);

    const auto visibleSpan = (double) getWidth () * samplesPerPixel;
    const auto maxStart     = juce::jmax (0.0, (double) numSamples - visibleSpan);
    viewStartSample         = juce::jlimit (0.0, maxStart, viewStartSample);
}

//==============================================================================
void WaveformView::paint (juce::Graphics& g)
{
    g.fillAll (colours.background);

    // centre line
    g.setColour (colours.centreLine);
    const auto mid = (float) getHeight () * 0.5f;
    g.drawHorizontalLine ((int) mid, 0.0f, (float) getWidth ());

    if (audioBuffer != nullptr && numSamples > 0 && getWidth () > 0 && getHeight () > 0)
    {
        if (isDrawingPeakEnvelope ())
            paintPeaks (g);
        else
            paintSamples (g);
    }

    if (onPaintOverlay != nullptr)
        onPaintOverlay (g, *this);
}

//==============================================================================
// Zoomed out: one vertical bar per pixel column spanning the true min..max of
// every sample under that column, with a brighter RMS body inside. Because each
// column reflects ALL of its samples, zooming keeps the envelope shape stable.
void WaveformView::paintPeaks (juce::Graphics& g)
{
    const auto width = getWidth ();
    columnCache.assign ((size_t) width, Summary { 0.0f, 0.0f, 0.0f });

    // First compute every column's summary (so we can draw peak & RMS in two
    // passes with a single colour change each, which is much cheaper).
    for (int px = 0; px < width; ++px)
    {
        auto s0 = (juce::int64) std::floor (xToSample ((float) px));
        auto s1 = (juce::int64) std::floor (xToSample ((float) (px + 1)));
        if (s1 <= s0)
            s1 = s0 + 1;

        s0 = juce::jlimit ((juce::int64) 0, numSamples, s0);
        s1 = juce::jlimit ((juce::int64) 0, numSamples, s1);

        if (s1 > s0)
            columnCache[(size_t) px] = querySummary (s0, s1);
        else
            columnCache[(size_t) px] = Summary { 0.0f, 0.0f, -1.0f }; // outside file
    }

    // Peak envelope.
    g.setColour (colours.peak);
    for (int px = 0; px < width; ++px)
    {
        const auto& c = columnCache[(size_t) px];
        if (c.rms < 0.0f)
            continue; // no data for this column
        const auto yTop = amplitudeToY (c.mx);
        const auto yBot = amplitudeToY (c.mn);
        g.drawVerticalLine (px, juce::jmin (yTop, yBot), juce::jmax (yTop, yBot) + 1.0f);
    }

    // RMS body (optional).
    if (rmsVisible)
    {
        g.setColour (colours.rms);
        for (int px = 0; px < width; ++px)
        {
            const auto& c = columnCache[(size_t) px];
            if (c.rms <= 0.0f)
                continue;
            const auto yTop = amplitudeToY (c.rms);
            const auto yBot = amplitudeToY (-c.rms);
            g.drawVerticalLine (px, juce::jmin (yTop, yBot), juce::jmax (yTop, yBot) + 1.0f);
        }
    }
}

//==============================================================================
// Zoomed in: draw the real samples as a polyline, with dots once samples are
// sparse enough on screen. At exactly 1 sample/pixel this matches the peak view,
// so the transition between the two modes is seamless.
//
// A raised envelope threshold (or DrawStyle::sampleLine) can also put us here
// with many samples per pixel, where one path point per sample would mean
// millions of points for a column of pixels that can only show a handful. So the
// polyline is decimated to a fixed budget of points per pixel: the shape stays
// the true sample line, it just stops paying for detail no pixel can resolve.
// Note that decimation IS point-subsampling, so unlike the peak envelope the
// zoomed-out sample line will alias - that is inherent to the representation.
void WaveformView::paintSamples (juce::Graphics& g)
{
    const auto width         = getWidth ();
    const auto pixelsPerSamp = 1.0 / samplesPerPixel;

    auto firstSample = (juce::int64) std::floor (viewStartSample) - 1;
    auto lastSample  = (juce::int64) std::ceil  (xToSample ((float) width)) + 1;
    firstSample = juce::jlimit ((juce::int64) 0, numSamples - 1, firstSample);
    lastSample  = juce::jlimit ((juce::int64) 0, numSamples - 1, lastSample);

    if (lastSample <= firstSample)
        return;

    const auto maxPoints = (juce::int64) juce::jmax (1, width) * kMaxLinePointsPerPixel;
    const auto numPoints = lastSample - firstSample + 1;
    const auto stride    = numPoints > maxPoints ? (numPoints + maxPoints - 1) / maxPoints
                                                 : (juce::int64) 1;

    juce::Path path;
    bool started = false;
    for (auto i = firstSample; i <= lastSample; i += stride)
    {
        const auto x = sampleToX ((double) i);
        const auto y = amplitudeToY (getDisplaySample (i));
        if (! started) { path.startNewSubPath (x, y); started = true; }
        else            path.lineTo (x, y);
    }

    // Stride rarely divides the span exactly - finish on the real last sample so
    // the line always reaches the right edge of the view.
    if (started && (lastSample - firstSample) % stride != 0)
        path.lineTo (sampleToX ((double) lastSample), amplitudeToY (getDisplaySample (lastSample)));

    g.setColour (colours.sampleLine);
    g.strokePath (path, juce::PathStrokeType (1.0f));

    // Dots need >= 8 pixels per sample, so we are never decimating when they show.
    if (pixelsPerSamp >= (double) kDotPixelsPerSample)
    {
        const float r = juce::jlimit (1.5f, 4.0f, (float) pixelsPerSamp * 0.18f);
        g.setColour (colours.sampleDot);
        for (auto i = firstSample; i <= lastSample; ++i)
        {
            const auto x = sampleToX ((double) i);
            const auto y = amplitudeToY (getDisplaySample (i));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }
    }
}

//==============================================================================
float WaveformView::getDisplaySample (juce::int64 index) const noexcept
{
    if (audioBuffer == nullptr || index < 0 || index >= numSamples)
        return 0.0f;

    const auto numCh = audioBuffer->getNumChannels ();
    if (numCh <= 0)
        return 0.0f;

    if (displayChannel >= 0 && displayChannel < numCh)
        return audioBuffer->getSample (displayChannel, (int) index);

    // downmix
    float sum = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        sum += audioBuffer->getSample (ch, (int) index);
    return sum / (float) numCh;
}

//==============================================================================
// Summary pyramid: level 0 bins raw samples in blocks of kBaseBinSize; each
// higher level halves resolution. Built once per buffer/channel change so that
// zoomed-out repaints never have to touch the raw data for large spans.
void WaveformView::buildPeakCache ()
{
    levels.clear ();
    if (audioBuffer == nullptr || numSamples <= 0)
        return;

    // Level 0 from raw samples.
    Level level0;
    level0.binSize = kBaseBinSize;
    const auto numBins0 = (numSamples + kBaseBinSize - 1) / kBaseBinSize;
    level0.bins.resize ((size_t) numBins0);
    for (juce::int64 b = 0; b < numBins0; ++b)
    {
        const auto start = b * kBaseBinSize;
        const auto end   = juce::jmin (numSamples, start + kBaseBinSize);
        Bin bin { std::numeric_limits<float>::max (), std::numeric_limits<float>::lowest (), 0.0, 0 };
        accumulateRaw (start, end, bin);
        level0.bins[(size_t) b] = bin;
    }
    levels.push_back (std::move (level0));

    // Higher levels: combine pairs of the previous level.
    while (levels.back ().bins.size () > 4)
    {
        const auto& prev = levels.back ();
        Level next;
        next.binSize = prev.binSize * 2;
        const auto numBins = (prev.bins.size () + 1) / 2;
        next.bins.resize (numBins);
        for (size_t b = 0; b < numBins; ++b)
        {
            const auto& a = prev.bins[b * 2];
            Bin combined = a;
            if (b * 2 + 1 < prev.bins.size ())
            {
                const auto& c = prev.bins[b * 2 + 1];
                combined.mn     = juce::jmin (combined.mn, c.mn);
                combined.mx     = juce::jmax (combined.mx, c.mx);
                combined.sumSq += c.sumSq;
                combined.count += c.count;
            }
            next.bins[b] = combined;
        }
        levels.push_back (std::move (next));
    }
}

void WaveformView::accumulateRaw (juce::int64 a, juce::int64 b, Bin& acc) const
{
    for (auto i = a; i < b; ++i)
    {
        const auto v = getDisplaySample (i);
        acc.mn     = juce::jmin (acc.mn, v);
        acc.mx     = juce::jmax (acc.mx, v);
        acc.sumSq += (double) v * (double) v;
        acc.count += 1;
    }
}

WaveformView::Summary WaveformView::querySummary (juce::int64 s0, juce::int64 s1) const
{
    Bin acc { std::numeric_limits<float>::max (), std::numeric_limits<float>::lowest (), 0.0, 0 };

    const auto span = s1 - s0;

    // Small spans: read raw for an exact result (cheap, and avoids any pyramid
    // over-coverage fattening thin transients when barely zoomed out).
    if (span <= kRawThreshold || levels.empty ())
    {
        accumulateRaw (s0, s1, acc);
    }
    else
    {
        // Pick the finest level whose bins keep the per-column work bounded.
        int chosen = (int) levels.size () - 1;
        for (int L = 0; L < (int) levels.size (); ++L)
        {
            if (levels[(size_t) L].binSize * kMaxBinsPerCol >= span)
            {
                chosen = L;
                break;
            }
        }

        const auto& level   = levels[(size_t) chosen];
        const auto  binSize = level.binSize;

        const auto firstFull = (s0 + binSize - 1) / binSize; // ceil
        const auto lastFull  = s1 / binSize;                 // exclusive (floor)

        if (firstFull >= lastFull)
        {
            // No whole bin fits - fall back to raw.
            accumulateRaw (s0, s1, acc);
        }
        else
        {
            // Raw head, aligned whole bins, raw tail: exact.
            accumulateRaw (s0, firstFull * binSize, acc);
            for (auto b = firstFull; b < lastFull; ++b)
            {
                const auto& bin = level.bins[(size_t) b];
                acc.mn     = juce::jmin (acc.mn, bin.mn);
                acc.mx     = juce::jmax (acc.mx, bin.mx);
                acc.sumSq += bin.sumSq;
                acc.count += bin.count;
            }
            accumulateRaw (lastFull * binSize, s1, acc);
        }
    }

    if (acc.count <= 0)
        return Summary { 0.0f, 0.0f, 0.0f };

    const auto rms = (float) std::sqrt (acc.sumSq / (double) acc.count);
    return Summary { acc.mn, acc.mx, rms };
}
