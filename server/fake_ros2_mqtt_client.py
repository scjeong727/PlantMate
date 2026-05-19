#!/usr/bin/env python3
import argparse
import socket
import struct
import sys
import time


def encode_remaining_length(length):
    encoded = bytearray()
    while True:
        digit = length % 128
        length //= 128
        if length > 0:
            digit |= 0x80
        encoded.append(digit)
        if length == 0:
            return bytes(encoded)


def pack_utf8(value):
    data = value.encode("utf-8")
    return struct.pack("!H", len(data)) + data


def read_exact(sock, size):
    chunks = bytearray()
    while len(chunks) < size:
        chunk = sock.recv(size - len(chunks))
        if not chunk:
            raise ConnectionError("connection closed")
        chunks.extend(chunk)
    return bytes(chunks)


def read_packet(sock):
    first = read_exact(sock, 1)[0]
    multiplier = 1
    remaining = 0
    while True:
        digit = read_exact(sock, 1)[0]
        remaining += (digit & 0x7F) * multiplier
        if (digit & 0x80) == 0:
            break
        multiplier *= 128
        if multiplier > 128 * 128 * 128:
            raise ValueError("invalid MQTT remaining length")
    return first, read_exact(sock, remaining)


def send_connect(sock, client_id):
    variable_header = (
        pack_utf8("MQTT")
        + bytes([4, 2])
        + struct.pack("!H", 60)
    )
    payload = pack_utf8(client_id)
    body = variable_header + payload
    sock.sendall(bytes([0x10]) + encode_remaining_length(len(body)) + body)

    packet_type, body = read_packet(sock)
    if packet_type != 0x20 or len(body) < 2 or body[1] != 0:
        raise RuntimeError(f"MQTT connect failed: packet=0x{packet_type:02x} body={body!r}")


def send_subscribe(sock, topic):
    packet_id = 1
    body = struct.pack("!H", packet_id) + pack_utf8(topic) + bytes([0])
    sock.sendall(bytes([0x82]) + encode_remaining_length(len(body)) + body)

    packet_type, body = read_packet(sock)
    if packet_type != 0x90 or len(body) < 3 or body[2] == 0x80:
        raise RuntimeError(f"MQTT subscribe failed: packet=0x{packet_type:02x} body={body!r}")


def parse_publish(body):
    if len(body) < 2:
        raise ValueError("invalid publish packet")
    topic_len = struct.unpack("!H", body[:2])[0]
    topic_start = 2
    topic_end = topic_start + topic_len
    if topic_end > len(body):
        raise ValueError("invalid publish topic length")
    topic = body[topic_start:topic_end].decode("utf-8", errors="replace")
    payload = body[topic_end:].decode("utf-8", errors="replace")
    return topic, payload


def main():
    parser = argparse.ArgumentParser(
        description="Fake ROS2/Jetrover MQTT client. Subscribes to one server command topic and prints each payload as one line."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--device-type", default="water")
    parser.add_argument("--device-id", default="jetrover-1")
    parser.add_argument("--action", default="water")
    parser.add_argument("--topic", help="Override full MQTT topic")
    args = parser.parse_args()

    topic = args.topic or f"device/{args.device_type}/{args.device_id}/{args.action}/command"
    client_id = f"fake-ros2-{args.device_type}-{args.device_id}-{int(time.time())}"

    try:
        sock = socket.create_connection((args.host, args.port), timeout=10)
    except ConnectionRefusedError:
        print(
            f"cannot connect to MQTT broker at {args.host}:{args.port}; "
            "start ./server first and check that it prints 'mqtt adapter listening on 1883'",
            file=sys.stderr,
        )
        return 1
    except OSError as exc:
        print(f"cannot connect to MQTT broker at {args.host}:{args.port}: {exc}", file=sys.stderr)
        return 1

    with sock:
        try:
            send_connect(sock, client_id)
            send_subscribe(sock, topic)
            sock.settimeout(None)
            print(f"listening {args.host}:{args.port} topic={topic}", flush=True)

            while True:
                packet_type, body = read_packet(sock)
                if packet_type >> 4 == 3:
                    _topic, payload = parse_publish(body)
                    print(payload, flush=True)
        except ConnectionError as exc:
            print(f"MQTT connection closed: {exc}", file=sys.stderr)
            return 1


if __name__ == "__main__":
    try:
        sys.exit(main() or 0)
    except KeyboardInterrupt:
        sys.exit(0)
