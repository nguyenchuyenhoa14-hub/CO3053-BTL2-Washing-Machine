# Report Image Assets

| File | Target Section | Source Reference |
|---|---|---|
| `led_standby.jpg` | Section 11.4 (a) | Cropped from `generated/standby_modes.jpg` |
| `led_collecting.jpg` | Section 11.4 (b) | Cropped from `generated/stanby_themtien.jpg` |
| `led_ready.jpg` | Section 11.4 (c) | Cropped from `generated/ready.jpg` |
| `led_running.jpg` | Section 11.4 (d) | Cropped from `generated/running.jpg` |

Image cropping command (ImageMagick):
```bash
convert generated/ready.jpg -crop 480x720+391+430 +repage -quality 90 led_ready.jpg
```

`generated/lcd_*.png` are simulated LCD UI frames rendered by `make preview` from firmware UI display logic (Section 11.5).
