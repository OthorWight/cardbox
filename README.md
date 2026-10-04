# Cardbox

Cardbox is a lightweight, scriptable Solitaire and card game engine built in C++ using Dear ImGui. It allows you to play various card games and easily create your own custom rules using Lua scripts.

## Features

* **Scriptable Games**: Write your own card games using simple Lua scripts.
* **Smooth UI**: Fast, responsive, and animated card movements.
* **Quality of Life**: Built-in undo/redo stack, smart drag-and-drop with magnetic snapping, and auto-solve mechanics.
* **Particle Effects**: Soft move sparkles, motion-driven drag trails, and timed
  victory confetti with tumbling card suits. A separate particle system handles
  fixed-step physics, DPI scaling, and a reusable buffer capped at 1,024 particles.

## Dependencies

This project relies on the following excellent open-source libraries (included in the `extern/` directory):

* Dear ImGui - Bloat-free Graphical User interface for C++ with minimal dependencies.
* GLFW - A multi-platform library for OpenGL, OpenGL ES, Vulkan, window and input.
* stb - Single-file public domain (or MIT licensed) libraries for C/C++.
* glad / OpenGL Loader
* sol2 / Lua - C++ bindings for Lua scripting.

## Building

This project uses standard build tools. Assuming you are using CMake, you can build the project by running the following commands from the root directory:

```bash
# Create a build directory
mkdir build
cd build

# Configure the project
cmake ..

# Build the project
make
```

Once built, you can run the executable generated in the `build/` directory.

Run the headless engine regression tests with:

```bash
ctest --test-dir build --output-on-failure
```

Tests are enabled by default; use `-DBUILD_TESTING=OFF` when configuring to
build only the application. Lua rules may request 1–8 decks with `NumDecks`.
Invalid vector accesses and card ranks/suits raise Lua errors. Rule files
must contain Lua source text.

## License

MIT
