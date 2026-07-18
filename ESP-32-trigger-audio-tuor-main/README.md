# GPS Audio Tour

Đồ án môn học sử dụng **ESP32**, có khả năng tự động phát các bài thuyết minh (âm thanh) khi người dùng di chuyển đến một tọa độ GPS cụ thể.

---

## Tính năng nổi bật
* **Định vị thời gian thực:** Sử dụng module GPS để xác định vị trí liên tục.
* **Phát âm thanh tự động:** So sánh tọa độ hiện tại với danh sách điểm đến, tự động kích hoạt file âm thanh.
* **Cấu hình linh hoạt:** Dữ liệu tọa độ được lưu trong file `config.txt` (SPIFFS), có thể sửa đổi mà không cần nạp lại code.
* **Cơ chế dự phòng:** Tự động tạo file cấu hình mẫu nếu hệ thống bị reset hoặc mất dữ liệu.

## 🛠️ Yêu cầu phần cứng
1.  **Vi điều khiển:** ESP32 (ESP32 DevKit V1).
2.  **GPS:** Module NEO-6M.
3.  **Âm thanh:** Module DFPlayer.
4.  **Lưu trữ:** Bộ nhớ Flash nội bộ (SPIFFS) hoặc thẻ nhớ SD.

## Cài đặt & Sử dụng

### 1. Chuẩn bị môi trường
Dự án được phát triển trên **ESP-IDF** (Espressif IoT Development Framework).

### 2. Biên dịch và Nạp code
Mở Terminal tại thư mục dự án và chạy các lệnh:

```bash
# Cấu hình dự án (nếu cần chỉnh Serial Port, Flash size...)
idf.py menuconfig

# Biên dịch toàn bộ dự án
idf.py build

# Nạp code và mở màn hình theo dõi (Monitor)
idf.py -p COMx flash monitor, x là cổng thực tế trên máy tính

#Cấu trúc file Config (config.txt): Vĩ_độ, Kinh_độ, ID_File_Nhạc, Tên_Địa_Điểm
