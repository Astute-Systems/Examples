// Copyright (c) 2025, Advent Atum PTY LTD
//
// This file is part of the HORAS project developed by Advent Atum.
//
// See the commercial LICENSE file in the project directory for full license details.
//
//! A simple example of using Zenoh to send and receive messages.
//!
//! Zenoh is a publish/subscribe messaging system that allows for
//! communication between different components in a distributed system.
//! Our Zenoh uses a router hosted on a DigitalOcean server encrypted with
//! TLS 1.3 and authenticated with a certificate. To install the config and
//! certificate, run the following command:
//!
//! ```bash
//! cargo deb --install -p common
//! ```
//!
//! This installs the config and certificate in the ```/etc/horas``` directory.
//!
//! # Example publisher:
//!
//! ```rust
//! use common::common_can::CANMessage;
//! use protobuf::Message; // Import for Protobuf serialization/deserialization
//!
//! let session = common::zenoh::zenoh_open(false).await;
//! let subscriber = session.declare_subscriber("can0/tx/raw").await.unwrap();
//! let can_message = CANMessage::parse_from_bytes(&sample.payload().to_bytes()).unwrap();
//! subscriber.put("can0/tx/raw", serialized_message).await.unwrap();
//! ```
//!

use common::common_can::CANMessage;
use protobuf::Message; // Import for Protobuf serialization/deserialization

#[tokio::main]
async fn main() {
    let session = common::zenoh::zenoh_open(false).await;
    let subscriber = session.declare_subscriber("can0/tx/raw").await.unwrap();

    while let Ok(sample) = subscriber.recv_async().await {
        // Deserialize the message from bytes
        let can_message = CANMessage::parse_from_bytes(&sample.payload().to_bytes()).unwrap();

        // Print the message
        println!("Received CAN message:");
        println!("ID: {:#X}", can_message.id);
        println!("Data: {:?}", can_message.data);
        println!("Is Extended ID: {}", can_message.is_extended_id);

        // Print the timestamp in human-readable format
        let timestamp = can_message.timestamp;

        // Convert the timestamp (milliseconds since Unix epoch) to a NaiveDateTime
        if let Some(datetime) = chrono::DateTime::from_timestamp_millis(timestamp as i64) {
            println!("Timestamp: {}", datetime);
        } else {
            println!("Invalid timestamp: {}", timestamp);
        }
    }
}
