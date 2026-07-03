# `/cheese/status`

[English](#english) | [繁體中文](#zh-tw)

<a id="english"></a>

## English

`/cheese/status` uses `std_msgs/msg/String` and publishes once per second. Its
JSON payload reports subscription state, stream health, frame rate, bandwidth,
and output directory usage.

<a id="zh-tw"></a>

## 繁體中文

`/cheese/status` 使用 `std_msgs/msg/String`，每秒發布一次。JSON 內容包含訂閱
狀態、串流健康狀態、FPS、頻寬，以及輸出資料夾的檔案數量與容量統計。

## JSON example／JSON 範例

```json
{
  "image_topic": "/camera/color/image_raw/compressed",
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

## Fields／欄位

| Field | English | 繁體中文 |
| --- | --- | --- |
| `image_topic` | Configured image topic | 目前指定的影像 topic |
| `subscribed` | Whether the node has subscribed successfully | 是否已成功訂閱影像 topic |
| `subscription_type` | `raw`, `compressed`, or `unknown` | 訂閱型態：`raw`、`compressed` 或 `unknown` |
| `stream_ok` | Whether the stream is healthy | 影像串流是否正常 |
| `stream_abnormal` | Whether the stream is abnormal | 影像串流是否異常 |
| `seconds_since_last_image` | Seconds since the last valid image; `-1` if none has arrived | 距離上一張有效影像的秒數；尚未收到影像時為 `-1` |
| `window` | Statistics for the latest status interval | 最近一次發布週期的統計，約一秒 |
| `window.frames` | Images received during the interval | 發布週期內收到的影像張數 |
| `window.bytes` | Image data received during the interval | 發布週期內收到的影像資料量 |
| `window.failures` | Processing failures during the interval | 發布週期內的影像處理失敗次數 |
| `total_failures` | Processing failures since startup | node 啟動後的累計處理失敗次數 |
| `stats_window_sec` | Sliding-window duration for FPS and bandwidth | FPS 與頻寬統計的滑動視窗秒數 |
| `fps.current` | Average FPS during the latest interval | 最近發布週期的平均 FPS |
| `fps.min` / `fps.avg` / `fps.max` | Minimum, average, and maximum FPS in the statistics window | 統計視窗內的 FPS 最小值、平均值與最大值 |
| `fps.samples` | FPS sample count | FPS 樣本數 |
| `bandwidth_mbps.current` | Average bandwidth during the latest interval, in Mbps | 最近發布週期的平均頻寬，單位 Mbps |
| `bandwidth_mbps.min` / `bandwidth_mbps.avg` / `bandwidth_mbps.max` | Minimum, average, and maximum bandwidth in the statistics window | 統計視窗內的頻寬最小值、平均值與最大值 |
| `bandwidth_mbps.samples` | Bandwidth sample count | 頻寬樣本數 |
| `capture_dir.path` | Output directory | 影像輸出資料夾 |
| `capture_dir.exists` | Whether the directory exists | 輸出資料夾是否存在 |
| `capture_dir.files` | Number of files | 輸出資料夾中的檔案數量 |
| `capture_dir.bytes` | Directory size in bytes | 輸出資料夾容量，單位 bytes |
| `capture_dir.mb` | Directory size in MB | 輸出資料夾容量，單位 MB |
| `capture_dir.max_files` | File limit; `0` means unlimited | 檔案數量上限；`0` 表示不限制 |
| `capture_dir.max_mb` | Storage limit in MB; `0` means unlimited | 容量上限，單位 MB；`0` 表示不限制 |
| `capture_dir.max_bytes` | Byte representation of `max_mb` | `max_mb` 換算後的 bytes 上限 |
