# Hướng Dẫn Mô Phỏng Nhúng Wokwi (CO3053 - BTL 2)

Thư mục này chứa toàn bộ cấu hình và mã nguồn để chạy **Mô phỏng phần cứng nhúng trực quan 100% trên trình duyệt web (Wokwi Simulator)** hoặc qua VS Code Wokwi Extension.

---

## 1. Thành Phần Mạch Phần Cứng (Bố Trí Trên Wokwi)

1. **Khối vi điều khiển:**
   * Bo mạch **Arduino Uno** (hoặc ESP32).
2. **Khối hiển thị:**
   * **Màn hình LCD 1602 I2C** (địa chỉ `0x27`, kết nối chân `A4-SDA`, `A5-SCL`).
   * Dòng 1 hiển thị: Trạng thái máy (`STANDBY`, `READY`, `RUN`, `PAUS`) và Đồng hồ đếm lùi `MM:SS`.
   * Dòng 2 hiển thị: Trạng thái Động cơ (`AGIT`, `SPIN`, `OFF`), Bơm xả (`PUMP: 1/0`), và Khóa cửa (`LCK: 1/0`).
3. **Khối nút bấm điều khiển (Active-Low kèm pull-up):**
   * Nút **RUN** (Xanh lá) - Chân `D2`.
   * Nút **PAUSE** (Vàng) - Chân `D3`.
   * Nút **STOP** (Đỏ) - Chân `D4` (Hỗ trợ bắt nhấp đúp $T \le 1.5\,\text{s}$).
4. **Khối nạp tiền xu:**
   * Nút **10¢** (Trắng) - Chân `D5`.
   * Nút **20¢** (Trắng) - Chân `D6`.
   * Nút **50¢** (Cam/Vàng) - Chân `D7`.
5. **Khối cảm biến sự cố:**
   * Công tắc gạt **DOOR FAULT** - Chân `D8` (Gạt sang trái để giả lập mở cửa lúc giặt $\rightarrow$ chuyển sang `ERROR`).
6. **Khối đèn LED báo hiệu:**
   * **RLED (Đỏ)** - Chân `D9` kèm điện trở 220 $\Omega$: Sáng đứng ở `STANDBY`, nhấp nháy 2.0 Hz ở `ERROR`.
   * **BLED (Xanh dương)** - Chân `D10` kèm điện trở 220 $\Omega$: Sáng đứng ở `READY`, nhấp nháy 1.0 Hz ở `RUNNING`.
7. **Khối chỉ thị cơ cấu chấp hành (Relays):**
   * Đèn **MTR AGITATE** (Xanh lá) - Chân `D11` (Đảo chiều giặt).
   * Đèn **MTR SPIN DRY** (Vàng) - Chân `D12` (Vắt cao tốc).
   * Đèn **DRAIN PUMP** (Xanh cyan) - Chân `D13` (Bơm xả nước).
   * Đèn **DOOR LOCK** (Trắng) - Chân `A0` (Khóa chốt an toàn).

---

## 2. Cách Chạy Mô Phỏng Trên Web Wokwi (1-Click)

1. Truy cập [https://wokwi.com/projects/new/arduino-uno](https://wokwi.com/projects/new/arduino-uno)
2. Thay thế nội dung tab **`diagram.json`** bằng nội dung file [`sim/wokwi/diagram.json`](./diagram.json).
3. Thay thế nội dung tab **`sketch.ino`** bằng nội dung file [`sim/wokwi/sketch.ino`](./sketch.ino).
4. Thêm thư viện `LiquidCrystal I2C` vào tab **Library Manager**.
5. Bấm nút **Start Simulation (Play)** để trải nghiệm trực quan!
