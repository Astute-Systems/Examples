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
//! use chrono::Utc;
//! use common::common_can::CANMessage;
//! use protobuf::Message; // Add this import for UTC timestamp
//!
//! let session = common::zenoh::zenoh_open(false).await;
//! let mut can_message = CANMessage::new();
//! can_message.id = 0x123;
//! can_message.data = vec![vec![0x01, 0x02, 0x03, 0x04]];
//! can_message.is_extended_id = false;
//! can_message.timestamp = Utc::now().timestamp_millis() as u64;
//!
//! let serialized_message = can_message.write_to_bytes().unwrap();
//! session
//!     .put("can0/tx/raw", serialized_message)
//!     .await
//!     .unwrap();
//! ```

use chrono::Utc;
use common::common_can::CANMessage;
use protobuf::Message; // Add this import for UTC timestamp

#[tokio::main]
async fn main() {
    // Init logger
    let session = common::zenoh::zenoh_open(false).await;

    // Create a protobuf CAN message
    let mut can_message = CANMessage::new();
    can_message.id = 0x123;
    can_message.data = vec![vec![0x01, 0x02, 0x03, 0x04]];
    can_message.is_extended_id = false;
    // Add UTC timestamp in milliseconds since Unix epoch
    can_message.timestamp = Utc::now().timestamp_millis() as u64;

    // Serialize the message to bytes
    let serialized_message = can_message.write_to_bytes().unwrap();

    session
        .put("can0/tx/raw", serialized_message)
        .await
        .unwrap();
    session.close().await.unwrap();

    println!("Message published successfully!");
}
