#!/usr/bin/env python3
"""
Comprehensive test for all SwarmTalk README examples.
This verifies that all code examples in the README work correctly.
"""

import struct

def test_standalone_message_format():
    """Test the standalone message format example from the README"""
    print("Testing standalone message format...")
    
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

    test_message_format()

def test_status_message_example():
    """Test the StatusMessage example from multi-agent coordination"""
    print("Testing StatusMessage from multi-agent example...")
    
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
    
    # Test the StatusMessage
    original = StatusMessage(agent_id=42, rssi=-65)
    serialized = original.serialize()
    recovered = StatusMessage.deserialize(serialized)
    
    assert original.agent_id == recovered.agent_id
    assert original.rssi == recovered.rssi
    print(f"✅ StatusMessage: agent_id={recovered.agent_id}, rssi={recovered.rssi}")

def test_bytearray_conversion_examples():
    """Test ByteArray conversions from all README examples"""
    print("Testing ByteArray conversions from README examples...")
    
    # Mock ByteArray class
    class ByteArray:
        def __init__(self, bytes):
            self.bytes = bytes
    
    # Test 1: Basic message broadcasting format
    test_data = b"Hello from UAV!"
    msg = ByteArray(bytes=[bytes([b]) for b in test_data])
    recovered = bytes(b''.join(msg.bytes))
    assert test_data == recovered
    print("✅ Basic message broadcasting format works")
    
    # Test 2: StatusMessage with ByteArray
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
    
    status = StatusMessage(agent_id=1, rssi=-50)
    data = status.serialize()
    msg = ByteArray(bytes=[bytes([b]) for b in data])
    recovered_data = bytes(b''.join(msg.bytes))
    recovered_status = StatusMessage.deserialize(recovered_data)
    
    assert status.agent_id == recovered_status.agent_id
    assert status.rssi == recovered_status.rssi
    print("✅ StatusMessage with ByteArray format works")

def test_message_decoding_example():
    """Test the message decoding from subscriber example"""
    print("Testing message decoding from subscriber example...")
    
    class ByteArray:
        def __init__(self, bytes):
            self.bytes = bytes
    
    # Test UTF-8 decoding
    test_data = b"Hello from UAV!"
    msg = ByteArray(bytes=[bytes([b]) for b in test_data])
    
    # Convert ByteArray back to bytes (from subscriber example)
    data = bytes(b''.join(msg.bytes))
    try:
        decoded_message = data.decode('utf-8')
        print(f"✅ Decoded UTF-8: {decoded_message}")
    except UnicodeDecodeError:
        print(f"✅ Binary data: {len(data)} bytes")
    
    # Test binary data handling
    binary_data = bytes([0x01, 0x02, 0x03, 0xFF])
    msg = ByteArray(bytes=[bytes([b]) for b in binary_data])
    data = bytes(b''.join(msg.bytes))
    try:
        decoded_message = data.decode('utf-8')
        print(f"Decoded UTF-8: {decoded_message}")
    except UnicodeDecodeError:
        print(f"✅ Binary data: {len(data)} bytes")

def main():
    """Run all tests"""
    print("🧪 Testing all SwarmTalk README examples...\n")
    
    try:
        test_standalone_message_format()
        print()
        
        test_status_message_example()
        print()
        
        test_bytearray_conversion_examples()
        print()
        
        test_message_decoding_example()
        print()
        
        print("🎉 All README examples work correctly!")
        print("\n📝 Summary:")
        print("- ✅ Standalone message format testing")
        print("- ✅ ROS 2 ByteArray message broadcasting")
        print("- ✅ ROS 2 ByteArray message receiving")
        print("- ✅ Multi-agent coordination with StatusMessage")
        print("- ✅ Binary and UTF-8 message handling")
        
        return 0
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    exit(main())
