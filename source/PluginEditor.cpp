#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (inspectButton);

    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible (true);
    };

    // ==========================================================
    // BANK 1 WIRING: THE ENGINE (RED)
    // ==========================================================
    voxButton.setLookAndFeel(&vintageLookAndFeel);
    spaceButton.setLookAndFeel(&vintageLookAndFeel);
    punchButton.setLookAndFeel(&vintageLookAndFeel);

    voxButton.onClick = [this] {
        if (voxButton.getToggleState()) {
            spaceButton.setToggleState(false, juce::dontSendNotification);
            punchButton.setToggleState(false, juce::dontSendNotification);
            processorRef.currentMode.store(1);
        } else { processorRef.currentMode.store(0); }
    };
    spaceButton.onClick = [this] {
        if (spaceButton.getToggleState()) {
            voxButton.setToggleState(false, juce::dontSendNotification);
            punchButton.setToggleState(false, juce::dontSendNotification);
            processorRef.currentMode.store(2);
        } else { processorRef.currentMode.store(0); }
    };
    punchButton.onClick = [this] {
        if (punchButton.getToggleState()) {
            voxButton.setToggleState(false, juce::dontSendNotification);
            spaceButton.setToggleState(false, juce::dontSendNotification);
            processorRef.currentMode.store(3);
        } else { processorRef.currentMode.store(0); }
    };

    addAndMakeVisible(voxButton);
    addAndMakeVisible(spaceButton);
    addAndMakeVisible(punchButton);

    // ==========================================================
    // BANK 2 WIRING: THE MODIFIERS (PURPLE)
    // ==========================================================
    flipButton.setLookAndFeel(&purpleLookAndFeel);
    shredButton.setLookAndFeel(&purpleLookAndFeel);
    chopButton.setLookAndFeel(&purpleLookAndFeel);

    // ==========================================================
    // SHRED TRILOGY SUB-MENU WIRING
    // ==========================================================
    shredMode1.setLookAndFeel(&miniPurpleLookAndFeel);
    shredMode2.setLookAndFeel(&miniPurpleLookAndFeel);
    shredMode3.setLookAndFeel(&miniPurpleLookAndFeel);

    shredMode1.setToggleState(true, juce::dontSendNotification); // Default to Mode I

    auto shredModeClick = [this](int m, juce::ToggleButton* btn) {
        if (btn->getToggleState()) {
            if (btn != &shredMode1) shredMode1.setToggleState(false, juce::dontSendNotification);
            if (btn != &shredMode2) shredMode2.setToggleState(false, juce::dontSendNotification);
            if (btn != &shredMode3) shredMode3.setToggleState(false, juce::dontSendNotification);
            processorRef.currentShredMode.store(m);
        } else {
            // Prevent turning all of them off. One must always be active!
            btn->setToggleState(true, juce::dontSendNotification);
        }
    };

    shredMode1.onClick = [this, shredModeClick] { shredModeClick(1, &shredMode1); };
    shredMode2.onClick = [this, shredModeClick] { shredModeClick(2, &shredMode2); };
    shredMode3.onClick = [this, shredModeClick] { shredModeClick(3, &shredMode3); };

    addAndMakeVisible(shredMode1);
    addAndMakeVisible(shredMode2);
    addAndMakeVisible(shredMode3);

    flipButton.onClick = [this] {
        if (flipButton.getToggleState()) {
            shredButton.setToggleState(false, juce::dontSendNotification);
            chopButton.setToggleState(false, juce::dontSendNotification);
            processorRef.currentModifier.store(1);
        } else { processorRef.currentModifier.store(0); }
    };
    shredButton.onClick = [this] {
        if (shredButton.getToggleState()) {
            flipButton.setToggleState(false, juce::dontSendNotification);
            chopButton.setToggleState(false, juce::dontSendNotification);
            processorRef.currentModifier.store(2);
        } else { processorRef.currentModifier.store(0); }
    };
    chopButton.onClick = [this] {
        if (chopButton.getToggleState()) {
            flipButton.setToggleState(false, juce::dontSendNotification);
            shredButton.setToggleState(false, juce::dontSendNotification);
            processorRef.currentModifier.store(3);
        } else { processorRef.currentModifier.store(0); }
    };

    addAndMakeVisible(flipButton);
    addAndMakeVisible(shredButton);
    addAndMakeVisible(chopButton);

    // ==========================================================
    // BANK 3 WIRING: RATIO DIAGNOSTICS (RED)
    // ==========================================================
    ratio3Button.setLookAndFeel(&vintageLookAndFeel);
    ratio6Button.setLookAndFeel(&vintageLookAndFeel);
    ratio9Button.setLookAndFeel(&vintageLookAndFeel);

    auto ratioClick = [this](int r, juce::ToggleButton* btn) {
        if (btn->getToggleState()) {
            if (btn != &ratio3Button) ratio3Button.setToggleState(false, juce::dontSendNotification);
            if (btn != &ratio6Button) ratio6Button.setToggleState(false, juce::dontSendNotification);
            if (btn != &ratio9Button) ratio9Button.setToggleState(false, juce::dontSendNotification);
            processorRef.currentRatio.store(r);
        } else {
            processorRef.currentRatio.store(1); // Default 1:1 when all unclicked
        }
    };

    ratio3Button.onClick = [this, ratioClick] { ratioClick(3, &ratio3Button); };
    ratio6Button.onClick = [this, ratioClick] { ratioClick(6, &ratio6Button); };
    ratio9Button.onClick = [this, ratioClick] { ratioClick(9, &ratio9Button); };

    addAndMakeVisible(ratio3Button);
    addAndMakeVisible(ratio6Button);
    addAndMakeVisible(ratio9Button);

    startTimerHz(30);
    setSize (600, 220);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    
    voxButton.setLookAndFeel(nullptr);
    spaceButton.setLookAndFeel(nullptr);
    punchButton.setLookAndFeel(nullptr);
    
    flipButton.setLookAndFeel(nullptr);
    shredButton.setLookAndFeel(nullptr);
    chopButton.setLookAndFeel(nullptr);

    ratio3Button.setLookAndFeel(nullptr);
    ratio6Button.setLookAndFeel(nullptr);
    ratio9Button.setLookAndFeel(nullptr);
}

void PluginEditor::paint (juce::Graphics& g)
{
    // MAIN BACKGROUND
    {
        juce::ColourGradient bgGradient(juce::Colour(0xff2d2d2d), 0.0f, 0.0f,
                                        juce::Colour(0xff0d0d0d), 0.0f, static_cast<float>(getHeight()),
                                        false);
        g.setGradientFill(bgGradient);
        g.fillRect(getLocalBounds());
    }

    // Procedural grain texture
    {
        juce::Random rng(12345);
        int numGrains = (getWidth() * getHeight()) / 20;
        for (int i = 0; i < numGrains; ++i)
        {
            int gx = rng.nextInt(getWidth());
            int gy = rng.nextInt(getHeight());
            g.setColour(juce::Colours::white.withAlpha(0.03f + rng.nextFloat() * 0.02f));
            g.fillRect(gx, gy, 1, 1);
        }
    }

    // 3D BEVEL
    auto bounds = getLocalBounds();
    g.setColour(juce::Colour(0xff5a5a5a));
    g.drawLine(0.0f, 0.0f, static_cast<float>(bounds.getWidth()), 0.0f, 1.0f);
    g.setColour(juce::Colour(0xff000000));
    g.drawLine(0.0f, static_cast<float>(bounds.getHeight() - 1),
               static_cast<float>(bounds.getWidth()), static_cast<float>(bounds.getHeight() - 1), 1.0f);

    // HARDWARE DETAILS
    g.setColour(juce::Colour(0xffc0c0c0));
    g.fillEllipse(10.0f, 10.0f, 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(10.0f, 10.0f, 6.0f, 6.0f, 0.5f);

    g.setColour(juce::Colour(0xffc0c0c0));
    g.fillEllipse(static_cast<float>(bounds.getWidth() - 16), 10.0f, 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(static_cast<float>(bounds.getWidth() - 16), 10.0f, 6.0f, 6.0f, 0.5f);

    g.setColour(juce::Colour(0xffc0c0c0));
    g.fillEllipse(10.0f, static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(10.0f, static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f, 0.5f);

    g.setColour(juce::Colour(0xffc0c0c0));
    g.fillEllipse(static_cast<float>(bounds.getWidth() - 16), static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(static_cast<float>(bounds.getWidth() - 16), static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f, 0.5f);

    drawVintageMeter(g, analyzedMeter, smoothedAnalyzed);
    drawActionMeter(g, actionMeter,    smoothedAction);
    drawVintageMeter(g, outputMeter,   smoothedOutput);

    auto drawMeterRecess = [&](juce::Rectangle<int> meterRect) {
        float x = static_cast<float>(meterRect.getX());
        float y = static_cast<float>(meterRect.getY());
        float w = static_cast<float>(meterRect.getWidth());
        float h = static_cast<float>(meterRect.getHeight());

        g.setColour(juce::Colour(0xff000000).withAlpha(0.3f));
        g.drawLine(x + 1, y + 1, x + w - 1, y + 1, 1.0f);  
        g.drawLine(x + 1, y + 1, x + 1, y + h - 1, 1.0f);  

        g.setColour(juce::Colour(0xffffffff).withAlpha(0.1f));
        g.drawLine(x + 1, y + h - 2, x + w - 1, y + h - 2, 1.0f);  
        g.drawLine(x + w - 2, y + 1, x + w - 2, y + h - 1, 1.0f);  

        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(x, y, w, h, 5.0f, 1.0f);
    };

    drawMeterRecess(analyzedMeter);
    drawMeterRecess(actionMeter);
    drawMeterRecess(outputMeter);

    int stripWidth = 180;
    int stripHeight = 36;
    int stripY = actionMeter.getBottom() + 15; 

    // DRAW CONTROL STRIP 1 (CENTER)
    int strip1X = actionMeter.getX() + (actionMeter.getWidth() - stripWidth) / 2;
    juce::Rectangle<int> strip1Area(strip1X, stripY, stripWidth, stripHeight);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRect(strip1Area);
    g.setColour(juce::Colour(0xff333333));
    g.drawLine(strip1X, stripY, strip1X + stripWidth, stripY, 1.0f);
    g.drawLine(strip1X, stripY, strip1X, stripY + stripHeight, 1.0f);
    g.setColour(juce::Colour(0xff666666).withAlpha(0.5f));
    g.drawLine(strip1X, stripY + stripHeight, strip1X + stripWidth, stripY + stripHeight, 1.0f);
    g.drawLine(strip1X + stripWidth, stripY, strip1X + stripWidth, stripY + stripHeight, 1.0f);

    // DRAW CONTROL STRIP 2 (RIGHT)
    int strip2X = outputMeter.getX() + (outputMeter.getWidth() - stripWidth) / 2;
    juce::Rectangle<int> strip2Area(strip2X, stripY, stripWidth, stripHeight);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRect(strip2Area);
    g.setColour(juce::Colour(0xff333333));
    g.drawLine(strip2X, stripY, strip2X + stripWidth, stripY, 1.0f);
    g.drawLine(strip2X, stripY, strip2X, stripY + stripHeight, 1.0f);
    g.setColour(juce::Colour(0xff666666).withAlpha(0.5f));
    g.drawLine(strip2X, stripY + stripHeight, strip2X + stripWidth, stripY + stripHeight, 1.0f);
    g.drawLine(strip2X + stripWidth, stripY, strip2X + stripWidth, stripY + stripHeight, 1.0f);

    // DRAW CONTROL STRIP 3 (LEFT - RATIO)
    int strip3X = analyzedMeter.getX() + (analyzedMeter.getWidth() - stripWidth) / 2;
    juce::Rectangle<int> strip3Area(strip3X, stripY, stripWidth, stripHeight);
    g.setColour(juce::Colour(0xff0a0a0a));
    g.fillRect(strip3Area);
    g.setColour(juce::Colour(0xff333333));
    g.drawLine(strip3X, stripY, strip3X + stripWidth, stripY, 1.0f);
    g.drawLine(strip3X, stripY, strip3X, stripY + stripHeight, 1.0f);
    g.setColour(juce::Colour(0xff666666).withAlpha(0.5f));
    g.drawLine(strip3X, stripY + stripHeight, strip3X + stripWidth, stripY + stripHeight, 1.0f);
    g.drawLine(strip3X + stripWidth, stripY, strip3X + stripWidth, stripY + stripHeight, 1.0f);

    // SCREEN-PRINT LABELS
    {
        juce::Font labelFont(juce::FontOptions(13.0f).withStyle("Bold"));
        g.setFont(labelFont);
        g.setColour(juce::Colour(0xffe6e6e6));

        int labelY = analyzedMeter.getBottom() + 8;
        int labelH = 20;

        g.drawText("ANALYZED", analyzedMeter.getX(), labelY, analyzedMeter.getWidth(), labelH, juce::Justification::centred);
        // "OUTPUT" text completely removed to unblock the purple buttons!

        juce::GlyphArrangement textGlyphs;
        textGlyphs.addLineOfText(labelFont, "ANALYZED", 0.0f, 0.0f);
        float textWidth = textGlyphs.getBoundingBox(0, -1, true).getWidth();

        juce::GlyphArrangement dGlyphs;
        dGlyphs.addLineOfText(labelFont, "D", 0.0f, 0.0f);
        float dWidth = dGlyphs.getBoundingBox(0, -1, true).getWidth();

        float labelCenterX = static_cast<float>(analyzedMeter.getCentreX());
        float textRightEdge = labelCenterX + textWidth / 2.0f;
        float ledLeftEdge   = textRightEdge + dWidth / 2.0f;
        float ledCenterX    = ledLeftEdge + 4.0f;   
        float ledCenterY    = static_cast<float>(labelY) + static_cast<float>(labelH) / 2.0f;

        float ledRadius = 4.0f;
        juce::Rectangle<float> ledRect(ledCenterX - ledRadius, ledCenterY - ledRadius, 8.0f, 8.0f);

        g.setColour(sidechainActive ? juce::Colour(0xFFFF0000) : juce::Colour(0xFF440000));
        g.fillEllipse(ledRect);
    }
}

void PluginEditor::resized()
{
    analyzedMeter = juce::Rectangle<int>(20, 30, 160, 110);
    actionMeter   = juce::Rectangle<int>(220, 30, 160, 110);
    outputMeter   = juce::Rectangle<int>(420, 30, 160, 110);

    int stripWidth = 180;
    int stripHeight = 36;
    int stripY = actionMeter.getBottom() + 15;
    int buttonWidth = stripWidth / 3;

    int strip1X = actionMeter.getX() + (actionMeter.getWidth() - stripWidth) / 2;
    voxButton.setBounds(strip1X, stripY, buttonWidth, stripHeight);
    spaceButton.setBounds(strip1X + buttonWidth, stripY, buttonWidth, stripHeight);
    punchButton.setBounds(strip1X + (buttonWidth * 2), stripY, buttonWidth, stripHeight);

    int strip2X = outputMeter.getX() + (outputMeter.getWidth() - stripWidth) / 2;
    flipButton.setBounds(strip2X, stripY, buttonWidth, stripHeight);
    shredButton.setBounds(strip2X + buttonWidth, stripY, buttonWidth, stripHeight);
    chopButton.setBounds(strip2X + (buttonWidth * 2), stripY, buttonWidth, stripHeight);
    // Place the Shred Sub-Menu directly under the SHRED button
    int miniStripY = stripY + stripHeight + 4;
    int miniBtnW = buttonWidth / 3;
    shredMode1.setBounds(strip2X + buttonWidth, miniStripY, miniBtnW, 16);
    shredMode2.setBounds(strip2X + buttonWidth + miniBtnW, miniStripY, miniBtnW, 16);
    shredMode3.setBounds(strip2X + buttonWidth + (miniBtnW * 2), miniStripY, miniBtnW, 16);

    int strip3X = analyzedMeter.getX() + (analyzedMeter.getWidth() - stripWidth) / 2;
    ratio3Button.setBounds(strip3X, stripY, buttonWidth, stripHeight);
    ratio6Button.setBounds(strip3X + buttonWidth, stripY, buttonWidth, stripHeight);
    ratio9Button.setBounds(strip3X + (buttonWidth * 2), stripY, buttonWidth, stripHeight);

    inspectButton.setBounds(getWidth() - 110, getHeight() - 35, 100, 25);
}

void PluginEditor::timerCallback()
{
    float mainLevel      = processorRef.getMainBusLevel();
    float sidechainLevel = processorRef.getSidechainBusLevel();
    float gainDb         = processorRef.getCurrentGainDb();

    float calibrationOffset = 12.0f;
    float targetAnalyzed = juce::Decibels::gainToDecibels(sidechainLevel, -90.0f) + calibrationOffset;
    float targetOutput   = juce::Decibels::gainToDecibels(mainLevel,      -90.0f) + calibrationOffset;
    float targetAction   = gainDb;

    float prevAnalyzed = smoothedAnalyzed;
    float prevOutput   = smoothedOutput;
    float prevAction   = smoothedAction;

    auto smooth = [](float& current, float target)
    {
        float coeff = (target > current) ? 0.1f : 0.04f;
        current += coeff * (target - current);
    };

    smooth(smoothedAnalyzed, targetAnalyzed);
    smooth(smoothedOutput,   targetOutput);
    smooth(smoothedAction,   targetAction);

    bool newSidechain = (sidechainLevel > 0.0001f);
    bool sidechainChanged = (newSidechain != sidechainActive);
    sidechainActive = newSidechain;

    peakActive  = (mainLevel > 1.0f || sidechainLevel > 1.0f);
    actionPeak  = (std::abs(gainDb) > 9.0f);

    bool needsRepaint = sidechainChanged
                      || peakActive
                      || actionPeak
                      || (std::abs(smoothedAnalyzed - prevAnalyzed) > 0.05f)
                      || (std::abs(smoothedOutput   - prevOutput)   > 0.05f)
                      || (std::abs(smoothedAction   - prevAction)   > 0.05f);

    if (needsRepaint)
        repaint();
}

void PluginEditor::drawVintageMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float levelDb)
{
    float boxX = static_cast<float>(bounds.getX());
    float boxY = static_cast<float>(bounds.getY());
    float boxWidth = static_cast<float>(bounds.getWidth());
    float boxHeight = static_cast<float>(bounds.getHeight());

    g.setColour(juce::Colour(0xFFE8D9A1));
    g.fillRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 5.0f);

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 5.0f, 1.5f);

    float pivotX = boxX + (boxWidth * 0.5f);
    float pivotY = boxY + boxHeight;

    float arcWidth = boxWidth * 0.85f;
    float arcHeight = boxHeight * 0.7f;
    float arcX = pivotX - (arcWidth * 0.5f);
    float arcY = boxY + 8.0f;

    float startAngleFromVertical = -55.0f;
    float endAngleFromVertical   =  55.0f;

    float startAngle = juce::degreesToRadians(-90.0f + startAngleFromVertical);
    float endAngle = juce::degreesToRadians(-90.0f + endAngleFromVertical);

    float arcCenterX = arcX + (arcWidth * 0.5f);
    float arcCenterY = arcY + (arcHeight * 0.5f);
    float radiusX = arcWidth * 0.5f;
    float radiusY = arcHeight * 0.5f;

    juce::Path arcPath;
    arcPath.addCentredArc(arcCenterX, arcCenterY, radiusX, radiusY, 0.0f, startAngle, endAngle, true);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.strokePath(arcPath, juce::PathStrokeType(1.5f));

    float tickMarks[] = {-60, -20, -10, -7, -5, -3, 0, 3};

    auto dbToAngle = [](float db) -> float {
        if (db == -60.0f) return -55.00f;
        if (db == -20.0f) return -28.70f;
        if (db == -10.0f) return  -7.17f;
        if (db ==  -7.0f) return   7.17f;
        if (db ==  -5.0f) return  16.74f;
        if (db ==  -3.0f) return  26.30f;
        if (db ==   0.0f) return  40.65f;
        if (db ==   3.0f) return  55.00f;
        return 0.0f;
    };

    for (float db : tickMarks)
    {
        float angleFromVertical = dbToAngle(db);
        float angle = juce::degreesToRadians(-90.0f + angleFromVertical);

        float outerX = arcCenterX + radiusX * std::cos(angle);
        float outerY = arcCenterY + radiusY * std::sin(angle);
        float innerX = arcCenterX + (radiusX - 6.0f) * std::cos(angle);
        float innerY = arcCenterY + (radiusY - 6.0f) * std::sin(angle);

        if (db >= 0.0f)
            g.setColour(juce::Colours::red);
        else
            g.setColour(juce::Colours::black.withAlpha(0.6f));

        float thickness = (db == 0 || db == 3) ? 2.0f : 1.2f;
        g.drawLine(innerX, innerY, outerX, outerY, thickness);

        float textRadiusX = radiusX - 6.0f - 8.0f;
        float textRadiusY = radiusY - 6.0f - 8.0f;

        float textX = arcCenterX + textRadiusX * std::cos(angle);
        float textY = arcCenterY + textRadiusY * std::sin(angle);

        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));

        if (db == 0.0f || db == 3.0f)
            g.setColour(juce::Colours::red);
        else
            g.setColour(juce::Colour(0xff333333));

        juce::String label = juce::String(static_cast<int>(std::abs(db)));
        g.drawText(label, static_cast<int>(textX - 12), static_cast<int>(textY - 8),
                   24, 16, juce::Justification::centred);
    }

    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("VU", static_cast<int>(boxX + boxWidth * 0.5f - 15),
                      static_cast<int>(boxY + boxHeight * 0.5f),
                      30, 20, juce::Justification::centred);

    g.setColour(juce::Colour(0xff333333));
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.drawText("J-RIDER", static_cast<int>(boxX + boxWidth - 42),
                           static_cast<int>(boxY + boxHeight - 12),
                           38, 10, juce::Justification::centredRight);

    float db = levelDb;
    float angleDegrees;
    if (db <= -90.0f)      angleDegrees = -70.0f;
    else if (db <= -60.0f) angleDegrees = juce::jmap(db, -90.0f, -60.0f, -70.0f, -55.0f);
    else if (db <= -20.0f) angleDegrees = juce::jmap(db, -60.0f, -20.0f, -55.0f, -28.70f);
    else if (db <= -10.0f) angleDegrees = juce::jmap(db, -20.0f, -10.0f, -28.70f,  -7.17f);
    else if (db <= -7.0f)  angleDegrees = juce::jmap(db, -10.0f,  -7.0f,  -7.17f,   7.17f);
    else if (db <= -5.0f)  angleDegrees = juce::jmap(db,  -7.0f,  -5.0f,   7.17f,  16.74f);
    else if (db <= -3.0f)  angleDegrees = juce::jmap(db,  -5.0f,  -3.0f,  16.74f,  26.30f);
    else if (db <= 0.0f)   angleDegrees = juce::jmap(db,  -3.0f,   0.0f,  26.30f,  40.65f);
    else if (db <= 3.0f)   angleDegrees = juce::jmap(db,   0.0f,   3.0f,  40.65f,  55.0f);
    else                   angleDegrees = 55.0f;

    float angleRadians = juce::degreesToRadians(angleDegrees);

    float needleLength = boxHeight * 0.7f;
    float needleEndX = pivotX + (needleLength * std::sin(angleRadians));
    float needleEndY = pivotY - (needleLength * std::cos(angleRadians));

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(pivotX + 1, pivotY + 1, needleEndX + 1, needleEndY + 1, 2.0f);

    g.setColour(juce::Colours::black);
    g.drawLine(pivotX, pivotY, needleEndX, needleEndY, 2.0f);

    g.setColour(peakActive ? juce::Colours::red : juce::Colour(0xFF440000));
    g.fillEllipse(boxX + boxWidth - 16, boxY + 8, 8, 8);

    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.setColour(juce::Colour(0xff333333));
    g.drawText("90", static_cast<int>(boxX + 15.0f - 12), static_cast<int>(boxY + boxHeight - 25.0f - 8),
               24, 16, juce::Justification::centred);
}

void PluginEditor::drawActionMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float gainDb)
{
    float boxX = static_cast<float>(bounds.getX());
    float boxY = static_cast<float>(bounds.getY());
    float meterWidth = static_cast<float>(bounds.getWidth());
    float meterHeight = static_cast<float>(bounds.getHeight());

    g.setColour(juce::Colour(0xFFE8D9A1));
    g.fillRoundedRectangle(boxX, boxY, meterWidth, meterHeight, 5.0f);

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(boxX, boxY, meterWidth, meterHeight, 5.0f, 1.5f);

    float pivotX = boxX + (meterWidth * 0.5f);
    float pivotY = boxY + 0.0f;

    float needleLength = meterHeight * 0.8f;
    float tickRadius = needleLength;
    float textRadius = meterHeight * 0.92f;

    const float maxAngleRadians = 0.7f;
    const float maxAngleDegrees = juce::radiansToDegrees(maxAngleRadians);

    float tickMarks[] = {-9, -6, -3, 0, 3, 6, 9};

    for (float db : tickMarks)
    {
        float angleFromVertical = juce::jmap(db, -9.0f, 9.0f, maxAngleDegrees, -maxAngleDegrees);
        float angle = juce::degreesToRadians(90.0f + angleFromVertical);

        float tickOuterX = pivotX + tickRadius * std::cos(angle);
        float tickOuterY = pivotY + tickRadius * std::sin(angle);
        float tickInnerX = pivotX + (tickRadius - 6.0f) * std::cos(angle);
        float tickInnerY = pivotY + (tickRadius - 6.0f) * std::sin(angle);

        if (db > 0.0f)       g.setColour(juce::Colours::red);
        else if (db < 0.0f)  g.setColour(juce::Colours::black.withAlpha(0.6f));
        else                 g.setColour(juce::Colours::black.withAlpha(0.8f));

        float thickness = (db == 0) ? 2.0f : 1.2f;
        g.drawLine(tickInnerX, tickInnerY, tickOuterX, tickOuterY, thickness);

        float textX = pivotX + textRadius * std::cos(angle);
        float textY = pivotY + textRadius * std::sin(angle);

        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        juce::String label = (db > 0) ? ("+" + juce::String(static_cast<int>(db)))
                                      : juce::String(static_cast<int>(db));

        g.drawText(label, static_cast<int>(textX - 15), static_cast<int>(textY - 8),
                   30, 16, juce::Justification::centred);
    }

    float needleAngleFromVertical;
    if (gainDb <= -9.0f)    needleAngleFromVertical =  maxAngleDegrees; 
    else if (gainDb >= 9.0f) needleAngleFromVertical = -maxAngleDegrees; 
    else                    needleAngleFromVertical = juce::jmap(gainDb, -9.0f, 9.0f, maxAngleDegrees, -maxAngleDegrees);

    float needleAngle = juce::degreesToRadians(90.0f + needleAngleFromVertical);

    float needleTipX = pivotX + needleLength * std::cos(needleAngle);
    float needleTipY = pivotY + needleLength * std::sin(needleAngle);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(pivotX + 1, pivotY + 1, needleTipX + 1, needleTipY + 1, 2.0f);

    g.setColour(juce::Colours::black);
    g.drawLine(pivotX, pivotY, needleTipX, needleTipY, 2.0f);

    g.setColour(juce::Colour(0xff333333));
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.drawText("J-RIDER", static_cast<int>(boxX + meterWidth - 42),
                           static_cast<int>(boxY + meterHeight - 12),
                           38, 10, juce::Justification::centredRight);

    g.setColour(actionPeak ? juce::Colours::red : juce::Colour(0xFF440000));
    g.fillEllipse(boxX + meterWidth - 16, boxY + 8, 8, 8);
}

void PluginEditor::drawMeterArc(juce::Graphics& g, juce::Point<float> arcCenter, float arcRadius, juce::Rectangle<float> box)
{
    float startAngle = -juce::MathConstants<float>::halfPi - 0.7f;
    float endAngle = -juce::MathConstants<float>::halfPi + 0.7f;

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    juce::Path arcPath;
    arcPath.addCentredArc(arcCenter.x, arcCenter.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.strokePath(arcPath, juce::PathStrokeType(1.5f));

    float tickMarks[] = {-20, -10, -7, -5, -3, 0, 3};
    for (float db : tickMarks)
    {
        float angleFromVertical = juce::jmap(db, -20.0f, 3.0f, -0.7f, 0.7f);
        float angle = -juce::MathConstants<float>::halfPi + angleFromVertical;

        auto outerPoint = arcCenter.getPointOnCircumference(arcRadius, angle);
        auto innerPoint = arcCenter.getPointOnCircumference(arcRadius - 6.0f, angle);

        if (db >= 0.0f)      g.setColour(juce::Colours::red);
        else                 g.setColour(juce::Colours::black.withAlpha(0.6f));

        float thickness = (db == 0 || db == 3) ? 2.0f : 1.2f;
        g.drawLine(innerPoint.x, innerPoint.y, outerPoint.x, outerPoint.y, thickness);
    }
}

void PluginEditor::drawPeakLED(juce::Graphics& g, float x, float y)
{
    juce::Rectangle<float> ledRect(x - 4, y - 4, 8, 8);

    if (peakActive)
    {
        g.setColour(juce::Colours::red.withAlpha(0.3f));
        g.fillEllipse(ledRect.expanded(3.0f));
        g.setColour(juce::Colours::red.withAlpha(0.5f));
        g.fillEllipse(ledRect.expanded(1.5f));
        g.setColour(juce::Colours::red);
    }
    else
    {
        g.setColour(juce::Colour(0xFF440000));
    }

    g.fillEllipse(ledRect);
}