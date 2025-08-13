#!/usr/bin/env python3
"""
Test SwarmTalk message format without ROS 2 dependencies
"""
import struct

# Mock ByteArray class (matches ROS 2 ByteArray)
class ByteArray:
    def __init__(self, bytes):
        self.bytes = bytes

def test_message_format():
    # Test data conversion
    test_data = b"Hello from UAV!"
    
    # Convert to ByteArray format (as used in SwarmTalk)
    msg = ByteArray(bytes=[bytes([b]) for b in test_data])
    
    # Convert back to bytes
    recovered = bytes(b''.join(msg.bytes))
    
    assert test_data == recovered
    print(f"✅ Sent: {test_data}")
    print(f"✅ Received: {recovered}")

if __name__ == '__main__':
    test_message_format()
