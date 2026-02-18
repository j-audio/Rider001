#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    // Start timer to update meters (30 fps)
    startTimerHz(30);

    // Set the plugin window size to 600x220 pixels
    setSize (600, 220);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    // MAIN BACKGROUND - Vertical gradient for metallic look
    {
        juce::ColourGradient bgGradient(juce::Colour(0xff2d2d2d), 0.0f, 0.0f,
                                        juce::Colour(0xff0d0d0d), 0.0f, static_cast<float>(getHeight()),
                                        false);
        g.setGradientFill(bgGradient);
        g.fillRect(getLocalBounds());
    }

    // Procedural grain texture (3-5% opacity)
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

    // 3D BEVEL - Top highlight and bottom shadow
    auto bounds = getLocalBounds();
    // Top edge - light grey highlight (1px)
    g.setColour(juce::Colour(0xff5a5a5a));
    g.drawLine(0.0f, 0.0f, static_cast<float>(bounds.getWidth()), 0.0f, 1.0f);
    // Bottom edge - black shadow (1px)
    g.setColour(juce::Colour(0xff000000));
    g.drawLine(0.0f, static_cast<float>(bounds.getHeight() - 1),
               static_cast<float>(bounds.getWidth()), static_cast<float>(bounds.getHeight() - 1), 1.0f);

    // HARDWARE DETAILS - Silver screw heads in corners (inset 10px)
    g.setColour(juce::Colour(0xffc0c0c0));  // Silver
    // Top-left screw
    g.fillEllipse(10.0f, 10.0f, 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(10.0f, 10.0f, 6.0f, 6.0f, 0.5f);

    // Top-right screw
    g.setColour(juce::Colour(0xffc0c0c0));
    g.fillEllipse(static_cast<float>(bounds.getWidth() - 16), 10.0f, 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(static_cast<float>(bounds.getWidth() - 16), 10.0f, 6.0f, 6.0f, 0.5f);

    // Bottom-left screw
    g.setColour(juce::Colour(0xffc0c0c0));
    g.fillEllipse(10.0f, static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(10.0f, static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f, 0.5f);

    // Bottom-right screw
    g.setColour(juce::Colour(0xffc0c0c0));
    g.fillEllipse(static_cast<float>(bounds.getWidth() - 16), static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f);
    g.setColour(juce::Colour(0xff808080));
    g.drawEllipse(static_cast<float>(bounds.getWidth() - 16), static_cast<float>(bounds.getHeight() - 16), 6.0f, 6.0f, 0.5f);

    // Draw meters using smoothed dB values from timerCallback
    drawVintageMeter(g, analyzedMeter, smoothedAnalyzed);
    drawActionMeter(g, actionMeter,    smoothedAction);
    drawVintageMeter(g, outputMeter,   smoothedOutput);

    // METER 'RECESSED' LOOK - Inner shadows and dark borders
    auto drawMeterRecess = [&](juce::Rectangle<int> meterRect) {
        float x = static_cast<float>(meterRect.getX());
        float y = static_cast<float>(meterRect.getY());
        float w = static_cast<float>(meterRect.getWidth());
        float h = static_cast<float>(meterRect.getHeight());

        // Inner shadow (top-left darker, bottom-right lighter) for sunken effect
        g.setColour(juce::Colour(0xff000000).withAlpha(0.3f));
        g.drawLine(x + 1, y + 1, x + w - 1, y + 1, 1.0f);  // Top inner shadow
        g.drawLine(x + 1, y + 1, x + 1, y + h - 1, 1.0f);  // Left inner shadow

        g.setColour(juce::Colour(0xffffffff).withAlpha(0.1f));
        g.drawLine(x + 1, y + h - 2, x + w - 1, y + h - 2, 1.0f);  // Bottom inner highlight
        g.drawLine(x + w - 2, y + 1, x + w - 2, y + h - 1, 1.0f);  // Right inner highlight

        // 1px dark border around meter box
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRoundedRectangle(x, y, w, h, 5.0f, 1.0f);
    };

    drawMeterRecess(analyzedMeter);
    drawMeterRecess(actionMeter);
    drawMeterRecess(outputMeter);

    // SCREEN-PRINT LABELS - Vintage style, drawn directly below each meter
    {
        juce::Font labelFont(juce::FontOptions(13.0f).withStyle("Bold"));
        g.setFont(labelFont);
        g.setColour(juce::Colour(0xffe6e6e6));

        int labelY = analyzedMeter.getBottom() + 8;
        int labelH = 20;

        g.drawText("ANALYZED", analyzedMeter.getX(), labelY, analyzedMeter.getWidth(), labelH,
                   juce::Justification::centred);
        g.drawText("ACTION",   actionMeter.getX(),   labelY, actionMeter.getWidth(),   labelH,
                   juce::Justification::centred);
        g.drawText("OUTPUT",   outputMeter.getX(),   labelY, outputMeter.getWidth(),   labelH,
                   juce::Justification::centred);

        // SIDECHAIN LED - geometric precision next to 'ANALYZED'
        juce::GlyphArrangement textGlyphs;
        textGlyphs.addLineOfText(labelFont, "ANALYZED", 0.0f, 0.0f);
        float textWidth = textGlyphs.getBoundingBox(0, -1, true).getWidth();

        juce::GlyphArrangement dGlyphs;
        dGlyphs.addLineOfText(labelFont, "D", 0.0f, 0.0f);
        float dWidth = dGlyphs.getBoundingBox(0, -1, true).getWidth();

        float labelCenterX = static_cast<float>(analyzedMeter.getCentreX());
        float textRightEdge = labelCenterX + textWidth / 2.0f;
        float ledLeftEdge   = textRightEdge + dWidth / 2.0f;
        float ledCenterX    = ledLeftEdge + 4.0f;   // 4px = LED radius
        float ledCenterY    = static_cast<float>(labelY) + static_cast<float>(labelH) / 2.0f;

        float ledRadius = 4.0f;
        juce::Rectangle<float> ledRect(ledCenterX - ledRadius, ledCenterY - ledRadius, 8.0f, 8.0f);

        // Flat physical LED — driven by sidechainActive (set in timerCallback)
        g.setColour(sidechainActive ? juce::Colour(0xFFFF0000) : juce::Colour(0xFF440000));
        g.fillEllipse(ledRect);
    }
}

void PluginEditor::resized()
{
    // Define the three meter rectangles with exact dimensions and positions
    // Each meter is 160x110 pixels
    analyzedMeter = juce::Rectangle<int>(20, 30, 160, 110);
    actionMeter = juce::Rectangle<int>(220, 30, 160, 110);
    outputMeter = juce::Rectangle<int>(420, 30, 160, 110);

    // Position inspect button at the bottom
    inspectButton.setBounds(getLocalBounds().withSizeKeepingCentre(100, 50));
}

void PluginEditor::timerCallback()
{
    float mainLevel      = processorRef.getMainBusLevel();
    float sidechainLevel = processorRef.getSidechainBusLevel();
    float gainDb         = processorRef.getCurrentGainDb();

    // Convert linear levels to dB (-90 floor)
    float targetAnalyzed = juce::Decibels::gainToDecibels(sidechainLevel, -90.0f);
    float targetOutput   = juce::Decibels::gainToDecibels(mainLevel,      -90.0f);
    float targetAction   = gainDb;

    // Save previous values to detect movement
    float prevAnalyzed = smoothedAnalyzed;
    float prevOutput   = smoothedOutput;
    float prevAction   = smoothedAction;

    // Ballistics: fast attack (0.2), slow release (0.08)
    auto smooth = [](float& current, float target)
    {
        float coeff = (target > current) ? 0.2f : 0.08f;
        current += coeff * (target - current);
    };

    smooth(smoothedAnalyzed, targetAnalyzed);
    smooth(smoothedOutput,   targetOutput);
    smooth(smoothedAction,   targetAction);

    // Sidechain LED state
    bool newSidechain = (sidechainLevel > 0.0001f);
    bool sidechainChanged = (newSidechain != sidechainActive);
    sidechainActive = newSidechain;

    // Peak LED: fires if either bus clips (> 1.0 = 0 dBFS)
    peakActive  = (mainLevel > 1.0f || sidechainLevel > 1.0f);

    // Action peak: fires if gain correction exceeds ±9 dB
    actionPeak  = (std::abs(gainDb) > 9.0f);

    // Repaint only if values moved significantly
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
    // Fixed coordinates - no transformations
    float boxX = static_cast<float>(bounds.getX());
    float boxY = static_cast<float>(bounds.getY());
    float boxWidth = static_cast<float>(bounds.getWidth());
    float boxHeight = static_cast<float>(bounds.getHeight());

    // Draw parchment background (solid color)
    g.setColour(juce::Colour(0xFFE8D9A1));
    g.fillRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 5.0f);

    // Draw border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(boxX, boxY, boxWidth, boxHeight, 5.0f, 1.5f);

    // Manual pivot point at bottom-center of THIS box
    float pivotX = boxX + (boxWidth * 0.5f);
    float pivotY = boxY + boxHeight;

    // Draw horizontal arc at top of meter
    // Arc bounding box centered on pivot, positioned near top
    float arcWidth = boxWidth * 0.85f;
    float arcHeight = boxHeight * 0.7f;
    float arcX = pivotX - (arcWidth * 0.5f);
    float arcY = boxY + 8.0f;

    // Arc angles: -55° to +55° from vertical (widened scale)
    float startAngleFromVertical = -55.0f;
    float endAngleFromVertical   =  55.0f;

    // Convert from "angle from vertical" to JUCE angles (radians)
    // JUCE: 0 = right, π/2 = down, π = left, 3π/2 = up
    // Vertical (up) = -π/2, so add our angle to that
    float startAngle = juce::degreesToRadians(-90.0f + startAngleFromVertical);
    float endAngle = juce::degreesToRadians(-90.0f + endAngleFromVertical);

    // Draw arc using Path
    float arcCenterX = arcX + (arcWidth * 0.5f);
    float arcCenterY = arcY + (arcHeight * 0.5f);
    float radiusX = arcWidth * 0.5f;
    float radiusY = arcHeight * 0.5f;

    juce::Path arcPath;
    arcPath.addCentredArc(arcCenterX, arcCenterY, radiusX, radiusY, 0.0f, startAngle, endAngle, true);

    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.strokePath(arcPath, juce::PathStrokeType(1.5f));

    // Draw tick marks on the arc
    float tickMarks[] = {-60, -20, -10, -7, -5, -3, 0, 3};

    // Angle mapping widened to ±55° (scale factor 55/45 ≈ 1.2222 applied to all values)
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
        // Use custom angle mapping
        float angleFromVertical = dbToAngle(db);
        float angle = juce::degreesToRadians(-90.0f + angleFromVertical);

        // Calculate tick positions on the arc ellipse (using values from above)
        float outerX = arcCenterX + radiusX * std::cos(angle);
        float outerY = arcCenterY + radiusY * std::sin(angle);
        float innerX = arcCenterX + (radiusX - 6.0f) * std::cos(angle);
        float innerY = arcCenterY + (radiusY - 6.0f) * std::sin(angle);

        // Red section from 0dB to +3dB
        if (db >= 0.0f)
            g.setColour(juce::Colours::red);
        else
            g.setColour(juce::Colours::black.withAlpha(0.6f));

        // Thicker marks at 0 and +3
        float thickness = (db == 0 || db == 3) ? 2.0f : 1.2f;
        g.drawLine(innerX, innerY, outerX, outerY, thickness);

        // SCALE NUMBERS - Use EXACT same angle as tick mark
        // STRICT RULE: angle is already calculated above - DO NOT recalculate
        // Reduce radius by 8 pixels from inner tick (tightened gap)
        float textRadiusX = radiusX - 6.0f - 8.0f;  // Account for ellipse X
        float textRadiusY = radiusY - 6.0f - 8.0f;  // Account for ellipse Y

        // Calculate text position at EXACT same angle, reduced radius
        float textX = arcCenterX + textRadiusX * std::cos(angle);
        float textY = arcCenterY + textRadiusY * std::sin(angle);

        // Font consistency - same as Action meter (8pt bold)
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));

        // Color scheme: 0 and 3 in RED, others in dark grey
        if (db == 0.0f || db == 3.0f)
            g.setColour(juce::Colours::red);  // Red for zero and peak
        else
            g.setColour(juce::Colour(0xff333333));  // Dark grey for normal values

        // Create label text: REMOVE signs, display absolute values
        // 20, 10, 7, 5, 3, 0, 3 (no + or -)
        juce::String label = juce::String(static_cast<int>(std::abs(db)));

        // Draw text CENTERED on the point (Justification::centred)
        g.drawText(label, static_cast<int>(textX - 12), static_cast<int>(textY - 8),
                   24, 16, juce::Justification::centred);
    }

    // Labels
    g.setColour(juce::Colours::black);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText("VU", static_cast<int>(boxX + boxWidth * 0.5f - 15),
                     static_cast<int>(boxY + boxHeight * 0.5f),
                     30, 20, juce::Justification::centred);

    g.setColour(juce::Colour(0xff333333));  // Dark grey for crisp branding
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.drawText("J-RIDER", static_cast<int>(boxX + boxWidth - 42),
                          static_cast<int>(boxY + boxHeight - 12),
                          38, 10, juce::Justification::centredRight);

    // Hybrid needle angle: -90→-60 dB sweeps from rest position to the scale edge
    float db = levelDb;
    float angleDegrees;
    if (db <= -90.0f)
        angleDegrees = -70.0f;                                                       // Rest: beyond arc left edge
    else if (db <= -60.0f)
        angleDegrees = juce::jmap(db, -90.0f, -60.0f, -70.0f, -55.0f);             // Swing into visible scale
    else if (db <= -20.0f)
        angleDegrees = juce::jmap(db, -60.0f, -20.0f, -55.0f, -28.70f);
    else if (db <= -10.0f)
        angleDegrees = juce::jmap(db, -20.0f, -10.0f, -28.70f,  -7.17f);
    else if (db <= -7.0f)
        angleDegrees = juce::jmap(db, -10.0f,  -7.0f,  -7.17f,   7.17f);
    else if (db <= -5.0f)
        angleDegrees = juce::jmap(db,  -7.0f,  -5.0f,   7.17f,  16.74f);
    else if (db <= -3.0f)
        angleDegrees = juce::jmap(db,  -5.0f,  -3.0f,  16.74f,  26.30f);
    else if (db <= 0.0f)
        angleDegrees = juce::jmap(db,  -3.0f,   0.0f,  26.30f,  40.65f);
    else if (db <= 3.0f)
        angleDegrees = juce::jmap(db,   0.0f,   3.0f,  40.65f,  55.0f);
    else
        angleDegrees = 55.0f;

    float angleRadians = juce::degreesToRadians(angleDegrees);

    // Calculate needle endpoint using sin/cos
    // 0 degrees = straight up, positive = clockwise
    float needleLength = boxHeight * 0.7f;
    float needleEndX = pivotX + (needleLength * std::sin(angleRadians));
    float needleEndY = pivotY - (needleLength * std::cos(angleRadians));

    // Draw needle with shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(pivotX + 1, pivotY + 1, needleEndX + 1, needleEndY + 1, 2.0f);

    g.setColour(juce::Colours::black);
    g.drawLine(pivotX, pivotY, needleEndX, needleEndY, 2.0f);

    // PEAK LED in top right
    g.setColour(peakActive ? juce::Colours::red : juce::Colour(0xFF440000));
    g.fillEllipse(boxX + boxWidth - 16, boxY + 8, 8, 8);

    // '90' label — absolute bottom-left corner of the meter box
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.setColour(juce::Colour(0xff333333));
    g.drawText("90", static_cast<int>(boxX + 15.0f - 12), static_cast<int>(boxY + boxHeight - 25.0f - 8),
               24, 16, juce::Justification::centred);
}

void PluginEditor::drawActionMeter(juce::Graphics& g, juce::Rectangle<int> bounds, float gainDb)
{
    // Fixed coordinates - no transformations
    float boxX = static_cast<float>(bounds.getX());
    float boxY = static_cast<float>(bounds.getY());
    float meterWidth = static_cast<float>(bounds.getWidth());
    float meterHeight = static_cast<float>(bounds.getHeight());

    // Draw clean parchment background (solid color)
    g.setColour(juce::Colour(0xFFE8D9A1));
    g.fillRoundedRectangle(boxX, boxY, meterWidth, meterHeight, 5.0f);

    // Draw border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(boxX, boxY, meterWidth, meterHeight, 5.0f, 1.5f);

    // VERTICAL SPACE MANAGEMENT
    // Pivot at top-center
    float pivotX = boxX + (meterWidth * 0.5f);
    float pivotY = boxY + 0.0f;

    // Needle length: 80% of box height (leaves room for text)
    float needleLength = meterHeight * 0.8f;

    // Tick mark radius: Same as needle length (touches needle tip)
    float tickRadius = needleLength;

    // Text radius: 92% of box height (below ticks, inside bottom edge)
    float textRadius = meterHeight * 0.92f;

    // HORIZONTAL SPREAD - limited to ±0.7 radians (≈40°) for safe fit
    const float maxAngleRadians = 0.7f;
    const float maxAngleDegrees = juce::radiansToDegrees(maxAngleRadians);

    // Draw symmetrical tick marks: -9, -6, -3, 0, +3, +6, +9 dB
    float tickMarks[] = {-9, -6, -3, 0, 3, 6, 9};

    for (float db : tickMarks)
    {
        // Map dB to angle: -9→LEFT, 0→centre, +9→RIGHT (matches pin positions)
        float angleFromVertical = juce::jmap(db, -9.0f, 9.0f, maxAngleDegrees, -maxAngleDegrees);
        float angle = juce::degreesToRadians(90.0f + angleFromVertical);

        // Calculate TICK positions at needle tip radius
        float tickOuterX = pivotX + tickRadius * std::cos(angle);
        float tickOuterY = pivotY + tickRadius * std::sin(angle);
        float tickInnerX = pivotX + (tickRadius - 6.0f) * std::cos(angle);
        float tickInnerY = pivotY + (tickRadius - 6.0f) * std::sin(angle);

        // Color: red for positive, black for negative, thicker at 0
        if (db > 0.0f)
            g.setColour(juce::Colours::red);
        else if (db < 0.0f)
            g.setColour(juce::Colours::black.withAlpha(0.6f));
        else
            g.setColour(juce::Colours::black.withAlpha(0.8f));

        float thickness = (db == 0) ? 2.0f : 1.2f;
        g.drawLine(tickInnerX, tickInnerY, tickOuterX, tickOuterY, thickness);

        // Calculate TEXT position at 92% radius (below ticks)
        float textX = pivotX + textRadius * std::cos(angle);
        float textY = pivotY + textRadius * std::sin(angle);

        // Draw dB labels - CENTERED on coordinate
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        juce::String label = (db > 0) ? ("+" + juce::String(static_cast<int>(db)))
                                       : juce::String(static_cast<int>(db));

        // Center text on the coordinate (prevents overhang)
        g.drawText(label, static_cast<int>(textX - 15), static_cast<int>(textY - 8),
                   30, 16, juce::Justification::centred);
    }

    // NEEDLE LOGIC - rotates to match ±0.7 radian mapping
    // Pivot already defined above (pivotX, pivotY)
    // needleLength already defined above (80% of height)

    // Pinned needle: snaps to extremes outside ±9 dB, smooth inside
    float needleAngleFromVertical;
    if (gainDb <= -9.0f)
        needleAngleFromVertical =  maxAngleDegrees;   // Pin hard LEFT  (130°)
    else if (gainDb >= 9.0f)
        needleAngleFromVertical = -maxAngleDegrees;   // Pin hard RIGHT (50°)
    else
        needleAngleFromVertical = juce::jmap(gainDb, -9.0f, 9.0f, maxAngleDegrees, -maxAngleDegrees);

    float needleAngle = juce::degreesToRadians(90.0f + needleAngleFromVertical);

    // Calculate needle tip position
    float needleTipX = pivotX + needleLength * std::cos(needleAngle);
    float needleTipY = pivotY + needleLength * std::sin(needleAngle);

    // Draw needle with shadow
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(pivotX + 1, pivotY + 1, needleTipX + 1, needleTipY + 1, 2.0f);

    g.setColour(juce::Colours::black);
    g.drawLine(pivotX, pivotY, needleTipX, needleTipY, 2.0f);

    // Keep J-RIDER branding in bottom right
    g.setColour(juce::Colour(0xff333333));  // Dark grey for crisp branding
    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.drawText("J-RIDER", static_cast<int>(boxX + meterWidth - 42),
                          static_cast<int>(boxY + meterHeight - 12),
                          38, 10, juce::Justification::centredRight);

    // PEAK LED in top right (driven by actionPeak from timerCallback)
    g.setColour(actionPeak ? juce::Colours::red : juce::Colour(0xFF440000));
    g.fillEllipse(boxX + meterWidth - 16, boxY + 8, 8, 8);
}

void PluginEditor::drawMeterArc(juce::Graphics& g, juce::Point<float> arcCenter, float arcRadius, juce::Rectangle<float> box)
{
    // AXIS CORRECTION: Arc sweeps from -0.7 to +0.7 radians (horizontal left to right)
    float startAngle = -juce::MathConstants<float>::halfPi - 0.7f;  // Left side
    float endAngle = -juce::MathConstants<float>::halfPi + 0.7f;    // Right side

    // Draw main arc path (horizontal curve at top of box)
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    juce::Path arcPath;
    arcPath.addCentredArc(arcCenter.x, arcCenter.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.strokePath(arcPath, juce::PathStrokeType(1.5f));

    // Draw tick marks at dB values on the arc
    float tickMarks[] = {-20, -10, -7, -5, -3, 0, 3};
    for (float db : tickMarks)
    {
        // Map dB to angle: -20dB → -0.7 rad, 0dB → 0.0 rad, +3dB → +0.7 rad
        float angleFromVertical = juce::jmap(db, -20.0f, 3.0f, -0.7f, 0.7f);

        // Convert to JUCE coordinate system (straight up = -π/2)
        float angle = -juce::MathConstants<float>::halfPi + angleFromVertical;

        // Calculate tick mark positions on the arc
        auto outerPoint = arcCenter.getPointOnCircumference(arcRadius, angle);
        auto innerPoint = arcCenter.getPointOnCircumference(arcRadius - 6.0f, angle);

        // Red section from 0dB to +3dB
        if (db >= 0.0f)
            g.setColour(juce::Colours::red);
        else
            g.setColour(juce::Colours::black.withAlpha(0.6f));

        // Thicker marks at 0 and +3
        float thickness = (db == 0 || db == 3) ? 2.0f : 1.2f;
        g.drawLine(innerPoint.x, innerPoint.y, outerPoint.x, outerPoint.y, thickness);
    }
}

void PluginEditor::drawPeakLED(juce::Graphics& g, float x, float y)
{
    juce::Rectangle<float> ledRect(x - 4, y - 4, 8, 8);

    if (peakActive)
    {
        // Glow effect
        g.setColour(juce::Colours::red.withAlpha(0.3f));
        g.fillEllipse(ledRect.expanded(3.0f));
        g.setColour(juce::Colours::red.withAlpha(0.5f));
        g.fillEllipse(ledRect.expanded(1.5f));

        // Bright red LED
        g.setColour(juce::Colours::red);
    }
    else
    {
        // Dim dark red
        g.setColour(juce::Colour(0xFF440000));
    }

    g.fillEllipse(ledRect);
}
