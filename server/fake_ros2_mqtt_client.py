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


def send_subscribe(sock, topic, packet_id):
    body = struct.pack("!H", packet_id) + pack_utf8(topic) + bytes([0])
    sock.sendall(bytes([0x82]) + encode_remaining_length(len(body)) + body)

    packet_type, body = read_packet(sock)
    if packet_type != 0x90 or len(body) < 3 or body[2] == 0x80:
        raise RuntimeError(f"MQTT subscribe failed: packet=0x{packet_type:02x} body={body!r}")


def send_publish(sock, topic, payload):
    encoded_payload = payload.encode("utf-8")
    body = pack_utf8(topic) + encoded_payload
    sock.sendall(bytes([0x30]) + encode_remaining_length(len(body)) + body)


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
        description="Fake ROS2/Jetrover MQTT client. Subscribes to server command topics and prints each payload as one line."
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--device-type", default="water")
    parser.add_argument("--device-id", default="jetrover-1")
    parser.add_argument("--action", default="water", help="Command action, or comma-separated actions")
    parser.add_argument("--topic", action="append", help="Override full MQTT topic. Can be passed more than once")
    parser.add_argument("--show-topic", action="store_true", help="Print topic before each payload")
    args = parser.parse_args()

    if args.topic:
        topics = args.topic
    else:
        actions = [action.strip() for action in args.action.split(",") if action.strip()]
        topics = [
            f"device/{args.device_type}/{args.device_id}/{action}/command"
            for action in actions
        ]

    if not topics:
        print("no topics to subscribe", file=sys.stderr)
        return 1

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
            send_publish(
                sock,
                f"device/{args.device_type}/{args.device_id}/status",
                '{"eventType":"DEVICE_ONLINE","message":"online"}',
            )
            for packet_id, topic in enumerate(topics, start=1):
                send_subscribe(sock, topic, packet_id)
            sock.settimeout(None)
            print(f"listening {args.host}:{args.port} topics={','.join(topics)}", flush=True)

            while True:
                packet_type, body = read_packet(sock)
                if packet_type >> 4 == 3:
                    topic, payload = parse_publish(body)
                    if args.show_topic:
                        print(f"{topic} {payload}", flush=True)
                    else:
                        print(payload, flush=True)
        except ConnectionError as exc:
            print(f"MQTT connection closed: {exc}", file=sys.stderr)
            return 1


if __name__ == "__main__":
    try:
        sys.exit(main() or 0)
    except KeyboardInterrupt:
        sys.exit(0)
