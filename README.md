#Nguyễn Thế Dân-231432188

Link wokwi:[click tại đây](https://wokwi.com/projects/478017039221952513)

# ESP32 Aquarium Monitoring System

## 1. Giới thiệu

Hệ thống giám sát và điều khiển môi trường bể nuôi thủy, hải sản sử dụng vi điều khiển ESP32, cảm biến và công nghệ IoT.

Hệ thống có khả năng đo mực nước, nhiệt độ, truyền dữ liệu thông qua MQTT và hiển thị các thông số trên giao diện Web theo thời gian thực. Đồng thời, hệ thống hỗ trợ điều khiển máy bơm và van xả nước tự động hoặc thủ công thông qua FreeRTOS.

## 2. Phần cứng

- ESP32 DevKit C V4
- Cảm biến siêu âm HC-SR04
- Cảm biến nhiệt độ DS18B20
- Điện trở 4.7kΩ (Pull-up cho DS18B20)
- Relay mô phỏng máy bơm nước vào
- Relay mô phỏng van thoát nước

## 3. Công nghệ sử dụng

- Arduino IDE
- Wokwi Simulator
- MQTT Broker (EMQX)
- HTML, CSS, JavaScript
- Chart.js
- FreeRTOS
- WiFi

## 4. Chức năng

- Đo và hiển thị mực nước trong bể.
- Đo và hiển thị nhiệt độ nước.
- Gửi dữ liệu cảm biến lên MQTT Broker.
- Hiển thị dữ liệu trên Web theo thời gian thực.
- Hiển thị biểu đồ biến thiên mực nước và nhiệt độ.
- Điều khiển máy bơm và van xả ở chế độ MANUAL.
- Tự động điều khiển máy bơm và van xả theo ngưỡng mực nước ở chế độ AUTO.
- Phân chia các tác vụ đọc cảm biến, truyền dữ liệu và điều khiển thiết bị bằng FreeRTOS.

## 5. Sơ đồ kết nối phần cứng

| Thiết bị | Chân thiết bị | Chân ESP32 |
|---|---|---|
| HC-SR04 | TRIG | GPIO 5 |
| HC-SR04 | ECHO | GPIO 18 |
| HC-SR04 | VCC | VIN (5V) |
| HC-SR04 | GND | GND |
| DS18B20 | DATA | GPIO 4 |
| DS18B20 | VCC | 3V3 |
| DS18B20 | GND | GND |
| Relay máy bơm | IN | GPIO 26 |
| Relay máy bơm | VCC | VIN (5V) |
| Relay máy bơm | GND | GND |
| Relay van xả | IN | GPIO 27 |
| Relay van xả | VCC | VIN (5V) |
| Relay van xả | GND | GND |

**Lưu ý:**
- DS18B20 sử dụng điện trở 4.7kΩ nối giữa DATA và 3V3.
- HC-SR04 sử dụng nguồn 5V.
- Khi triển khai phần cứng thực tế, chân ECHO của HC-SR04 cần qua mạch chia áp để đảm bảo tín hiệu đầu vào ESP32 ở mức 3.3V.
- Relay cần được kiểm tra mức kích hoạt HIGH/LOW phù hợp với module sử dụng.

## 6. Phân chia tác vụ FreeRTOS

- **TaskReadData:** Đọc dữ liệu từ cảm biến, tính toán mực nước và gửi dữ liệu vào Queue.
- **TaskTransmitData:** Kết nối WiFi, nhận dữ liệu từ Queue và truyền lên MQTT Broker.
- **TaskAlert:** Kiểm tra mực nước và điều khiển máy bơm, van xả theo chế độ hoạt động.

Các tác vụ trao đổi dữ liệu thông qua Queue của FreeRTOS.
