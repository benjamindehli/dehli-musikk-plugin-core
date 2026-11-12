#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PaintCache.h"

namespace dm::ui {

/// How many segments a meter of this height divides into, about sixteen.
///
/// Shared because the meter and the fader drawn over it both paint segments and
/// must agree to the pixel.
int meterSegments(int height);

/// How far the bloom around a lit meter run reaches beyond it, in pixels.
///
/// The paint and the dirty band the meter asks for must agree on it, or the
/// old level leaves a smear.
inline constexpr float kMeterGlowSpread = 3.0f;

/// The track a channel's meter lights in, within the meter's whole bounds:
/// 62 % of the width, centred, and short of each end by the bloom's reach, so
/// the bloom rounds off inside the meter rather than being cut square by its
/// edge. That leaves room for the lip of the recess too. Shared for the same
/// reason as meterSegments.
juce::Rectangle<float> meterTrack(juce::Rectangle<float> whole);

/// Lamp i of a meter of count lamps, counted from the bottom of the track.
///
/// The lamps fill the track a pixel in from its edges on every side, with a
/// gap between neighbours, so there is the same pixel at both ends as along
/// the sides. Shared for the same reason as meterSegments.
juce::Rectangle<float> meterLamp(juce::Rectangle<float> track, int count,
                                 int i);

/// The corner of a meter's recess and groove: a lamp's corner plus the pixel
/// between the lamps and the groove's edge, so the two curves run together.
inline constexpr float kMeterRecessCorner = 3.0f;

namespace colours {
inline const juce::Colour background{0xff0b0d10};
inline const juce::Colour panel{0xff14181d};
inline const juce::Colour panelAlt{0xff15191e};

/// What every channel stands on.
inline const juce::Colour channel{0xff0d0e12};
inline const juce::Colour groove{0xff090b0e};
inline const juce::Colour outline{0xff222830};
inline const juce::Colour text{0xffd9dfe7};
inline const juce::Colour textDim{0xff77828d};
/// The global controls, in a cyan the channel colours never reach.
inline const juce::Colour accent{0xff62bbd9};
/// The lamp behind a lit mute button.
inline const juce::Colour muteOn{0xffee563f};
/// The lamp behind a lit solo button.
inline const juce::Colour soloOn{0xffe8c34a};

/// A control waiting for MIDI Learn, in a violet nothing else uses.
inline const juce::Colour learning{0xffb07bd4};
} // namespace colours

/// A tile of fine monochrome noise laid over the panels, built once and shared.
/// Deterministic, so screenshots are repeatable.
///
/// Held through a SharedResourcePointer, never a static: releasing a Direct2D
/// backed image under the Windows loader lock at unload hangs the host. The
/// last LookAndFeel holds it, so it dies with the editor.
struct GrainTile {
  GrainTile();
  juce::Image image;
};

/// How many times the tile has been built, so a test can check it is released
/// with the editor rather than kept for the life of the process.
int grainTileBuildCount();

/// The grain tile. Returned by value, since a reference would dangle when no
/// look and feel is alive to hold the shared tile.
juce::Image grainTile();

/// Lays the grain over an area at the weight the panels use.
void paintGrain(juce::Graphics &, juce::Rectangle<int> area);

/// A switch drawn as a white plastic cap with a lamp behind it.
///
/// textColourOnId is the lamp colour, shown only when the button is on.
/// textColourOffId is not used. See LookAndFeel::lampFace.
class GlowButton : public juce::TextButton {
public:
  /// Drawn in place of the word, in the colour the word would have had. The
  /// word is still what a screen reader and the tooltip get.
  std::function<void(juce::Graphics &, juce::Rectangle<float>, juce::Colour)>
      onIcon;

  /// Says that what onIcon draws has changed, and repaints. Needed because the
  /// paint cache cannot compare functions and would show the old icon.
  void iconChanged() {
    ++iconRevision;
    repaint();
  }

  void paintButton(juce::Graphics &, bool highlighted, bool down) override;

private:
  /// Everything paintButton draws, before it is kept as a picture.
  void paintCap(juce::Graphics &, bool highlighted, bool down);

  /// See iconChanged.
  int iconRevision = 0;

  /// Its last two looks, kept as pictures.
  PaintCache looks;
};

/// A switch shown as a letter in a screen. On, the letter lights in its lamp
/// colour, and off it is a faint print on the dark glass.
class ScreenSwitch : public juce::Button {
public:
  ScreenSwitch() : juce::Button({}) {}

  /// What the letter lights in. Accent unless told otherwise.
  juce::Colour lamp = colours::accent;

  /// Drawn in place of the letter, in the colour the letter would have had.
  std::function<void(juce::Graphics &, juce::Rectangle<float>, juce::Colour)>
      onIcon;

  /// How far along a bar under the letters is lit, nought to one, for a lamp
  /// that says how much as well as whether. None while negative, and only
  /// drawn while the lamp is on. Repaints on a change.
  void setLevel(float newLevel);
  float getLevel() const noexcept { return level; }

  void paintButton(juce::Graphics &, bool highlighted, bool down) override;

private:
  float level = -1.0f;
};

} // namespace dm::ui
