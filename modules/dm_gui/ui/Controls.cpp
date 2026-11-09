#include "Controls.h"

#include <cmath>

#include "LookAndFeel.h"

namespace dm::ui {

void Slider::paint(juce::Graphics &g) {
  // Everything the look and feel reads to draw this control: its value,
  // whether it is enabled, its colours, and every property, so a property
  // added later cannot leave a stale picture behind.
  LookKey key;
  key.add(getValue()).add(isEnabled()).add(getProperties());

  for (const auto id :
       {juce::Slider::rotarySliderFillColourId,
        juce::Slider::rotarySliderOutlineColourId, juce::Slider::trackColourId,
        juce::Slider::thumbColourId, juce::Slider::backgroundColourId})
    key.add(findColour(id));

  looks.paint(g, *this, key,
              [this](juce::Graphics &into) { juce::Slider::paint(into); });
}

LabelledKnob::LabelledKnob(juce::String captionText, juce::Colour fill)
    : caption(std::move(captionText)) {
  // The caption is the spoken name too, unless the owner sets another. Read
  // from the member, since captionText has been moved from.
  slider.setTitle(caption);

  slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  slider.setColour(juce::Slider::rotarySliderFillColourId, fill);
  addAndMakeVisible(slider);
}

namespace {
// Top to bottom through a labelled knob: room above the dial, the dial, the
// gap, the caption, room under it.
constexpr int kAboveDial = 3;
constexpr int kCaptionGap = 1;
constexpr int kCaptionHeight = 11;
constexpr int kBelowCaption = 1;
} // namespace

void LabelledKnob::paint(juce::Graphics &g) {
  g.setColour(colours::textDim);
  g.setFont(makeFont(9.0f, true));
  g.drawText(caption, captionBounds(getLocalBounds()),
             juce::Justification::centred, false);
}

juce::Rectangle<int> LabelledKnob::captionBounds(juce::Rectangle<int> bounds) {
  bounds.removeFromBottom(kBelowCaption);
  return bounds.removeFromBottom(kCaptionHeight);
}

juce::Rectangle<int> LabelledKnob::dialBounds(juce::Rectangle<int> bounds) {
  bounds.removeFromBottom(kBelowCaption + kCaptionHeight + kCaptionGap);
  bounds.removeFromTop(kAboveDial);
  return bounds;
}

void LabelledKnob::resized() { slider.setBounds(dialBounds(getLocalBounds())); }

void LabelledKnob::setCaption(const juce::String &text) {
  if (text == caption)
    return;

  caption = text;
  repaint(captionBounds(getLocalBounds()));
}

namespace {
/// Which of the seven bars are lit for each character we can draw.
///
///      aaa
///     f   b
///      ggg
///     e   c
///      ddd
constexpr uint8_t kSegA = 1, kSegB = 2, kSegC = 4, kSegD = 8, kSegE = 16,
                  kSegF = 32, kSegG = 64;

uint8_t segmentsFor(char c) {
  switch (c) {
  case '0':
    return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF;
  case '1':
    return kSegB | kSegC;
  case '2':
    return kSegA | kSegB | kSegG | kSegE | kSegD;
  case '3':
    return kSegA | kSegB | kSegG | kSegC | kSegD;
  case '4':
    return kSegF | kSegG | kSegB | kSegC;
  case '5':
    return kSegA | kSegF | kSegG | kSegC | kSegD;
  case '6':
    return kSegA | kSegF | kSegG | kSegE | kSegC | kSegD;
  case '7':
    return kSegA | kSegB | kSegC;
  case '8':
    return kSegA | kSegB | kSegC | kSegD | kSegE | kSegF | kSegG;
  case '9':
    return kSegA | kSegB | kSegC | kSegD | kSegF | kSegG;

  // A minus is the middle bar. It takes a cell like any digit.
  case '-':
    return kSegG;

  case 'E':
    return kSegA | kSegD | kSegE | kSegF | kSegG;
  case 'F':
    return kSegA | kSegE | kSegF | kSegG;
  case 't':
    return kSegD | kSegE | kSegF | kSegG;
  case 'n':
    return kSegC | kSegE | kSegG;
  case 'i':
    return kSegC;

  // ---- a sign sharing the hundreds digit's cell ---------------------------
  //
  // A minus and a one share a cell, for a three-digit reading that can be
  // negative. Only the macro panel's amount uses these.
  case '~':
    return kSegG;
  case '!':
    return kSegG | kSegB | kSegC;

  default:
    return 0;
  }
}

/// Whether a character is the decimal point, which every cell has beside it
/// rather than taking a cell of its own.
bool isNarrow(char c) { return c == '.'; }

/// One segment, as an elongated hexagon with mitred ends.
///
/// The ends slope so that neighbouring segments point at each other across
/// the gap between them.
juce::Path segmentShape(juce::Rectangle<float> r, float noseScale = 1.0f) {
  const bool flat = r.getWidth() >= r.getHeight();
  const auto thick = flat ? r.getHeight() : r.getWidth();
  const auto along = flat ? r.getWidth() : r.getHeight();

  // Never more than the bar can give, or a short bar becomes two triangles.
  const auto nose = juce::jmin(thick * 0.5f, along * 0.42f) * noseScale;

  juce::Path p;

  if (flat) {
    p.startNewSubPath(r.getX(), r.getCentreY());
    p.lineTo(r.getX() + nose, r.getY());
    p.lineTo(r.getRight() - nose, r.getY());
    p.lineTo(r.getRight(), r.getCentreY());
    p.lineTo(r.getRight() - nose, r.getBottom());
    p.lineTo(r.getX() + nose, r.getBottom());
  } else {
    p.startNewSubPath(r.getCentreX(), r.getY());
    p.lineTo(r.getRight(), r.getY() + nose);
    p.lineTo(r.getRight(), r.getBottom() - nose);
    p.lineTo(r.getCentreX(), r.getBottom());
    p.lineTo(r.getX(), r.getBottom() - nose);
    p.lineTo(r.getX(), r.getY() + nose);
  }

  p.closeSubPath();
  return p;
}

// ---- fourteen bars, for the displays that have to spell ---------------------
//
//      aaaaaaa
//     f  h i j  b
//     f   hij   b
//      ggg   GGG
//     e   mlk   c
//     e  m l k  c
//      ddddddd
//
// For the displays that spell names, which seven bars cannot do.
constexpr uint16_t kStA = 1 << 0, kStB = 1 << 1, kStC = 1 << 2, kStD = 1 << 3,
                   kStE = 1 << 4, kStF = 1 << 5, kStG1 = 1 << 6, kStG2 = 1 << 7,
                   kStH = 1 << 8, kStI = 1 << 9, kStJ = 1 << 10, kStK = 1 << 11,
                   kStL = 1 << 12, kStM = 1 << 13;

constexpr uint16_t kStG = kStG1 | kStG2;

uint16_t starburstFor(char c) {
  switch (c) {
  case '0':
    return kStA | kStB | kStC | kStD | kStE | kStF | kStJ | kStM;
  case '1':
    return kStB | kStC | kStJ;
  case '2':
    return kStA | kStB | kStG | kStE | kStD;
  case '3':
    return kStA | kStB | kStC | kStD | kStG2;
  case '4':
    return kStF | kStG | kStB | kStC;
  case '5':
    return kStA | kStF | kStG | kStC | kStD;
  case '6':
    return kStA | kStF | kStE | kStD | kStC | kStG;
  case '7':
    return kStA | kStB | kStC;
  case '8':
    return kStA | kStB | kStC | kStD | kStE | kStF | kStG;
  case '9':
    return kStA | kStB | kStC | kStD | kStF | kStG;

  // The sharp, for a root such as C# in the Tuning bar: the middle crossed by
  // the centre uprights, with the right uprights and the bottom bar.
  case '#':
    return kStB | kStC | kStD | kStG | kStI | kStL;

  case 'A':
    return kStA | kStB | kStC | kStE | kStF | kStG;
  case 'B':
    return kStA | kStB | kStC | kStD | kStG2 | kStI | kStL;
  case 'C':
    return kStA | kStD | kStE | kStF;
  case 'D':
    return kStA | kStB | kStC | kStD | kStI | kStL;
  case 'E':
    return kStA | kStD | kStE | kStF | kStG1;
  case 'F':
    return kStA | kStE | kStF | kStG1;
  case 'G':
    return kStA | kStC | kStD | kStE | kStF | kStG2;
  case 'H':
    return kStB | kStC | kStE | kStF | kStG;
  case 'I':
    return kStA | kStD | kStI | kStL;
  case 'J':
    return kStB | kStC | kStD | kStE;
  case 'K':
    return kStE | kStF | kStG1 | kStJ | kStK;
  case 'L':
    return kStD | kStE | kStF;
  case 'M':
    return kStB | kStC | kStE | kStF | kStH | kStJ;
  case 'N':
    return kStB | kStC | kStE | kStF | kStH | kStK;
  case 'O':
    return kStA | kStB | kStC | kStD | kStE | kStF;
  case 'P':
    return kStA | kStB | kStE | kStF | kStG;
  case 'Q':
    return kStA | kStB | kStC | kStD | kStE | kStF | kStK;
  case 'R':
    return kStA | kStB | kStE | kStF | kStG | kStK;
  case 'S':
    return kStA | kStC | kStD | kStF | kStG;
  case 'T':
    return kStA | kStI | kStL;
  case 'U':
    return kStB | kStC | kStD | kStE | kStF;
  case 'V':
    return kStE | kStF | kStJ | kStM;
  case 'W':
    return kStB | kStC | kStE | kStF | kStK | kStM;
  case 'X':
    return kStH | kStJ | kStK | kStM;
  case 'Y':
    return kStH | kStJ | kStL;
  case 'Z':
    return kStA | kStD | kStJ | kStM;

  // The punctuation worth having in a preset name.
  case '-':
    return kStG;
  case '_':
    return kStD;
  case '=':
    return kStG | kStD;
  case '+':
    return kStG | kStI | kStL;
  case '\'':
    return kStI;
  case '(':
    return kStJ | kStK;
  case ')':
    return kStH | kStM;
  case '!':
    return kStI | kStL;

  // A space is a cell with nothing lit.
  case ' ':
    return 0;

  default:
    return 0;
  }
}
} // namespace

bool SegmentDisplay::canDraw(char c, Bars bars) {
  if (bars == Bars::Fourteen)
    // A space has no bars lit and is still a character it can show, so the
    // table's zero cannot be the answer on its own here.
    return isNarrow(c) || c == ' ' ||
           starburstFor((char)juce::CharacterFunctions::toUpperCase(c)) != 0;

  return isNarrow(c) || segmentsFor(c) != 0;
}

SegmentDisplay::SegmentDisplay(juce::String unit, Bars howMany, int cells)
    : bars(howMany), fixedCells(cells), unitText(std::move(unit)) {}

juce::String SegmentDisplay::squeeze(const juce::String &name, int cells) {
  const auto upper = name.toUpperCase();

  if (cells <= 0)
    return upper;

  if (upper.length() <= cells)
    return upper.paddedRight(' ', cells);

  std::vector<juce::juce_wchar> kept;
  kept.reserve((size_t)upper.length());

  for (int i = 0; i < upper.length(); ++i)
    kept.push_back(upper[i]);

  const auto drop = [&kept, cells](bool (*wanted)(juce::juce_wchar)) {
    // From the right, and never index zero, so the name keeps its first
    // character.
    for (auto i = (int)kept.size() - 1; i > 0 && (int)kept.size() > cells; --i)
      if (wanted(kept[(size_t)i]))
        kept.erase(kept.begin() + i);
  };

  drop([](juce::juce_wchar c) { return c == ' '; });
  drop([](juce::juce_wchar c) {
    return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
  });

  juce::String out;

  for (auto c : kept)
    out += juce::String::charToString(c);

  return out.substring(0, cells).paddedRight(' ', cells);
}

void SegmentDisplay::setReading(const juce::String &digits, bool isActive) {
  // Upper case and fitted to the cells here, so getReading returns what the
  // cells show. A sharp goes to the sign rather than a cell, so the name is
  // fitted without it.
  const auto sharp = sharpSign && digits.containsChar('#');
  const auto name = sharpSign ? digits.removeCharacters("#") : digits;
  const auto shown =
      (bars == Bars::Fourteen ? squeeze(name, fixedCells) : name) +
      (sharp ? "#" : "");

  if (shown == reading && isActive == active)
    return;

  reading = shown;
  active = isActive;
  repaint();
}

void SegmentDisplay::showSharpSign(bool shouldShow) {
  sharpSign = shouldShow;
  repaint();
}

void SegmentDisplay::setLampColour(juce::Colour colour) {
  lamp = colour;
  repaint();
}

void SegmentDisplay::mouseUp(const juce::MouseEvent &e) {
  if (onClick && contains(e.getPosition()))
    onClick();
}

void SegmentDisplay::mouseEnter(const juce::MouseEvent &) {
  // Only a display with an onClick answers the pointer.
  if (onClick == nullptr)
    return;

  hovered = true;
  repaint();
}

void SegmentDisplay::mouseExit(const juce::MouseEvent &) {
  hovered = false;
  repaint();
}

void SegmentDisplay::paintGlyph(juce::Graphics &g, juce::Rectangle<float> area,
                                char c, juce::Colour on,
                                juce::Colour off) const {
  const auto lit = segmentsFor(c);

  // Every bar is drawn whether it is lit or not.
  const auto t = juce::jmax(1.0f, area.getHeight() * 0.16f);

  // The gap between segments. Below about a fifth of a bar's thickness the
  // mitred corners close up.
  const auto gap = t * 0.24f;
  const auto w = area.getWidth();
  const auto h = area.getHeight();
  const auto mid = (h - t) * 0.5f;

  struct Bar {
    uint8_t flag;
    juce::Rectangle<float> r;
  };

  const Bar straight[] = {
      {kSegA, {t * 0.5f + gap, 0.0f, w - t - gap * 2.0f, t}},
      {kSegB, {w - t, t * 0.5f + gap, t, mid - gap * 1.5f}},
      {kSegC, {w - t, mid + t * 0.5f + gap * 0.5f, t, mid - gap * 1.5f}},
      {kSegD, {t * 0.5f + gap, h - t, w - t - gap * 2.0f, t}},
      {kSegE, {0.0f, mid + t * 0.5f + gap * 0.5f, t, mid - gap * 1.5f}},
      {kSegF, {0.0f, t * 0.5f + gap, t, mid - gap * 1.5f}},
      {kSegG, {t * 0.5f + gap, mid, w - t - gap * 2.0f, t}},
  };

  for (const auto &bar : straight) {
    const bool isLit = (lit & bar.flag) != 0;
    const auto shape = segmentShape(bar.r.translated(area.getX(), area.getY()));

    // No bloom, which at this size reads as blur.
    g.setColour(isLit ? on : off);
    g.fillPath(shape);
  }
}

void SegmentDisplay::paintStarburst(juce::Graphics &g,
                                    juce::Rectangle<float> area, char c,
                                    juce::Colour on, juce::Colour off) const {
  const auto lit = starburstFor(c);

  // Thinner than a seven-bar cell's, so the four diagonals do not meet in a
  // blot.
  const auto t = juce::jmax(1.0f, area.getHeight() * 0.095f);
  // A little wider than the seven-bar gap, to keep clear of the diagonals.
  const auto gap = t * 0.32f;
  const auto w = area.getWidth();
  const auto h = area.getHeight();
  const auto mid = (h - t) * 0.5f;
  const auto half = (w - t) * 0.5f;

  // The uprights and the two halves of the middle, drawn as rectangles the
  // way the seven-bar ones are.
  struct Bar {
    uint16_t flag;
    juce::Rectangle<float> r;
  };

  const Bar straight[] = {
      {kStA, {t * 0.5f + gap, 0.0f, w - t - gap * 2.0f, t}},
      {kStB, {w - t, t * 0.5f + gap, t, mid - gap * 1.5f}},
      {kStC, {w - t, mid + t * 0.5f + gap * 0.5f, t, mid - gap * 1.5f}},
      {kStD, {t * 0.5f + gap, h - t, w - t - gap * 2.0f, t}},
      {kStE, {0.0f, mid + t * 0.5f + gap * 0.5f, t, mid - gap * 1.5f}},
      {kStF, {0.0f, t * 0.5f + gap, t, mid - gap * 1.5f}},
      // The two halves of the middle, each running from its own end of the cell
      // to its centre.
      {kStG1,
       {t * 0.5f + gap * 0.5f, mid, w * 0.5f - t * 0.5f - gap * 0.8f, t}},
      {kStG2,
       {w * 0.5f + gap * 0.3f, mid, w * 0.5f - t * 0.5f - gap * 0.8f, t}},
      {kStI, {half, t * 0.5f + gap, t, mid - gap * 1.5f}},
      {kStL, {half, mid + t * 0.5f + gap * 0.5f, t, mid - gap * 1.5f}},
  };

  // The bars inside the frame, the centre uprights and the diagonals, are
  // fainter unlit than the frame is.
  const auto inner = off.withMultipliedAlpha(0.55f);

  for (const auto &bar : straight) {
    const bool isLit = (lit & bar.flag) != 0;
    const bool inside = bar.flag == kStI || bar.flag == kStL;
    // The two halves of the middle are short enough that a full mitre at
    // each end leaves them a point, so they take a short bevel instead.
    const bool middle = bar.flag == kStG1 || bar.flag == kStG2;
    const auto shape = segmentShape(bar.r.translated(area.getX(), area.getY()),
                                    middle ? 0.35f : 1.0f);

    // No bloom, which at this size reads as blur.
    g.setColour(isLit ? on : inside ? inner : off);
    g.fillPath(shape);
  }

  // The four diagonals, from each corner of the cell to its middle. Trimmed
  // along their own direction, since a cell is not square.
  const auto cx = w * 0.5f;
  const auto cy = h * 0.5f;
  const auto corner = t * 0.9f;

  const auto spoke = [&](juce::Point<float> from) {
    const auto to = juce::Point<float>(cx, cy);
    const auto along = to - from;
    const auto len = std::sqrt(along.x * along.x + along.y * along.y);

    if (len < 1.0e-3f)
      return juce::Line<float>(from, to);

    const auto step = along / len;

    // Clear of the frame at the outer end and of the centre bars at the
    // inner one, both measured in bar widths so they hold at any size.
    return juce::Line<float>(from + step * (t * 0.3f), to - step * (t * 0.85f));
  };

  struct Slash {
    uint16_t flag;
    juce::Line<float> line;
  };

  const Slash slashes[] = {
      {kStH, spoke({corner, corner})},
      {kStJ, spoke({w - corner, corner})},
      {kStK, spoke({w - corner, h - corner})},
      {kStM, spoke({corner, h - corner})},
  };

  for (const auto &slash : slashes) {
    const bool isLit = (lit & slash.flag) != 0;
    const juce::Line<float> at{slash.line.getStart() + area.getPosition(),
                               slash.line.getEnd() + area.getPosition()};

    g.setColour(isLit ? on : inner);
    g.drawLine(at, t * 0.9f);
  }
}

/// The width the unit takes beside the digits, measured in the width the
/// component is given.
static constexpr float kUnitWidth = 21.0f;
static constexpr int kUnitInsets = 8;
static constexpr int kDigitsNeed = 44;

int SegmentDisplay::naturalWidth(int height) const {
  // The same insets paint takes: a pixel for the recess, then three across
  // and two down for the screen's margin.
  const auto inner = (float)height - 2.0f - 4.0f;
  const auto cellW = inner * (bars == Bars::Fourteen ? 0.74f : 0.72f);
  const auto sharpW = sharpSign ? inner * 0.5f : 0.0f;
  const auto unitW = unitText.isNotEmpty() ? kUnitWidth : 0.0f;

  return (int)std::ceil(cellW * (float)juce::jlimit(1, kMaxCells, fixedCells) +
                        sharpW + unitW + 2.0f + 6.0f);
}

bool SegmentDisplay::hasRoomForUnit(int width) {
  return width - kUnitInsets > kDigitsNeed;
}

std::unique_ptr<juce::AccessibilityHandler>
SegmentDisplay::createAccessibilityHandler() {
  // Asked for lazily, after the bar has set onClick.
  if (onClick == nullptr)
    return juce::Component::createAccessibilityHandler();

  return std::make_unique<juce::AccessibilityHandler>(
      *this, juce::AccessibilityRole::button,
      juce::AccessibilityActions().addAction(
          juce::AccessibilityActionType::press, [this] {
            if (onClick != nullptr)
              onClick();
          }));
}

void SegmentDisplay::paint(juce::Graphics &g) {
  LookKey key;
  key.add(reading).add(active).add(hovered).add(sharpSign).add(unitText).add(
      lamp);

  // How many cells the reading fitted is worked out while drawing, so it is
  // kept beside each picture for cellsDrawn to go on reporting.
  bool drewIt = false;
  const auto used = looks.paint(g, *this, key, [&](juce::Graphics &into) {
    paintReading(into);
    drewIt = true;
  });

  if (drewIt)
    drawnFor[used] = drawn;
  else
    drawn = drawnFor[used];
}

void SegmentDisplay::paintReading(juce::Graphics &g) {
  auto area = getLocalBounds().toFloat().reduced(1.0f);

  // Unlit bars are fainter where there are fourteen to a cell.
  const auto rest = bars == Bars::Fourteen ? 0.55f : 1.0f;
  const auto off = lamp.withAlpha((hovered ? 0.20f : 0.12f) * rest);

  // Inactive shows only unlit bars. A segment is lit or it is not.
  const auto lit = active ? lamp.withAlpha(0.95f) : off;

  // Set into the panel, using the pixel of margin around the ground.
  paintRecess(g, area, 2.5f);

  // The lit screen ground.
  paintDisplayGround(g, area, 2.5f, hovered, lamp);

  if (hovered) {
    g.setColour(colours::outline);
    g.drawRoundedRectangle(area.reduced(0.5f), 2.5f, 1.0f);
  }

  area = area.reduced(3.0f, 2.0f);

  // ---- the cells ----
  //
  // A fixed number of them, all drawn whatever the reading is and laid out
  // from the display's size alone.
  const auto cells = juce::jlimit(1, kMaxCells, fixedCells);

  // The unit is printed beside the cells and takes only the room its words
  // need.
  const auto unitFont = makeFont(7.5f, true);

  auto unitW = 0.0f;

  if (unitText.isNotEmpty() && hasRoomForUnit(getWidth())) {
    juce::GlyphArrangement measure;
    measure.addLineOfText(unitFont, unitText, 0.0f, 0.0f);

    unitW = juce::jmin(kUnitWidth,
                       measure.getBoundingBox(0, -1, true).getWidth() + 2.0f);
  }

  // Wider for fourteen bars, which have more to fit across the cell.
  const auto widest =
      area.getHeight() * (bars == Bars::Fourteen ? 0.74f : 0.72f);
  // The sharp sign stands after the cells, narrower than one.
  const auto sharpW = sharpSign ? area.getHeight() * 0.5f : 0.0f;

  const auto cellW =
      juce::jmin((area.getWidth() - unitW - sharpW) / (float)cells, widest);

  // The figure, and the point in the room to its right.
  const auto glyphW = cellW * 0.78f;
  const auto runW = cellW * (float)cells;
  const auto left = area.getCentreX() - (runW + sharpW + unitW) * 0.5f;

  if (unitW > 0.0f) {
    g.setColour(colours::textDim.withAlpha(0.8f));
    g.setFont(unitFont);
    g.drawText(unitText,
               area.withX(left + runW + sharpW + 2.0f).withWidth(unitW - 2.0f),
               juce::Justification::centredLeft, false);
  }

  if (sharpSign) {
    // Two uprights crossed by two bars that rise to the right, the shape a
    // sharp is printed in, at the weight of a fourteen-bar cell's own bars.
    const juce::Rectangle<float> slot{left + runW, area.getY(), sharpW,
                                      area.getHeight()};
    const auto w = slot.getWidth(), h = slot.getHeight();
    const auto weight = juce::jmax(1.0f, h * 0.095f);

    juce::Path sign;
    for (const auto x : {0.36f, 0.64f})
      sign.addLineSegment({slot.getX() + w * x, slot.getY() + h * 0.12f,
                           slot.getX() + w * x, slot.getY() + h * 0.88f},
                          weight);
    for (const auto y : {0.40f, 0.68f})
      sign.addLineSegment({slot.getX() + w * 0.1f, slot.getY() + h * y,
                           slot.getX() + w * 0.9f,
                           slot.getY() + h * (y - 0.1f)},
                          weight * 1.3f);

    g.setColour(active && reading.containsChar('#') ? lit : off);
    g.fillPath(sign);
  }

  // What each cell shows. A point lights the point of the cell before it,
  // and only starts a cell of its own at the front of a reading.
  struct Cell {
    char c = ' ';
    bool point = false;
  };

  std::array<Cell, kMaxCells> laid{};
  int used = 0;
  bool over = false;

  // Off draws as if there were no reading at all, so nothing it would have
  // lit shows in any shade.
  for (auto ch : active ? reading : juce::String()) {
    const auto c = (char)ch;

    if (sharpSign && c == '#')
      continue;

    if (c == '.' && used > 0 && !laid[(size_t)(used - 1)].point) {
      laid[(size_t)(used - 1)].point = true;
      continue;
    }

    if (used == cells) {
      over = true;
      break;
    }

    laid[(size_t)used++] = c == '.' ? Cell{' ', true} : Cell{c, false};
  }

  // More than the cells can hold shows as over range, a minus in every cell.
  if (over) {
    for (int i = 0; i < cells; ++i)
      laid[(size_t)i] = {'-', false};
    used = cells;
  }

  drawn = over ? 0 : reading.length();

  // A number stands to the right, the way a meter's does, so its units digit
  // keeps its place as it grows. A name starts at the left.
  const auto from = bars == Bars::Seven ? cells - used : 0;

  const auto t = bars == Bars::Fourteen
                     ? juce::jmax(1.0f, area.getHeight() * 0.095f)
                     : juce::jmax(1.0f, area.getHeight() * 0.16f);
  const auto dot = juce::jmin(t * (bars == Bars::Fourteen ? 1.3f : 1.0f),
                              (cellW - glyphW) * 0.9f);

  for (int i = 0; i < cells; ++i) {
    const auto at = i - from;
    const auto cell =
        at >= 0 && at < used ? laid[(size_t)at] : Cell{' ', false};
    const auto x = left + cellW * (float)i;
    const juce::Rectangle<float> glyph{x, area.getY(), glyphW,
                                       area.getHeight()};

    if (bars == Bars::Fourteen)
      paintStarburst(g, glyph, cell.c, lit, off);
    else
      paintGlyph(g, glyph, cell.c, lit, off);

    g.setColour(cell.point ? lit : off);
    g.fillRoundedRectangle(x + glyphW + (cellW - glyphW - dot) * 0.5f,
                           area.getBottom() - dot, dot, dot, dot * 0.35f);
  }
}

void ActivityLamp::setBackdrop(juce::Colour behind) {
  backdrop = behind;
  setOpaque(true);
  repaint();
}

bool ActivityLamp::push(float brightness) {
  const auto exact = juce::jlimit(0.0f, 1.0f, brightness) * (float)kSteps;
  const auto next = juce::jlimit(0, kSteps, (int)std::lround(exact));

  if (next == step)
    return false;

  step = next;
  return true;
}

void ActivityLamp::paint(juce::Graphics &g) {
  const auto bounds = getLocalBounds().toFloat();

  if (isOpaque()) {
    g.setColour(backdrop);
    g.fillRect(bounds);
  }

  // The rule the lamp is mounted on, drawn either side of it.
  const auto midY = std::floor(bounds.getCentreY());
  const auto diameter = juce::jmin(bounds.getHeight() - 2.0f, 7.0f);
  const auto lamp = juce::Rectangle<float>(diameter, diameter)
                        .withCentre({bounds.getCentreX(), midY + 0.5f});

  const auto leftRun = lamp.getX() - bounds.getX() - 2.0f;
  const auto rightRun = bounds.getRight() - lamp.getRight() - 2.0f;

  g.setColour(colours::outline.withAlpha(0.7f));
  g.fillRect(bounds.getX(), midY, leftRun, 1.0f);
  g.fillRect(lamp.getRight() + 2.0f, midY, rightRun, 1.0f);

  // A lit lip under the rule, so it reads as a groove. Inside the lamp's own
  // bounds, so its invalidated band covers it.
  g.setColour(juce::Colours::white.withAlpha(0.055f));
  g.fillRect(bounds.getX(), midY + 1.0f, leftRun, 1.0f);
  g.fillRect(lamp.getRight() + 2.0f, midY + 1.0f, rightRun, 1.0f);

  // The hole it is mounted in. The rule stops two pixels short either side
  // for its lip.
  paintRecess(g, lamp, diameter * 0.5f);

  // Unlit is the channel colour at low alpha rather than nothing.
  const auto lit = (float)step / (float)kSteps;

  // The same bloom the meters get, only when lit and inside the lamp's own
  // row, so the invalidated band is unchanged.
  if (lit > 0.02f) {
    g.setColour(colour.withAlpha(0.10f * lit));
    g.fillEllipse(lamp.expanded(diameter * 0.42f));

    g.setColour(colour.withAlpha(0.12f * lit));
    g.fillEllipse(lamp.expanded(diameter * 0.18f));
  }

  // Blended against the backdrop rather than drawn at that alpha, so the face
  // is opaque and covers the recess shadow under it.
  g.setColour(isOpaque()
                  ? backdrop.interpolatedWith(colour, 0.16f + 0.84f * lit)
                  : colour.withAlpha(0.16f + 0.84f * lit));
  g.fillEllipse(lamp);
}

void LevelMeter::setBackdrop(juce::Colour top, juce::Colour bottom) {
  backdropTop = top;
  backdropBottom = bottom;

  // An opaque child is left out of what its parent has to paint.
  setOpaque(true);
  repaint();
}

int LevelMeter::segments() const { return meterSegments(getHeight()); }

juce::Rectangle<int> LevelMeter::push(float level) {
  // A decibel scale with a floor at -48 dB. On a linear amplitude scale
  // everything above the fundamental sits squashed against the bottom.
  const float norm =
      level <= 1.0e-5f
          ? 0.0f
          : juce::jlimit(0.0f, 1.0f,
                         (juce::Decibels::gainToDecibels(level) + 48.0f) /
                             48.0f);

  // Instant rise and a gentle fall, so a partial decaying under its own
  // envelope reads as a decay rather than as flicker.
  const float next =
      norm > displayed ? norm : juce::jmax(norm, displayed - 0.06f);

  displayed = next;

  // Where the level sits, measured in lamps.
  const auto count = segments();
  const auto exact = juce::jlimit(0.0f, 1.0f, displayed) * (float)count;

  const auto before = juce::jlimit(0, count, lit);
  auto after = before;

  // A lamp lights when the level reaches it, and goes out only once the level
  // has fallen clear of it, so a level on a boundary does not flicker.
  constexpr float kClearOf = 0.3f;

  while (after < count && exact >= (float)after + 1.0f)
    ++after;

  while (after > 0 && exact < (float)after - kClearOf)
    --after;

  lit = after;

  // Most frames change no lamp and cost nothing.
  if (before == after)
    return {};

  // Only the lamps that lit or went out. Not repainted here: the editor
  // merges these from every meter and invalidates once.
  const auto track = meterTrack(getLocalBounds().toFloat());
  const auto lo = juce::jmin(before, after);
  const auto hi = juce::jmax(before, after);

  // From the top of the higher run to the top of the lower, where the bloom's
  // rounded top lands, or to the foot of the track when the run starts or
  // ends, since the bloom's foot comes and goes with it.
  const auto top = meterLamp(track, count, hi - 1).getY();
  const auto bottom =
      lo == 0 ? track.getBottom() : meterLamp(track, count, lo - 1).getY();

  // The margin covers the bloom, which reaches kMeterGlowSpread beyond the
  // run, plus a pixel for the rounding above.
  const auto margin = (int)kMeterGlowSpread + 1;

  // Out to the whole pixels the edges fall in, since the lamps' edges are not
  // on pixel boundaries.
  const auto y0 = (int)std::floor(top) - margin;
  const auto y1 = (int)std::ceil(bottom) + margin;

  return juce::Rectangle<int>(0, y0, getWidth(), y1 - y0)
      .getIntersection(getLocalBounds());
}

void LevelMeter::paint(juce::Graphics &g) {
  // The slice of channel background this meter stands on, painted here so
  // the component can be opaque.
  if (isOpaque()) {
    g.setGradientFill(juce::ColourGradient(backdropTop, 0.0f, 0.0f,
                                           backdropBottom, 0.0f,
                                           (float)getHeight(), false));
    g.fillRect(getLocalBounds());
  }

  // The meter sits behind the fader and owns its whole track.
  const auto full = getLocalBounds().toFloat();
  const auto track = meterTrack(full);
  const auto trackW = track.getWidth();

  const auto count = segments();
  const auto onNow = juce::jlimit(0, count, lit);
  const auto step = track.getHeight() / (float)count;

  // Off is the channel's own colour held down low, so an idle meter still says
  // which channel it belongs to.
  const auto on = colour.withAlpha(0.92f);
  const auto off = colour.withAlpha(0.13f);

  // Bloom around the lit run, drawn once for the whole run, rounded at both
  // ends inside the component's bounds so the invalidated band need not grow.
  const auto litTop =
      onNow > 0 ? meterLamp(track, count, onNow - 1).getY() : track.getBottom();
  const auto room = juce::jmax(0.0f, (full.getWidth() - trackW) * 0.5f);
  const auto spread = juce::jmin(kMeterGlowSpread, room);

  // The foot is the same however long the run is, so it is drawn on its own,
  // below a seam half way up the bottom lamp, from a shape that does not change
  // with the run. One rounded rectangle the length of the run comes out a shade
  // different at its foot as the run grows or shrinks, outside the band the
  // meter asks to have repainted. Each part runs on well past the seam, so
  // neither has a corner near it.
  const auto seam = (int)std::floor(track.getBottom() - step * 0.5f);
  const auto upper = juce::Rectangle<float>(
      track.getX(), litTop, track.getWidth(), full.getBottom() + 8.0f - litTop);
  const auto foot =
      juce::Rectangle<float>(track.getX(), track.getBottom() - step - 8.0f,
                             track.getWidth(), step + 8.0f);

  const auto layer = [&](float grow, float corner, float alpha) {
    g.setColour(colour.withAlpha(alpha));
    {
      juce::Graphics::ScopedSaveState keep(g);
      g.reduceClipRegion(0, 0, getWidth(), seam);
      g.fillRoundedRectangle(upper.expanded(grow, grow), corner);
    }
    juce::Graphics::ScopedSaveState keep(g);
    g.reduceClipRegion(0, seam, getWidth(), getHeight() - seam);
    g.fillRoundedRectangle(foot.expanded(grow, grow), corner);
  };

  const auto bloom = [&] {
    layer(spread, 4.0f, 0.09f);
    layer(spread * 0.45f, 3.0f, 0.10f);
  };

  // Under the recess, as a lamp's glow is, so the lip reads the same lit or
  // not.
  if (onNow > 0)
    bloom();

  // Recessed into the channel, as every other light is.
  paintRecess(g, track, kMeterRecessCorner);

  g.setColour(colours::groove);
  g.fillRoundedRectangle(track, kMeterRecessCorner);

  // And again on the groove, clipped to it, so the lamps light the groove
  // around them as well as the channel beside it.
  if (onNow > 0) {
    juce::Path groove;
    groove.addRoundedRectangle(track, kMeterRecessCorner);

    juce::Graphics::ScopedSaveState keep(g);
    g.reduceClipRegion(groove);
    bloom();
  }

  for (int i = 0; i < count; ++i) {
    const auto lamp = meterLamp(track, count, i);

    g.setColour(i < onNow ? on : off);
    g.fillRoundedRectangle(lamp, juce::jmin(2.0f, lamp.getHeight() * 0.4f));
  }
}

namespace {
/// Shared with the channel meters so the two read on the same scale.
constexpr float kMeterFloorDb = -48.0f;

float toNormalised(float level) {
  if (level <= 1.0e-5f)
    return 0.0f;

  return juce::jlimit(0.0f, 1.0f,
                      (juce::Decibels::gainToDecibels(level) - kMeterFloorDb) /
                          -kMeterFloorDb);
}

} // namespace

juce::Colour StereoOutputMeter::lampColourAt(float t) const {
  // Fixed per lamp, whatever the level.
  if (t < 0.80f)
    return lowColour;

  if (t < 0.94f)
    return lowColour.interpolatedWith(highColour, (t - 0.80f) / 0.14f);

  return highColour.interpolatedWith(topColour, (t - 0.94f) / 0.06f);
}

void StereoOutputMeter::setVertical(bool shouldStand) {
  vertical = shouldStand;
  repaint();
}

void StereoOutputMeter::setLampColours(juce::Colour low, juce::Colour high,
                                       juce::Colour top) {
  lowColour = low;
  highColour = high;
  topColour = top;
  repaint();
}

int StereoOutputMeter::segments() const {
  // A lamp every eight pixels or so along the bars, within reason.
  return juce::jlimit(12, 48, (vertical ? getHeight() : getWidth()) / 8);
}

bool StereoOutputMeter::Bar::advance(float level, int count) {
  const auto norm = toNormalised(level);
  const auto next =
      norm > displayed ? norm : juce::jmax(norm, displayed - 0.05f);

  displayed = next;

  // Hold the peak for a couple of seconds, then let it fall with the bar.
  if (next >= peak) {
    peak = next;
    hold = 30;
  } else if (hold > 0) {
    --hold;
  } else if (peak > 0.0f) {
    peak = juce::jmax(next, peak - 0.02f);
  }

  const auto wasLit = lit;
  const auto wasPeak = peakLamp;

  // The same hysteresis as the channel meters, so a level sitting on a
  // boundary does not flicker.
  constexpr float kClearOf = 0.3f;

  const auto exact = juce::jlimit(0.0f, 1.0f, displayed) * (float)count;
  lit = juce::jlimit(0, count, lit);

  while (lit < count && exact >= (float)lit + 1.0f)
    ++lit;

  while (lit > 0 && exact < (float)lit - kClearOf)
    --lit;

  peakLamp = peak > 0.001f
                 ? juce::jlimit(0, count - 1,
                                (int)(juce::jmin(peak, 1.0f) * (float)count))
                 : -1;

  return lit != wasLit || peakLamp != wasPeak;
}

void StereoOutputMeter::push(float l, float r) {
  const auto count = segments();
  if (count < 1)
    return;

  const bool a = left.advance(l, count);
  const bool b = right.advance(r, count);

  if (a || b)
    repaint();
}

void StereoOutputMeter::paintBar(juce::Graphics &g, juce::Rectangle<float> r,
                                 const Bar &bar, int count) const {
  const auto length = vertical ? r.getHeight() : r.getWidth();
  const auto step = length / (float)count;
  const auto gap = juce::jlimit(1.0f, 3.0f, step * 0.18f);

  for (int i = 0; i < count; ++i) {
    // Standing, the first lamp is at the bottom.
    const auto lamp =
        vertical ? juce::Rectangle<float>(r.getX(),
                                          r.getBottom() - (float)(i + 1) * step,
                                          r.getWidth(), step)
                       .reduced(1.0f, gap * 0.5f)
                 : juce::Rectangle<float>(r.getX() + (float)i * step, r.getY(),
                                          step, r.getHeight())
                       .reduced(gap * 0.5f, 1.0f);

    const auto colour = lampColourAt(((float)i + 0.5f) / (float)count);

    // The peak stays alight above the bar it came from.
    const bool on = i < bar.lit || i == bar.peakLamp;

    g.setColour(colour.withAlpha(on ? 0.92f : 0.13f));
    // Rounded by the lamp's length along the bar, which is what the corner
    // has to fit.
    const auto along = vertical ? lamp.getHeight() : lamp.getWidth();
    g.fillRoundedRectangle(lamp, juce::jmin(1.5f, along * 0.4f));
  }
}

void StereoOutputMeter::paint(juce::Graphics &g) {
  auto area = getLocalBounds().toFloat();

  // Scale marks at the decibel values worth aiming at.
  const auto scale = vertical ? area.removeFromLeft((float)kScaleHeight)
                              : area.removeFromBottom((float)kScaleHeight);
  for (const float db : {-48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f}) {
    const auto t = (db - kMeterFloorDb) / -kMeterFloorDb;

    g.setColour(colours::textDim.withAlpha(db >= -6.0f ? 0.55f : 0.30f));

    if (vertical)
      g.fillRect(scale.getX(),
                 scale.getBottom() - 1.0f - t * (scale.getHeight() - 1.5f),
                 3.0f, 1.0f);
    else
      g.fillRect(scale.getX() + t * (scale.getWidth() - 1.5f), scale.getY(),
                 1.0f, 3.0f);
  }

  // Both bars in one recess, as one two-line display, a pixel in for its lip.
  // Two of them would have their lips meet in the gap between the bars.
  area.reduce(1.0f, 1.0f);
  paintRecess(g, area, 1.5f);

  g.setColour(colours::groove);
  g.fillRoundedRectangle(area, 1.5f);

  // The bars meet, and each lamp's own pixel above and below leaves the same
  // dark line between them as between the lamps along a bar.
  const auto count = segments();

  if (vertical) {
    const auto barW = area.getWidth() * 0.5f;
    paintBar(g, area.removeFromLeft(barW), left, count);
    paintBar(g, area, right, count);
    return;
  }

  const auto barH = area.getHeight() * 0.5f;

  paintBar(g, area.removeFromTop(barH), left, count);
  paintBar(g, area, right, count);
}

} // namespace dm::ui
