# Ảnh dùng trong report

| File | Dùng ở | Nguồn |
|---|---|---|
| `led_standby.jpg` | §11.4 (a) | cắt từ `generated/standby_modes.jpg` |
| `led_collecting.jpg` | §11.4 (b) | cắt từ `generated/stanby_themtien.jpg` |
| `led_ready.jpg` | §11.4 (c) | cắt từ `generated/ready.jpg` |
| `led_running.jpg` | §11.4 (d) | cắt từ `generated/running.jpg` |

Cắt lại (ImageMagick), ví dụ:
`convert generated/ready.jpg -crop 480x720+391+430 +repage -quality 90 led_ready.jpg`

`generated/lcd_*.png` là frame LCD do `make preview` render từ code UI của firmware (§11.5).

Ảnh board, đấu dây, schematic, pin-out và screenshot terminal đã được bỏ khỏi report
(log test đầy đủ nằm ở Phụ lục, demo trên board có ở video).
