#pragma once

#include <JuceHeader.h>

class StretchableLayoutResizerBarWithCallback : public juce::StretchableLayoutResizerBar
{
public:
    StretchableLayoutResizerBarWithCallback (juce::StretchableLayoutManager* layoutToUse, int itemIndexInLayout, bool isBarVertical)
        : StretchableLayoutResizerBar (layoutToUse, itemIndexInLayout, isBarVertical) {}

    std::function<void ()> onLayoutChange;

private:
    void hasBeenMoved () override
    {
        StretchableLayoutResizerBar::hasBeenMoved ();

        if (onLayoutChange != nullptr)
            onLayoutChange ();
    }
};

class MouseProxy : public juce::MouseListener
{
public:
    std::function<void ()> onMouseEnter;
    std::function<void ()> onMouseExit;
    std::function<void (const juce::MouseEvent& me)> onMouseDrag;
private:
    void mouseDrag (const juce::MouseEvent& me) override
    {
        if (onMouseDrag)
            onMouseDrag (me);
    }
    void mouseEnter (const juce::MouseEvent&) override
    {
        if (onMouseEnter)
            onMouseEnter ();
    }
    void mouseExit (const juce::MouseEvent&) override
    {
        if (onMouseExit)
            onMouseExit ();
    }
};

class SplitWindowComponent : public juce::Component
{
public:
    SplitWindowComponent ();
    ~SplitWindowComponent ();

    void setComponents (juce::Component* firstComponent, juce::Component* secondComponent);
    bool getHorizontalSplit ();
    void setHorizontalSplit (bool horizontalSplit);
    void setSplitOffset (int newSplitOffset);

    /*
        The margin left around the pair of components. The default keeps a single
        splitter off the edges of its parent, but a splitter nested inside another
        one would then be inset twice, and its pane would not line up with the
        pane beside it. Set it to zero on the inner splitter.
    */
    void setOuterMargin (int newOuterMargin) { outerMargin = newOuterMargin; resized (); }
    int getSplitOffset ();

    /*
        Where a drag is allowed to put the split, given where the pointer is asking for
        it. Applied before the split moves, so a limit costs nothing: a drag past it
        simply stops, rather than laying the panes out at the wrong size and then again
        at the right one. When unset, the pointer position is used as it is.
    */
    std::function<int (int proposedSplitOffset)> constrainSplitOffset;
    std::function<void ()> onLayoutChange;

private:
    juce::Component* firstComponent { nullptr };
    juce::Component* secondComponent { nullptr };
    juce::Rectangle<int> resizeBarBounds;
    bool horizontalSplit { true };
    int outerMargin { 4 };
    int splitOffset { 0 };
    bool mouseOver { false };

    void resized () override;
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& me) override;
    void mouseDrag (const juce::MouseEvent& me) override;
    void mouseExit (const juce::MouseEvent& me) override;
};
