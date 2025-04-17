//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
/// \brief An example showing how to read every button on an Xbox 360 joystick using SDL2
///
/// \file joystick.cc
///

#include <SDL2/SDL.h>
#include <SDL2/SDL_joystick.h>

int main(int argc, char **argv) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
        return 1;
    }
    
    // Open the joystick
    SDL_Joystick *joystick = SDL_JoystickOpen(0);
    if (!joystick) {
        SDL_Log("Unable to open joystick: %s", SDL_GetError());
        return 1;
    }
    
    // Print joystick information
    SDL_Log("Joystick Name: %s", SDL_JoystickName(joystick));
    SDL_Log("Number of Axes: %d", SDL_JoystickNumAxes(joystick));
    SDL_Log("Number of Buttons: %d", SDL_JoystickNumButtons(joystick));
    
    // Main loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = false;
        } else if (event.type == SDL_JOYAXISMOTION) {
            // Axis motion event
            int axis = event.jaxis.axis;
            int value = event.jaxis.value;
            SDL_Log("Axis %d moved to %d", axis, value);
        } else if (event.type == SDL_JOYBUTTONDOWN) {
            // Button down event
            int button = event.jbutton.button;
            SDL_Log("Button %d pressed", button);
        } else if (event.type == SDL_JOYBUTTONUP) {
            // Button up event
            int button = event.jbutton.button;
            SDL_Log("Button %d released", button);
        }
        }
        // Sleep for a short time to avoid busy waiting
        SDL_Delay(10);
    }
    
    // Close the joystick and quit SDL
    SDL_JoystickClose(joystick);
    SDL_Quit();
    
    return 0;
    }