# KẾ HOẠCH TỔNG THỂ & ĐẶC TẢ KỸ THUẬT BTL 2 (CO3053 - HỆ THỐNG NHÚNG)
**Đề tài:** Thiết Kế & Hiện Thực Hóa Khối Điều Khiển Máy Giặt Trả Xu (*Coin-Operated Washing Machine Control Unit*)  
**Giảng viên hướng dẫn:** PGS. TS. Phạm Hoàng Anh (`anhpham@hcmut.edu.vn`)  
**Trường:** Đại học Bách Khoa ĐHQG-HCM (HCMUT)  

---

> [!IMPORTANT]
> **CHỈ THỊ ƯU TIÊN HÀNG ĐẦU TỪ NGƯỜI DÙNG:**
> 1. **CHƯA CẦN LÀM BÁO CÁO (REPORT):** Tạm thời gác lại toàn bộ việc viết báo cáo LaTeX. Tập trung 100% nguồn lực nghiên cứu, phân tích tỉ mỉ, tối ưu hóa mã nguồn, thiết kế phần cứng nhúng, chống lỗi biên và kiểm thử tự động sao cho code đạt độ hoàn hảo tuyệt đối (10/10).
> 2. **KỶ LUẬT NHÁNH GIT:** Chỉ commit và push lên nhánh `nguyen` (`origin/nguyen`). Tuyệt đối không merge/push vào nhánh `main` khi chưa có yêu cầu. Giữ nguyên tắc không push thư mục `assignment1/`.
> 3. **TIÊU CHÍ "ĐÚNG, ĐỦ, CHUẨN HOÀN HẢO - KHÔNG LÀM THIẾU, KHÔNG LÀM DƯ":** Bám sát 100% từng câu chữ trong đề bài của PGS. TS. Phạm Hoàng Anh (`anhpham@hcmut.edu.vn`). Không bỏ sót bất kỳ yêu cầu nghiệp vụ nào, đồng thời không vẽ thêm tính năng rườm rà làm sai lệch tính chất cốt lõi của bài toán điều khiển nhúng.

---

## 1. TỔNG QUAN YÊU CẦU ĐỀ BÀI

Thiết kế và hiện thực hóa khối điều khiển (Control Unit) cho máy giặt tự động dùng tiền xu với các thành phần giao tiếp:
* **Ngõ vào điều khiển (Inputs):**
  * 3 nút bấm: `STOP` (Dừng), `RUN` (Chạy / Tiếp tục), `PAUSE` (Tạm dừng).
  * 1 đầu đọc tiền xu: Chỉ nhận 3 mệnh giá `10-cent`, `20-cent`, `50-cent`.
  * Cảm biến lỗi hệ thống (mở cửa khi vắt, quá tải motor, kẹt van cấp nước).
* **Ngõ ra hiển thị & chấp hành (Outputs):**
  * 2 đèn LED:
    * `RLED` (Đỏ): **Sáng đứng** khi ở trạng thái chờ phục vụ (`STANDBY`); **Nhấp nháy (2 Hz)** khi hệ thống gặp lỗi (`ERROR`).
    * `BLED` (Xanh): **Sáng đứng** khi máy đã nhận đủ tiền và sẵn sàng chạy (`READY`); **Nhấp nháy (1 Hz)** khi máy đang trong chu trình giặt (`RUNNING`).
  * Tải chấp hành: Động cơ lồng giặt (quay đảo chiều / vắt tốc độ cao), van cấp nước, bơm xả, khóa chốt cửa an toàn.

---

## 2. BỐN "BẪY LOGIC" QUAN TRỌNG NHẤT CẦN NẮM VỮNG

| Bẫy logic | Yêu cầu khắt khe của đề bài | Cách xử lý trong mã nguồn |
| :--- | :--- | :--- |
| **1. Nuốt tiền không thối lại (Zero Refund)** | Khách phải nạp tối thiểu $50¢$ máy mới cho phép chạy. Nếu nạp dư (ví dụ $60¢$, $70¢$, $100¢$), khi bấm `RUN`, máy kích hoạt giặt và **xóa sạch tiền về $0¢$ mà không hoàn trả tiền thừa**. | Khi chuyển từ `READY` $\rightarrow$ `RUNNING`, gán ngay `coin_balance = 0`. Tiền dư bị xóa hoàn toàn, không lưu sang phiên sau. |
| **2. Timer 30 phút bất biến khi Pause** | Chu trình giặt kéo dài 30 phút ($1800\,\text{giây}$). Khi đang chạy bấm `PAUSE`, động cơ ngừng quay nhưng **bộ đếm giờ 30 phút VẪN TIẾP TỤC ĐẾM LÙI**! | Bộ đếm thời gian được gắn vào ngắt Timer $1\,\text{giây}$ độc lập với trạng thái động cơ. Cả ở `RUNNING` lẫn `PAUSED`, mỗi giây trôi qua biến `remaining_cycle_sec` đều giảm đi 1. Nếu để Pause quá 30 phút, máy tự động kết thúc chu trình về `STANDBY`. |
| **3. Bấm STOP 2 lần mới dừng (Double Press)** | Để tránh người dùng vô tình quẹt tay vào nút `STOP` làm hỏng mẻ giặt, bấm `STOP` 1 lần máy **KHÔNG ĐƯỢC PHÉP DỪNG**. Phải bấm **2 lần liên tiếp** mới dừng cưỡng bức (Force Stop). | Khi phát hiện bấm `STOP` lần 1, mở cửa sổ trượt $T_{\text{double}} \le 1.5\,\text{giây}$. Nếu có lần bấm 2 trong $1.5\,\text{giây}$ $\rightarrow$ ngắt toàn bộ tải, về `STANDBY`. Nếu quá $1.5\,\text{giây}$ không bấm nữa $\rightarrow$ xóa cờ đệm, máy vẫn giặt bình thường. |
| **4. Tần số nhấp nháy LED độc lập** | LED nhấp nháy phải mượt mà, không được dùng hàm chờ `delay()` làm đơ máy trạng thái. | Điều khiển LED bằng hàm `tick` $1\,\text{ms}$ không chặn: `BLED` nhấp nháy chu kỳ $1.0\,\text{Hz}$ (500ms Sáng / 500ms Tắt); `RLED` báo lỗi nhấp nháy $2.0\,\text{Hz}$ (250ms Sáng / 250ms Tắt). |

---

## 3. MÔ HÌNH MÁY TRẠNG THÁI HỮU HẠN (FSM)

Hệ thống được thiết kế theo mô hình chuẩn 5 trạng thái:

```text
                  +----------------------------------------------------+
                  |                 [ 0. STANDBY ]                     |<----+
                  |   - RLED: Sáng đứng (Available to serve)           |     |
                  |   - BLED: Tắt, Động cơ: Tắt                        |     |
                  +----------------------------------------------------+     |
                    | (Đút tiền: 10¢, 20¢, 50¢)                              |
                    | [Điều kiện: Tổng tiền >= 50¢]                          |
                    v                                                        |
                  +----------------------------------------------------+     |
                  |                 [ 1. READY ]                       |     |
                  |   - RLED: Tắt                                      |     |
                  |   - BLED: Sáng đứng (Đủ tiền, chờ bấm RUN)         |     |
                  +----------------------------------------------------+     |
                    |                                                        |
                    | (Bấm nút RUN) -> [Xóa tiền = 0¢, Kích hoạt Timer 1800s]|
                    v                                                        |
      +---------> +----------------------------------------------------+     |
      |           |                 [ 2. RUNNING ]                     |     |
      |           |   - RLED: Tắt                                      |     |
      |           |   - BLED: Nhấp nháy 1 Hz (Đang giặt)               |     |
      |           |   - Động cơ: Hoạt động                             |     |
      |           |   - Timer 30 phút: ĐANG ĐẾM LÙI                    |     |
(Bấm RUN)         +----------------------------------------------------+     |
      |             |                                        |               |
      |       (Bấm PAUSE)                        (Hết 30 phút HOẶC           |
      |             |                             Bấm STOP 2 lần)            |
      |             v                                        |               |
      |           +------------------------------------+     |               |
      |           |          [ 3. PAUSED ]             |     |               |
      +-----------| - BLED: Sáng đứng                  |-----+               |
                  | - Động cơ: TẠM DỪNG                |                     |
                  | - Timer 30 phút: VẪN ĐẾM LÙI!      |                     |
                  +------------------------------------+                     |
                                                                             |
      * Bất kỳ trạng thái nào gặp sự cố (Kẹt cửa, mất nước, kẹt motor)       |
        -> Chuyển ngay sang [ 4. ERROR ]                                     |
           - RLED: Nhấp nháy 2 Hz (Báo lỗi khẩn cấp)                         |
           - BLED: Tắt, Ngắt toàn bộ tải động cơ/van/bơm                     |
           - Bấm nút CLEAR FAULT -> Khôi phục về [ 0. STANDBY ] -------------+
```

---

## 4. KIẾN TRÚC MÃ NGUỒN PHÂN TẦNG (LAYERED ARCHITECTURE)

```text
[ ỨNG DỤNG / SIMULATOR TƯƠNG TÁC / BỘ TEST TỰ ĐỘNG ]
                        ▲
                        │ Dispatch Event / Query State
                        ▼
[ LÕI MÁY TRẠNG THÁI FSM (washing_machine_fsm.c) ]
  - Quản lý trạng thái: STANDBY, READY, RUNNING, PAUSED, ERROR
  - Quản lý bộ đếm ngược 1800s liên tục
  - Quản lý nuốt tiền và bắt STOP đúp 1.5s
  - Quản lý chẩn đoán mã lỗi đa cảm biến
                        ▲
                        │ Gọi các hàm trừu tượng (Function Pointers)
                        ▼
[ TẦNG TRỪU TƯỢNG PHẦN CỨNG HAL (hal_interfaces.h) ]
  - hal_led: Điều khiển bật/tắt/chớp LED không chặn
  - hal_motor: Bật/tắt động cơ đảo chiều
  - hal_timer: Bộ đếm SysTick 1ms
                        ▲
          ┌─────────────┴─────────────┐
          ▼                           ▼
[ PHẦN CỨNG THẬT ]          [ MÔ PHỎNG VIRTUAL HAL ]
- Bo mạch STM32 / Arduino   - Chạy trên máy tính (mock_hal.c)
- Nút bấm, LED thật         - Tự động hóa kiểm thử 25 test cases
```

---

## 5. BẢNG 28 KỊCH BẢN KIỂM THỬ TỰ ĐỘNG (TEST MATRIX - 100% PASS)

| Mã Test | Tên kịch bản kiểm thử | Mô tả hành vi & Điều kiện kiểm tra | Kết quả |
| :---: | :--- | :--- | :---: |
| **TC-01** | Nạp tiền chưa đủ ngưỡng | Nạp 10¢ rồi 20¢ (=30¢) $\rightarrow$ Máy ở lại `STANDBY`, `RLED=ON`, `BLED=OFF`. | **PASS** |
| **TC-02** | Nạp đúng ngưỡng 50¢ | Nạp 50¢ $\rightarrow$ Chuyển sang `READY`, `RLED=OFF`, `BLED=ON` sáng đứng. | **PASS** |
| **TC-03** | Nạp tiền dư (Surplus) | Nạp 3 đồng 20¢ (=60¢) hoặc thêm 50¢ (=110¢) $\rightarrow$ Nhận hết, ở `READY`. | **PASS** |
| **TC-04** | **Chạy & Nuốt tiền (Zero Refund)** | Nạp 70¢, bấm `RUN` $\rightarrow$ Tiền bị xóa về **0¢**, Timer = 1800s, `RUNNING`, `BLED` chớp 1Hz. | **PASS** |
| **TC-05** | Bấm RUN sớm khi chưa đủ 50¢ | Khi tiền < 50¢ bấm `RUN` $\rightarrow$ Lệnh bị từ chối, máy không chạy. | **PASS** |
| **TC-06** | Tạm dừng & Tiếp tục | Đang giặt bấm `PAUSE` $\rightarrow$ Động cơ ngắt; Bấm `RUN` $\rightarrow$ Động cơ quay tiếp. | **PASS** |
| **TC-07** | **Timer đếm lùi trong Pause (Quan trọng)** | Giữ máy ở `PAUSED` 300 giây $\rightarrow$ Timer giảm đúng từ 1600s xuống 1300s dù động cơ tắt! | **PASS** |
| **TC-08** | Hết giờ khi đang Pause | Để máy ở `PAUSED` cho đến khi Timer về 0 $\rightarrow$ Máy tự ngắt về `STANDBY`. | **PASS** |
| **TC-09** | Bấm STOP 1 lần không dừng | Đang chạy bấm `STOP` 1 lần và đợi 2s $\rightarrow$ Máy vẫn tiếp tục giặt bình thường. | **PASS** |
| **TC-10** | **Bấm STOP 2 lần dừng khẩn cấp** | Đang chạy bấm `STOP` 2 lần cách nhau 300ms $\rightarrow$ Ngắt tức thì về `STANDBY`. | **PASS** |
| **TC-11** | Bấm STOP 2 lần khi đang Pause | Đang ở `PAUSED` bấm `STOP` 2 lần $\rightarrow$ Hủy chu trình ngay lập tức về `STANDBY`. | **PASS** |
| **TC-12** | Giặt đủ 30 phút hoàn thành | Cho máy chạy đủ 1800 giây $\rightarrow$ Hoàn tất chu trình, ngắt tải, về `STANDBY`. | **PASS** |
| **TC-13** | Xử lý sự cố lỗi (Fault) | Đang chạy phát hiện mở cửa/kẹt motor $\rightarrow$ Sang `ERROR`, `RLED` chớp 2Hz, ngắt tải. | **PASS** |
| **TC-14** | Stress test bấm Pause/Run liên tục | Chuyển đổi liên tục 10 lần giữa Pause và Run $\rightarrow$ Đồng hồ vẫn chuẩn xác từng giây. | **PASS** |
| **TC-15** | Chặn nạp tiền khi đang giặt | Đút xu khi máy đang `RUNNING`, `PAUSED` hay `ERROR` $\rightarrow$ Đều bị từ chối. | **PASS** |
| **TC-16** | Báo lỗi từ Standby hoặc Ready | Đang ở `STANDBY` hoặc `READY` mà bị lỗi $\rightarrow$ Sang `ERROR`, `RLED` chớp 2Hz. | **PASS** |
| **TC-17** | Khóa nút khi đang có lỗi | Khi ở `ERROR`, mọi nút `RUN`, `PAUSE`, `STOP` đều bị vô hiệu hóa hoàn toàn. | **PASS** |
| **TC-18** | Hủy tiền trước khi chạy | Nạp 60¢ ở `READY` nhưng đổi ý bấm STOP 2 lần $\rightarrow$ Hủy phiên, về `STANDBY`. | **PASS** |
| **TC-19** | Bấm STOP đơn lẻ ngắt quãng | Bấm STOP 4 lần cách nhau >1.5s $\rightarrow$ Cửa sổ trượt tự xóa, máy không bao giờ bị dừng nhầm. | **PASS** |
| **TC-20** | Kháng lỗi con trỏ NULL | Truyền con trỏ `NULL` vào mọi hàm API $\rightarrow$ An toàn 100%, không bị crash bộ nhớ. | **PASS** |
| **TC-21** | **Chu trình giặt liên tiếp (Multi-session)** | Chạy xong mẻ 1 $\rightarrow$ Nạp tiền mẻ 2 dừng khẩn cấp $\rightarrow$ Nạp tiền mẻ 3 bắt lỗi: Không bị tồn dư trạng thái rác. | **PASS** |
| **TC-22** | **Biên thời gian Double-Stop (1499ms vs 1501ms)** | Bấm lần 2 ở 1499ms $\rightarrow$ Dừng cưỡng bức; Bấm lần 2 ở 1501ms $\rightarrow$ Cửa sổ đã hết hạn, không bị dừng. | **PASS** |
| **TC-23** | **Bấm STOP 1 lần ở READY giữ nguyên tiền** | Ở `READY` (60¢) bấm STOP 1 lần và đợi quá 1.5s $\rightarrow$ Không bị hủy, tiền vẫn nguyên 60¢, bấm RUN giặt bình thường. | **PASS** |
| **TC-24** | **Chẩn đoán lỗi đa cảm biến (Bitmask)** | Ghi nhận từng loại lỗi (Cửa mở, Kẹt van nước, Quá tải motor) dạng chuỗi chẩn đoán rõ ràng. | **PASS** |
| **TC-25** | **Kháng tràn số nguyên (MISRA-C Rule 12.4)** | Đút xu khi số dư ở biên cực đại `UINT32_MAX` $\rightarrow$ Tuyệt đối không bị cuốn số về 0 (wrap-around defense). | **PASS** |
| **TC-26** | **Chuyển pha chấp hành (Agitate -> Spin & Drain)** | 5/6 thời gian đầu động cơ đảo chiều giặt (Agitate); 1/6 thời gian cuối kích hoạt bơm xả và vắt tốc độ cao (Spin dry); Hết giờ ngắt toàn bộ tải. | **PASS** |
| **TC-27** | **Lọc sự kiện hợp lệ (Event Acceptance Protocol)** | Hàm `wm_fsm_can_accept_event()` sàng lọc chặt chẽ sự kiện hợp lệ/không hợp lệ trên toàn bộ 5 trạng thái FSM. | **PASS** |
| **TC-28** | **Truy vấn phân pha chu trình (Cycle Sub-Phase Query)** | Nhận diện chính xác pha `IDLE`, `WASH_AGITATE` và `FINAL_SPIN` kèm bộ giải mã chuỗi trực quan. | **PASS** |
| **TC-29** | **Tạm dừng xuyên biên giới phân pha (Pause Phase Transition)** | Tạm dừng máy ở pha Agitate (310s), để timer đếm lùi trong Pause vượt mốc 300s (xuống 290s) $\rightarrow$ Khi bấm RUN tiếp tục, cơ cấu chấp hành tự động chuyển mượt sang pha Vắt cao tốc (Spin) và Bơm xả. | **PASS** |
| **TC-30** | **Kháng lỗi biên MISRA-C & Trạng thái hỏng (MISRA-C Resilience)** | Sự kiện ngoài enum `(wm_event_t)999` trả về false an toàn; Trạng thái `(wm_state_t)999` kích hoạt nhánh default; Vận hành trọn vẹn chu trình với 100% Callback rỗng (`NULL`) không gây crash; Từ chối các mệnh giá xu phi chuẩn (0¢, 5¢, 15¢, 25¢, 30¢, 99¢, 100¢). | **PASS** |

### Bộ Kiểm Thử Tầng Phần Cứng HAL (HAL Engines Suite - 6 Tests)

| Mã Kiểm Thử | Tên Kịch Bản | Hành Vi & Tiêu Chí Kiểm Tra Thực Tế | Kết Quả |
| :--- | :--- | :--- | :---: |
| **HAL-01** | **Chống Rung Phím Bấm (30ms Debounce)** | Xung nhiễu 10ms bị lọc bỏ hoàn toàn; Nhấn giữ liên tục 30ms nhận diện falling-edge; Nhả phím 30ms nhận diện rising-edge. | **PASS** |
| **HAL-02** | **Bộ Giải Mã Xung Tiền Xu (Coin Pulse Validator)** | Giải mã chuẩn công nghiệp: 1 xung = 10¢, 2 xung = 20¢, 5 xung = 50¢; 3 xung bị loại bỏ do không hợp lệ. | **PASS** |
| **HAL-03** | **Máy Phát Dạng Sóng LED (Non-blocking Blinkers)** | Định thời modulo chính xác: 1.0 Hz (500ms ON / 500ms OFF) cho BLED; 2.0 Hz (250ms ON / 250ms OFF) cho RLED. | **PASS** |
| **HAL-04** | **Khóa Liên Động Cơ Cấu Chấp Hành (Actuator Interlock)** | Ngăn chặn động cơ quay khi cửa mở; Bắt buộc bật bơm xả khi vắt cao tốc; Cấm mở van cấp nước khi đang vắt; Cấm mở van cấp nước và bật bơm xả đồng thời. | **PASS** |
| **HAL-05** | **Hàng Đợi FIFO Nạp Xu Liên Tiếp (Coin Burst Queue)** | Hàng đợi vòng tròn (Ring buffer FIFO) 4 phần tử lưu trữ trọn vẹn chuỗi nạp xu liên tiếp (10¢ + 20¢ + 50¢) mà không bị mất sự kiện. | **PASS** |
| **HAL-06** | **Kháng Lỗi Con Trỏ NULL Toàn Diện (HAL API Robustness)** | Truyền `NULL` vào toàn bộ 13 hàm API thuộc tầng HAL $\rightarrow$ Hệ thống an toàn 100%, không bị crash hoặc segmentation fault. | **PASS** |

### Bộ Kiểm Thử STM32 Bare-Metal Hardware-in-the-Loop (STM32 HIL Suite - 6 Tests)

| Mã Kiểm Thử | Tên Kịch Bản | Hành Vi & Tiêu Chí Kiểm Tra Cấp Thanh Ghi Phần Cứng | Kết Quả |
| :--- | :--- | :--- | :---: |
| **STM32-HIL-01** | **Khởi Tạo Trạng Thái STANDBY** | Kiểm tra thanh ghi `GPIOB->ODR`: Chân PB0 (`RLED`) tích điện (ON), các rơ-le chấp hành PB12..PB15 tắt hoàn toàn. | **PASS** |
| **STM32-HIL-02** | **Nạp Tiền 50¢ Vào Chân PA5** | Xung active-low 35ms kéo tụt PA5 xuống GND $\rightarrow$ Bộ debounce 30ms nhận diện $\rightarrow$ FSM chuyển sang `READY` $\rightarrow$ `GPIOB->ODR` bật PB1 (`BLED`). | **PASS** |
| **STM32-HIL-03** | **Bấm Nút RUN Trên Chân PA0** | Xung active-low 35ms trên PA0 $\rightarrow$ FSM chuyển sang `RUNNING` $\rightarrow$ `GPIOB->ODR` bật đồng thời PB15 (Khóa cửa) và PB12 (Đảo chiều giặt). | **PASS** |
| **STM32-HIL-04** | **Định Thời Chu Trình Qua SysTick 1ms** | SysTick Handler tích lũy 1000 ngắt timer non-blocking $\rightarrow$ Bộ đếm ngược chu trình giảm chính xác 1 giây (1800s $\rightarrow$ 1799s). | **PASS** |
| **STM32-HIL-05** | **Bảo Vệ Khẩn Cấp Khi Mở Cửa (PA6)** | Chân PA6 chạm GND $\rightarrow$ Ngắt toàn bộ tải động cơ và khóa cửa ngay lập tức, chuyển sang `ERROR` với RLED chớp 2Hz. | **PASS** |
| **STM32-HIL-06** | **Tự Động Phục Hồi Khi Đóng Cửa (PA6)** | Chân PA6 trở về mức HIGH $\rightarrow$ FSM tự động giải phóng lỗi an toàn và trở về trạng thái `STANDBY`. | **PASS** |

---

## 6. HƯỚNG DẪN THỰC THI & SỬ DỤNG

### 1. Biên dịch và chạy bộ kiểm thử tự động (30 Tests):
```bash
mingw32-make test
```

### 2. Biên dịch và chạy bản thực thi Super-Loop Bare-Metal:
```bash
mingw32-make demo
```

### 3. Biên dịch và bật Simulator tương tác trực quan (CLI):
```bash
mingw32-make sim
.\sim_wm.exe
```

### 4. Biên dịch và kiểm thử Tầng Phần Cứng HAL (Debounce 30ms, Xung tiền xu, LED 1Hz/2Hz, Khóa an toàn):
```bash
mingw32-make test_hal
```

### 5. Biên dịch và kiểm thử Driver STM32 Bare-Metal (SysTick 1ms Super-Loop):
```bash
mingw32-make stm32
```

---

## 7. MÔ PHỎNG PHẦN CỨNG NHÚNG TRỰC QUAN TRÊN TRÌNH DUYỆT (WOKWI)

Dự án cung cấp gói mô phỏng nhúng trực quan 100% tại thư mục `sim/wokwi/` gồm:
* `diagram.json`: Sơ đồ nguyên lý mạch hoàn chỉnh kết nối vi điều khiển với màn hình LCD 1602 I2C, 3 nút bấm (RUN, PAUSE, STOP), 3 nút nạp xu (10¢, 20¢, 50¢), 2 đèn LED báo trạng thái và 4 đèn Relay cơ cấu chấp hành (Động cơ giặt, Động cơ vắt, Bơm xả, Khóa cửa).
* `sketch.ino`: Firmware nhúng điều khiển vòng lặp thời gian thực non-blocking, chống rung phím 30ms, chớp LED chuẩn 1Hz/2Hz và cập nhật LCD 1602 trực tiếp.
* Link chạy nhanh: Mở [Wokwi Arduino Uno](https://wokwi.com/projects/new/arduino-uno), nạp file `diagram.json` và `sketch.ino` để xem mạch chạy trực tiếp trên web!



