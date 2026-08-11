#!/usr/bin/env python3
"""Dependency-free live test for the Linux find-my-device daemon."""

import base64
import hashlib
import http.client
import json
import os
import re
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.parse


DEVICE_ID = "0123456789abcdef"
CLIENT_ID = "com.example.freertosonboarding"
CALLBACK = "freertosonboarding://oauth/callback"
VERIFIER = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"
CHALLENGE = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"
PORT = 18080


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def request(method, path, body=None, headers=None, expected=200):
    connection = http.client.HTTPConnection("127.0.0.1", PORT, timeout=4)
    connection.request(method, path, body=body, headers=headers or {})
    response = connection.getresponse()
    payload = response.read()
    result_headers = dict(response.getheaders())
    status = response.status
    connection.close()
    check(status == expected,
          f"{method} {path}: HTTP {status}, expected {expected}: {payload!r}")
    return payload, result_headers


def wait_ready():
    for _ in range(40):
        try:
            request("GET", "/api/info")
            return
        except (ConnectionError, OSError):
            time.sleep(0.05)
    raise AssertionError("daemon did not start listening")


def start_daemon(binary, state_path, log):
    process = subprocess.Popen(
        [binary, "--interface", "lo", "--address", "127.0.0.1",
         "--port", str(PORT), "--state", state_path,
         "--device-id", DEVICE_ID, "--no-gpio"],
        stdout=log, stderr=subprocess.STDOUT,
    )
    wait_ready()
    return process


def json_request(method, path, body=None, headers=None, expected=200):
    payload, result_headers = request(method, path, body, headers, expected)
    return json.loads(payload), result_headers


def authorize(process):
    query = urllib.parse.urlencode({
        "client_id": CLIENT_ID,
        "redirect_uri": CALLBACK,
        "response_type": "code",
        "state": "integration-state",
        "code_challenge": CHALLENGE,
        "code_challenge_method": "S256",
    })
    page, _ = request("GET", "/auth/authorize?" + query)
    check(b"Confirm device" in page and b"press its button" in page,
          "authorization page omitted physical confirmation")
    os.kill(process.pid, signal.SIGUSR1)
    for _ in range(20):
        status, _ = json_request("GET", "/auth/confirmation/status")
        if status.get("confirmed"):
            break
        time.sleep(0.05)
    else:
        raise AssertionError("SIGUSR1 physical confirmation was not accepted")
    form = query + "&decision=authorize"
    _, headers = request(
        "POST", "/auth/decision", form,
        {"Content-Type": "application/x-www-form-urlencoded"}, 303)
    match = re.search(r"[?&]code=([A-Za-z0-9_-]+)", headers.get("Location", ""))
    check(match, "authorization redirect omitted its code")
    token_form = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": match.group(1),
        "code_verifier": VERIFIER,
        "client_id": CLIENT_ID,
        "redirect_uri": CALLBACK,
    })
    tokens, _ = json_request(
        "POST", "/auth/token", token_form,
        {"Content-Type": "application/x-www-form-urlencoded"})
    check(tokens.get("access_token") and tokens.get("refresh_token"),
          "token exchange omitted tokens")
    return tokens, token_form


def receive_exact(sock, count):
    result = b""
    while len(result) < count:
        chunk = sock.recv(count - len(result))
        check(chunk, "WebSocket closed unexpectedly")
        result += chunk
    return result


def receive_frame(sock):
    first, second = receive_exact(sock, 2)
    check((first & 0x0f) == 1 and (second & 0x80) == 0,
          "unexpected server WebSocket frame")
    length = second & 0x7f
    if length == 126:
        length = struct.unpack("!H", receive_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", receive_exact(sock, 8))[0]
    return json.loads(receive_exact(sock, length))


def send_frame(sock, value):
    payload = json.dumps(value, separators=(",", ":")).encode()
    mask = os.urandom(4)
    if len(payload) < 126:
        header = bytes((0x81, 0x80 | len(payload)))
    else:
        header = bytes((0x81, 0xfe)) + struct.pack("!H", len(payload))
    masked = bytes(byte ^ mask[index & 3] for index, byte in enumerate(payload))
    sock.sendall(header + mask + masked)


def test_websocket(access_token):
    sock = socket.create_connection(("127.0.0.1", PORT), timeout=5)
    key = base64.b64encode(os.urandom(16)).decode()
    upgrade = (
        f"GET /api/ws HTTP/1.1\r\nHost: 127.0.0.1:{PORT}\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
    )
    sock.sendall(upgrade.encode())
    response = b""
    while b"\r\n\r\n" not in response:
        response += receive_exact(sock, 1)
    check(response.startswith(b"HTTP/1.1 101 "), "WebSocket upgrade failed")
    expected = base64.b64encode(hashlib.sha1(
        (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest())
    check(b"Sec-WebSocket-Accept: " + expected in response,
          "WebSocket accept hash was invalid")
    check(receive_frame(sock).get("type") == "auth_required",
          "WebSocket did not require authentication")
    send_frame(sock, {"access_token": access_token})
    check(receive_frame(sock).get("type") == "auth_ok", "WebSocket auth failed")
    send_frame(sock, {"id": 1, "type": "subscribe_status"})
    check(receive_frame(sock).get("success") is True, "subscription failed")
    event = receive_frame(sock)
    check(event.get("event", {}).get("event_type") == "server_status",
          "status event was not received")
    sock.close()


def main():
    binary = os.environ.get("FMD_BINARY", "../src/find-my-device")
    with tempfile.TemporaryDirectory(prefix="find-my-device-test-") as directory:
        state_path = os.path.join(directory, "state")
        log_path = os.path.join(directory, "daemon.log")
        with open(log_path, "wb") as log:
            process = start_daemon(binary, state_path, log)
            try:
                info, _ = json_request("GET", "/api/info")
                check(info["api_version"] == "3" and info["device_id"] == DEVICE_ID,
                      "public device information is invalid")
                json_request("GET", "/api/device-info", expected=401)
                tokens, token_form = authorize(process)
                replay, _ = json_request(
                    "POST", "/auth/token", token_form,
                    {"Content-Type": "application/x-www-form-urlencoded"}, 400)
                check(replay.get("error") == "invalid_grant", "OAuth code replay worked")
                auth = {"Authorization": "Bearer " + tokens["access_token"]}
                protected, _ = json_request("GET", "/api/device-info", headers=auth)
                check(protected["device_id"] == DEVICE_ID, "protected info changed ID")
                rename_headers = dict(auth, **{"Content-Type": "application/json"})
                renamed, _ = json_request("PUT", "/api/device-name",
                    json.dumps({"device_name": "Workshop Finder"}), rename_headers)
                check(renamed.get("success") is True, "rename failed")
                registration, _ = json_request("POST", "/api/mobile/registrations",
                    json.dumps({"device_id": "phone-001"}), rename_headers)
                check(registration.get("websocket_url") ==
                      f"ws://127.0.0.1:{PORT}/api/ws", "registration URL is invalid")
                test_websocket(tokens["access_token"])
            finally:
                process.terminate()
                process.wait(timeout=5)

            process = start_daemon(binary, state_path, log)
            try:
                info, _ = json_request("GET", "/api/info")
                check(info["device_name"] == "Workshop Finder",
                      "device name was not restored")
                refresh = urllib.parse.urlencode({
                    "grant_type": "refresh_token",
                    "refresh_token": tokens["refresh_token"],
                    "client_id": CLIENT_ID,
                })
                refreshed, _ = json_request("POST", "/auth/token", refresh,
                    {"Content-Type": "application/x-www-form-urlencoded"})
                check(refreshed.get("access_token") != tokens["access_token"],
                      "restored refresh token did not rotate")
            finally:
                process.terminate()
                process.wait(timeout=5)
        print("find-my-device live integration tests passed")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"integration test failed: {error}", file=sys.stderr)
        raise
