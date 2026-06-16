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
