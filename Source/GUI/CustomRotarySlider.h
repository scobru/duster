#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace gui
{

class CustomRotaryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomRotaryLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff00d2ff)); // Neon Cyan
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff1f2430)); // Dark Track
        setColour(juce::Slider::thumbColourId, juce::Colour(0xffffffff));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(6.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();

        // 1. Draw Background Track Ring
        const float lineThickness = 4.5f;
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, radius, radius, 0.0f,
                                    rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff222735));
        g.strokePath(backgroundArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 2. Draw Active Value Arc (Gradient)
        float toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        if (toAngle > rotaryStartAngle)
        {
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, radius, radius, 0.0f,
                                   rotaryStartAngle, toAngle, true);

            juce::Colour primaryColor = slider.findColour(juce::Slider::rotarySliderFillColourId);
            juce::ColourGradient grad(primaryColor.withMultipliedBrightness(1.3f), center.x, bounds.getY(),
                                       primaryColor, center.x, bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.strokePath(valueArc, juce::PathStrokeType(lineThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Arc Glow
            g.setColour(primaryColor.withAlpha(0.25f));
            g.strokePath(valueArc, juce::PathStrokeType(lineThickness + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Draw Center Metallic Dial Face
        auto dialRadius = radius - lineThickness - 5.0f;
        auto dialBounds = juce::Rectangle<float>(center.x - dialRadius, center.y - dialRadius,
                                                 dialRadius * 2.0f, dialRadius * 2.0f);

        // Dial outer subtle bevel
        juce::ColourGradient dialGrad(juce::Colour(0xff2c3242), center.x, dialBounds.getY(),
                                      juce::Colour(0xff181c25), center.x, dialBounds.getBottom(), false);
        g.setGradientFill(dialGrad);
        g.fillEllipse(dialBounds);

        g.setColour(juce::Colour(0xff3b4356));
        g.drawEllipse(dialBounds, 1.2f);

        // 4. Draw Dial Pointer Notch
        juce::Path pointer;
        auto pointerLength = dialRadius * 0.65f;
        auto pointerThickness = 3.0f;
        pointer.addRoundedRectangle(-pointerThickness * 0.5f, -dialRadius + 2.0f,
                                     pointerThickness, pointerLength * 0.5f, 1.5f);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));

        g.setColour(juce::Colour(0xffffffff));
        g.fillPath(pointer);
    }
};

class CustomRotarySlider : public juce::Component
{
public:
    CustomRotarySlider(const juce::String& paramName, const juce::String& suffix = "", juce::Colour accentColour = juce::Colour(0xff00d2ff))
        : name(paramName), unitSuffix(suffix), accent(accentColour)
    {
        customLnF.setColour(juce::Slider::rotarySliderFillColourId, accent);
        slider.setLookAndFeel(&customLnF);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue(true, 0.0);
        addAndMakeVisible(slider);

        slider.onValueChange = [this]()
        {
            updateValueText();
            repaint();
        };

        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centred);
        nameLabel.setFont(juce::Font(12.0f, juce::Font::bold));
        nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa5b0c4));
        addAndMakeVisible(nameLabel);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setFont(juce::Font(11.0f, juce::Font::plain));
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe2e8f0));
        addAndMakeVisible(valueLabel);
    }

    ~CustomRotarySlider() override
    {
        slider.setLookAndFeel(nullptr);
    }

    juce::Slider& getSlider() { return slider; }

    void resized() override
    {
        auto bounds = getLocalBounds();
        nameLabel.setBounds(bounds.removeFromTop(18));
        valueLabel.setBounds(bounds.removeFromBottom(18));
        slider.setBounds(bounds);
    }

    void updateValueText()
    {
        float val = (float)slider.getValue();
        juce::String text;
        if (std::abs(val) < 0.001f)
            text = "0.0";
        else if (std::abs(val) < 10.0f)
            text = juce::String(val, 1);
        else
            text = juce::String((int)std::round(val));

        if (unitSuffix.isNotEmpty())
            text += " " + unitSuffix;

        valueLabel.setText(text, juce::dontSendNotification);
    }

private:
    CustomRotaryLookAndFeel customLnF;
    juce::Slider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    juce::String name;
    juce::String unitSuffix;
    juce::Colour accent;
};

} // namespace gui
