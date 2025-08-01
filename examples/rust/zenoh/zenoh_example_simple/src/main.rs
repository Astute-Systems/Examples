//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
//! A simple example of using Zenoh to send and receive messages.
#[tokio::main]
async fn main() {
    let session = common::zenoh::zenoh_open(false).await;
    println!("Zenoh session opened");

    // Declare the subscriber first
    let subscriber = session.declare_subscriber("can0/tx/raw").await.unwrap();
    println!("Subscriber declared");

    // Publish the value
    session.put("can0/tx/raw", "value").await.unwrap();
    println!("Published value");

    // Receive the value
    while let Ok(sample) = subscriber.recv_async().await {
        println!("Received: {:?}", sample);
    }

    // Wait for more messages
    while let Ok(sample) = subscriber.recv_async().await {
        println!("Received: {:?}", sample);
    }

    session.close().await.unwrap();
}
