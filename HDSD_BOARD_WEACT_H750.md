# Hướng dẫn sử dụng – Máy giặt trên board WeAct STM32H750

Tài liệu này hướng dẫn build, nạp firmware, đấu nối nút ngoài và vận hành bộ điều khiển máy giặt
(CO3053 – BTL2) trên board **WeAct Studio STM32H750VBT6** có màn hình LCD 0.96" ST7735 (160x80).

> Ghi chú trung thực: firmware đã biên dịch và các test trên PC đều pass, nhưng phần điều khiển
> phần cứng (clock, SPI4/LCD, nút) viết theo schematic + SDK của WeAct. Nếu có gì lệch khi chạy thật,
> xem mục 7 (Xử lý sự cố).

## 1. Chuẩn bị

| Cần có | Ghi chú |
|---|---|
| Board WeAct STM32H750VBT6 + LCD 0.96" cắm vào đầu nối LCD | Lắp đúng chiều, cắm khi board đang tắt |
| Cáp USB-C | Cấp nguồn và nạp qua DFU |
| (Tùy chọn) ST-Link V2 | Nạp qua SWD: SWDIO, SWCLK, GND, 3V3 |
| (Tùy chọn) 2 nút nhấn ngoài + dây | Nút RUN/PAUSE và STOP, xem mục 4 |
| Linux + `make`, `gcc` | Đã thử trên Ubuntu |

Toolchain ARM (`arm-none-eabi-gcc`), `dfu-util` và `openocd`: `make` tự tìm trong `PATH`, nếu không có
thì dùng bản của PlatformIO (`~/.platformio/packages`). Cài thủ công nếu thiếu:

```bash
sudo apt install gcc-arm-none-eabi dfu-util openocd
# hoặc, nếu dùng PlatformIO:
pio pkg install -g --tool platformio/tool-dfuutil
```

Chỉ lần đầu, để nạp không cần `sudo`:

```bash
sudo cp board/weact_h750/99-weact-h750.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

## 2. Build

```bash
make h750          # tạo build/h750/washer.elf, washer.bin, washer.hex
```

Kết quả in ra dung lượng (khoảng 5.5 KB flash, 128 KB sẵn có). Xem thử giao diện LCD ngay trên PC,
không cần board:

```bash
make preview       # tạo build/preview/frame_*.ppm (mở bằng trình xem ảnh bất kỳ)
make test_gesture  # test logic nút bấm (29 test)
make test test_hal # test FSM và HAL sẵn có
```

## 3. Nạp firmware

### Cách A – USB-C (DFU), không cần mạch nạp
1. Giữ nút **BOOT0**.
2. Bấm rồi thả nút **NRST** (hoặc cắm USB-C khi đang giữ BOOT0).
3. Thả **BOOT0**. Kiểm tra: `lsusb | grep 0483:df11` phải thấy thiết bị.
4. Chạy `make flash-dfu` (hoặc `make flash`). Thấy `Flash OK.` là xong, board tự chạy chương trình.

### Cách B – ST-Link (SWD)
1. Nối ST-Link: SWDIO, SWCLK, GND, 3V3 vào header SW Debug của board.
2. Chạy `make flash-swd` (hoặc `make flash`, tự nhận ST-Link).

Nếu nạp xong mà không chạy, bấm **NRST** một lần. Nếu `make flash` báo "No board found" thì board chưa
vào chế độ DFU hoặc chưa cắm ST-Link.

## 4. Các nút trên board và nút ngoài (tùy chọn)

Board có 3 nút: K1 (PC13), NRST, BOOT0 (B0). Chỉ **K1** đọc được bằng phần mềm:
- **NRST** reset chip.
- **BOOT0 (B0)** là chân chuyên dụng (datasheet STM32H750: *"Dedicated BOOT0 pin"*, không có chức năng GPIO), chỉ được lấy mẫu lúc reset. Nên **không thể dùng B0 làm nút nhập liệu**.

Mọi thao tác nạp tiền và điều khiển đều làm được chỉ bằng **K1**. Nếu muốn có thêm nút, có thể gắn nút ngoài (không bắt buộc):

| Nút ngoài | Chân | Đấu | Chức năng |
|---|---|---|---|
| K0 (phím tắt tiền) | PA0 | một đầu nút vào PA0, đầu kia vào GND | STANDBY: nhấn +10¢, nhấn đúp hoặc giữ +20¢; trạng thái khác: RUN/PAUSE |
| STOP | PA1 | một đầu nút vào PA1, đầu kia vào GND | nhấn 2 lần trong 1.5 s → về STANDBY |

Không cần điện trở ngoài (đã bật pull-up nội). Không dùng PA13/PA14 (đó là SWD).

## 5. Vận hành

### 5.1 Nạp tiền và điều khiển bằng K1

Ở STANDBY, tiền nạp vào chỉ được **giữ tạm** (hiện ở góc phải trên của LCD). Máy chỉ chuyển sang READY khi bạn
**xác nhận** bằng cách giữ K1 tới 1.2 s.

| K1 | STANDBY | READY | RUNNING | PAUSED | ERROR |
|---|---|---|---|---|---|
| Nhấn 1 lần | +10¢ | RUN | PAUSE | RUN (tiếp tục) | xóa lỗi |
| Nhấn đúp | +20¢ | giả lập mở cửa (lỗi) | giả lập mở cửa | giả lập mở cửa | xóa lỗi |
| Giữ 0.6 s rồi **thả** | +50¢ | STOP lần 1 | STOP lần 1 | STOP lần 1 | – |
| **Giữ tới 1.2 s** | **xác nhận** | STANDBY | STANDBY | STANDBY | STANDBY |

Khi xác nhận (giữ K1 tới 1.2 s ở STANDBY):
- **Đủ tiền** (tổng ≥ 50¢): sang **READY**. Nếu dư thì LCD/UART báo `READY! CHANGE xxxC` (số tiền "thối lại").
- **Không đủ tiền** (< 50¢, kể cả chưa bỏ gì): LCD/UART báo `NOT ENOUGH! REFUND xxxC`, tiền tạm bị hoàn lại, **ở lại STANDBY**.

> Lưu ý đặc tả: đề BTL2 (BR-02) quy định không trả lại tiền dư và READY ngay khi đủ 50¢. Việc "thối tiền"
> và bước xác nhận là cách làm riêng của bảng điều khiển này (chỉ là thông báo; FSM và các test của nó giữ nguyên).
> Khi bảo vệ nên nêu rõ đây là lớp giao diện bổ sung.

- Click được xác nhận sau 0.3 s (để phân biệt với nhấn đúp), nên có độ trễ nhỏ là bình thường.
- Khi giữ K1, thanh dưới cùng của LCD chạy: vạch trắng = 0.6 s (thả ở đây: +50¢ ở STANDBY, STOP lần 1 ở các trạng thái khác), đầy thanh đỏ = 1.2 s (xác nhận / STOP lần 2).
- Tiền tạm tối đa 990¢. Nếu máy chuyển sang ERROR trong lúc có tiền tạm thì số tiền tạm đó bị xóa (chưa vào FSM).
- Ở READY, giữ K1 tới 1.2 s (STOP đôi) sẽ **hủy và hoàn tiền**: LCD/UART báo `CANCELLED! RETURN xxxC`.
- Sau khi xóa lỗi: tiền đã xác nhận được giữ nguyên (READY vẫn READY); nếu lỗi xảy ra giữa chu trình giặt thì máy về **PAUSED** (đồng hồ vẫn chạy trong lúc lỗi), nhấn RUN để tiếp tục.

### 5.2 Đọc màn hình LCD

```
WASHER                     050C      <- số tiền đã nạp (cent)
RUNNING                              <- trạng thái (màu: đỏ STANDBY, xanh dương READY,
23:20                      WASH         xanh lá RUNNING, vàng PAUSED, cam ERROR)
[#####-----------------------]       <- tiến độ chu trình
[RLED][BLED][AGIT][PUMP][LOCK]       <- đèn/rơ-le đang bật thì sáng màu
CLK:PAUSE HOLD:STOP                  <- gợi ý thao tác cho trạng thái hiện tại
```

- **RLED / BLED**: đèn đỏ / đèn xanh của FSM (BLED nhấp nháy 1 Hz khi RUNNING, RLED nhấp nháy 2 Hz khi ERROR).
- **AGIT / SPIN**: motor đang giặt hoặc vắt; **PUMP**: bơm xả; **LOCK**: khóa cửa.
- Cuối chu trình (1/6 thời gian cuối) máy chuyển sang **SPIN** và bật bơm xả.
- Đèn LED xanh PE3 trên board sáng/nhấp nháy theo `RLED hoặc BLED`.
- Trên board, chu trình mặc định là **30 giây** để xem trực tiếp (1/6 cuối, tức 5 giây, là SPIN). Muốn chu trình thật 30 phút (1800 s): `make clean && make flash FULL_CYCLE=1`. Bản chạy trên PC và các test vẫn dùng 30 phút.

### 5.3 Kịch bản demo gợi ý (khoảng 1–2 phút khi rút ngắn chu trình)

1. Bật nguồn → STANDBY, đèn đỏ sáng, 000C.
2. K1 nhấn 1 lần → 010C, K1 nhấn đúp → 030C (vẫn STANDBY).
3. Giữ K1 tới 1.2 s → `NOT ENOUGH! REFUND 030C`, về 000C. Làm lại: giữ K1 0.6 s rồi thả (+50¢) rồi nhấn 1 lần (+10¢) = 060C.
4. Giữ K1 tới 1.2 s → `READY! CHANGE 010C` → **READY**, đèn xanh sáng.
5. K1 click → **RUNNING**, đồng hồ đếm ngược, khóa cửa + motor giặt, BLED nhấp nháy.
6. Click → **PAUSED** (motor dừng, đồng hồ vẫn chạy); click lại để tiếp tục.
7. K1 nhấn đúp → giả lập mở cửa → **ERROR**, tắt toàn bộ, RLED nhấp nháy nhanh; click để xóa lỗi.
8. Chạy lại, giữ K1 1.2 s → STOP cưỡng bức về STANDBY.

### 5.4 Xem và điều khiển qua UART (không cần LCD)

Firmware xuất trạng thái ra **USART1** và nhận phím điều khiển từ máy tính. Dùng khi LCD chưa sẵn sàng
hoặc muốn demo trên màn hình máy tính.

| Dây | Nối |
|---|---|
| PA9 (TX của board) | RX của cáp USB-TTL |
| PA10 (RX của board) | TX của cáp USB-TTL |
| GND | GND của cáp USB-TTL |

Chỉ nối 3 dây này, **không nối 5V** (USB-TTL phải ở mức 3.3 V). Mở cổng (115200, 8N1) trên máy tính:

```bash
picocom -b 115200 /dev/ttyUSB0        # thoát: Ctrl+A rồi Ctrl+X
# hoặc: python3 -m serial.tools.miniterm /dev/ttyUSB0 115200
```

Mỗi khi trạng thái đổi (và mỗi giây khi đang chạy), board in một dòng, ví dụ:

```
[12s] STATE=RUNNING PHASE=WASH TIME=29:45 COIN=000C RLED=0 BLED=1 MOTOR=AGIT PUMP=0 LOCK=1 FAULT=-
```

Phím điều khiển (gõ trong terminal, không cần Enter):

| Phím | Tác dụng |
|---|---|
| `c` | K1 nhấn 1 lần (+10¢ ở STANDBY; RUN/PAUSE ở trạng thái khác) |
| `d` | K1 nhấn đúp (+20¢ ở STANDBY; giả lập mở cửa ở trạng thái khác) |
| `p` | K1 giữ 0.6 s rồi thả (+50¢ ở STANDBY) |
| `o` | K1 giữ tới 1.2 s: **xác nhận** ở STANDBY |
| `a`, `b` | nút ngoài K0 nhấn / giữ (nếu có) |
| `s` | STOP (nhấn 2 lần trong 1.5 s) |
| `f` | force stop → STANDBY |
| `?` | in lại danh sách phím |

## 6. Cấu trúc mã nguồn liên quan

| Đường dẫn | Nội dung |
|---|---|
| `src/fsm/`, `src/hal/` | FSM và HAL dùng chung với bản chạy trên PC (đã kiểm thử) |
| `src/hal/hal_button_gesture.*` | Nhận dạng click / nhấn đúp / giữ từ một nút |
| `src/app/wm_single_button.*` | Ánh xạ cử chỉ và nút ngoài thành sự kiện FSM |
| `board/weact_h750/` | Mã riêng cho board: startup, linker, clock 400 MHz, GPIO, SPI4 + ST7735, đồ họa, giao diện |
| `board/weact_h750/flash.sh`, `board.mk` | Script nạp và luật build (`make h750 / flash / preview`) |

## 7. Xử lý sự cố

| Hiện tượng | Nguyên nhân thường gặp | Cách xử lý |
|---|---|---|
| `make h750`: không thấy `arm-none-eabi-gcc` | Chưa cài toolchain | Cài như mục 1 hoặc `make h750 ARM_PREFIX=/đường/dẫn/arm-none-eabi-` |
| `make flash`: "No board found" | Chưa vào DFU / chưa cắm ST-Link | Làm lại mục 3A; kiểm tra `lsusb` |
| `dfu-util`: không mở được thiết bị / permission | Chưa cài udev rule | Cài rule ở mục 1, rút cắm lại USB |
| LED PE3 nháy chậm N lần (0,5 s sáng / 0,5 s tắt) rồi nghỉ 3 s, lặp lại | Mã lỗi khởi động: **2** SPI4 không sẵn sàng, **3** SPI4 không kết thúc truyền, **4** SysTick không chạy, **5** CPU bị fault | Ghi lại số lần nháy để chẩn đoán |
| Đèn nền mờ dần rồi tắt | Cấp DC trực tiếp vào PE10 không ổn định trên board này | Đã xử lý: đèn nền dùng PWM TIM1 (`LCD_BL_MODE=4`). Thử `make clean && make flash BL_TEST=1` để chạy lại chế độ thử BL1..BL4 |
| LED sáng liên tục, đèn nền LCD không bật | Đang kẹt khi khởi tạo LCD | Xem mã nháy ở dòng trên; kiểm tra cắm LCD |
| Màn hình đen nhưng LED PE3 vẫn chạy | LCD cắm lệch hoặc không nhận | Cắm lại LCD, bấm NRST |
| Hình bị lệch vài pixel hoặc màu bị đảo | Module LCD là loại panel BOE | `make flash LCD_PANEL=BOE` |
| Nút K1 không phản ứng | Chưa nạp đúng firmware | Nạp lại; K1 là mức cao khi bấm, đã bật pull-down nội |
| Chạy chậm / thời gian sai | Thạch anh 25 MHz không khởi động, đã chuyển sang HSI | Firmware tự bù để vẫn 400 MHz; kiểm tra thạch anh nếu cần độ chính xác |
| Muốn nạp lại khi chương trình đang chạy | – | Giữ BOOT0 + bấm NRST (mục 3A) |

Đổi panel: `LCD_PANEL=BOE` chỉ cần truyền khi build (ví dụ `make flash LCD_PANEL=BOE`); đổi giá trị này
thì chạy `make clean` trước để build lại toàn bộ.
