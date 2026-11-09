#pragma once

#include <array>
#include <functional>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PaintCache.h"
#include "Theme.h"

namespace dm::ui {

/// A Slider drawn from a cached picture, which knows whether the user is
/// holding it.
///
/// Knowing that tells a change from a drag apart from one from automation or a
/// preset load, so a plugin can pass on what the user did without passing on
/// its own changes and feeding back.
class Slider : public juce::Slider {
public:
  /// A right-click is left to the parent, for a menu. The slider would start a
  /// drag it never ends, since the mouse-up goes to the menu.
  void mouseDown(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu())
      return;

    // A drawn drag never becomes a slider drag, or this fader would move twice.
    if (onDrawStart != nullptr && onDrawStart(e)) {
      drawing = true;
      return;
    }

    juce::Slider::mouseDown(e);
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (e.mods.isPopupMenu())
      return;

    if (drawing) {
      if (onDrawMove != nullptr)
        onDrawMove(e);

      return;
    }

    juce::Slider::mouseDrag(e);
  }

  void mouseUp(const juce::MouseEvent &e) override {
    if (drawing) {
      drawing = false;

      if (onDrawEnd != nullptr)
        onDrawEnd();

      return;
    }

    juce::Slider::mouseUp(e);
  }

  /// Offered every drag before the slider takes it. Returning true means the
  /// gesture belongs to something else, which then gets the pointer until the
  /// button comes up. Only the faders are given these.
  std::function<bool(const juce::MouseEvent &)> onDrawStart;
  std::function<void(const juce::MouseEvent &)> onDrawMove;
  std::function<void()> onDrawEnd;

  void startedDragging() override {
    dragging = true;
    if (onUserDragStart != nullptr)
      onUserDragStart();
  }

  void stoppedDragging() override {
    dragging = false;
    if (onUserDragEnd != nullptr)
      onUserDragEnd();
  }

  bool isUserDragging() const noexcept { return dragging; }

  std::function<void()> onUserDragStart, onUserDragEnd;

  /// Drawn from a cached picture when nothing that decides its look has
  /// changed. See paint for what counts as a change.
  void paint(juce::Graphics &) override;

private:
  bool dragging = false;
  bool drawing = false;

  PaintCache looks;
};

/// A knob with a caption underneath.
///
/// For a control that stands on its own, rather than in a row whose captions
/// are drawn elsewhere.
class LabelledKnob : public juce::Component {
public:
  explicit LabelledKnob(juce::String captionText,
                        juce::Colour fill = colours::accent);

  void paint(juce::Graphics &) override;
  void resized() override;

  /// Where the dial sits inside bounds of this size.
  ///
  /// Public so a panel can line buttons up with the dials rather than with the
  /// middle of the row.
  static juce::Rectangle<int> dialBounds(juce::Rectangle<int>);

  /// Where the caption under the dial sits, so a group name can share its line.
  static juce::Rectangle<int> captionBounds(juce::Rectangle<int>);

  /// Changes the caption only, leaving the accessible name to the owner.
  void setCaption(const juce::String &);
  const juce::String &getCaption() const noexcept { return caption; }

  Slider slider;

private:
  juce::String caption;
};

/// A small segment readout with a fixed number of cells.
///
/// For figures and short names. With an onClick it becomes a control that
/// opens a menu and shows a hover outline.
class SegmentDisplay : public juce::Component,
                       public juce::SettableTooltipClient {
public:
  /// How many bars a cell has: seven for digits and a few letters, fourteen
  /// for the whole alphabet.
  enum class Bars { Seven, Fourteen };

  /// The most cells a display can have.
  static constexpr int kMaxCells = 16;

  /// @param unit   drawn small beside the digits, or empty for none.
  /// @param cells  how many cells the display has. Fixed, so the figures are
  ///               the same size whatever they say.
  explicit SegmentDisplay(juce::String unit, Bars bars = Bars::Seven,
                          int cells = 4);

  /// @param digits  0 to 9, a minus, which takes a cell, a decimal point,
  ///                 which lights the point of the cell before it, and the
  ///                 letters in segmentsFor. A fourteen-bar display also takes
  ///                 the whole alphabet, upper-cased. Anything else is blank.
  /// @param active  false leaves every bar unlit.
  void setReading(const juce::String &digits, bool active);

  /// Gives the display a fixed sharp sign beside its cells, lit when the
  /// reading has a # in it.
  void showSharpSign(bool shouldShow);

  /// What the bars light in, and what tints the glass. Accent unless told
  /// otherwise.
  void setLampColour(juce::Colour);
  juce::Colour getLampColour() const noexcept { return lamp; }

  /// How wide this display has to be at @p height for every cell to stand at
  /// full width, with room for the sharp sign and the unit if it has them.
  int naturalWidth(int height) const;

  /// How many bars its cells have. Read back by the tests.
  Bars howManyBars() const noexcept { return bars; }

  /// Fits a name to a fixed number of cells.
  ///
  /// Upper case, then spaces go from the right, then vowels (not Y), then the
  /// rest is cut. The first character is always kept. Shorter names come back
  /// padded to exactly @p cells characters.
  static juce::String squeeze(const juce::String &, int cells);

  /// How many of the reading's characters the last paint had room for, so
  /// the tests can see a dropped letter.
  int cellsDrawn() const noexcept { return drawn; }

  /// What it is showing, and whether it is lit. Read back by the tests.
  const juce::String &getReading() const noexcept { return reading; }
  bool isActive() const noexcept { return active; }

  /// Whether a display this wide has room to name its unit beside the digits.
  ///
  static bool hasRoomForUnit(int width);

  /// Whether this character has a form to draw. Anything else comes out as an
  /// unlit digit.
  static bool canDraw(char, Bars = Bars::Seven);

  std::function<void()> onClick;

  void paint(juce::Graphics &) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseEnter(const juce::MouseEvent &) override;
  void mouseExit(const juce::MouseEvent &) override;

  /// Announces a display with an onClick as a button, so a screen reader can
  /// name and press it. A plain readout gets no handler of its own.
  std::unique_ptr<juce::AccessibilityHandler>
  createAccessibilityHandler() override;

private:
  /// Everything paint draws, before it is kept as a picture.
  void paintReading(juce::Graphics &);

  /// Draws one character in the classic seven-bar arrangement.
  void paintGlyph(juce::Graphics &, juce::Rectangle<float>, char,
                  juce::Colour on, juce::Colour off) const;

  /// The same, in fourteen.
  void paintStarburst(juce::Graphics &, juce::Rectangle<float>, char,
                      juce::Colour on, juce::Colour off) const;

  const Bars bars;
  const int fixedCells;

  /// Set by paint, read by cellsDrawn.
  mutable int drawn = 0;

  /// Its last two looks, and what cellsDrawn was for each.
  PaintCache looks;
  std::array<int, 2> drawnFor{};

  juce::String reading, unitText;
  juce::Colour lamp = colours::accent;
  bool active = false;
  bool hovered = false;
  bool sharpSign = false;
};

/// A slim vertical level meter.
///
/// Segmented, so it only repaints when the level crosses a segment boundary.
/// Unlit segments keep the meter's colour at low alpha.
class LevelMeter : public juce::Component {
public:
  explicit LevelMeter(juce::Colour barColour) : colour(barColour) {
    setInterceptsMouseClicks(false, false);
  }

  /// The channel background behind this meter, at its top and bottom edges.
  ///
  /// Given these, the meter paints its own backdrop and declares itself
  /// opaque, so the channel underneath is not redrawn when a lamp changes.
  void setBackdrop(juce::Colour top, juce::Colour bottom);

  /// @param level  linear amplitude from the audio thread, 0 to 1.
  /// @returns the region it asked to have repainted, empty when nothing moved.
  ///
  juce::Rectangle<int> push(float level);

  void paint(juce::Graphics &) override;

private:
  int segments() const;

  juce::Colour colour;
  juce::Colour backdropTop, backdropBottom;
  float displayed = 0.0f;

  /// How many lamps are lit. Kept rather than derived so it can hold still
  /// while the level wobbles across a boundary.
  int lit = 0;
};

/// A round lamp that shows how hard something is working, such as an
/// envelope's position or how far a compressor has pulled the level down.
/// Brightness is quantised so it only repaints when it moves a step.
class ActivityLamp : public juce::Component {
public:
  explicit ActivityLamp(juce::Colour litColour) : colour(litColour) {
    setInterceptsMouseClicks(false, false);
  }

  /// The channel background behind the lamp, so it can paint its own backdrop
  /// and declare itself opaque. See LevelMeter::setBackdrop.
  void setBackdrop(juce::Colour);

  /// @param brightness  0 to 1, how hard the group is working.
  /// @returns true when the lamp moved a step and needs repainting.
  bool push(float brightness);

  void paint(juce::Graphics &) override;

  /// How many steps the brightness is rounded to. Enough to read as
  /// continuous, few enough that a slow move is mostly free.
  static constexpr int kSteps = 12;

private:
  juce::Colour colour, backdrop;
  int step = 0;
};

/// The stereo output meter: two segmented bars with a peak hold, lying down
/// unless stood up. It only repaints when a lamp changes.
class StereoOutputMeter : public juce::Component {
public:
  StereoOutputMeter() { setInterceptsMouseClicks(false, false); }

  /// The strip along the bottom, or down the left side when standing, that
  /// carries the decibel marks rather than any lamps.
  static constexpr int kScaleHeight = 4;

  /// Where the two bars are, beside the scale. A fader can lie over this.
  juce::Rectangle<int> barBounds() const {
    return vertical ? getLocalBounds().withTrimmedLeft(kScaleHeight)
                    : getLocalBounds().withTrimmedBottom(kScaleHeight);
  }

  /// Stands the meter up: the bars side by side, left then right, rising from
  /// the bottom.
  void setVertical(bool shouldStand);
  bool isVertical() const noexcept { return vertical; }

  /// The lamps' colours: @p low up to -4 dB or so, turning to @p high, then to
  /// @p top in the last lamps before full scale. The accent, the solo yellow
  /// and the mute red unless told otherwise.
  void setLampColours(juce::Colour low, juce::Colour high, juce::Colour top);

  /// The colour of the lamp standing at this fraction of full scale.
  juce::Colour lampColourAt(float fraction) const;

  /// @param l,r  linear peaks from the audio thread.
  void push(float l, float r);

  void paint(juce::Graphics &) override;

private:
  /// How many lamps fit across the meter.
  int segments() const;

  struct Bar {
    float displayed = 0.0f;
    float peak = 0.0f;
    int hold = 0;

    /// Lamps alight, and the one holding the peak, or -1 when there is none.
    /// Kept rather than derived so they hold still at a boundary.
    int lit = 0;
    int peakLamp = -1;

    /// @returns true when a lamp changed and a repaint is needed.
    bool advance(float level, int count);
  };

  void paintBar(juce::Graphics &, juce::Rectangle<float>, const Bar &,
                int count) const;

  Bar left, right;

  bool vertical = false;
  juce::Colour lowColour = colours::accent;
  juce::Colour highColour = colours::soloOn;
  juce::Colour topColour = colours::muteOn;
};

} // namespace dm::ui
