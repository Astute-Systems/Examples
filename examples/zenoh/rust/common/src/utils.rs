// Copyright (c) 2025,Astute Systems PTY LTD
//
// This file is part of the HORAS project developed byAstute Systems.
//
// See the commercial LICENSE file in the project directory for full license details.
//
//! Common utility functions
//!

use std::process::Command;
use sysinfo::System;

///
/// Dump hex and ASCII data with the first column being the offset
///
/// # Arguments
/// * `data` - The data to be dumped.
/// * `prefix` - The prefix to be printed before the offset.
///
pub fn dump_hex(data: &[u8], prefix: &str) {
    let mut offset = 0;

    for chunk in data.chunks(16) {
        // Print the prefix and offset
        print!("{}{:08X} ", prefix, offset);
        offset += chunk.len();

        // Print hex values in uppercase
        for byte in chunk {
            print!("{:02X} ", byte);
        }

        // Print ASCII representation
        print!(" | ");
        for byte in chunk {
            if byte.is_ascii() && byte.is_ascii_graphic() {
                print!("{}", *byte as char);
            } else {
                print!(".");
            }
        }
        println!();
    }
}

///
/// Set the GPIO line to a specific value using the gpioset command.
///
/// # Arguments
/// * `chip` - The GPIO chip number.
/// * `line` - The GPIO line number.
/// * `value` - The value to set (0 or 1).
///
pub fn set_gpio(chip: u32, line: u32, value: u32) {
    let gpio_arg = format!("{}={}", line, value);
    let output = Command::new("gpioset")
        .arg(chip.to_string()) // GPIO chip
        .arg(gpio_arg) // GPIO line=value
        .output();

    match output {
        Ok(output) => {
            if output.status.success() {
                println!("GPIO {} on chip {} set to {}", line, chip, value);
            } else {
                eprintln!(
                    "Failed to set GPIO: {}",
                    String::from_utf8_lossy(&output.stderr)
                );
            }
        }
        Err(e) => {
            eprintln!("Error executing gpioset: {}", e);
        }
    }
}

///
/// Get the hostname of the system.
///
/// # Returns
/// * `String` - The hostname of the system.
///
pub fn get_hostname() -> String {
    let mut sys = System::new_all();
    // First we update all information of our `System` struct.
    sys.refresh_all();
    let hostname = System::host_name();
    hostname.unwrap_or_else(|| "unknown".to_string())
}

///
/// Convert a floating-point number to an unsigned integer.
///
/// # Arguments
/// * `value` - The floating-point value to convert.
///
/// # Returns
///
pub fn decimal_to_dms(decimal: f64) -> (i32, i32, f64) {
    let degrees = decimal.trunc() as i32;
    let minutes = ((decimal.abs() - degrees.abs() as f64) * 60.0).trunc() as i32;
    let seconds = ((decimal.abs() - degrees.abs() as f64) * 60.0 - minutes as f64) * 60.0;
    (degrees, minutes, seconds)
}
