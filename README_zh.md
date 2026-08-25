# ros2_cheese

[English](README.md) | **繁體中文**

適用於 ROS 2 的觸發式影像擷取工具。

## Package 列表

| Package 名稱          | 功能說明                       |
| --------------------- | ------------------------------ |
| `cheese`              | 觸發式影像擷取與儲存           |
| `cheese_interfaces`   | Cheese 自訂服務介面             |

## Node 列表

| Node 名稱 | 功能說明                                |
| --------- | --------------------------------------- |
| `cheese`  | 訂閱影像 topic，收到觸發要求後儲存影像 |

## Launch 列表

| Launch 名稱        | 功能說明                 |
| ------------------ | ------------------------ |
| `cheese.launch.py` | 啟動 Cheese 拍照服務     |

## 使用方法

啟動 Cheese 節點：

```shell
ros2 run cheese cheese
```

在另一個終端機中，呼叫標準 ROS 2 service 觸發拍照：

```shell
ros2 service call /cheese/trigger std_srvs/srv/Trigger "{}"
```

也可以指定字串作為檔名後綴：

```shell
ros2 service call /cheese/string_trigger cheese_interfaces/srv/StringTrigger "{message: test}"
```

預設圖片輸出路徑：

```shell
/tmp/ros2_cheese
```

## Services

| Service 名稱      | 型別                   | 功能說明       |
| ----------------- | ---------------------- | -------------- |
| `/cheese/trigger` | `std_srvs/srv/Trigger` | 觸發拍照並存圖 |
| `/cheese/string_trigger` | [`cheese_interfaces/srv/StringTrigger`](cheese_interfaces/srv/StringTrigger.srv) | 觸發拍照並使用 `message` 作為檔名後綴 |

## Topics

| Topic 名稱       | 型別                  | 功能說明             |
| ---------------- | --------------------- | -------------------- |
| [`/cheese/status`](doc/status.md#zh-tw) | `std_msgs/msg/String` | 每秒發布影像串流狀態 |

## Parameters

### Node 參數

| 參數 | 型別 | 預設值 | 功能說明 |
| --- | --- | --- | --- |
| `image_topic` | `string` | `/camera/color/image_raw` | 要訂閱的影像 topic |
| `capture_dir` | `string` | `/tmp/ros2_cheese` | 儲存拍攝影像的資料夾 |
| `max_files` | `integer` | `1000` | 最多保留的影像數量；`0` 表示不限制 |
| `max_mb` | `integer` | `1024` | 儲存容量上限，單位 MB；`0` 表示不限制 |

### Launch 參數

| 參數 | 型別 | 預設值 | 功能說明 |
| --- | --- | --- | --- |
| `image_topic` | `string` | `/camera/color/image_raw` | 傳遞給 node 的 `image_topic` 參數 |
| `capture_dir` | `string` | `/tmp/ros2_cheese` | 傳遞給 node 的 `capture_dir` 參數 |
| `max_files` | `integer` | `1000` | 傳遞給 node 的 `max_files` 參數 |
| `max_mb` | `integer` | `1024` | 傳遞給 node 的 `max_mb` 參數 |
| `node_name` | `string` | `cheese` | 指定 node 名稱 |
| `namespace` | `string` | 空字串 | 指定 node namespace |

### 進階使用方式

透過 launch file 啟動 Cheese 節點：

```shell
ros2 launch cheese cheese.launch.py
```

指定影像 topic：

```shell
ros2 launch cheese cheese.launch.py image_topic:=/camera/color/image_raw
```

指定輸出路徑：

```shell
ros2 launch cheese cheese.launch.py capture_dir:=/tmp/ros2_cheese
```

指定 node 名稱：

```shell
ros2 launch cheese cheese.launch.py node_name:=camera_cheese
```

指定 namespace：

```shell
ros2 launch cheese cheese.launch.py namespace:=camera node_name:=cheese
```

指定檔案數量與儲存容量上限：

```shell
ros2 launch cheese cheese.launch.py max_files:=1000 max_mb:=1024
```

## 輸出

拍照成功時會輸出 `.jpg` 圖片，`message` 空白時會省略後綴：

```shell
/tmp/ros2_cheese/YYYYMMDD-HHMMSS-mmm-<message>.jpg
```

Service request 的 `message` 會作為檔名後綴，會自動將不適合檔名的字元轉成 `_`。Service response 的 `message` 會回傳圖片完整路徑。

## 注意事項

`image_topic` 支援下列 ROS 2 message 型別：

```text
sensor_msgs/msg/Image
sensor_msgs/msg/CompressedImage
```

將 `max_files` 或 `max_mb` 設為 `0`，即可停用對應的保留上限。

## 開發者文件

- [建立 Debian 安裝包](doc/debian-packaging.md)
