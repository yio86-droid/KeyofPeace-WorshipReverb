#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

//==============================================================================
class KeyofPeaceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KeyofPeaceLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xfff2e6c8));
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId,               juce::Colour (0xffc9b27c));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (8.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        const float arcW = 5.0f;
        const float arcR = radius - arcW * 0.5f;

        juce::Path track;
        track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2a3550));
        g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (0xffe0b85c));
        g.strokePath (value, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float knobR = radius * 0.66f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232c4a), cx, cy - knobR,
                                                 juce::Colour (0xff10162a), cx, cy + knobR, false));
        g.fillEllipse (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colour (0xff3a4668));
        g.drawEllipse (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.75f, -knobR + 4.0f, 3.5f, knobR * 0.45f, 1.5f);
        g.setColour (juce::Colour (0xfff2e6c8));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (cx, cy));
    }
};

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;   // se destruye antes que el slider
    };

    void setupKnob (Knob& knob, const juce::String& paramID, const juce::String& knobName,
                    const juce::String& suffix, int decimals);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;

    KeyofPeaceLookAndFeel lookAndFeel;
    std::array<Knob, 5> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
