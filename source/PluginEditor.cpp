#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    setupKnob (knobs[0], ParamIDs::decay,    "DECAY TIME", " s",  2);
    setupKnob (knobs[1], ParamIDs::preDelay, "PRE-DELAY",  " ms", 1);
    setupKnob (knobs[2], ParamIDs::lowCut,   "LOW-CUT",    " Hz", 0);
    setupKnob (knobs[3], ParamIDs::highCut,  "HIGH-CUT",   " Hz", 0);
    setupKnob (knobs[4], ParamIDs::mix,      "DRY / WET",  " %",  0);

    setSize (660, 320);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::setupKnob (Knob& knob, const juce::String& paramID,
                              const juce::String& knobName,
                              const juce::String& suffix, int decimals)
{
    auto& s = knob.slider;
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
    s.setTextValueSuffix (suffix);
    s.setNumDecimalPlacesToDisplay (decimals);
    s.setRotaryParameters (juce::degreesToRadians (225.0f),
                           juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (s);

    knob.label.setText (knobName, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<SliderAttachment> (processorRef.apvts, paramID, s);
}

//==============================================================================
void PluginEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141a30), 0.0f, 0.0f,
                                             juce::Colour (0xff0a0e1c), 0.0f, (float) getHeight(), false));
    g.fillAll();

    auto header = getLocalBounds().removeFromTop (70);

    g.setColour (juce::Colour (0xfff2e6c8));
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("KeyofPeace", header.removeFromTop (42).reduced (20, 0).withTrimmedTop (10),
                juce::Justification::centred);

    g.setColour (juce::Colour (0xffe0b85c));
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::plain)));
    g.drawText ("W O R S H I P   R E V E R B", header, juce::Justification::centredTop);

    g.setColour (juce::Colour (0x33e0b85c));
    g.drawHorizontalLine (72, 30.0f, (float) getWidth() - 30.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    area.removeFromTop (60);

    const int colW = area.getWidth() / 5;
    for (auto& k : knobs)
    {
        auto col = area.removeFromLeft (colW);
        k.label.setBounds (col.removeFromTop (24));
        k.slider.setBounds (col);
    }
}
