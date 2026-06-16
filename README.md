# ros2_cheese

ROS 2 拍照工具 package。



## Package 列表

| Package 名稱          | 功能說明                       |
| --------------------- | ------------------------------ |
| `cheese`              | 觸發式影像擷取與儲存           |
| `cheese_interfaces`   | Cheese 自訂 service interface  |
|                       |                                |



## Node 列表

| Node 名稱 | 功能說明                                |
| --------- | --------------------------------------- |
| `cheese`  | 訂閱 image topic，收到 trigger 後存圖   |
|           |                                         |
|           |                                         |



## Launch 列表

| Launch 名稱        | 功能說明                 |
| ------------------ | ------------------------ |
| `cheese.launch.py` | 啟動 Cheese 拍照服務     |
|                    |                          |



## 使用方法

啟動 Cheese 拍照服務：

```shell
ros2 launch cheese cheese.launch.py
```

預設圖片輸出路徑：

```shell
/tmp/ros2_cheese
```

可指定 image topic：

```shell
ros2 launch cheese cheese.launch.py image_topic:=/camera/color/image_raw
```

可指定輸出路徑：

```shell
ros2 launch cheese cheese.launch.py capture_dir:=/tmp/ros2_cheese
```

可指定保留上限：

```shell
ros2 launch cheese cheese.launch.py max_files:=1000 max_mb:=1024
```



## 測試方法

直接呼叫 ROS 2 service 拍照：

```shell
ros2 service call /cheese/trigger std_srvs/srv/Trigger "{}"
```



## Service

| Service 名稱      | Type                   | 功能說明       |
| ----------------- | ---------------------- | -------------- |
| `/cheese/trigger` | `std_srvs/srv/Trigger` | 觸發拍照並存圖 |
|                   |                        |                |



## Topic

| Topic 名稱       | Type                  | 功能說明             |
| ---------------- | --------------------- | -------------------- |
| `/cheese/status` | `std_msgs/msg/String` | 每秒發布影像串流狀態 |
|                  |                       |                      |

`/cheese/status` 內容為 JSON 字串，包含訂閱狀態、串流是否異常、fps、bandwidth，以及目前輸出資料夾的檔案數量與容量統計。

Status JSON 範例：

```json
{
  "image_topic": "/head_camera/color/image_raw/compressed",
  "subscribed": true,
  "subscription_type": "compressed",
  "stream_ok": true,
  "stream_abnormal": false,
  "seconds_since_last_image": 0.012,
  "window": {
    "seconds": 1.0,
    "frames": 20,
    "bytes": 7123456,
    "failures": 0
  },
  "total_failures": 0,
  "stats_window_sec": 5.0,
  "fps": {
    "current": 20.0,
    "min": 19.8,
    "avg": 20.1,
    "max": 20.4,
    "samples": 100
  },
  "bandwidth_mbps": {
    "current": 57.0,
    "min": 55.8,
    "avg": 56.5,
    "max": 58.1,
    "samples": 100
  },
  "capture_dir": {
    "path": "/tmp/ros2_cheese",
    "exists": true,
    "files": 12,
    "bytes": 3456789,
    "mb": 3.3,
    "max_files": 1000,
    "max_mb": 1024,
    "max_bytes": 1073741824
  }
}
```

Status JSON 欄位說明：

| 欄位 | 功能說明 |
| ---- | -------- |
| `image_topic` | 目前指定的影像 topic |
| `subscribed` | 是否已成功訂閱 image topic |
| `subscription_type` | 訂閱型態：`raw`、`compressed` 或 `unknown` |
| `stream_ok` | 影像串流是否正常 |
| `stream_abnormal` | 影像串流是否異常 |
| `seconds_since_last_image` | 距離上一張成功影像的秒數，尚未收到影像時為 `-1` |
| `window` | 最近一次 status 發布週期內的統計，約 1 秒 |
| `window.frames` | 最近一次 status 發布週期內收到的影像張數 |
| `window.bytes` | 最近一次 status 發布週期內收到的影像資料量 |
| `window.failures` | 最近一次 status 發布週期內的影像處理失敗次數 |
| `total_failures` | node 啟動後累計影像處理失敗次數 |
| `stats_window_sec` | `fps` 與 `bandwidth_mbps` 的 min/avg/max 滑動視窗秒數 |
| `fps.current` | 最近一次 status 發布週期的平均 FPS |
| `fps.min` / `fps.avg` / `fps.max` | 最近 `stats_window_sec` 秒內，每次 image callback 統計出的 FPS 最小/平均/最大值 |
| `fps.samples` | 最近 `stats_window_sec` 秒內的 FPS 統計 sample 數量 |
| `bandwidth_mbps.current` | 最近一次 status 發布週期的平均 bandwidth，單位 Mbps |
| `bandwidth_mbps.min` / `bandwidth_mbps.avg` / `bandwidth_mbps.max` | 最近 `stats_window_sec` 秒內，每次 image callback 統計出的 bandwidth 最小/平均/最大值，單位 Mbps |
| `bandwidth_mbps.samples` | 最近 `stats_window_sec` 秒內的 bandwidth 統計 sample 數量 |
| `capture_dir.path` | 圖片輸出資料夾 |
| `capture_dir.exists` | 圖片輸出資料夾是否存在 |
| `capture_dir.files` | 圖片輸出資料夾中的檔案數量 |
| `capture_dir.bytes` | 圖片輸出資料夾目前容量，單位 bytes |
| `capture_dir.mb` | 圖片輸出資料夾目前容量，單位 MB |
| `capture_dir.max_files` | `max_files` 設定值，`0` 表示不限制 |
| `capture_dir.max_mb` | `max_mb` 設定值，`0` 表示不限制 |
| `capture_dir.max_bytes` | `max_mb` 換算後的 bytes 上限 |



## 輸出

拍照成功時會輸出 `.jpg` 圖片：

```shell
/tmp/ros2_cheese/cheese-YYYYMMDD-HHMMSS-mmm.jpg
```

Service response 的 `message` 會回傳圖片完整路徑。



## Note

`image_topic` 可使用以下 ROS 2 message type：

```shell
sensor_msgs/msg/Image
sensor_msgs/msg/CompressedImage
```

`max_files` 與 `max_mb` 設為 `0` 時表示不限制。
