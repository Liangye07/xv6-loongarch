#!/usr/bin/env python3
"""
Low-noise regression runner for xv6 disk interrupt stability.

Each round boots xv6, runs:
  1) stressfs
  2) forktest

The script fails fast on timeout/hang and prints a short output tail.
"""

import argparse
import os
import re
import select
import subprocess
import sys
import time

PROMPT_RE = re.compile(r"(?m)^\$ ")
FORK_OK_RE = re.compile(r"fork test OK")


def wait_for(proc, pattern, timeout_s, buf, start_idx):
    deadline = time.time() + timeout_s
    fd = proc.stdout.fileno()

    while time.time() < deadline:
        if pattern.search(buf[start_idx:]):
            return buf

        if proc.poll() is not None:
            raise RuntimeError(f"qemu exited early with code {proc.returncode}")

        ready, _, _ = select.select([fd], [], [], 0.5)
        if not ready:
            continue

        chunk = os.read(fd, 4096)
        if not chunk:
            continue
        buf += chunk.decode("utf-8", "replace")

    raise TimeoutError(f"timeout waiting for pattern: {pattern.pattern}")


def shutdown_qemu(proc):
    if proc.poll() is not None:
        return
    try:
        proc.stdin.write(b"\x01x")  # Ctrl-a x for qemu -nographic.
        proc.stdin.flush()
    except Exception:
        pass
    try:
        proc.wait(timeout=20)
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=5)


def run_round(repo, make_cmd, round_i, total, boot_t, stress_t, fork_t, prompt_t):
    print(f"ROUND {round_i}/{total}: boot")
    proc = subprocess.Popen(
        make_cmd,
        cwd=repo,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
    )
    buf = ""
    started = time.time()

    try:
        mark = len(buf)
        buf = wait_for(proc, PROMPT_RE, boot_t, buf, mark)

        mark = len(buf)
        proc.stdin.write(b"stressfs\n")
        proc.stdin.flush()
        buf = wait_for(proc, PROMPT_RE, stress_t, buf, mark)

        mark = len(buf)
        proc.stdin.write(b"forktest\n")
        proc.stdin.flush()
        buf = wait_for(proc, FORK_OK_RE, fork_t, buf, mark)
        buf = wait_for(proc, PROMPT_RE, prompt_t, buf, mark)

        elapsed = time.time() - started
        shutdown_qemu(proc)
        print(f"ROUND {round_i}/{total}: PASS ({elapsed:.1f}s)")
        return True
    except Exception as e:
        print(f"ROUND {round_i}/{total}: FAIL: {e}")
        print("----- OUTPUT TAIL START -----")
        print(buf[-4000:])
        print("----- OUTPUT TAIL END -----")
        shutdown_qemu(proc)
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Run stressfs+forktest rounds to catch disk interrupt stalls."
    )
    parser.add_argument("--repo", default=".", help="xv6 repo root (default: cwd)")
    parser.add_argument(
        "--rounds", type=int, default=10, help="number of rounds (default: 10)"
    )
    parser.add_argument(
        "--make-cmd",
        nargs="+",
        default=["make", "qemu"],
        help="command used to boot xv6 (default: make qemu)",
    )
    parser.add_argument("--boot-timeout", type=int, default=90)
    parser.add_argument("--stress-timeout", type=int, default=240)
    parser.add_argument("--fork-timeout", type=int, default=120)
    parser.add_argument("--prompt-timeout", type=int, default=120)
    args = parser.parse_args()

    if args.rounds <= 0:
        print("rounds must be > 0", file=sys.stderr)
        return 2

    for i in range(1, args.rounds + 1):
        ok = run_round(
            repo=args.repo,
            make_cmd=args.make_cmd,
            round_i=i,
            total=args.rounds,
            boot_t=args.boot_timeout,
            stress_t=args.stress_timeout,
            fork_t=args.fork_timeout,
            prompt_t=args.prompt_timeout,
        )
        if not ok:
            return 1

    print(f"ALL {args.rounds} ROUNDS PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
