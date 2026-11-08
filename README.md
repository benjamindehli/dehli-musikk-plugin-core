# Dehli Musikk plugin core

The code the Dehli Musikk audio plugins share, so each of them uses one copy instead of keeping its own. It comes in two parts. The DSP is plain C++17 with no dependencies. The look and the controls are a JUCE module. Everything is in the `dm` namespace.

## What is in it

- **The output stage** (`include/dm/dsp/OutputStage.h`): eight ways to keep a mix within full scale, from a plain soft clipper to a lookahead limiter. Every one of them stays within full scale however hard it is driven.
- **Console** (`Console.h`): a console's mix bus, measured from a real chain of a transformer, a bus compressor and a tape stage.
- **Cassette** (`Cassette.h`): a four-track cassette recorder with Dolby B, measured from a real deck.
- **Filters** (`Filters.h`): the first and second order sections and the antialiased curve step the two machines are built from.

[docs/output-stage.md](docs/output-stage.md) describes each machine, how the two measured ones were measured and fitted, and why the limiter looks ahead.

The `dm_gui` JUCE module (`modules/dm_gui`), in `dm::ui`, has the look the plugins share:

- **The look and feel** (`LookAndFeel.h`): the knob, the fader, the lamp caps, the combo box, the standalone's title bar and the fonts, and the shading the panels are drawn with, such as the recess round a screen and the pocket behind a group.
- **The controls** (`Controls.h`): a knob with and without a caption, a segment readout, a level meter, a stereo output meter and an activity lamp.
- **The lamp buttons and the palette** (`Theme.h`): a button drawn as a lit plastic cap, a switch drawn as a letter on a screen, the colours, and the grain laid over the panels.
- **The paint cache** (`PaintCache.h`): a control's last two looks, kept as pictures, so a window full of controls is copied rather than drawn again.

## Using it

A plugin pins one commit of this repository and checks it against its SHA-256, the same way it pins JUCE, so the code only changes when the plugin chooses to move to a newer commit:

```cmake
include(FetchContent)
FetchContent_Declare(dm_plugin_core
    URL      https://github.com/benjamindehli/dehli-musikk-plugin-core/archive/<commit>.tar.gz
    URL_HASH SHA256=<hash of that archive>)
FetchContent_MakeAvailable(dm_plugin_core)

target_link_libraries(MyPlugin PRIVATE dm::plugin_core dm::dm_gui)
```

`dm::dm_gui` is there only when JUCE has been added before the library, and a plugin compiles it with its own JUCE settings, as it does JUCE's own modules. Include it as `<dm_gui/dm_gui.h>`.

To change the library and a plugin together, point the plugin at a local checkout instead of the pinned archive:

```sh
cmake -B build -DFETCHCONTENT_SOURCE_DIR_DM_PLUGIN_CORE=../dehli-musikk-plugin-core
```

Built as part of a plugin, the DSP compiles with the library's own warnings switched on, as warnings rather than errors, and the tests are not built. The GUI module is compiled as part of the plugin, so it gets the plugin's warnings.

## Building and testing it on its own

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Warnings are errors when the library is built on its own. With `-DDM_PLUGIN_CORE_SANITIZE=ON`, the library and its tests are built with the address and undefined behaviour sanitizers.

The GUI module's test needs JUCE, so it is off unless asked for. With `-DDM_PLUGIN_CORE_GUI_TESTS=ON`, JUCE is downloaded at the commit `CMakeLists.txt` pins, or taken from `-DJUCE_PATH`, and the test paints every control with no window or display. On Linux it needs the headers JUCE's graphics use, which the workflow lists.

The output stage's test also builds with no build system at all:

```sh
c++ -std=c++17 -I include tests/output_stage_test.cpp src/dsp/*.cpp
```

The C++ is formatted with clang-format 23.1.1 in the stock LLVM style, and the docs, the readme and the workflows with the prettier pinned in `package-lock.json`. CI checks both.

## Licence

GNU Affero General Public License, version 3. See [LICENSE](LICENSE).
