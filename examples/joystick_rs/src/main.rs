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
/// \file joystick_rs.cc
///
use gilrs::{Event, EventType, Gilrs};

fn main() {
    let mut gilrs = Gilrs::new().unwrap();

    // Iterate over all connected gamepads
    for (_id, gamepad) in gilrs.gamepads() {
        println!("{} is {:?}", gamepad.name(), gamepad.power_info());
    }

    loop {
        // Examine new events
        while let Some(Event {
            id, event, time, ..
        }) = gilrs.next_event()
        {
            println!("{:?} New event from {}: {:?}", time, id, event);

            // Print every action
            match event {
                EventType::ButtonPressed(button, _) => {
                    println!("Button {:?} pressed", button);
                }
                EventType::ButtonReleased(button, _) => {
                    println!("Button {:?} released", button);
                }
                EventType::AxisChanged(axis, value, _) => {
                    println!("Axis {:?} changed to {}", axis, value);
                }
                EventType::Connected => {
                    println!("Gamepad connected");
                }
                EventType::Disconnected => {
                    println!("Gamepad disconnected");
                }
                _ => {
                    println!("Other event: {:?}", event);
                }
            }
        }
    }
}
