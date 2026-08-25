# 建立 Debian 安裝包

本文件供開發者建立 `cheese` 與 `cheese_interfaces` 的 Debian 安裝包使用。

## 前置條件

- Docker 可正常執行。
- 若在 x86_64 主機上建立 arm64 套件，Docker 必須設定 ARM 模擬。Docker
  Desktop 通常已內建；否則請在 arm64 主機上原生建置。

在 Linux x86_64 Docker 主機上，可用下列指令一次性安裝 arm64 QEMU/binfmt
模擬器（需要能執行 privileged Docker container）：

```shell
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

確認模擬器已可用：

```shell
docker run --rm --platform linux/arm64 alpine uname -m
# 預期輸出：aarch64
```

## 建置

打包腳本會先建立目標架構的 Docker builder image。image 包含 ROS 建置工具與
`package.xml` 所列的已發布相依；只要這些相依未變更，後續執行會重用 Docker
快取，不必每次重新安裝。接著它會啟動暫存容器，依下列順序建置：

1. 建立 `cheese_interfaces` 的 deb。
2. 在容器內安裝該本地 deb 作為建置相依。
3. 建立 `cheese` 的 deb。

```shell
# x86_64（Debian 套件架構名稱為 amd64）
./scripts/build_debs.sh --arch amd64

# 在 x86_64 主機建立 ARM64 套件前，先啟用 Docker 的 arm64 模擬
docker run --privileged --rm tonistiigi/binfmt --install arm64

# ARM64 套件
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

成功後，腳本會詢問是否移除 builder image：

```text
Remove builder image ros2-cheese-deb-builder:jazzy-amd64? [y/N]
```

直接按 Enter 或輸入其他值會保留 image，以加速下次建置；輸入 `y` 或 `yes` 才會
移除。非互動式環境一律保留 image。
