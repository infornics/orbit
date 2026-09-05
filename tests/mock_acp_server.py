#!/usr/bin/env python3
"""Minimal ACP stdio server used by Orbit's unit tests."""
import json
import sys


def send(payload):
    sys.stdout.write(json.dumps(payload) + "\n")
    sys.stdout.flush()


def main():
    for raw in sys.stdin:
        line = raw.strip()
        if not line:
            continue
        try:
            message = json.loads(line)
        except json.JSONDecodeError:
            continue

        method = message.get("method")
        request_id = message.get("id")

        if method == "initialize":
            send({
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {
                    "protocolVersion": 1,
                    "agentCapabilities": {"loadSession": False},
                    "agentInfo": {"name": "mock-antigravity", "title": "Mock", "version": "0"},
                    "authMethods": [],
                },
            })
        elif method == "session/new":
            send({
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {"sessionId": "sess_test"},
            })
        elif method == "session/prompt":
            send({
                "jsonrpc": "2.0",
                "method": "session/update",
                "params": {
                    "sessionId": "sess_test",
                    "update": {
                        "sessionUpdate": "agent_message_chunk",
                        "content": {"type": "text", "text": "hello from mock"},
                    },
                },
            })
            send({
                "jsonrpc": "2.0",
                "id": request_id,
                "result": {"stopReason": "end_turn"},
            })
        elif method == "authenticate":
            send({"jsonrpc": "2.0", "id": request_id, "result": {}})


if __name__ == "__main__":
    main()
