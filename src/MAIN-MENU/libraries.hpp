/*-----
    This file is meant to be included in all source files in the MAIN-MENU part of the Aetherion project
    It includes all necessary headers and libraries so we don't have to keep calling them. Niche thing, but works for efficiency sake.
    If you want to add a library, just do it here for simplicity.
    - Liam (Head developer/founder of project)
-----*/

// imgui - included as to not rely on opengl/vulkan shaders, both are an absolute dumpster fire to cross-port & maintain.

#include "imgui.h" // for the core ImGui functionality, which provides the immediate mode GUI system that we use for our user interface elements like buttons, sliders, and text displays.
#include "imgui-SFML.h" // for compatibility with SFML. It allows us to use ImGui's immediate mode GUI features within our SFML application, making it easier to create and manage user interfaces without having to deal with the complexities of OpenGL or Vulkan shaders directly.

// SFML - rendering and window management

#include <SFML/Graphics.hpp> // for rendering, textures, sprites, shapes, and all that good stuff.
#include <SFML/Window.hpp> // for window management, event handling, and input processing (keyboard, mouse, etc.)
#include <SFML/System.hpp> // for time management, vectors, and other utility functions that SFML provides, which are useful

// C++ - I think we can put two and two together for this one.

#include <iostream> // for debugging and logging purposes, also pretty much essential.
#include <vector> // for storing our bodies, buttons, and other data structures.
#include <cmath> // for graphical scaling logic in accordance with refresh rate, window size, etc.
#include <string> // for std::string, used in the UI and such.
#include <memory> // memory management since we're using a lot of pointers for the bodies and such, we need this for smart pointers to avoid memory leaks.

/*-------------------------END OF FILE-------------------------*/

