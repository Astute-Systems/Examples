#include <iostream>
#include <chrono>
#include <zenoh/zenoh.hpp>
#include "common_can.pb.h" // Generated Protobuf header

int main()
{
    try
    {
        // Initialize Zenoh
        zenoh::Config config;
        auto session = zenoh::open(config);

        // Create a Protobuf CAN message
        CANMessage can_message;
        can_message.set_id(0x123);

        // Add multiple bytes to the repeated field
        for (int i = 0; i < 8; ++i)
        {
            can_message.add_data(0xFF);
        }
        can_message.mutable_data()->Set(7, 0xFC); // Set the last byte to 0xFC
        can_message.set_is_extended_id(false);

        // Add UTC timestamp in milliseconds since Unix epoch
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        can_message.set_timestamp(timestamp);

        // Serialize the message to bytes
        std::string serialized_message;
        if (!can_message.SerializeToString(&serialized_message))
        {
            std::cerr << "Failed to serialize CAN message." << std::endl;
            return 1;
        }

        // Publish the message
        session.put("can0/tx/raw", serialized_message);

        std::cout << "Message published successfully." << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}