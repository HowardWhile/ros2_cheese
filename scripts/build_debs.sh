#!/usr/bin/env bash
# Build ROS 2 binary Debian packages in an architecture-matched ROS container.
#
# Examples:
#   ./scripts/build_debs.sh --arch amd64
#   ./scripts/build_debs.sh --arch arm64 --ros-distro jazzy
set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly REPOSITORY_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

ARCH=""
ROS_DISTRO="${ROS_DISTRO:-jazzy}"
OS_VERSION="${OS_VERSION:-noble}"

usage() {
  cat <<'EOF'
Usage: scripts/build_debs.sh --arch {amd64|arm64} [options]

Builds ros-<distro>-cheese-interfaces and ros-<distro>-cheese Debian packages.

Options:
  --arch ARCH          Target Debian architecture: amd64 or arm64 (required)
  --ros-distro DISTRO  ROS 2 distribution (default: jazzy, or $ROS_DISTRO)
  --os-version CODE    Ubuntu codename used by bloom (default: noble, or $OS_VERSION)
  -h, --help           Show this help

Output: dist/deb/<ros-distro>/<arch>/

The script uses Docker's --platform support.  Building arm64 on an amd64 host
requires Docker with ARM emulation configured (Docker Desktop normally provides it).
EOF
}

while (($#)); do
  case "$1" in
    --arch)
      ARCH="${2:-}"
      shift 2
      ;;
    --ros-distro)
      ROS_DISTRO="${2:-}"
      shift 2
      ;;
    --os-version)
      OS_VERSION="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "$ARCH" in
  amd64|arm64) ;;
  *)
    echo "--arch must be either amd64 or arm64." >&2
    usage >&2
    exit 2
    ;;
esac

command -v docker >/dev/null || {
  echo "Docker is required. Install Docker (with build emulation for arm64) and retry." >&2
  exit 1
}

readonly PLATFORM="linux/$ARCH"
readonly IMAGE="ros:${ROS_DISTRO}-ros-base"
readonly OUTPUT_DIR="$REPOSITORY_DIR/dist/deb/$ROS_DISTRO/$ARCH"
mkdir -p "$OUTPUT_DIR"

echo "Building $ROS_DISTRO Debian packages for $ARCH..."
docker run --rm \
  --platform "$PLATFORM" \
  --volume "$REPOSITORY_DIR:/workspace:ro" \
  --volume "$OUTPUT_DIR:/output" \
  --env "ROS_DISTRO=$ROS_DISTRO" \
  --env "OS_VERSION=$OS_VERSION" \
  "$IMAGE" \
  bash -ceu '
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends python3-bloom fakeroot dpkg-dev python3-rosdep

    # Resolve released ROS and system dependencies. cheese_interfaces is built
    # locally below, so it must not be resolved from an external ROS repository.
    rosdep update
    rosdep install --from-paths /workspace/cheese_interfaces --ignore-src -r -y \
      --rosdistro "$ROS_DISTRO"
    rosdep install --from-paths /workspace/cheese --ignore-src -r -y \
      --rosdistro "$ROS_DISTRO" --skip-keys cheese_interfaces

    build_package() {
      package_name="$1"
      rm -rf "/tmp/$package_name"
      cp -a "/workspace/$package_name" "/tmp/$package_name"
      cd "/tmp/$package_name"
      bloom-generate rosdebian --os-name ubuntu --os-version "$OS_VERSION" \
        --ros-distro "$ROS_DISTRO"
      dpkg-buildpackage -b -uc -us
    }

    build_package cheese_interfaces
    apt-get install -y "/tmp/ros-${ROS_DISTRO}-cheese-interfaces_"*.deb
    build_package cheese

    cp /tmp/ros-"$ROS_DISTRO"-cheese*.deb /output/
  '

echo "Packages written to: $OUTPUT_DIR"
