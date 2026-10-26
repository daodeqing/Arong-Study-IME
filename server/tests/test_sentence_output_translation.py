"""Exercise the real sentence buffer and HTTP worker; replace only window/pipe boundaries."""
import ctypes
from ctypes import wintypes
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from threading import Thread
import time


class Handler(BaseHTTPRequestHandler):
    requests_seen = []
    delay = 0
    finish_reason = "legacy_no_reason"

    def log_message(self, *_):
        pass

    def do_POST(self):
        assert self.path == "/v1/chat/completions"
        assert "Authorization" not in self.headers
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        assert body["model"] == "local-test-model"
        text = body["messages"][-1]["content"]
        try:
            texts = json.loads(text)
            if isinstance(texts, list):
                text = texts[0]
        except (json.JSONDecodeError, TypeError):
            pass
        self.requests_seen.append(text)
        time.sleep(6 if text == "旧句测试" else self.delay)
        translations = {
            "你": "You.",
            "你好": "Hello.",
            "我今天很高兴": "I am very happy today.",
            "旧句测试": "Stale sentence.",
            "新句测试": "New sentence.",
            "长译文": "a" * 800,
        }
        content = translations.get(text, "")
        choice = {"message": {"content": content}}
        if self.finish_reason != "legacy_no_reason":
            choice["finish_reason"] = self.finish_reason
            if self.finish_reason != "stop":
                choice["message"]["content"] = "Partial translation"
        result = json.dumps({"choices": [choice]}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(result)))
        self.end_headers()
        try:
            self.wfile.write(result)
        except (BrokenPipeError, ConnectionResetError):
            pass


def recycle(directory):
    # Test artifacts remain recoverable on the Windows workstation and on CI.
    class FileOperation(ctypes.Structure):
        _fields_ = [
            ("hwnd", wintypes.HWND), ("operation", wintypes.UINT),
            ("source", wintypes.LPCWSTR), ("destination", wintypes.LPCWSTR),
            ("flags", wintypes.WORD), ("aborted", wintypes.BOOL),
            ("mappings", ctypes.c_void_p), ("title", wintypes.LPCWSTR),
        ]
    source = str(directory.resolve()) + "\0\0"
    operation = FileOperation(None, 3, source, None, 0x40 | 0x10 | 0x04 | 0x400, False, None, None)
    result = ctypes.windll.shell32.SHFileOperationW(ctypes.byref(operation))
    if result or operation.aborted:
        raise RuntimeError(f"Could not recycle sentence test fixture: {result}")


def main():
    executable = str(Path(sys.argv[1]).resolve())
    temporary_root = Path(__file__).resolve().parents[2] / ".tmp-test"
    temporary_root.mkdir(exist_ok=True)
    config = Path(tempfile.mkdtemp(prefix="sentence-probe-", dir=temporary_root))
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        base = f"http://127.0.0.1:{server.server_port}/v1"
        (config / "config.toml").write_text(
            '[general]\nsentence_translation=true\n'
            '[ai_assistant]\nprovider="local"\ntoken=""\nmodel_local="local-test-model"\n'
            f'endpoint_local="{base}"\n[network]\nproxy_mode="custom"\nproxy_server="127.0.0.1:1"\n',
            encoding="utf-8",
        )
        env = dict(os.environ, ARONG_STUDY_IME_CONFIG_DIR=str(config),
                   HTTP_PROXY="http://127.0.0.1:1", HTTPS_PROXY="http://127.0.0.1:1", NO_PROXY="")
        Handler.delay = 0.25
        subprocess.run([executable, "--sentence-output-probe", "online"], env=env, check=True, timeout=30)
        assert "你好" in Handler.requests_seen
        for reason in ("length", "content_filter", None):
            Handler.finish_reason = reason
            subprocess.run([executable, "--sentence-output-probe", "incomplete"],
                           env=env, check=True, timeout=30)
        # Both explicit stop and the legacy omitted field remain supported.
        Handler.finish_reason = "stop"
        subprocess.run([executable, "--sentence-output-probe", "online"], env=env, check=True, timeout=30)
        Handler.finish_reason = "legacy_no_reason"
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)
        subprocess.run([executable, "--sentence-output-probe", "offline"], env=env, check=True, timeout=30)
        print("sentence flow passed: separate selections, retained Chinese, English sentence, pending/offline retention, "
              "Chinese confirmation, undo, stale owner protection, incomplete response rejection, empty Authorization and proxy bypass")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)
        size = sum(p.stat().st_size for p in config.rglob("*") if p.is_file())
        recycle(config)
        print(f"Recycled sentence-probe fixture: {size} bytes")


if __name__ == "__main__":
    main()
