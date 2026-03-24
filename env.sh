#!/usr/bin/env bash

is_sourced=0
if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  is_sourced=1
else
  set -euo pipefail
fi

script_dir="$(cd -- "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

XV6_REPO_ROOT="${XV6_REPO_ROOT:-$script_dir}"
XV6_TOOLCHAIN_TARGET="${XV6_TOOLCHAIN_TARGET:-loongarch64-unknown-linux-gnu}"
XV6_TOOLCHAIN_DIR="${XV6_TOOLCHAIN_DIR:-$HOME/.local/opt/x-tools/${XV6_TOOLCHAIN_TARGET}}"
XV6_QEMU_BIN="${XV6_QEMU_BIN:-qemu-system-loongarch64}"
XV6_TOOLCHAIN_API="${XV6_TOOLCHAIN_API:-https://api.github.com/repos/loong64/cross-tools/releases/latest}"

finish() {
  local code="$1"
  if [[ "$is_sourced" -eq 1 ]]; then
    return "$code"
  fi
  exit "$code"
}

prepend_path() {
  local dir="$1"
  [[ -d "$dir" ]] || return 0
  case ":$PATH:" in
    *":$dir:"*) ;;
    *) export PATH="$dir:$PATH" ;;
  esac
}

prepend_ld_library_path() {
  local dir="$1"
  [[ -d "$dir" ]] || return 0
  local current="${LD_LIBRARY_PATH:-}"
  case ":$current:" in
    *":$dir:"*) ;;
    *) export LD_LIBRARY_PATH="$dir${current:+:$current}" ;;
  esac
}

activate_environment() {
  export XV6_REPO_ROOT XV6_TOOLCHAIN_TARGET XV6_TOOLCHAIN_DIR XV6_QEMU_BIN XV6_TOOLCHAIN_API
  prepend_path "$XV6_TOOLCHAIN_DIR/bin"
  prepend_ld_library_path "$XV6_TOOLCHAIN_DIR/lib"
  prepend_ld_library_path "$XV6_TOOLCHAIN_DIR/$XV6_TOOLCHAIN_TARGET/lib"
}

log() {
  printf '[xv6-env] %s\n' "$*"
}

warn() {
  printf '[xv6-env] warning: %s\n' "$*" >&2
}

die() {
  printf '[xv6-env] error: %s\n' "$*" >&2
  finish 1
}

check_required() {
  local label="$1"
  local cmd="$2"
  if command -v "$cmd" >/dev/null 2>&1; then
    printf '[ok] %s -> %s\n' "$label" "$(command -v "$cmd")"
  else
    printf '[missing] %s (%s)\n' "$label" "$cmd" >&2
    missing=1
  fi
}

doctor() {
  activate_environment
  missing=0

  echo "xv6-loongarch environment"
  echo "  repo: $XV6_REPO_ROOT"
  echo "  toolchain target: $XV6_TOOLCHAIN_TARGET"
  echo "  toolchain dir: $XV6_TOOLCHAIN_DIR"
  echo "  qemu: $XV6_QEMU_BIN"

  check_required "host gcc" gcc
  check_required "make" make
  check_required "python3" python3
  check_required "perl" perl
  check_required "xxd" xxd
  check_required "cross gcc" "${XV6_TOOLCHAIN_TARGET}-gcc"
  check_required "cross ld" "${XV6_TOOLCHAIN_TARGET}-ld"
  check_required "cross objdump" "${XV6_TOOLCHAIN_TARGET}-objdump"
  check_required "cross objcopy" "${XV6_TOOLCHAIN_TARGET}-objcopy"
  check_required "qemu" "$XV6_QEMU_BIN"

  if [[ "$missing" -ne 0 ]]; then
    cat >&2 <<'EOF_INNER'

Some required tools are missing.

Recommended workflow:
  1. Run: ./env.sh install
  2. Then: source ./env.sh
  3. Finally: make -j"$(nproc)" && make qemu
EOF_INNER
    return 1
  fi

  cat <<'EOF_INNER'

Environment is ready.

Typical workflow:
  source ./env.sh
  make -j"$(nproc)"
  make qemu
EOF_INNER
}

require_root_runner() {
  if [[ "$(id -u)" -eq 0 ]]; then
    ROOT_RUNNER=()
    return 0
  fi
  if command -v sudo >/dev/null 2>&1; then
    ROOT_RUNNER=(sudo)
    return 0
  fi
  die "package installation requires root privileges; please run as root or install sudo"
}

detect_package_manager() {
  if command -v apt-get >/dev/null 2>&1; then
    PKG_MANAGER=apt
  elif command -v dnf >/dev/null 2>&1; then
    PKG_MANAGER=dnf
  elif command -v pacman >/dev/null 2>&1; then
    PKG_MANAGER=pacman
  else
    PKG_MANAGER=unknown
  fi
}

install_host_dependencies() {
  activate_environment

  if command -v gcc >/dev/null 2>&1 \
    && command -v make >/dev/null 2>&1 \
    && command -v python3 >/dev/null 2>&1 \
    && command -v perl >/dev/null 2>&1 \
    && command -v xxd >/dev/null 2>&1 \
    && command -v "$XV6_QEMU_BIN" >/dev/null 2>&1; then
    log "host-side dependencies already look complete; skipping package-manager install"
    return 0
  fi

  detect_package_manager
  require_root_runner

  case "$PKG_MANAGER" in
    apt)
      log "installing host packages with apt"
      "${ROOT_RUNNER[@]}" apt-get update
      "${ROOT_RUNNER[@]}" apt-get install -y \
        build-essential python3 perl xxd curl ca-certificates xz-utils qemu-system-misc
      ;;
    dnf)
      log "installing host packages with dnf"
      "${ROOT_RUNNER[@]}" dnf install -y \
        gcc make python3 perl xxd curl ca-certificates xz qemu-system-loongarch64
      ;;
    pacman)
      log "installing host packages with pacman"
      "${ROOT_RUNNER[@]}" pacman -Sy --needed --noconfirm \
        gcc make python perl xxd curl ca-certificates xz qemu-system-loongarch64
      ;;
    *)
      die "unsupported package manager; install gcc/make/python3/perl/xxd/curl/${XV6_QEMU_BIN} manually, then rerun ./env.sh install"
      ;;
  esac
}

map_host_arch() {
  case "$(uname -m)" in
    x86_64|amd64) echo "x86_64" ;;
    aarch64|arm64) echo "aarch64" ;;
    loongarch64) echo "loongarch64" ;;
    *) echo "unsupported" ;;
  esac
}

download_to_file() {
  local url="$1"
  local out="$2"
  if command -v curl >/dev/null 2>&1; then
    curl -fL "$url" -o "$out"
    return 0
  fi
  if command -v wget >/dev/null 2>&1; then
    wget -O "$out" "$url"
    return 0
  fi
  if command -v python3 >/dev/null 2>&1; then
    python3 - "$url" "$out" <<'PY'
import sys
import urllib.request

url, out = sys.argv[1], sys.argv[2]
with urllib.request.urlopen(url) as resp, open(out, "wb") as f:
    f.write(resp.read())
PY
    return 0
  fi
  die "no downloader available; install curl or wget first"
}

resolve_toolchain_url() {
  if [[ -n "${XV6_TOOLCHAIN_URL:-}" ]]; then
    printf '%s\n' "$XV6_TOOLCHAIN_URL"
    return 0
  fi

  local host_arch="$1"
  [[ "$host_arch" != "loongarch64" ]] || return 0

  local api_json
  api_json="$(mktemp)"
  download_to_file "$XV6_TOOLCHAIN_API" "$api_json"

  local url
  url="$(python3 - "$host_arch" "$XV6_TOOLCHAIN_TARGET" "$api_json" <<'PY'
import json
import sys

host_arch, target, path = sys.argv[1:4]
with open(path, "r", encoding="utf-8") as f:
    data = json.load(f)

for asset in data.get("assets", []):
    name = asset.get("name", "")
    if host_arch in name and target in name and name.endswith(".tar.xz"):
        print(asset["browser_download_url"])
        break
else:
    raise SystemExit(1)
PY
)"
  rm -f "$api_json"

  if [[ -z "$url" ]]; then
    die "could not find a matching cross-toolchain asset from $XV6_TOOLCHAIN_API"
  fi
  printf '%s\n' "$url"
}

install_toolchain() {
  activate_environment

  if command -v "${XV6_TOOLCHAIN_TARGET}-gcc" >/dev/null 2>&1; then
    log "cross compiler already available; skipping toolchain download"
    return 0
  fi

  local host_arch
  host_arch="$(map_host_arch)"
  case "$host_arch" in
    loongarch64)
      log "native loongarch64 host detected; external cross-toolchain download is not required"
      return 0
      ;;
    x86_64|aarch64) ;;
    *)
      die "unsupported host architecture $(uname -m); set XV6_TOOLCHAIN_URL manually or install ${XV6_TOOLCHAIN_TARGET}-gcc yourself"
      ;;
  esac

  local url
  url="$(resolve_toolchain_url "$host_arch")"
  [[ -n "$url" ]] || die "failed to resolve toolchain download URL"

  local tmpdir archive extract_root source_dir
  tmpdir="$(mktemp -d)"
  archive="$tmpdir/toolchain.tar.xz"
  extract_root="$tmpdir/extract"

  log "downloading LoongArch cross-toolchain from $url"
  download_to_file "$url" "$archive"

  mkdir -p "$extract_root"
  tar -xf "$archive" -C "$extract_root"

  if [[ -d "$extract_root/cross-tools" ]]; then
    source_dir="$extract_root/cross-tools"
  else
    local dirs=()
    while IFS= read -r entry; do
      dirs+=("$entry")
    done < <(find "$extract_root" -mindepth 1 -maxdepth 1 -type d | sort)

    if [[ "${#dirs[@]}" -eq 1 ]]; then
      source_dir="${dirs[0]}"
    else
      die "unexpected toolchain archive layout; set XV6_TOOLCHAIN_URL to a known-good tarball if needed"
    fi
  fi

  rm -rf "$XV6_TOOLCHAIN_DIR.tmp"
  mkdir -p "$XV6_TOOLCHAIN_DIR.tmp"
  cp -a "$source_dir"/. "$XV6_TOOLCHAIN_DIR.tmp"/
  rm -rf "$XV6_TOOLCHAIN_DIR"
  mv "$XV6_TOOLCHAIN_DIR.tmp" "$XV6_TOOLCHAIN_DIR"
  rm -rf "$tmpdir"

  log "toolchain installed under $XV6_TOOLCHAIN_DIR"
}

install_all() {
  install_host_dependencies
  install_toolchain
  activate_environment
  doctor
  cat <<'EOF_INNER'

Next steps:
  source ./env.sh
  make -j"$(nproc)"
  make qemu
EOF_INNER
}

usage() {
  cat <<'EOF_INNER'
Usage:
  ./env.sh install     Install Linux host dependencies and download the LoongArch toolchain
  ./env.sh doctor      Check whether the current shell can build and run xv6-loongarch
  source ./env.sh      Export PATH / LD_LIBRARY_PATH for the current shell and run the same checks

Optional environment variables:
  XV6_TOOLCHAIN_DIR    Override where the downloaded toolchain is installed
  XV6_TOOLCHAIN_URL    Override the toolchain tarball URL directly
  XV6_QEMU_BIN         Override the QEMU executable name
EOF_INNER
}

main() {
  local cmd="${1:-install}"
  case "$cmd" in
    install)
      install_all
      ;;
    doctor)
      doctor
      ;;
    help|-h|--help)
      usage
      ;;
    *)
      die "unknown command: $cmd"
      ;;
  esac
}

if [[ "$is_sourced" -eq 1 ]]; then
  activate_environment
  doctor
else
  main "${1:-install}"
fi
