# Cardbox

Cardbox is a lightweight, scriptable Solitaire and card game engine built in C++ using Dear ImGui. It allows you to play various card games and easily create your own custom rules using Lua scripts.

## Features

* **Scriptable Games**: Write your own card games using simple Lua scripts.
* **Smooth UI**: Fast, responsive, and animated card movements.
* **Quality of Life**: Built-in undo/redo stack, smart drag-and-drop with magnetic snapping, and auto-solve mechanics.

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

## License

MIT