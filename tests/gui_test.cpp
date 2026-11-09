// The GUI module, painted with no window and no display: every control draws
// something, the paint cache keeps two looks, and the grain is released with
// the last look and feel.

#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

#include <dm_gui/dm_gui.h>

namespace {

int failures = 0;
int checks = 0;

/// Counts a check, and prints it when it fails.
void check(bool ok, const std::string &what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("  FAIL  %s\n", what.c_str());
  }
}

void section(const char *name) { std::printf("%s\n", name); }

/// How many pixels a component paints that are not fully transparent, drawn
/// into an image of its own size.
int paintedPixels(juce::Component &c) {
  juce::Image image(juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
  {
    juce::Graphics g(image);
    c.paintEntireComponent(g, false);
  }

  int count = 0;
  const juce::Image::BitmapData data(image, juce::Image::BitmapData::readOnly);
  for (int y = 0; y < image.getHeight(); ++y)
    for (int x = 0; x < image.getWidth(); ++x)
      if (data.getPixelColour(x, y).getAlpha() > 0)
        ++count;

  return count;
}

void testControlsPaint() {
  section("Every control paints");

  dm::ui::LookAndFeel lnf;

  const auto paints = [&](juce::Component &c, int w, int h, const char *name) {
    c.setLookAndFeel(&lnf);
    c.setBounds(0, 0, w, h);
    check(paintedPixels(c) > w * h / 20, std::string(name) + " paints");
    c.setLookAndFeel(nullptr);
  };

  dm::ui::Slider knob;
  knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  knob.setValue(0.4);
  paints(knob, 40, 40, "a knob");

  dm::ui::Slider fader;
  fader.setSliderStyle(juce::Slider::LinearVertical);
  fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  paints(fader, 24, 120, "a fader");

  dm::ui::LabelledKnob labelled("DRIVE");
  paints(labelled, 48, 60, "a labelled knob");

  dm::ui::GlowButton button;
  button.setButtonText("LOOK");
  button.setClickingTogglesState(true);
  button.setToggleState(true, juce::dontSendNotification);
  paints(button, 48, 22, "a lit lamp button");

  dm::ui::ScreenSwitch screen;
  screen.setButtonText("D");
  paints(screen, 22, 22, "a screen switch");

  dm::ui::SegmentDisplay display("dB");
  display.setReading("-12.5", true);
  paints(display, 80, 22, "a segment readout");

  dm::ui::LevelMeter meter(dm::ui::colours::accent);
  meter.setBounds(0, 0, 12, 160);
  meter.push(0.5f);
  paints(meter, 12, 160, "a level meter");

  dm::ui::ActivityLamp lamp(dm::ui::colours::accent);
  lamp.push(1.0f);
  paints(lamp, 14, 14, "an activity lamp");

  dm::ui::StereoOutputMeter output;
  output.setBounds(0, 0, 200, 16);
  output.push(0.5f, 0.25f);
  paints(output, 200, 16, "the stereo meter");

  check(dm::ui::logoMakersMark() != nullptr, "the maker's mark parses");
}

void testPaintCache() {
  section("The paint cache keeps two looks");

  juce::Component c;
  c.setBounds(0, 0, 10, 10);

  juce::Image image(juce::Image::ARGB, 10, 10, true);
  juce::Graphics g(image);

  dm::ui::PaintCache cache;
  int draws = 0;
  const auto draw = [&](juce::Graphics &) { ++draws; };

  const auto lit = cache.paint(g, c, dm::ui::LookKey().add(true), draw);
  const auto unlit = cache.paint(g, c, dm::ui::LookKey().add(false), draw);
  check(draws == 2, "two new looks are each drawn once");
  check(lit != unlit, "the two looks are kept apart");

  check(cache.paint(g, c, dm::ui::LookKey().add(true), draw) == lit &&
            cache.paint(g, c, dm::ui::LookKey().add(false), draw) == unlit &&
            draws == 2,
        "switching between them draws neither again");

  cache.paint(g, c, dm::ui::LookKey().add(3), draw);
  cache.paint(g, c, dm::ui::LookKey().add(true), draw);
  check(draws == 4, "a third look puts out the oldest");
}

void testGrainGoesWithTheLookAndFeel() {
  section("The grain goes with the last look and feel");

  const auto before = dm::ui::grainTileBuildCount();
  {
    dm::ui::LookAndFeel lnf;
    dm::ui::grainTile();
    dm::ui::grainTile();
  }
  check(dm::ui::grainTileBuildCount() == before + 1,
        "one tile is built while a look and feel holds it");

  {
    dm::ui::LookAndFeel lnf;
    dm::ui::grainTile();
  }
  check(dm::ui::grainTileBuildCount() == before + 2,
        "it is built again once the last one has gone");
}

/// The colour of one pixel of a component drawn on its own.
juce::Colour pixelAt(juce::Component &c, int x, int y) {
  juce::Image image(juce::Image::ARGB, c.getWidth(), c.getHeight(), true);
  {
    juce::Graphics g(image);
    c.paintEntireComponent(g, false);
  }
  return image.getPixelAt(x, y);
}

void testLooksAPluginCanChange() {
  section("Looks a plugin can change");

  dm::ui::LookAndFeel lnf;
  const juce::Colour red{0xffff3020};

  dm::ui::SegmentDisplay display("");
  display.setLookAndFeel(&lnf);
  display.setBounds(0, 0, 60, 24);
  display.setReading("888", true);
  const auto cyan = pixelAt(display, 30, 12);
  display.setLampColour(red);
  check(display.getLampColour() == red, "a readout keeps its lamp colour");
  const auto lit = pixelAt(display, 30, 12);
  check(lit.getRed() > lit.getBlue() && cyan.getBlue() > cyan.getRed(),
        "and lights in it rather than the accent");
  display.setLookAndFeel(nullptr);

  // A full knob in green, looked at just inside its edge where the ring
  // starts. Lit green with the ring, not green without it.
  const auto ringAt = [&](bool ringless) {
    dm::ui::Slider knob;
    knob.setLookAndFeel(&lnf);
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::lime);
    knob.getProperties().set("ringless", ringless);
    knob.setValue(knob.getMaximum());
    knob.setBounds(0, 0, 80, 80);

    const auto angle = knob.getRotaryParameters().startAngleRadians;
    const auto r = 39.0f * 0.94f;
    const auto pixel =
        pixelAt(knob, juce::roundToInt(40.0f + r * std::sin(angle)),
                juce::roundToInt(40.0f - r * std::cos(angle)));
    knob.setLookAndFeel(nullptr);
    return pixel;
  };

  const auto ringed = ringAt(false);
  const auto bare = ringAt(true);
  check(ringed.getGreen() > 150 && ringed.getRed() < 100,
        "a knob's ring lights in its colour");
  check(bare.getGreen() < 100, "a ringless knob has no ring");

  dm::ui::StereoOutputMeter meter;
  check(meter.lampColourAt(0.5f) == dm::ui::colours::accent &&
            meter.lampColourAt(1.0f) == dm::ui::colours::muteOn,
        "the stereo meter keeps its colours unless told");

  meter.setLampColours(red, juce::Colours::yellow, juce::Colours::white);
  check(meter.lampColourAt(0.5f) == red &&
            meter.lampColourAt(1.0f) == juce::Colours::white,
        "and takes the ones it is given");

  meter.setVertical(true);
  meter.setBounds(0, 0, 20, 160);
  check(meter.barBounds() == juce::Rectangle<int>(4, 0, 16, 160),
        "standing, its scale runs down the left");

  // Half of full scale lights most of the way up, not to the top.
  for (int i = 0; i < 4; ++i)
    meter.push(0.5f, 0.5f);

  const auto bottom = pixelAt(meter, 8, 154);
  const auto top = pixelAt(meter, 8, 5);
  check(bottom.getRed() > 200 && bottom.getGreen() < 120,
        "it lights from the bottom");
  check(top.getBrightness() < 0.4f, "and not to the top");
}

void testHelpers() {
  section("Helpers");

  check(dm::ui::SegmentDisplay::squeeze("Cassette", 4) == "CSST",
        "a name is squeezed by its spaces and vowels, then cut");
  check(dm::ui::SegmentDisplay::squeeze("Off", 4) == "OFF ",
        "a short name is padded to the cells");
  check(dm::ui::meterSegments(240) == 16, "a meter 240 high has 16 lamps");
}

} // namespace

int main() {
  const juce::ScopedJuceInitialiser_GUI juce;

  testControlsPaint();
  testPaintCache();
  testGrainGoesWithTheLookAndFeel();
  testLooksAPluginCanChange();
  testHelpers();

  std::printf("\n%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
