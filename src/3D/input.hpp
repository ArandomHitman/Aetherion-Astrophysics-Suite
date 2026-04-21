#pragma once
// ============================================================
// input.hpp — Input state abstraction
// ============================================================

#include <SFML/Window/Keyboard.hpp>

// Movement / modifier key state, updated each frame from SFML events.
struct KeyState {
    bool w    = false;
    bool a    = false;
    bool s    = false;
    bool d    = false;
    bool up   = false;  // Space
    bool down = false;  // LControl
    bool fast = false;  // LShift
    bool q    = false;  // Roll left
    bool e    = false;  // Roll right

    void onKeyPressed(sf::Keyboard::Key code) {
        switch (code) {
            case sf::Keyboard::Key::W:        w = true;    break;
            case sf::Keyboard::Key::A:        a = true;    break;
            case sf::Keyboard::Key::S:        s = true;    break;
            case sf::Keyboard::Key::D:        d = true;    break;
            case sf::Keyboard::Key::Q:        q = true;    break;
            case sf::Keyboard::Key::E:        e = true;    break;
            case sf::Keyboard::Key::Space:    up = true;   break;
            case sf::Keyboard::Key::LControl: down = true; break;
            case sf::Keyboard::Key::LShift:   fast = true; break;
            default: break;
        }
    }

    void onKeyReleased(sf::Keyboard::Key code) {
        switch (code) {
            case sf::Keyboard::Key::W:        w = false;    break;
            case sf::Keyboard::Key::A:        a = false;    break;
            case sf::Keyboard::Key::S:        s = false;    break;
            case sf::Keyboard::Key::D:        d = false;    break;
            case sf::Keyboard::Key::Q:        q = false;    break;
            case sf::Keyboard::Key::E:        e = false;    break;
            case sf::Keyboard::Key::Space:    up = false;   break;
            case sf::Keyboard::Key::LControl: down = false; break;
            case sf::Keyboard::Key::LShift:   fast = false; break;
            default: break;
        }
    }
};
// fun fact: I think this is the shortest file in my entire repo!