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

The script first builds (or reuses) a Docker builder image. After a successful
package build it asks whether to remove that image; the default is to keep it.
Building arm64 on an amd64 host requires Docker with ARM emulation configured.
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
readonly BUILDER_IMAGE="ros2-cheese-deb-builder:${ROS_DISTRO}-${ARCH}"
readonly OUTPUT_DIR="$REPOSITORY_DIR/dist/deb/$ROS_DISTRO/$ARCH"

if [[ "$ARCH" == "arm64" ]] && ! docker run --rm \
  --platform "$PLATFORM" \
  --entrypoint /bin/sh \
  "ros:${ROS_DISTRO}-ros-base" \
  -c true >/dev/null 2>&1; then
  cat >&2 <<'EOF'
Docker cannot execute arm64 containers on this host.

Enable the arm64 QEMU/binfmt emulator once, then rerun this script:
  docker run --privileged --rm tonistiigi/binfmt --install arm64

Verify the setup with:
  docker run --rm --platform linux/arm64 alpine uname -m

Expected output: aarch64
EOF
  exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "Building or reusing builder image: $BUILDER_IMAGE"
docker build \
  --platform "$PLATFORM" \
  --build-arg "ROS_DISTRO=$ROS_DISTRO" \
  --tag "$BUILDER_IMAGE" \
  --file "$SCRIPT_DIR/Dockerfile.deb-builder" \
  "$REPOSITORY_DIR"

echo "Building $ROS_DISTRO Debian packages for $ARCH..."
docker run --rm \
  --platform "$PLATFORM" \
  --volume "$REPOSITORY_DIR:/workspace:ro" \
  --volume "$OUTPUT_DIR:/output" \
  --env "ROS_DISTRO=$ROS_DISTRO" \
  --env "OS_VERSION=$OS_VERSION" \
  "$BUILDER_IMAGE" \
  bash -ceu '
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

if [[ -t 0 ]]; then
  if read -r -p "Remove builder image $BUILDER_IMAGE? [y/N] " response; then
    case "$response" in
      [Yy]|[Yy][Ee][Ss])
        docker image rm "$BUILDER_IMAGE"
        ;;
      *)
        echo "Keeping builder image: $BUILDER_IMAGE"
        ;;
    esac
  fi
else
  echo "Non-interactive shell: keeping builder image: $BUILDER_IMAGE"
fi
