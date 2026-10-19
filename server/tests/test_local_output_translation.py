"""Run the real C++ engine/translation worker against an isolated loopback model."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from threading import Thread
import time


class Handler(BaseHTTPRequestHandler):
    requests_seen = []
    delay = 0

    def log_message(self, *_):
        pass

    def send_json(self, value):
        body = json.dumps(value).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_GET(self):
        self.requests_seen.append((self.path, "Authorization" in self.headers))
        assert self.path == "/v1/models"
        self.send_json({"data": [{"id": "local-test-model"}]})

    def do_POST(self):
        self.requests_seen.append((self.path, "Authorization" in self.headers))
        assert self.path == "/v1/chat/completions"
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        assert body["model"] == "local-test-model"
        texts = json.loads(body["messages"][1]["content"])
        time.sleep(self.delay)
        translated = {"translations": ["Hello" if text == "你好" else "" for text in texts]}
        self.send_json({"choices": [{"message": {"content": json.dumps(translated)}}]})


def main():
    executable = str(Path(sys.argv[1]).resolve())
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        with tempfile.TemporaryDirectory(prefix="arong-local-model-") as temporary:
            config = Path(temporary)
            base = f"http://127.0.0.1:{server.server_port}/v1"
            (config / "config.toml").write_text(
                '[ai_assistant]\nprovider="local"\ntoken=""\nmodel_local="local-test-model"\n'
                f'endpoint_local="{base}"\n[network]\nproxy_mode="custom"\nproxy_server="127.0.0.1:1"\n',
                encoding="utf-8",
            )
            env = dict(os.environ, ARONG_STUDY_IME_CONFIG_DIR=str(config),
                       HTTP_PROXY="http://127.0.0.1:1", HTTPS_PROXY="http://127.0.0.1:1", NO_PROXY="")
            subprocess.run([executable, "--local-output-probe", "online"], env=env, check=True, timeout=25)
            Handler.delay = 0.5
            subprocess.run([executable, "--local-output-probe", "pending"], env=env, check=True, timeout=25)
            server.shutdown()
            server.server_close()
            thread.join(timeout=3)
            subprocess.run([executable, "--local-output-probe", "offline"], env=env, check=True, timeout=25)
        assert Handler.requests_seen
        assert all(not auth for _, auth in Handler.requests_seen)
        assert {path for path, _ in Handler.requests_seen} == {"/v1/models", "/v1/chat/completions"}
        print("real C++ translation: models, English, Shift policy, pending/offline fallback, proxy bypass and empty auth passed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=3)


if __name__ == "__main__":
    main()
