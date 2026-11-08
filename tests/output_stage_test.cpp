// The output stage and its two measured machines, the console and the
// cassette. Builds with any C++17 compiler and nothing else:
//
//   c++ -std=c++17 -I include tests/output_stage_test.cpp src/dsp/*.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "dm/dsp/Cassette.h"
#include "dm/dsp/Console.h"
#include "dm/dsp/OutputStage.h"

using namespace dm;

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

void section(const char *name) { std::printf("\n== %s ==\n", name); }

/// Bit-for-bit equality, written as two comparisons so the float-equality
/// warning stays on everywhere else.
bool same(float a, float b) { return !(a < b) && !(b < a); }

/// Single-bin DFT magnitude with a Hann window, whose low sidelobes keep a
/// strong partial from hiding the low-level alias images being looked for.
double binMagnitude(const std::vector<float> &x, double freq,
                    double sampleRate) {
  double re = 0.0, im = 0.0, norm = 0.0;
  const double w = 6.283185307179586 * freq / sampleRate;
  const double N = (double)x.size();

  for (size_t n = 0; n < x.size(); ++n) {
    const double win =
        0.5 * (1.0 - std::cos(6.283185307179586 * (double)n / N));

    re += win * x[n] * std::cos(w * (double)n);
    im -= win * x[n] * std::sin(w * (double)n);
    norm += win;
  }

  return std::sqrt(re * re + im * im) / norm;
}

/// The output stage: five ways to run out of room, all of them bounded, since
/// the host was told what to expect. Soft is bit for bit what it was, each type
/// differs in the way it claims to, and the limiter holds the ceiling itself,
/// with or without its lookahead.
void testOutputStage() {
  section("The output stage");

  constexpr double sr = 48000.0;
  constexpr int kN = 4096;

  // The stage is late by its lookahead, so everything below renders that much
  // extra and drops it, or every comparison against the input would be most of
  // a quarter cycle out on a 220 Hz sine.
  const auto run = [&](ClipType type, float drive) {
    OutputStage stage;
    stage.prepare(sr);

    const auto late = (size_t)stage.latency();

    std::vector<float> l((size_t)kN + late), r((size_t)kN + late);

    for (size_t n = 0; n < l.size(); ++n) {
      const auto s =
          (float)(drive * std::sin(6.283185307179586 * 220.0 * (double)n / sr));
      l[n] = s;
      r[n] = s;
    }

    stage.process(l.data(), r.data(), (int)l.size(), type, true, true);
    l.erase(l.begin(), l.begin() + (long)late);

    return l;
  };

  // ---- bounded, every one of them ----------------------------------------
  int unbounded = 0;

  for (int i = 0; i < (int)ClipType::NumTypes; ++i) {
    const auto type = (ClipType)i;

    // Far past anything the instrument can produce, so the answer is about the
    // shape rather than about what happens to reach it.
    for (float drive : {1.01f, 2.0f, 11.0f, 400.0f}) {
      const auto out = run(type, drive);

      for (float v : out)
        if (!(std::abs(v) <= 1.0f) || !std::isfinite(v)) {
          ++unbounded;
          break;
        }
    }
  }

  check(unbounded == 0, "every type stays inside unity however hard it is "
                        "driven (" +
                            std::to_string(unbounded) + " did not)");

  // ---- Soft is exactly what it always was --------------------------------
  //
  // Every preset was dialled in against this curve, so it has to stay the same
  // bit for bit.
  {
    OutputStage stage;
    stage.prepare(sr);

    const auto late = (size_t)stage.latency();

    std::vector<float> l, r;
    for (int n = -2000; n <= 2000; ++n)
      l.push_back((float)n / 1000.0f);

    const auto before = l;

    // Enough silence after the ramp to push all of it out of the delay.
    l.resize(l.size() + late, 0.0f);
    r = l;

    stage.process(l.data(), r.data(), (int)l.size(), ClipType::Soft, true,
                  true);
    l.erase(l.begin(), l.begin() + (long)late);

    int differed = 0;
    for (size_t i = 0; i < l.size(); ++i) {
      const float x = before[i];
      const float a = std::abs(x);

      // The old function, written out rather than called, so this compares
      // against the formula and not against the code under test.
      float want = x;
      if (a > 0.7f) {
        const float over = (a - 0.7f) / 0.3f;
        const float y = 0.7f + 0.3f * std::tanh(over);
        want = x < 0.0f ? -y : y;
      }

      if (!same(l[i], want))
        ++differed;
    }

    check(differed == 0, "Soft is the curve every preset was dialled against "
                         "(" +
                             std::to_string(differed) + " samples differ)");
  }

  // ---- and each one differs in the way it claims to ----------------------
  const auto harmonic = [&](ClipType type, float drive, double multiple) {
    return binMagnitude(run(type, drive), 220.0 * multiple, sr);
  };

  // Soft and Hard fold both halves alike, so the even harmonics cancel.
  // Asymmetric does not, which is the whole of what it is for.
  const auto soft2 = harmonic(ClipType::Soft, 2.0f, 2.0);
  const auto asym2 = harmonic(ClipType::Asymmetric, 2.0f, 2.0);

  std::printf("  second harmonic: soft %.5f, asymmetric %.5f\n", soft2, asym2);
  check(asym2 > 20.0 * soft2 + 1.0e-6,
        "Asymmetric puts a second harmonic where Soft puts none");

  // A limiter turns the signal down rather than bending it, so what comes out
  // is still a sine where a clipper's is not. The bound is tight on purpose: a
  // gain that jumps into place holds the ceiling but puts 0.00185 into the
  // third harmonic, where the lookahead and the two box filters give 0.00012.
  const auto hard3 = harmonic(ClipType::Hard, 2.0f, 3.0);
  const auto limit3 = harmonic(ClipType::Limiter, 2.0f, 3.0);

  std::printf("  third harmonic: hard %.5f, limiter %.5f\n", hard3, limit3);
  check(limit3 < hard3 * 0.005,
        "the Limiter holds the ceiling without distorting to do it (" +
            std::to_string(limit3) + ")");

  // And the folder makes more of everything than Soft.
  const auto soft5 = harmonic(ClipType::Soft, 2.0f, 5.0);
  const auto fold5 = harmonic(ClipType::Fold, 2.0f, 5.0);

  std::printf("  fifth harmonic: soft %.5f, fold %.5f\n", soft5, fold5);
  check(fold5 > soft5 * 2.0, "and Fold generates far more than Soft");

  // ---- below the knee nothing touches it ---------------------------------
  //
  // The input is built once and kept, since 0.4f and 0.4 are not the same
  // number.
  {
    std::vector<float> in((size_t)kN);
    for (int n = 0; n < kN; ++n)
      in[(size_t)n] =
          (float)(0.4 * std::sin(6.283185307179586 * 220.0 * n / sr));

    OutputStage stage;
    stage.prepare(sr);

    const auto late = (size_t)stage.latency();

    auto l = in;
    auto r = in;
    l.resize(l.size() + late, 0.0f);
    r.resize(r.size() + late, 0.0f);

    stage.process(l.data(), r.data(), (int)l.size(), ClipType::Soft, true,
                  true);
    l.erase(l.begin(), l.begin() + (long)late);

    int moved = 0;
    for (size_t i = 0; i < in.size(); ++i)
      if (!same(l[i], in[i]))
        ++moved;

    check(moved == 0, "and a signal under the knee comes through untouched");
  }

  // ---- the limiter limits, rather than leaving it to the backstop ---------
  //
  // With the lookahead the gain is in place before the peak lands, so the hard
  // clip behind it has nothing to do. Asserted as a ceiling below unity rather
  // than as a count of clipped samples, since anything at unity got there by
  // being cut off.
  {
    OutputStage stage;
    stage.prepare(sr);

    // Silence, then a loud sine arriving at full amplitude with no ramp. The
    // envelopes in this instrument open in as little as 0.2 ms, so an onset
    // this abrupt is not far past what it really produces.
    std::vector<float> l((size_t)kN), r((size_t)kN);

    for (int n = kN / 4; n < kN; ++n) {
      const auto s =
          (float)(2.0 * std::sin(6.283185307179586 * 220.0 * n / sr));
      l[(size_t)n] = s;
      r[(size_t)n] = s;
    }

    stage.process(l.data(), r.data(), kN, ClipType::Limiter, true, true);

    // Four milliseconds from the onset, twice the lookahead. Fixed in
    // milliseconds rather than taken from latency(), so a zero latency cannot
    // leave an empty window that passes.
    const int onsetSamples = (int)(0.004 * sr);

    float onset = 0.0f;
    for (int n = kN / 4; n < kN / 4 + onsetSamples; ++n)
      onset = std::max(onset, std::abs(l[(size_t)n]));

    std::printf("  limiter through the onset at %.4f\n", (double)onset);

    check(onset <= 0.99f,
          "the Limiter holds an abrupt onset below unity with gain rather "
          "than with the clip behind it (" +
              std::to_string(onset) + ")");
  }

  // ---- and without its window it is rougher, not broken -------------------
  //
  // With no lookahead the detector sees one sample, so the gain is exact and
  // immediate and the ceiling still holds. What is lost is the smoothing,
  // since a gain that steps is itself a distortion.
  {
    const auto limited = [&](bool ahead) {
      OutputStage stage;
      stage.prepare(sr);

      std::vector<float> l((size_t)kN), r((size_t)kN);

      for (int n = kN / 4; n < kN; ++n) {
        const auto s =
            (float)(2.0 * std::sin(6.283185307179586 * 220.0 * n / sr));
        l[(size_t)n] = s;
        r[(size_t)n] = s;
      }

      stage.process(l.data(), r.data(), kN, ClipType::Limiter, true, ahead);

      return std::pair<float, int>{*std::max_element(l.begin(), l.end(),
                                                     [](float a, float b) {
                                                       return std::abs(a) <
                                                              std::abs(b);
                                                     }),
                                   stage.latency()};
    };

    const auto with = limited(true);
    const auto without = limited(false);

    check(with.second > 0 && without.second == 0,
          "refusing the lookahead gives the latency back (" +
              std::to_string(with.second) + " samples to " +
              std::to_string(without.second) + ")");

    check(std::abs(without.first) <= 0.99f,
          "and still holds the ceiling without it (" +
              std::to_string(std::abs(without.first)) + ")");

    // A steady sine rather than the onset above, since what is being compared
    // is how the gain moves once it is working rather than how it arrives.
    const auto steady = [&](bool ahead) {
      OutputStage stage;
      stage.prepare(sr);

      const auto late = (size_t)(ahead ? stage.latency() : 0);

      std::vector<float> l((size_t)kN + late), r((size_t)kN + late);

      for (size_t n = 0; n < l.size(); ++n) {
        const auto s =
            (float)(2.0 * std::sin(6.283185307179586 * 220.0 * (double)n / sr));
        l[n] = s;
        r[n] = s;
      }

      stage.process(l.data(), r.data(), (int)l.size(), ClipType::Limiter, true,
                    ahead);
      l.erase(l.begin(), l.begin() + (long)late);

      return l;
    };

    const auto ahead3 = binMagnitude(steady(true), 660.0, sr);
    const auto flat3 = binMagnitude(steady(false), 660.0, sr);

    std::printf("  third harmonic: looking ahead %.5f, not %.5f\n", ahead3,
                flat3);

    check(flat3 > ahead3 * 4.0,
          "and the smoothing is what the window was for (" +
              std::to_string(flat3 / std::max(1.0e-9, ahead3)) + " times)");
  }

  // ---- the window's ring stays inside itself ------------------------------
  //
  // The front of the window moves on every time a peak falls out of it. A
  // count that only goes up would overflow after about a day of sound, so it
  // moves back a lap whenever it completes one. A second of a sine is many
  // laps of a ring a millisecond long.
  {
    OutputStage stage;
    stage.prepare(sr);

    std::vector<float> l((size_t)sr), r((size_t)sr);

    for (size_t n = 0; n < l.size(); ++n) {
      const auto s =
          (float)(2.0 * std::sin(6.283185307179586 * 220.0 * (double)n / sr));
      l[n] = s;
      r[n] = s;
    }

    stage.process(l.data(), r.data(), (int)l.size(), ClipType::Limiter, true,
                  true);

    const auto ring = OutputStage::lookaheadSamples(sr) / 2 + 1;

    check(stage.wedgeFront() >= 0 && stage.wedgeFront() < ring,
          "the limiter's window stays inside its ring after a second (" +
              std::to_string(stage.wedgeFront()) + " of " +
              std::to_string(ring) + ")");
  }
}

/// slowly after a long one.
void testConsole() {
  section("Console");

  constexpr double kTwoPi = 6.283185307179586;
  constexpr double sr = 48000.0;
  constexpr int kN = 144000;

  struct Read {
    double gainDb, thirdDb;
  };

  const auto steady = [sr, kTwoPi, kN](double f, double db) {
    OutputStage stage;
    stage.prepare(sr);

    const double a = std::pow(10.0, db / 20.0);
    std::vector<float> l((size_t)kN), r((size_t)kN);

    for (int i = 0; i < kN; ++i)
      l[(size_t)i] = r[(size_t)i] =
          (float)(a * std::sin(kTwoPi * f * (double)i / sr));

    stage.process(l.data(), r.data(), kN, ClipType::Console, true, true);

    const int from = kN - 24000;
    std::complex<double> h1{}, h3{};
    double norm = 0.0;

    for (int i = from; i < kN; ++i) {
      const double t = (double)(i - from) / 24000.0;
      const double w =
          0.42 - 0.5 * std::cos(kTwoPi * t) + 0.08 * std::cos(2.0 * kTwoPi * t);
      const double ph = kTwoPi * f * (double)i / sr;
      h1 += w * (double)l[(size_t)i] * std::polar(1.0, -ph);
      h3 += w * (double)l[(size_t)i] * std::polar(1.0, -3.0 * ph);
      norm += w;
    }

    return Read{20.0 * std::log10(2.0 * std::abs(h1) / norm / a),
                20.0 * std::log10(std::abs(h3) / std::abs(h1))};
  };

  struct Expect {
    double f, db, gainDb, thirdDb;
  };

  // The third harmonic is only held where the model has one to speak of.
  const Expect expect[] = {
      {1000.0, -40.0, 0.0, -999.0},  {1000.0, -6.0, -0.69, -37.88},
      {1000.0, -1.0, -2.23, -39.72}, {60.0, -40.0, 0.19, -999.0},
      {60.0, -1.0, -0.94, -40.49},   {250.0, -1.0, -2.13, -37.77}};

  double worstGain = 0.0, worstThird = 0.0;

  for (const auto &e : expect) {
    const auto got = steady(e.f, e.db);

    std::printf("  %4.0f Hz at %3.0f dBFS: gain %+6.2f dB (model %+6.2f), "
                "third %7.2f dB (model %7.2f)\n",
                e.f, e.db, got.gainDb, e.gainDb, got.thirdDb, e.thirdDb);

    worstGain = std::max(worstGain, std::abs(got.gainDb - e.gainDb));

    if (e.thirdDb > -100.0)
      worstThird = std::max(worstThird, std::abs(got.thirdDb - e.thirdDb));
  }

  check(worstGain < 0.15, "the console's gain matches the model it was "
                          "fitted as, within 0.15 dB (" +
                              std::to_string(worstGain) + ")");
  check(worstThird < 1.5, "and its third harmonic within 1.5 dB (" +
                              std::to_string(worstThird) + ")");

  // ---- the compressor, as recorded ----------------------------------------
  //
  // The reduction it applies to a quiet 1 kHz tone, read as the tone's level
  // in 5 ms windows against where it stood before anything loud arrived.
  const auto reduction = [sr, kTwoPi](const std::vector<float> &x, int at) {
    const int n = 240;
    std::complex<double> h{};

    for (int i = at; i < at + n; ++i)
      h += (double)x[(size_t)i] *
           std::polar(1.0, -kTwoPi * 1000.0 * (double)i / sr);

    return 20.0 * std::log10(2.0 * std::abs(h) / (double)n);
  };

  // A -30 dBFS bed of 1 kHz on both sides, with something loud added to one
  // or both from one second in, for `seconds`.
  const auto withBurst = [sr, kTwoPi](double burstHz, double seconds,
                                      bool leftOnly, int length) {
    std::vector<float> l((size_t)length), r((size_t)length);
    const double bed = std::pow(10.0, -30.0 / 20.0);
    const double loud = std::pow(10.0, -1.0 / 20.0);
    const int start = (int)sr, stop = start + (int)(seconds * sr);

    for (int i = 0; i < length; ++i) {
      const double t = (double)i / sr;
      const double b = bed * std::sin(kTwoPi * 1000.0 * t);
      const double hit =
          i >= start && i < stop ? loud * std::sin(kTwoPi * burstHz * t) : 0.0;
      l[(size_t)i] = (float)(b + hit);
      r[(size_t)i] = (float)(b + (leftOnly ? 0.0 : hit));
    }

    OutputStage stage;
    stage.prepare(sr);
    stage.process(l.data(), r.data(), length, ClipType::Console, true, true);

    return std::make_pair(l, r);
  };

  {
    // The burst at 400 Hz on the left alone, read on the right's 1 kHz bed.
    const auto [l, r] = withBurst(400.0, 1.0, true, (int)(3.0 * sr));
    const double before = reduction(r, (int)(0.8 * sr));
    const double during = reduction(r, (int)(1.9 * sr));

    std::printf("  a loud burst on the left alone: the right moves %+.2f dB\n",
                during - before);
    check(during - before < -1.0,
          "the two sides are linked: a burst on one side turns both down");
  }

  {
    // The same burst at 60 Hz and at 400 Hz on both sides, read on the bed.
    // Over 100 ms with a taper, which keeps a loud 60 Hz out of the reading
    // of the bed's 1 kHz, where 5 ms with none lets it straight in.
    const auto bedAt = [sr, kTwoPi](const std::vector<float> &x, int at) {
      const int n = 4800;
      std::complex<double> h{};
      double norm = 0.0;

      for (int i = at; i < at + n; ++i) {
        const double t = (double)(i - at) / (double)n;
        const double w = 0.42 - 0.5 * std::cos(kTwoPi * t) +
                         0.08 * std::cos(2.0 * kTwoPi * t);
        h += w * (double)x[(size_t)i] *
             std::polar(1.0, -kTwoPi * 1000.0 * (double)i / sr);
        norm += w;
      }

      return 20.0 * std::log10(2.0 * std::abs(h) / norm);
    };

    const auto [l60, r60] = withBurst(60.0, 1.0, false, (int)(3.0 * sr));
    const auto [l4, r4] = withBurst(400.0, 1.0, false, (int)(3.0 * sr));
    const double low =
        bedAt(l60, (int)(1.8 * sr)) - bedAt(l60, (int)(0.8 * sr));
    const double mid = bedAt(l4, (int)(1.8 * sr)) - bedAt(l4, (int)(0.8 * sr));

    std::printf("  a loud burst at 60 Hz takes %.2f dB, at 400 Hz %.2f dB\n",
                -low, -mid);
    check(mid < -1.0 && low > mid + 1.5,
          "the sidechain's filter keeps a 60 Hz burst from taking what a "
          "400 Hz one takes");
  }

  {
    // Release after a 20 ms hit and after a 3 s one, as the share of the
    // reduction still there 300 ms after each ends.
    const auto left = [&](double seconds) {
      const auto [l, r] =
          withBurst(400.0, seconds, false, (int)((2.0 + seconds) * sr));
      const int end = (int)((1.0 + seconds) * sr);
      const double before = reduction(l, (int)(0.8 * sr));
      double deepest = 0.0;

      for (int at = (int)sr; at < end; at += 240)
        deepest = std::min(deepest, reduction(l, at) - before);

      return (reduction(l, end + (int)(0.3 * sr)) - before) / deepest;
    };

    const double shortHit = left(0.02), longHit = left(3.0);

    std::printf("  300 ms after a 20 ms hit %.0f%% of the reduction is left, "
                "after a 3 s hit %.0f%%\n",
                100.0 * shortHit, 100.0 * longHit);
    check(shortHit < 0.2 && longHit > 0.5,
          "the release lets go fast after a short hit and slowly after a "
          "long one");
  }
}

/// The Cassette output machine against the measured recorder: unity and the
/// lows at a quiet tone, Dolby's mistracking, the tape speed, hiss that follows
/// the notes, a fade when it is chosen mid-note, and a NaN that stays a NaN.
void testCassette() {
  section("Cassette");

  constexpr double kTwoPi = 6.283185307179586;
  constexpr double sr = 48000.0;

  // A tone through the recorder, read over its last second with a taper.
  struct Read {
    double gainDb, thirdDb;
  };

  const auto steady = [sr, kTwoPi](double f, double db) {
    constexpr int kN = 96000;
    OutputStage stage;
    stage.prepare(sr);

    const double a = std::pow(10.0, db / 20.0);
    std::vector<float> l((size_t)kN), r((size_t)kN);

    for (int i = 0; i < kN; ++i)
      l[(size_t)i] = r[(size_t)i] =
          (float)(a * std::sin(kTwoPi * f * (double)i / sr));

    stage.process(l.data(), r.data(), kN, ClipType::Cassette, true, false);

    const int from = kN - 48000;
    std::complex<double> h1{}, h3{};
    double norm = 0.0;

    for (int i = from; i < kN; ++i) {
      const double t = (double)(i - from) / 48000.0;
      const double w = 0.5 - 0.5 * std::cos(kTwoPi * t);
      norm += w;

      // Each stage starts its transport from the same place, so two tones
      // read here flutter alike and compare fairly, and over a second the
      // flutter moves a tone by less than the window can see.
      const double ph = kTwoPi * f * (double)i / sr;
      h1 += w * (double)l[(size_t)i] * std::polar(1.0, -ph);
      h3 += w * (double)l[(size_t)i] * std::polar(1.0, -3.0 * ph);
    }

    return Read{20.0 * std::log10(2.0 * std::abs(h1) / norm / a),
                20.0 * std::log10(std::abs(h3) / std::abs(h1))};
  };

  // ---- unity, and the lows first -------------------------------------------
  const auto quiet = steady(1000.0, -40.0);
  std::printf("  1 kHz at -40 dBFS: %+.2f dB\n", quiet.gainDb);
  check(std::abs(quiet.gainDb) < 0.8,
        "a quiet 1 kHz tone comes through at unity, give or take the tape's "
        "slow wobble (" +
            std::to_string(quiet.gainDb) + " dB)");

  const auto low = steady(30.0, -3.0);
  const auto mid = steady(1000.0, -3.0);
  std::printf("  third harmonic at -3 dBFS: 30 Hz %.1f dB, 1 kHz %.1f dB\n",
              low.thirdDb, mid.thirdDb);
  check(low.thirdDb > -24.0 && low.thirdDb < -16.0 && mid.thirdDb < -38.0,
        "the tape saturates the lows first, as measured: a third harmonic "
        "near -18 dB at 30 Hz and under -38 at 1 kHz");

  // ---- Dolby's mistracking --------------------------------------------------
  const auto quietTop = steady(10000.0, -36.0);
  const auto loudTop = steady(10000.0, -8.0);
  std::printf("  10 kHz: %+.2f dB at -36 dBFS, %+.2f dB at -8\n",
              quietTop.gainDb, loudTop.gainDb);
  check(loudTop.gainDb - quietTop.gainDb > 2.0,
        "quiet highs come back duller than loud ones, as the machine's Dolby "
        "does");

  // ---- the speed -----------------------------------------------------------
  {
    // A 3150 Hz tone's period over 16 cycles at a time, which is the speed the
    // tape played it back at. Sixteen rather than one, since a single cycle's
    // two crossings move with the hiss as much as with the speed. That keeps
    // what flutters below about 100 Hz, which is most of it.
    constexpr int kN = 480000;
    OutputStage stage;
    stage.prepare(sr);

    std::vector<float> l((size_t)kN), r((size_t)kN);
    const double a = std::pow(10.0, -18.0 / 20.0);

    for (int i = 0; i < kN; ++i)
      l[(size_t)i] = r[(size_t)i] =
          (float)(a * std::sin(kTwoPi * 3150.0 * (double)i / sr));

    stage.process(l.data(), r.data(), kN, ClipType::Cassette, true, false);

    std::vector<double> crossings;

    for (int i = (int)sr; i < kN; ++i) {
      const double y0 = (double)l[(size_t)i - 1], y1 = (double)l[(size_t)i];

      if (y0 < 0.0 && y1 >= 0.0)
        crossings.push_back((double)(i - 1) + y0 / (y0 - y1));
    }

    constexpr size_t kSpan = 16;
    const size_t spans = crossings.size() - kSpan;

    double mean = 0.0;
    for (size_t k = 0; k < spans; ++k)
      mean += crossings[k + kSpan] - crossings[k];
    mean /= (double)spans;

    double spread = 0.0;
    for (size_t k = 0; k < spans; ++k) {
      const double d = (crossings[k + kSpan] - crossings[k]) / mean - 1.0;
      spread += d * d;
    }

    const double rms = 100.0 * std::sqrt(spread / (double)spans);

    std::printf("  wow and flutter: %.3f%% rms\n", rms);
    check(rms > 0.06 && rms < 0.16,
          "the speed wanders by about a tenth of a percent, as the "
          "machine's did (" +
              std::to_string(rms) + "%)");
  }

  // ---- the hiss follows the notes -------------------------------------------
  {
    constexpr int kN = 96000;

    const auto noiseAbove = [sr, kN, kTwoPi](double toneDb) {
      OutputStage stage;
      stage.prepare(sr);

      std::vector<float> l((size_t)kN), r((size_t)kN);
      const double a = toneDb < -200.0 ? 0.0 : std::pow(10.0, toneDb / 20.0);

      for (int i = 0; i < kN; ++i)
        l[(size_t)i] = r[(size_t)i] =
            (float)(a * std::sin(kTwoPi * 125.0 * (double)i / sr));

      stage.process(l.data(), r.data(), kN, ClipType::Cassette, true, false);

      // What is left once the tone and its harmonics are taken out: the
      // difference between neighbouring cycles, which a steady 125 Hz tone
      // repeats every 384 samples and hiss does not.
      double power = 0.0;
      int count = 0;

      for (int i = kN / 2; i < kN; ++i) {
        const double d = (double)l[(size_t)i] - (double)l[(size_t)i - 384];
        power += d * d;
        ++count;
      }

      return 10.0 * std::log10(std::max(1.0e-30, power / (2.0 * count)));
    };

    const double silent = noiseAbove(-400.0);
    const double playing = noiseAbove(-20.0);

    std::printf("  hiss: %.1f dBFS with nothing playing, %.1f under a note\n",
                silent, playing);
    check(silent < -140.0, "there is no hiss with nothing playing");
    check(playing > -90.0 && playing < -50.0,
          "and there is while something plays, at about the tape's level");
  }

  // ---- chosen part way through a note ---------------------------------------
  //
  // It runs only while chosen, so choosing it mid-note starts it from rest,
  // with its delay empty, and the output fades to it over 30 ms. Without the
  // fade the first millisecond would be silence with a step on each side.
  {
    OutputStage stage;
    stage.prepare(sr);

    constexpr int kBlock = 256, kBlocks = 40;
    std::vector<float> l((size_t)(kBlock * kBlocks)), r(l.size());

    for (size_t i = 0; i < l.size(); ++i)
      l[i] = r[i] = (float)(0.5 * std::sin(kTwoPi * 220.0 * (double)i / sr));

    // The largest step from one sample to the next a 220 Hz sine at half
    // scale takes, with room for the tape's own colouring.
    const double allowed = 2.0 * 0.5 * kTwoPi * 220.0 / sr;
    double worst = 0.0;

    for (int b = 0; b < kBlocks; ++b) {
      const auto type = b < kBlocks / 2 ? ClipType::Soft : ClipType::Cassette;
      stage.process(l.data() + b * kBlock, r.data() + b * kBlock, kBlock, type,
                    true, false);
    }

    for (size_t i = 1; i < l.size(); ++i)
      worst = std::max(worst, std::abs((double)l[i] - (double)l[i - 1]));

    std::printf("  switched to mid-note: largest step %.4f against %.4f\n",
                worst, allowed);
    check(worst < allowed, "chosen part way through a note, it fades in "
                           "without a click");
  }

  // ---- a NaN, which must not become a table index ---------------------------
  //
  // The recorder runs on every sample whatever machine is chosen, and its limit
  // is read from a table, so a NaN arriving there must come out as a NaN and
  // not as an index into the table. A build with -fsanitize=float-cast-overflow
  // reports the conversion if it happens.
  {
    OutputStage stage;
    stage.prepare(sr);

    std::vector<float> l(64, 0.1f), r(64, 0.1f);
    l[10] = r[10] = std::numeric_limits<float>::quiet_NaN();

    stage.process(l.data(), r.data(), 64, ClipType::Soft, true, false);
    check(true, "a NaN through the output stage reads nothing it should not");
  }
}

/// The Print output machine, the console printed to the cassette recorder:
/// unity at a quiet tone, the console's third harmonic at a moderate level
/// where the deck alone is clean, tape hiss 6 dB under the deck's alone, and
/// the deck's low end at full scale.
void testPrint() {
  section("Print");

  constexpr double kTwoPi = 6.283185307179586;
  constexpr double sr = 48000.0;

  struct Read {
    double gainDb, thirdDb;
  };

  const auto steady = [sr, kTwoPi](ClipType type, double f, double db) {
    constexpr int kN = 96000;
    OutputStage stage;
    stage.prepare(sr);

    const double a = std::pow(10.0, db / 20.0);
    std::vector<float> l((size_t)kN), r((size_t)kN);

    for (int i = 0; i < kN; ++i)
      l[(size_t)i] = r[(size_t)i] =
          (float)(a * std::sin(kTwoPi * f * (double)i / sr));

    stage.process(l.data(), r.data(), kN, type, true, false);

    const int from = kN - 48000;
    std::complex<double> h1{}, h3{};
    double norm = 0.0;

    for (int i = from; i < kN; ++i) {
      const double w =
          0.5 - 0.5 * std::cos(kTwoPi * (double)(i - from) / 48000.0);
      const double ph = kTwoPi * f * (double)i / sr;
      norm += w;
      h1 += w * (double)l[(size_t)i] * std::polar(1.0, -ph);
      h3 += w * (double)l[(size_t)i] * std::polar(1.0, -3.0 * ph);
    }

    return Read{20.0 * std::log10(2.0 * std::abs(h1) / norm / a),
                20.0 * std::log10(std::abs(h3) / std::abs(h1))};
  };

  const auto quiet = steady(ClipType::Print, 1000.0, -40.0);
  check(std::abs(quiet.gainDb) < 0.8,
        "a quiet 1 kHz tone comes through Print at unity (" +
            std::to_string(quiet.gainDb) + " dB)");

  const auto printed = steady(ClipType::Print, 1000.0, -9.0);
  const auto taped = steady(ClipType::Cassette, 1000.0, -9.0);
  std::printf("  1 kHz at -9 dBFS: third harmonic %.1f dB printed, %.1f on "
              "the deck alone\n",
              printed.thirdDb, taped.thirdDb);
  check(printed.thirdDb > -45.0 && taped.thirdDb < -50.0,
        "at a moderate level it carries the console's third harmonic, where "
        "the deck alone is clean");

  // The hiss under a quiet note, what is left once the note is taken out:
  // the difference between neighbouring cycles of a 125 Hz tone, which
  // repeats every 384 samples where hiss does not. At -38 dBFS, quiet enough
  // that the flutter's own difference between cycles stays under the hiss,
  // and loud enough to hold the hiss gate open.
  const auto hissUnder = [sr, kTwoPi](ClipType type) {
    constexpr int kN = 96000;
    OutputStage stage;
    stage.prepare(sr);

    std::vector<float> l((size_t)kN), r((size_t)kN);

    for (int i = 0; i < kN; ++i)
      l[(size_t)i] = r[(size_t)i] =
          (float)(0.012 * std::sin(kTwoPi * 125.0 * (double)i / sr));

    stage.process(l.data(), r.data(), kN, type, true, false);

    double power = 0.0;

    for (int i = kN / 2; i < kN; ++i) {
      const double d = (double)l[(size_t)i] - (double)l[(size_t)i - 384];
      power += d * d;
    }

    return 10.0 * std::log10(power / (double)(kN / 2));
  };

  const double quieter =
      hissUnder(ClipType::Print) - hissUnder(ClipType::Cassette);
  std::printf("  hiss under a quiet note: %+.1f dB against the deck alone\n",
              quieter);
  check(quieter < -4.5 && quieter > -7.5,
        "its tape hiss sits about 6 dB under the deck's on its own");

  const auto low = steady(ClipType::Print, 60.0, 0.0);
  std::printf("  60 Hz at full scale: third harmonic %.1f dB\n", low.thirdDb);
  check(low.thirdDb > -25.0,
        "and at full scale the deck still saturates the lows first");
}

} // namespace

int main() {
  // Unbuffered, so a crash leaves behind everything printed up to it. ctest
  // reads the output through a pipe, which buffers it. Not _IOLBF, which the
  // Windows CRT treats as full buffering.
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  testOutputStage();
  testConsole();
  testCassette();
  testPrint();

  std::printf("\n%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
