// Copyright (c) 2025,Astute Systems PTY LTD
//
// This file is part of the HORAS project developed byAstute Systems.
//
// See the commercial LICENSE file in the project directory for full license details.
//
//! Zenoh publish / subscribe messages
//!
use crate::utils::dump_hex;
use tokio::time::{self, Duration};
use zenoh::Config;

///
/// Zenoh message format
///
pub enum Format {
    Thermal,
    LaserRangeFinder,
}

///
/// Open a Zenoh session
///
/// # Returns
/// * `zenoh::Session` - The Zenoh session
///
pub async fn zenoh_open(verbose: bool) -> zenoh::Session {
    let mut config: Config = Config::default();
    let mut _config_file: Result<String, std::io::Error> = Ok(String::new());
    let zenoh_config: String;
    // Check to see is the environment variable ZENOH_CONFIG is set
    if let Ok(zenoh_config_env) = std::env::var("ZENOH_CONFIG") {
        zenoh_config = zenoh_config_env;
    } else {
        zenoh_config = "/etc/horas/config.json".to_string();
    }
    if verbose {
        println!("Zenoh config file: {}", zenoh_config);
    }
    _config_file = std::fs::read_to_string(zenoh_config);

    // Read config from file /etc/horas/config.json
    if let Ok(config_str) = _config_file {
        // Parse the JSON config file
        let config_json: serde_json::Value = serde_json::from_str(&config_str).unwrap();
        // Convert the JSON config to a Zenoh config
        config = Config::from_json5(&config_json.to_string()).unwrap();
    } else {
        println!("Failed to read config file, using default config");
    }

    // Return the Zenoh session
    zenoh::open(config)
        .await
        .expect("Failed to open Zenoh session")
}

///
/// Send a raw zenoh message
///
/// # Arguments
/// * `session` - The zenoh session
/// * `topic_prefix` - The topic prefix
/// * `data` - The data to send
/// * `format` - The format of the message
/// * `verbose` - Whether to print verbose output
///
pub async fn send_topic_message(
    session: &zenoh::Session,
    topic_prefix: &str,
    data: &mut [u8],
    format: Format,
    verbose: bool,
) {
    match format {
        Format::Thermal => {
            // Calculate the checksum, sum all bytes up to last -3 bytes and mod 256
            let mut checksum_total: u64 = 0;
            for byte in data.iter().take(data.len() - 3) {
                checksum_total += *byte as u64;
            }
            let checksum = (checksum_total % 256) as u8;

            // Update the checksum
            let offset_checksum = data.len() - 3;
            data[offset_checksum] = checksum;
        }
        Format::LaserRangeFinder => {
            // Calculate the checksum, sum all bytes up to last -3 bytes and mod 256
            let mut checksum_total: u64 = 0;
            for byte in data.iter().take(data.len() - 1) {
                checksum_total += *byte as u64;
            }

            // checksum_total exclisive or of 0x50
            let checksum = (checksum_total ^ 0x50) as u8;

            // Update the checksum
            let offset_checksum = data.len() - 1;
            data[offset_checksum] = checksum;
        }
    }

    let data1: &[u8] = data;

    // Publish the data to the specified topic
    let topic_tx = format!("{}/tx/raw", topic_prefix);
    if verbose {
        println!("Publishing to topic: {}", topic_tx);
    }
    let topic = topic_tx.as_str();
    session.put(topic, data1).await.unwrap();
    if verbose {
        dump_hex(data, "TX: ");
    }
}

///
/// Send a CAN message
///
/// # Arguments
/// * `session` - The zenoh session
/// * `topic_prefix` - The topic prefix
/// * `data` - The data to send
/// * `format` - The format of the message
///
pub async fn send_topic_message_response(
    session: &zenoh::Session,
    topic_prefix: &str,
    data: &mut [u8],
    format: Format,
    verbose: bool,
) {
    // Publish the data to the specified topic
    send_topic_message(session, topic_prefix, data, format, verbose).await;

    // Wait for a response for 500ms
    let topic_rx = format!("{}/rx/raw", topic_prefix);
    if verbose {
        println!("Publishing to topic: {}", topic_rx);
    }
    let topic = topic_rx.as_str();
    let subscriber = session.declare_subscriber(topic).await.unwrap();
    let timeout_duration = Duration::from_secs(1); // Set timeout to 1 seconds

    match time::timeout(timeout_duration, subscriber.recv_async()).await {
        Ok(Ok(sample)) => {
            // Loop over bytes and copy to array
            let mut data2: Vec<u8> = vec![0; sample.payload().len()];
            for (i, byte) in sample.payload().to_bytes().iter().enumerate() {
                data2[i] = *byte;
            }
            if verbose {
                dump_hex(&data2, "RX: ");
            }

            // If first bytes is 0x55
            if data2[0] == 0x59 {
                // Match second byte
                match data2[1] as u8 {
                    0xCC => {
                        // Loop over three measurements for the range and level
                        for i in 0..3 {
                            let offset = i * 6;
                            // 3 bytes 2-5 in the data
                            let measurement: f32 = f32::from_bits(
                                ((data2[5 + offset] as u32) << 24)
                                    | ((data2[4 + offset] as u32) << 16)
                                    | ((data2[3 + offset] as u32) << 8)
                                    | (data2[2 + offset] as u32),
                            );
                            // Signal level
                            let signal_level: u16 =
                                (data2[6 + offset] as u16) | ((data2[7 + offset] as u16) << 8);
                            println!(
                                "Range Measurement {}, Signal Level {}",
                                measurement, signal_level
                            );
                        }
                    }
                    0x01 => {
                        println!("RX: Sight LRF");
                    }
                    _ => {
                        // Do nothing
                    }
                }
            }
            return;
        }
        Ok(Err(e)) => {
            // Error while receiving a message
            eprintln!("Error receiving message: {}", e);
        }
        Err(_) => {
            // Timeout occurred, return
            return;
        }
    }
}

///
/// Send a sight thermal message
///
/// # Arguments
/// * `session` - The zenoh session
/// * `hostname` - The hostname
/// * `data` - The data to send
/// * `verbose` - Whether to print verbose output
///
pub async fn send_sight_thermal_message(
    session: &zenoh::Session,
    hostname: &str,
    data: &mut [u8],
    verbose: bool,
) {
    let prefix = format!("{}/sight/thermal", hostname);
    send_topic_message(session, &prefix, data, Format::Thermal, verbose).await;
    println!("Done response");
}

///
/// Send a sight thermal message and wait for a response
///
/// # Arguments
/// * `session` - The zenoh session
/// * `hostname` - The hostname
/// * `data` - The data to send
/// * `verbose` - Whether to print verbose output
///
pub async fn send_sight_thermal_message_response(
    session: &zenoh::Session,
    hostname: &str,
    data: &mut [u8],
    verbose: bool,
) {
    let prefix = format!("{}/sight/thermal", hostname);
    send_topic_message_response(session, &prefix, data, Format::Thermal, verbose).await;
    println!("Done response");
}

///
/// Send a sight LRF message and wait for a response
///
/// # Arguments
/// * `session` - The zenoh session
/// * `hostname` - The hostname
/// * `data` - The data to send
/// * `verbose` - Whether to print verbose output
///
pub async fn send_sight_lrf_message_response(
    session: &zenoh::Session,
    hostname: &str,
    data: &mut [u8],
    verbose: bool,
) {
    let prefix = format!("{}/sight/lrf", hostname);
    send_topic_message_response(session, &prefix, data, Format::LaserRangeFinder, verbose).await;
}

///
/// Send a sight LRF message
///
/// # Arguments
/// * `session` - The zenoh session
/// * `hostname` - The hostname
/// * `data` - The data to send
/// * `verbose` - Whether to print verbose output
///
pub async fn send_sight_lrf_message(
    session: &zenoh::Session,
    hostname: &str,
    data: &mut [u8],
    verbose: bool,
) {
    let prefix = format!("{}/sight/lrf", hostname);
    send_topic_message(session, &prefix, data, Format::LaserRangeFinder, verbose).await;
}
