#!/usr/bin/env python3
import argparse
import datetime as dt
import http.server
import pathlib
import socketserver
import urllib.parse


class PXEHandler(http.server.SimpleHTTPRequestHandler):
    server_version = "pxenode1-pxe-http/1.0"

    def log_line(self, message):
        line = f"{dt.datetime.now(dt.UTC).isoformat()} {self.client_address[0]} {message}\n"
        with self.server.log_path.open("a", encoding="utf-8") as log:
            log.write(line)
        print(line, end="", flush=True)

    def log_message(self, fmt, *args):
        self.log_line(fmt % args)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/booted":
            query = urllib.parse.parse_qs(parsed.query)
            self.log_line(f"BOOTED {urllib.parse.urlencode(query, doseq=True)}")
            body = b"ok\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        return super().do_GET()


def parse_args():
    parser = argparse.ArgumentParser(description="HTTP server for pxenode1 PXE boot files.")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", default=8080, type=int)
    parser.add_argument("--root", required=True)
    parser.add_argument("--log", required=True)
    return parser.parse_args()


def main():
    args = parse_args()
    root = pathlib.Path(args.root).resolve()
    log_path = pathlib.Path(args.log).resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)
    handler = lambda *hargs, **hkwargs: PXEHandler(*hargs, directory=str(root), **hkwargs)

    socketserver.ThreadingTCPServer.allow_reuse_address = True
    with socketserver.ThreadingTCPServer((args.host, args.port), handler) as httpd:
        httpd.log_path = log_path
        print(f"serving {root} on {args.host}:{args.port}", flush=True)
        httpd.serve_forever()


if __name__ == "__main__":
    raise SystemExit(main())
