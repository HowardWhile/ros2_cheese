# 建立 Debian 安裝包

本文件供開發者建立 `cheese` 與 `cheese_interfaces` 的 Debian 安裝包使用。

## 前置條件

- Docker 可正常執行。
- 若在 x86_64 主機上建立 arm64 套件，Docker 必須設定 ARM 模擬。Docker
  Desktop 通常已內建；否則請在 arm64 主機上原生建置。

## 建置

打包腳本會在目標架構的 ROS Docker 容器中，依下列順序建置：

1. 建立 `cheese_interfaces` 的 deb。
2. 在容器內安裝該本地 deb 作為建置相依。
3. 建立 `cheese` 的 deb。

```shell
# x86_64（Debian 套件架構名稱為 amd64）
./scripts/build_debs.sh --arch amd64

# ARM64
./scripts/build_debs.sh --arch arm64
```

預設目標為 ROS 2 Jazzy 與 Ubuntu Noble。輸出會寫入：

```text
dist/deb/jazzy/amd64/
dist/deb/jazzy/arm64/
```

可透過環境變數或選項調整 ROS 發行版與 Ubuntu 代號：

```shell
ROS_DISTRO=jazzy OS_VERSION=noble ./scripts/build_debs.sh --arch arm64

# 等效寫法
./scripts/build_debs.sh --arch arm64 --ros-distro jazzy --os-version noble
```

使用 `./scripts/build_debs.sh --help` 可查看所有選項。
