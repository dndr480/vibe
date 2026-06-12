#!/usr/bin/env python3
import argparse
import os
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode, urljoin
from urllib.request import (
    HTTPDigestAuthHandler,
    HTTPPasswordMgrWithDefaultRealm,
    Request,
    build_opener,
)


ACTIONS = {
    "power-on": {
        "label": "Turn power on",
        "radio_value": "2",
        "boot_special": "1",
    },
    "power-off": {
        "label": "Turn power off",
        "radio_value": "1",
        "boot_special": None,
    },
    "cycle-power": {
        "label": "Cycle power off and on",
        "radio_value": "3",
        "boot_special": "1",
    },
    "reset": {
        "label": "Reset",
        "radio_value": "4",
        "boot_special": "1",
    },
}


class RemoteFormParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.in_remote_form = False
        self.token = None
        self.radio_values = set()

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag.lower() == "form" and attrs.get("action") == "/remoteform":
            self.in_remote_form = True
            return

        if not self.in_remote_form or tag.lower() != "input":
            return

        name = attrs.get("name")
        if name == "t":
            self.token = attrs.get("value")
        elif name == "amt_html_rc_radio_group":
            value = attrs.get("value")
            if value is not None:
                self.radio_values.add(value)

    def handle_endtag(self, tag):
        if tag.lower() == "form" and self.in_remote_form:
            self.in_remote_form = False


def read_password(path):
    try:
        return Path(path).read_text(encoding="utf-8").strip()
    except FileNotFoundError:
        raise RuntimeError(f"password file not found: {path}")
    except OSError as exc:
        raise RuntimeError(f"cannot read password file {path}: {exc}")


def build_digest_opener(base_url, username, password):
    password_mgr = HTTPPasswordMgrWithDefaultRealm()
    password_mgr.add_password(None, base_url, username, password)
    return build_opener(HTTPDigestAuthHandler(password_mgr))


def fetch_remote_form(opener, base_url, timeout):
    remote_url = urljoin(base_url, "remote.htm")
    request = Request(remote_url, method="GET")
    with opener.open(request, timeout=timeout) as response:
        html = response.read().decode("utf-8", errors="replace")

    parser = RemoteFormParser()
    parser.feed(html)

    if not parser.token:
        raise RuntimeError("could not find the remote control form token")

    return parser


def post_remote_command(opener, base_url, token, action, timeout):
    data = {
        "t": token,
        "amt_html_rc_radio_group": action["radio_value"],
    }
    if action["boot_special"] is not None:
        data["amt_html_rc_boot_special"] = action["boot_special"]

    body = urlencode(data).encode("ascii")
    request = Request(
        urljoin(base_url, "remoteform"),
        data=body,
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )

    with opener.open(request, timeout=timeout) as response:
        response.read()
        return response.status, response.geturl()


def confirm_or_exit(action_label, host, assume_yes, dry_run):
    if dry_run:
        return
    if assume_yes:
        return
    if not sys.stdin.isatty():
        raise RuntimeError("refusing to send command without --yes in non-interactive mode")

    prompt = f"Send '{action_label}' to {host}? Type YES to continue: "
    if input(prompt) != "YES":
        raise RuntimeError("cancelled")


def parse_args():
    default_password_file = os.environ.get(
        "PXENODE1_AMT_PASSWORD_FILE",
        str(Path.home() / "vibe-secrets" / "pxenode1-mebx-old.txt"),
    )
    parser = argparse.ArgumentParser(
        description="Send an Intel AMT/MEBx remote control command to pxenode1."
    )
    parser.add_argument("action", choices=sorted(ACTIONS))
    parser.add_argument("--host", default=os.environ.get("PXENODE1_AMT_HOST", "192.168.1.218"))
    parser.add_argument("--port", default=int(os.environ.get("PXENODE1_AMT_PORT", "16992")), type=int)
    parser.add_argument("--username", default=os.environ.get("PXENODE1_AMT_USER", "admin"))
    parser.add_argument("--password-file", default=default_password_file)
    parser.add_argument("--timeout", default=8.0, type=float)
    parser.add_argument("--yes", "-y", action="store_true", help="send the command without prompting")
    parser.add_argument("--dry-run", action="store_true", help="log in and validate the form without sending")
    return parser.parse_args()


def main():
    args = parse_args()
    action = ACTIONS[args.action]
    host = f"{args.host}:{args.port}"
    base_url = f"http://{host}/"

    try:
        confirm_or_exit(action["label"], host, args.yes, args.dry_run)
        password = read_password(args.password_file)
        opener = build_digest_opener(base_url, args.username, password)
        form = fetch_remote_form(opener, base_url, args.timeout)

        if action["radio_value"] not in form.radio_values:
            raise RuntimeError(f"remote page does not advertise command value {action['radio_value']}")

        if args.dry_run:
            print(f"dry run OK: authenticated to {host}; '{action['label']}' is available")
            return 0

        status, final_url = post_remote_command(opener, base_url, form.token, action, args.timeout)
        print(f"sent '{action['label']}' to {host}; response HTTP {status}; final URL {final_url}")
        return 0
    except HTTPError as exc:
        print(f"HTTP error {exc.code}: {exc.reason}", file=sys.stderr)
        return 1
    except (RuntimeError, URLError, TimeoutError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
