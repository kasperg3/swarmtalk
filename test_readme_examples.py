#!/usr/bin/env python3
"""
Test script to verify that the README examples work correctly.
This script tests the ByteArray message format and basic ROS 2 functionality.
"""

import rclpy
from rclpy.node import Node
from common_interface.msg import ByteArray
import struct

class StatusMessage:
    def __init__(self, agent_id: int, rssi: int):
        self.agent_id = agent_id
        self.rssi = rssi

    def serialize(self) -> bytes:
        return struct.pack("ii", self.agent_id, self.rssi)

    @staticmethod
    def deserialize(data: bytes) -> 'StatusMessage':
        agent_id, rssi = struct.unpack("ii", data)
        return StatusMessage(agent_id, rssi)

def test_bytearray_format():
    """Test the ByteArray message format conversion"""
    print("Testing ByteArray format conversion...")
    
    # Test data
    test_data = b"Hello from UAV!"
    
    # Convert to ByteArray format (as used in SwarmTalk)
    byte_array_msg = ByteArray(bytes=[bytes([b]) for b in test_data])
    
    # Convert back to bytes
    recovered_data = bytes(b''.join(byte_array_msg.bytes))
    
    assert test_data == recovered_data, f"Data mismatch: {test_data} != {recovered_data}"
    print("✅ ByteArray format conversion works correctly")

def test_status_message():
    """Test the StatusMessage serialization/deserialization"""
    print("Testing StatusMessage serialization...")
    
    # Create status message
    original = StatusMessage(agent_id=42, rssi=-65)
    
    # Serialize
    serialized = original.serialize()
    
    # Deserialize
    recovered = StatusMessage.deserialize(serialized)
    
    assert original.agent_id == recovered.agent_id, f"Agent ID mismatch: {original.agent_id} != {recovered.agent_id}"
    assert original.rssi == recovered.rssi, f"RSSI mismatch: {original.rssi} != {recovered.rssi}"
    print("✅ StatusMessage serialization works correctly")

def test_bytearray_with_status_message():
    """Test combining StatusMessage with ByteArray format"""
    print("Testing StatusMessage with ByteArray format...")
    
    # Create and serialize status message
    status = StatusMessage(agent_id=1, rssi=-50)
    data = status.serialize()
    
    # Convert to ByteArray format
    msg = ByteArray(bytes=[bytes([b]) for b in data])
    
    # Convert back and deserialize
    recovered_data = bytes(b''.join(msg.bytes))
    recovered_status = StatusMessage.deserialize(recovered_data)
    
    assert status.agent_id == recovered_status.agent_id
    assert status.rssi == recovered_status.rssi
    print("✅ StatusMessage with ByteArray format works correctly")

class ExamplePublisher(Node):
    def __init__(self):
        super().__init__('example_publisher')
        self.publisher = self.create_publisher(ByteArray, '/communication/broadcast', 10)
        
    def send_message(self, data: bytes):
        # Convert bytes to the ByteArray format used by SwarmTalk
        msg = ByteArray(bytes=[bytes([b]) for b in data])
        self.publisher.publish(msg)
        self.get_logger().info(f'Sent {len(data)} bytes')

class ExampleSubscriber(Node):
    def __init__(self):
        super().__init__('example_subscriber')
        self.subscription = self.create_subscription(
            ByteArray, '/communication/message', self.message_callback, 10)
        self.received_messages = []
            
    def message_callback(self, msg):
        # Convert ByteArray back to bytes
        data = bytes(b''.join(msg.bytes))
        try:
            decoded_message = data.decode('utf-8')
            self.get_logger().info(f'Received: {decoded_message}')
            self.received_messages.append(decoded_message)
        except UnicodeDecodeError:
            self.get_logger().info(f'Received binary data: {len(data)} bytes')
            self.received_messages.append(data)

def test_ros_nodes():
    """Test that the ROS 2 nodes can be created without errors"""
    print("Testing ROS 2 node creation...")
    
    rclpy.init()
    
    try:
        # Test publisher creation
        publisher = ExamplePublisher()
        print("✅ ExamplePublisher created successfully")
        
        # Test subscriber creation
        subscriber = ExampleSubscriber()
        print("✅ ExampleSubscriber created successfully")
        
        # Test message sending (will publish to topic)
        test_message = b"Test message"
        publisher.send_message(test_message)
        print("✅ Message sending works without errors")
        
    finally:
        rclpy.shutdown()

def main():
    """Run all tests"""
    print("Running SwarmTalk README examples verification...\n")
    
    try:
        test_bytearray_format()
        test_status_message()
        test_bytearray_with_status_message()
        test_ros_nodes()
        
        print("\n🎉 All tests passed! The README examples should work correctly.")
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        return 1
    
    return 0

if __name__ == '__main__':
    exit(main())
