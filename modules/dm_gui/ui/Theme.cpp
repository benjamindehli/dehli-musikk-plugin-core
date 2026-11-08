#include "Theme.h"

#include <cmath>

#include "LookAndFeel.h"

namespace dm::ui {

namespace {
/// Bit-for-bit equality, written as two comparisons so the float-equality
/// warning can stay on.
bool exactly(float a, float b) noexcept { return !(a < b) && !(b < a); }
} // namespace

int meterSegments(int height) { return juce::jlimit(6, 24, height / 15); }

juce::Rectangle<float> meterTrack(juce::Rectangle<float> whole) {
  const auto width = juce::jmax(6.0f, whole.getWidth() * 0.62f);
  return whole.withSizeKeepingCentre(width, whole.getHeight())
      .reduced(0.0f, kMeterGlowSpread);
}

juce::Rectangle<float> meterLamp(juce::Rectangle<float> track, int count,
                                 int i) {
  const auto step = track.getHeight() / (float)count;
  const auto gap = juce::jlimit(1.0f, 3.0f, step * 0.18f);

  const auto inner = track.reduced(1.0f);
  const auto lampH =
      (inner.getHeight() - gap * (float)(count - 1)) / (float)count;
  const auto bottom = inner.getBottom() - (float)i * (lampH + gap);

  return {inner.getX(), bottom - lampH, inner.getWidth(), lampH};
}

void ScreenSwitch::setLevel(float newLevel) {
  newLevel = newLevel < 0.0f ? -1.0f : juce::jlimit(0.0f, 1.0f, newLevel);

  if (exactly(newLevel, level))
    return;

  level = newLevel;
  repaint();
}

void ScreenSwitch::paintButton(juce::Graphics &g, bool highlighted, bool) {
  // Two pixels in from the bounds, so a run of them leaves panel between each.
  const auto area = getLocalBounds().toFloat().reduced(2.0f);

  paintRecess(g, area, 2.5f);
  paintDisplayGround(g, area, 2.5f, highlighted);

  const bool on = getToggleState();
  const auto ink = on ? lamp : colours::textDim.withAlpha(0.55f);

  if (onIcon != nullptr) {
    const auto glyph = area.reduced(2.5f);

    // The same bloom the letters get, as a wider faint pass under the icon.
    if (on)
      onIcon(g, glyph.translated(0.0f, 0.5f), lamp.withAlpha(0.35f));

    onIcon(g, glyph, ink);
    return;
  }

  // A thin bar along the foot of the glass, lit as far as the level, so a
  // lamp can say how much. Clear of the letters, which keep the middle.
  if (on && level >= 0.0f) {
    const auto track =
        area.reduced(4.0f, 0.0f).removeFromBottom(4.0f).withHeight(1.5f);
    g.setColour(lamp.withAlpha(0.18f));
    g.fillRect(track);
    g.setColour(lamp);
    g.fillRect(track.withWidth(track.getWidth() * level));
  }

  g.setFont(makeFont(10.0f, true));

  // A half-pixel echo under the lit letter, its bloom.
  if (on) {
    g.setColour(lamp.withAlpha(0.35f));
    g.drawText(getButtonText(), area.translated(0.0f, 0.5f),
               juce::Justification::centred, false);
  }

  // Unlit, faint but still readable.
  g.setColour(ink);
  g.drawText(getButtonText(), area, juce::Justification::centred, false);
}

void GlowButton::paintButton(juce::Graphics &g, bool highlighted, bool down) {
  // The icon is a function and cannot be compared, so the key holds a count
  // of its changes instead. See iconChanged.
  LookKey key;
  key.add(getButtonText())
      .add(getToggleState())
      .add(highlighted)
      .add(down)
      .add(isEnabled())
      .add(onIcon != nullptr)
      .add(iconRevision)
      .add(findColour(juce::TextButton::buttonColourId))
      .add(findColour(juce::TextButton::textColourOnId))
      .add(getProperties());

  looks.paint(g, *this, key,
              [&](juce::Graphics &into) { paintCap(into, highlighted, down); });
}

void GlowButton::paintCap(juce::Graphics &g, bool highlighted, bool down) {
  // The cap colour, for the few buttons that set one.
  const auto fill = findColour(juce::TextButton::buttonColourId);

  if (auto *laf = dynamic_cast<LookAndFeel *>(&getLookAndFeel()))
    laf->drawButtonFace(g, *this, getToggleState(), fill, highlighted, down);
  else
    getLookAndFeel().drawButtonBackground(g, *this, fill, highlighted, down);

  // The same ink whatever the lamp is doing.
  const auto colour = LookAndFeel::lampLegend();

  const auto h = (float)getHeight();

  // The cap rather than the whole button, which includes the bezel.
  const auto cap = LookAndFeel::lampCapBounds(getLocalBounds().toFloat(),
                                              LookAndFeel::lampGangOf(*this));

  // An icon takes a flat opaque ink, mixed against the plastic here. Icons are
  // overlapping shapes, and a translucent colour would show the overlaps. The
  // plastic is the lamp the face lights, not the cap colour asked for.
  if (onIcon != nullptr) {
    const auto plastic = LookAndFeel::lampFace(
        LookAndFeel::lampColourOf(*this, fill), getToggleState());

    onIcon(g, cap,
           colour.interpolatedWith(plastic, getToggleState() ? 0.14f : 0.06f));
    return;
  }

  const auto font = makeFont(juce::jlimit(8.0f, 13.0f, h * 0.58f), true);

  // Centred on the cap, since a ganged cap is not centred in its button, but
  // as wide as the button, since the longest words would not fit the cap.
  const auto line = juce::Rectangle<float>((float)getWidth(), h)
                        .withCentre({cap.getCentreX(), h * 0.5f});

  juce::GlyphArrangement glyphs;
  glyphs.addFittedText(font, getButtonText(), line.getX(), line.getY(),
                       line.getWidth(), line.getHeight(),
                       juce::Justification::centred, 1, 1.0f);

  // Thinnest over the middle where the lamp shows through. See
  // LookAndFeel::legendInk.
  g.setGradientFill(LookAndFeel::legendInk(colour, cap, getToggleState()));

  glyphs.draw(g);
}

} // namespace dm::ui
