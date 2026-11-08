# Dehli Musikk plugin core

The code the Dehli Musikk audio plugins share, so each of them uses one copy instead of keeping its own. It is plain C++17 with no dependencies, and everything in it is in the `dm` namespace.

## What is in it

- **The output stage** (`include/dm/dsp/OutputStage.h`): eight ways to keep a mix within full scale, from a plain soft clipper to a lookahead limiter. Every one of them stays within full scale however hard it is driven.
- **Console** (`Console.h`): a console's mix bus, measured from a real chain of a transformer, a bus compressor and a tape stage.
- **Cassette** (`Cassette.h`): a four-track cassette recorder with Dolby B, measured from a real deck.
- **Filters** (`Filters.h`): the first and second order sections and the antialiased curve step the two machines are built from.

[docs/output-stage.md](docs/output-stage.md) describes each machine, how the two measured ones were measured and fitted, and why the limiter looks ahead.

## Using it

A plugin pins one commit of this repository and checks it against its SHA-256, the same way it pins JUCE, so the code only changes when the plugin chooses to move to a newer commit:

```cmake
include(FetchContent)
FetchContent_Declare(dm_plugin_core
    URL      https://github.com/benjamindehli/dehli-musikk-plugin-core/archive/<commit>.tar.gz
    URL_HASH SHA256=<hash of that archive>)
FetchContent_MakeAvailable(dm_plugin_core)

target_link_libraries(MyPlugin PRIVATE dm::plugin_core)
```

To change the library and a plugin together, point the plugin at a local checkout instead of the pinned archive:

```sh
cmake -B build -DFETCHCONTENT_SOURCE_DIR_DM_PLUGIN_CORE=../dehli-musikk-plugin-core
```

Built as part of a plugin, the library compiles with its own warnings switched on, as warnings rather than errors, and does not build its tests.

## Building and testing it on its own

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Warnings are errors when the library is built on its own. With `-DDM_PLUGIN_CORE_SANITIZE=ON`, the library and its tests are built with the address and undefined behaviour sanitizers.

The tests also build with no build system at all:

```sh
c++ -std=c++17 -I include tests/output_stage_test.cpp src/dsp/*.cpp
```

The C++ is formatted with clang-format 23.1.1 in the stock LLVM style, and the docs, the readme and the workflows with the prettier pinned in `package-lock.json`. CI checks both.

## Licence

GNU Affero General Public License, version 3. See [LICENSE](LICENSE).
