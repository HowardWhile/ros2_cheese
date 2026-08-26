#!/usr/bin/env bash

# Download and install ros2_cheese Debian packages for the local architecture.
set -euo pipefail

github_repository="HowardWhile/ros2_cheese"
github_release_api_url="https://api.github.com/repos/${github_repository}/releases/latest"
ros_distro="${ROS_DISTRO:-jazzy}"
download_directory="${CHEESE_DOWNLOAD_DIR:-${XDG_DATA_HOME:-${HOME}/.local/share}/ros2-cheese}"

if (( $# == 0 )); then
  install_mode="online"
elif (( $# == 1 )) && [[ "$1" == "--offline" ]]; then
  install_mode="offline"
else
  echo "Usage: $0 [--offline]" >&2
  exit 2
fi

require_command() {
  local command_name="$1"
  command -v "$command_name" >/dev/null 2>&1 || {
    echo "Required command not found: $command_name" >&2
    exit 1
  }
}

run_as_root() {
  if (( $(id -u) == 0 )); then "$@"; else require_command sudo; sudo "$@"; fi
}

require_command uname
require_command dpkg
require_command dpkg-deb
require_command find
require_command unzip

system_name="$(uname -s)"
machine_architecture="$(uname -m)"
case "${system_name}:${machine_architecture}" in
  Linux:x86_64|Linux:amd64) release_architecture="x86_64"; debian_architecture="amd64" ;;
  Linux:aarch64|Linux:arm64) release_architecture="arm64"; debian_architecture="arm64" ;;
  *)
    echo "Unsupported system or CPU architecture: ${system_name} ${machine_architecture}" >&2
    echo "Available releases: Linux x86_64 and arm64 Debian packages." >&2
    exit 1
    ;;
esac

if [[ "$(dpkg --print-architecture)" != "$debian_architecture" ]]; then
  echo "The detected CPU architecture does not match this Debian system." >&2
  exit 1
fi

asset_filename="ros-${ros_distro}-cheese-${release_architecture}.zip"
script_path="${BASH_SOURCE[0]-}"
release_directory=""
if [[ -n "$script_path" && -f "$script_path" ]]; then
  script_directory="$(cd -- "$(dirname -- "$script_path")" && pwd)"
  repository_directory="$(cd -- "$script_directory/.." && pwd)"
  release_directory="$repository_directory/dist/deb/$ros_distro/$release_architecture"
fi

archive_path=""
release_version=""

if [[ "$install_mode" == "offline" ]]; then
  if [[ -z "$release_directory" || ! -d "$release_directory" ]]; then
    echo "Offline mode could not find the release directory: ${release_directory:-repository path unavailable}" >&2
    echo "Build the Debian packages first: ./scripts/build_debs.sh --arch $release_architecture" >&2
    exit 1
  fi
  archive_path="$release_directory/$asset_filename"
  if [[ ! -f "$archive_path" ]]; then
    echo "Offline mode could not find $asset_filename: $release_directory" >&2
    echo "Build the Debian packages first: ./scripts/build_debs.sh --arch $release_architecture" >&2
    exit 1
  fi
else
  require_command wget
  require_command mktemp
  require_command sed
  require_command tr
  require_command grep
  require_command head

  temporary_directory="$(mktemp -d)"
  release_metadata_path="$temporary_directory/release.json"
  download_path=""
  cleanup_download() {
    rm -f -- "$release_metadata_path" "$download_path"
    rmdir -- "$temporary_directory" 2>/dev/null || true
  }
  trap cleanup_download EXIT

  echo "Fetching the latest GitHub release..."
  if ! wget --quiet --header='Accept: application/vnd.github+json' \
    --header='X-GitHub-Api-Version: 2022-11-28' \
    --output-document="$release_metadata_path" "$github_release_api_url"; then
    echo "Failed to fetch the latest GitHub release: $github_repository" >&2
    exit 1
  fi

  archive_url="$({ tr ',' '\n' < "$release_metadata_path" |
    sed -n 's/.*"browser_download_url"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' |
    { grep -F "/$asset_filename" || true; }; } | head -n 1)"
  release_version="$(sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$release_metadata_path" | head -n 1)"

  if [[ -z "$archive_url" ]]; then
    echo "The latest GitHub release does not contain $asset_filename." >&2
    exit 1
  fi
  if [[ "${archive_url##*/}" != "$asset_filename" ]]; then
    echo "Unexpected GitHub release asset name: ${archive_url##*/}" >&2
    exit 1
  fi

  mkdir -p "$download_directory"
  download_path="$temporary_directory/$asset_filename"
  echo "Downloading: $asset_filename"
  if ! wget --progress=bar:force:noscroll --output-document="$download_path" "$archive_url"; then
    echo "Release download failed: $archive_url" >&2
    exit 1
  fi
  [[ -s "$download_path" ]] || { echo "The downloaded release archive is empty." >&2; exit 1; }

  archive_path="$download_directory/$asset_filename"
  mv -f -- "$download_path" "$archive_path"
  download_path=""
fi

temporary_install_directory="$(mktemp -d)"
cleanup_install() {
  rm -rf -- "$temporary_install_directory"
  if [[ -n "${temporary_directory:-}" ]]; then
    rm -f -- "${release_metadata_path:-}" "${download_path:-}"
    rmdir -- "$temporary_directory" 2>/dev/null || true
  fi
}
trap cleanup_install EXIT

echo "Extracting: $(basename "$archive_path")"
unzip -q "$archive_path" -d "$temporary_install_directory"
mapfile -d '' deb_packages < <(find "$temporary_install_directory" -type f -name '*.deb' -print0 | sort -z)
if (( ${#deb_packages[@]} == 0 )); then
  echo "The release archive does not contain any Debian packages." >&2
  exit 1
fi

for deb_package in "${deb_packages[@]}"; do
  if [[ "$(dpkg-deb --field "$deb_package" Architecture)" != "$debian_architecture" ]]; then
    echo "Package architecture does not match this system: $(basename "$deb_package")" >&2
    exit 1
  fi
done

[[ -n "$release_version" ]] || release_version="$(dpkg-deb --field "${deb_packages[0]}" Version)"
require_command apt-get
echo "Installing ${#deb_packages[@]} Debian package(s)..."
run_as_root apt-get install --yes "${deb_packages[@]}"

echo "ros2_cheese installed."
echo "Version: $release_version"
echo
echo "Run the node:"
echo "  ros2 run cheese cheese"
echo
echo "Downloaded archive: $archive_path"
echo "To uninstall:"
echo "  sudo apt-get remove ros-${ros_distro}-cheese ros-${ros_distro}-cheese-interfaces"
