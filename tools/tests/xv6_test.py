#!/usr/bin/env python3
"""
Unified xv6 QEMU test runner.
"""

import argparse
import os
import re
import select
import signal
import subprocess
import sys
import time

PROMPT_RE = re.compile(r"(?m)^\$ ")
FORK_OK_RE = re.compile(r"fork test OK")
RECLAIM_RE = re.compile(r"(?m)^ireclaim")
WAIT_RE = re.compile(r"wait for kill and reclaim")
ALL_TESTS_PASSED_RE = re.compile(r"(?m)^ALL TESTS PASSED$")
F5_RE = re.compile(r"\bf5\b")
LOGOK_RE = re.compile(r"(?m)^logok$")


class TestFailure(RuntimeError):
    pass


def resolve_log_path(repo, log_path):
    if os.path.isabs(log_path):
        return log_path
    return os.path.join(repo, log_path)


class QemuSession:
    def __init__(self, repo, make_cmd, log_path, reset=False):
        self.repo = os.path.abspath(repo)
        self.make_cmd = list(make_cmd)
        self.log_path = resolve_log_path(self.repo, log_path)
        self.output = ""
        self.proc = None

        if reset:
            self.build_xv6()
            self.reset_fs()

        self.start()

    def start(self):
        self.proc = subprocess.Popen(
            self.make_cmd,
            cwd=self.repo,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )

    def build_xv6(self):
        subprocess.run(["make", "kernel/kernel"], cwd=self.repo, check=True)

    def reset_fs(self):
        fs_img = os.path.join(self.repo, "fs.img")
        if os.path.exists(fs_img):
            os.remove(fs_img)
        subprocess.run(["make", "fs.img"], cwd=self.repo, check=True)

    def mark(self):
        return len(self.output)

    def save_output(self):
        with open(self.log_path, "w", encoding="utf-8") as f:
            f.write(self.output)

    def read_nonblocking(self):
        if self.proc is None or self.proc.stdout is None:
            return
        fd = self.proc.stdout.fileno()
        while True:
            ready, _, _ = select.select([fd], [], [], 0)
            if not ready:
                return
            chunk = os.read(fd, 4096)
            if not chunk:
                return
            self.output += chunk.decode("utf-8", "replace")

    def wait_for(self, pattern, timeout_s, start_idx=0):
        if self.proc is None or self.proc.stdout is None:
            raise TestFailure("qemu session is not running")

        deadline = time.time() + timeout_s
        fd = self.proc.stdout.fileno()

        while time.time() < deadline:
            if pattern.search(self.output[start_idx:]):
                return self.output

            if self.proc.poll() is not None:
                self.read_nonblocking()
                raise TestFailure(f"qemu exited early with code {self.proc.returncode}")

            wait_s = min(0.5, max(0, deadline - time.time()))
            ready, _, _ = select.select([fd], [], [], wait_s)
            if not ready:
                continue

            chunk = os.read(fd, 4096)
            if not chunk:
                continue
            self.output += chunk.decode("utf-8", "replace")

        self.read_nonblocking()
        raise TestFailure(f"timeout waiting for pattern: {pattern.pattern}")

    def wait_for_any(self, patterns, timeout_s, start_idx=0):
        if self.proc is None or self.proc.stdout is None:
            raise TestFailure("qemu session is not running")

        deadline = time.time() + timeout_s
        fd = self.proc.stdout.fileno()

        while time.time() < deadline:
            segment = self.output[start_idx:]
            for pattern in patterns:
                if pattern.search(segment):
                    return pattern

            if self.proc.poll() is not None:
                self.read_nonblocking()
                raise TestFailure(f"qemu exited early with code {self.proc.returncode}")

            wait_s = min(0.5, max(0, deadline - time.time()))
            ready, _, _ = select.select([fd], [], [], wait_s)
            if not ready:
                continue

            chunk = os.read(fd, 4096)
            if not chunk:
                continue
            self.output += chunk.decode("utf-8", "replace")

        self.read_nonblocking()
        joined = ", ".join(pattern.pattern for pattern in patterns)
        raise TestFailure(f"timeout waiting for one of: {joined}")

    def send(self, text):
        if self.proc is None or self.proc.stdin is None:
            raise TestFailure("qemu session is not running")
        if isinstance(text, str):
            text = text.encode("utf-8")
        self.proc.stdin.write(text)
        self.proc.stdin.flush()

    def run_to_prompt(self, command, timeout_s):
        start_idx = self.mark()
        self.send(command + "\n")
        self.wait_for(PROMPT_RE, timeout_s, start_idx)
        return self.output[start_idx:]

    def crash(self):
        if self.proc is None:
            raise TestFailure("qemu session is not running")
        ps = subprocess.run(
            ["ps", "-opid=", "--ppid", str(self.proc.pid)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
        kids = [int(line.strip()) for line in ps.stdout.splitlines() if line.strip()]
        if not kids:
            raise TestFailure("no qemu child process found to crash")
        os.kill(kids[0], signal.SIGKILL)
        time.sleep(1)
        self.read_nonblocking()

    def stop(self):
        if self.proc is None:
            return
        self.read_nonblocking()
        if self.proc.poll() is None and self.proc.stdin is not None:
            try:
                self.proc.stdin.write(b"\x01x")
                self.proc.stdin.flush()
            except Exception:
                pass
            try:
                self.proc.wait(timeout=20)
            except subprocess.TimeoutExpired:
                self.proc.terminate()
                try:
                    self.proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.proc.kill()
                    self.proc.wait(timeout=5)
        self.read_nonblocking()
        self.proc = None


def print_failure(session, message):
    print(f"FAIL: {message}", file=sys.stderr)
    if session is not None:
        session.read_nonblocking()
        session.save_output()
        print("----- OUTPUT TAIL START -----", file=sys.stderr)
        print(session.output[-4000:], file=sys.stderr)
        print("----- OUTPUT TAIL END -----", file=sys.stderr)
        print(f"saved full output to {session.log_path}", file=sys.stderr)


def run_diskirq(args):
    for round_i in range(1, args.rounds + 1):
        session = None
        started = time.time()
        print(f"ROUND {round_i}/{args.rounds}: boot")
        try:
            session = QemuSession(args.repo, args.make_cmd, args.log_path)
            session.wait_for(PROMPT_RE, args.boot_timeout)
            session.run_to_prompt("stressfs", args.stress_timeout)

            start_idx = session.mark()
            session.send("forktest\n")
            session.wait_for(FORK_OK_RE, args.fork_timeout, start_idx)
            session.wait_for(PROMPT_RE, args.prompt_timeout, start_idx)

            elapsed = time.time() - started
            print(f"ROUND {round_i}/{args.rounds}: PASS ({elapsed:.1f}s)")
        except Exception as exc:
            print(f"ROUND {round_i}/{args.rounds}: FAIL", file=sys.stderr)
            print_failure(session, str(exc))
            return 1
        finally:
            if session is not None:
                session.stop()

    print(f"ALL {args.rounds} ROUNDS PASS")
    return 0


def crash_log_once(args):
    session = None
    try:
        session = QemuSession(args.repo, args.make_cmd, args.log_path, reset=True)
        session.wait_for(PROMPT_RE, args.boot_timeout)
        session.send("logstress f0 f1 f2 f3 f4 f5\n")
        time.sleep(args.crash_delay)
        session.read_nonblocking()
        session.crash()
    except Exception as exc:
        print_failure(session, str(exc))
        raise
    finally:
        if session is not None:
            session.stop()


def recover_log(args):
    session = None
    try:
        session = QemuSession(args.repo, args.make_cmd, args.log_path)
        session.wait_for(PROMPT_RE, args.boot_timeout)

        listing = session.run_to_prompt("ls", args.command_timeout)
        has_f5 = bool(F5_RE.search(listing))

        session.run_to_prompt("echo logok > _logcheck", args.command_timeout)
        cat_output = session.run_to_prompt("cat _logcheck", args.command_timeout)
        session.run_to_prompt("rm _logcheck", args.command_timeout)
        cat_ok = bool(LOGOK_RE.search(cat_output))
        return has_f5 and cat_ok
    except Exception as exc:
        print_failure(session, str(exc))
        raise
    finally:
        if session is not None:
            session.stop()


def run_log(args):
    try:
        print("Test recovery of log")
        for attempt in range(1, args.retries + 1):
            crash_log_once(args)
            if recover_log(args):
                print("OK")
                return 0
            print(f"log attempt {attempt}")
        raise TestFailure("log recovery validation did not succeed")
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


def crash_orphan(args, command):
    session = None
    try:
        session = QemuSession(args.repo, args.make_cmd, args.log_path, reset=True)
        session.wait_for(PROMPT_RE, args.boot_timeout)
        session.send(command + "\n")
        session.wait_for(WAIT_RE, args.command_timeout)
        session.crash()
    except Exception as exc:
        print_failure(session, str(exc))
        raise
    finally:
        if session is not None:
            session.stop()


def recover_orphan(args):
    session = None
    try:
        session = QemuSession(args.repo, args.make_cmd, args.log_path)
        session.wait_for(RECLAIM_RE, args.recovery_timeout)
    except Exception as exc:
        print_failure(session, str(exc))
        raise
    finally:
        if session is not None:
            session.stop()


def run_orphan_case(args, label, command):
    try:
        print(f"Test recovery of an orphaned {label}")
        crash_orphan(args, command)
        recover_orphan(args)
        print("OK")
        return 0
    except Exception as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


def run_crash(args):
    if run_log(args) != 0:
        return 1
    if run_orphan_case(args, "file", "forphan") != 0:
        return 1
    if run_orphan_case(args, "directory", "dorphan") != 0:
        return 1
    return 0


def run_usertests(args):
    session = None
    try:
        session = QemuSession(args.repo, args.make_cmd, args.log_path, reset=True)
        session.wait_for(PROMPT_RE, args.boot_timeout)

        command = "usertests"
        timeout_s = args.timeout
        if args.quick:
            command += " -q"
            if timeout_s is None:
                timeout_s = 300
        elif args.case:
            command += " " + args.case
        if timeout_s is None:
            timeout_s = 600

        start_idx = session.mark()
        session.send(command + "\n")
        matched = session.wait_for_any(
            [ALL_TESTS_PASSED_RE, PROMPT_RE], timeout_s, start_idx
        )
        if matched is PROMPT_RE:
            raise TestFailure("usertests returned to the shell without reporting success")
        print("OK")
        return 0
    except Exception as exc:
        print_failure(session, str(exc))
        return 1
    finally:
        if session is not None:
            session.stop()


def add_session_args(parser):
    parser.add_argument("--repo", default=".", help="xv6 repo root (default: cwd)")
    parser.add_argument(
        "--make-cmd",
        nargs="+",
        default=["make", "qemu"],
        help="command used to boot xv6 (default: make qemu)",
    )
    parser.add_argument(
        "--log-path",
        default="test-xv6.out",
        help="path used to save full console output on failure",
    )
    parser.add_argument("--boot-timeout", type=int, default=90)


def build_parser():
    parser = argparse.ArgumentParser(description="Unified xv6 QEMU test runner.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    diskirq = subparsers.add_parser(
        "diskirq", help="run repeated stressfs + forktest stability rounds"
    )
    add_session_args(diskirq)
    diskirq.add_argument("--rounds", type=int, default=10)
    diskirq.add_argument("--stress-timeout", type=int, default=240)
    diskirq.add_argument("--fork-timeout", type=int, default=120)
    diskirq.add_argument("--prompt-timeout", type=int, default=120)
    diskirq.set_defaults(func=run_diskirq)

    log_cmd = subparsers.add_parser("log", help="run the log crash-recovery test")
    add_session_args(log_cmd)
    log_cmd.add_argument("--retries", type=int, default=5)
    log_cmd.add_argument("--crash-delay", type=int, default=2)
    log_cmd.add_argument("--command-timeout", type=int, default=30)
    log_cmd.set_defaults(func=run_log)

    crash = subparsers.add_parser(
        "crash", help="run log recovery and orphan recovery tests"
    )
    add_session_args(crash)
    crash.add_argument("--retries", type=int, default=5)
    crash.add_argument("--crash-delay", type=int, default=2)
    crash.add_argument("--command-timeout", type=int, default=30)
    crash.add_argument("--recovery-timeout", type=int, default=30)
    crash.set_defaults(func=run_crash)

    usertests = subparsers.add_parser(
        "usertests", help="run usertests or a single usertests case"
    )
    add_session_args(usertests)
    usertests.add_argument("--quick", action="store_true", help="run quick usertests")
    usertests.add_argument("--case", help="run a single usertests case")
    usertests.add_argument(
        "--timeout",
        type=int,
        default=None,
        help="override usertests timeout in seconds",
    )
    usertests.set_defaults(func=run_usertests)

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
