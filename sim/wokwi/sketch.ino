/**
 * @file sketch.ino
 * @brief Wokwi Interactive Embedded Firmware for Washing Machine Control Unit
 * @details HCMUT CO3053 Embedded Systems - Assignment 2 (BTL 2)
 *          Target: Arduino Uno / Nano / ESP32 with I2C LCD1602, 3 Buttons, 2 LEDs, 4 Actuators
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* --- Pin Assignments --- */
#define PIN_BTN_RUN       2
#define PIN_BTN_PAUSE     3
#define PIN_BTN_STOP      4
#define PIN_COIN_10       5
#define PIN_COIN_20       6
#define PIN_COIN_50       7
#define PIN_SW_DOOR       8

#define PIN_RLED          9
#define PIN_BLED          10
#define PIN_MTR_AGITATE   11
#define PIN_MTR_SPIN      12
#define PIN_DRAIN         13
#define PIN_DOOR_LOCK     A0

/* LCD Display */
LiquidCrystal_I2C lcd(0x27, 16, 2);

/* --- Core FSM Definitions --- */
typedef enum {
    WM_STATE_STANDBY = 0,
    WM_STATE_READY,
    WM_STATE_RUNNING,
    WM_STATE_PAUSED,
    WM_STATE_ERROR
} wm_state_t;

typedef enum {
    HAL_LED_OFF = 0,
    HAL_LED_ON,
    HAL_LED_BLINK_1HZ,
    HAL_LED_BLINK_2HZ
} hal_led_state_t;

typedef enum {
    HAL_MOTOR_OFF = 0,
    HAL_MOTOR_AGITATE,
    HAL_MOTOR_SPIN
} hal_motor_state_t;

typedef enum {
    WM_EVT_NONE = 0,
    WM_EVT_COIN_10,
    WM_EVT_COIN_20,
    WM_EVT_COIN_50,
    WM_EVT_BTN_RUN,
    WM_EVT_BTN_PAUSE,
    WM_EVT_BTN_STOP,
    WM_EVT_TIMER_TICK_1S,
    WM_EVT_TIMER_TICK_1MS,
    WM_EVT_FAULT_OCCURRED,
    WM_EVT_FAULT_CLEARED
} wm_event_t;

#define WM_CYCLE_DURATION_SEC     1800U
#define WM_COIN_THRESHOLD_CENTS   50U
#define WM_DOUBLE_PRESS_WINDOW_MS 1500U
#define WM_DEBOUNCE_MS            30U

/* --- Debounce Engine --- */
typedef struct {
    uint8_t pin;
    bool debounced_state;
    uint32_t counter;
    bool pressed_event;
} button_t;

static button_t btn_run, btn_pause, btn_stop, btn_c10, btn_c20, btn_c50;

static void btn_init(button_t *b, uint8_t pin) {
    b->pin = pin;
    b->debounced_state = false;
    b->counter = 0;
    b->pressed_event = false;
    pinMode(pin, INPUT_PULLUP);
}

static void btn_update(button_t *b, uint32_t delta_ms) {
    bool raw_pressed = (digitalRead(b->pin) == LOW);
    if (raw_pressed != b->debounced_state) {
        b->counter += delta_ms;
        if (b->counter >= WM_DEBOUNCE_MS) {
            b->debounced_state = raw_pressed;
            b->counter = 0;
            if (b->debounced_state) {
                b->pressed_event = true;
            }
        }
    } else {
        b->counter = 0;
    }
}

static bool btn_was_pressed(button_t *b) {
    if (b->pressed_event) {
        b->pressed_event = false;
        return true;
    }
    return false;
}

/* --- LED Blinker Engine --- */
typedef struct {
    uint8_t pin;
    hal_led_state_t mode;
    bool level;
    uint32_t timer_ms;
    uint32_t half_period_ms;
} blinker_t;

static blinker_t blink_rled, blink_bled;

static void blinker_init(blinker_t *blk, uint8_t pin) {
    blk->pin = pin;
    blk->mode = HAL_LED_OFF;
    blk->level = false;
    blk->timer_ms = 0;
    blk->half_period_ms = 0;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

static void blinker_set(blinker_t *blk, hal_led_state_t mode) {
    if (blk->mode == mode) return;
    blk->mode = mode;
    blk->timer_ms = 0;
    if (mode == HAL_LED_OFF) {
        blk->level = false;
        blk->half_period_ms = 0;
    } else if (mode == HAL_LED_ON) {
        blk->level = true;
        blk->half_period_ms = 0;
    } else if (mode == HAL_LED_BLINK_1HZ) {
        blk->level = true;
        blk->half_period_ms = 500;
    } else if (mode == HAL_LED_BLINK_2HZ) {
        blk->level = true;
        blk->half_period_ms = 250;
    }
    digitalWrite(blk->pin, blk->level ? HIGH : LOW);
}

static void blinker_tick(blinker_t *blk, uint32_t delta_ms) {
    if (blk->mode == HAL_LED_BLINK_1HZ || blk->mode == HAL_LED_BLINK_2HZ) {
        blk->timer_ms += delta_ms;
        if (blk->timer_ms >= blk->half_period_ms) {
            blk->level = !blk->level;
            blk->timer_ms = 0;
            digitalWrite(blk->pin, blk->level ? HIGH : LOW);
        }
    }
}

/* --- FSM Context & State Machine --- */
typedef struct {
    wm_state_t state;
    uint32_t balance_cents;
    uint32_t remaining_sec;
    uint32_t stop_press_count;
    uint32_t stop_window_ms;
    hal_motor_state_t motor;
    bool drain_pump;
    bool door_lock;
} wm_sim_t;

static wm_sim_t g_wm;

static void enter_standby(void) {
    g_wm.state = WM_STATE_STANDBY;
    g_wm.balance_cents = 0;
    g_wm.remaining_sec = 0;
    g_wm.stop_press_count = 0;
    g_wm.stop_window_ms = 0;
    g_wm.motor = HAL_MOTOR_OFF;
    g_wm.drain_pump = false;
    g_wm.door_lock = false;

    blinker_set(&blink_rled, HAL_LED_ON);  /* RLED Solid ON */
    blinker_set(&blink_bled, HAL_LED_OFF); /* BLED OFF */
    digitalWrite(PIN_MTR_AGITATE, LOW);
    digitalWrite(PIN_MTR_SPIN, LOW);
    digitalWrite(PIN_DRAIN, LOW);
    digitalWrite(PIN_DOOR_LOCK, LOW);
}

static void enter_ready(void) {
    g_wm.state = WM_STATE_READY;
    blinker_set(&blink_rled, HAL_LED_OFF);
    blinker_set(&blink_bled, HAL_LED_ON); /* BLED Solid ON */
}

static void update_running_actuators(void) {
    g_wm.door_lock = true;
    digitalWrite(PIN_DOOR_LOCK, HIGH);

    uint32_t spin_threshold = WM_CYCLE_DURATION_SEC / 6U; /* Final 5 minutes */
    if (g_wm.remaining_sec <= spin_threshold && g_wm.remaining_sec > 0) {
        /* High-speed spin and drain pump active */
        g_wm.motor = HAL_MOTOR_SPIN;
        g_wm.drain_pump = true;
        digitalWrite(PIN_MTR_AGITATE, LOW);
        digitalWrite(PIN_MTR_SPIN, HIGH);
        digitalWrite(PIN_DRAIN, HIGH);
    } else {
        /* Wash agitation mode */
        g_wm.motor = HAL_MOTOR_AGITATE;
        g_wm.drain_pump = false;
        digitalWrite(PIN_MTR_AGITATE, HIGH);
        digitalWrite(PIN_MTR_SPIN, LOW);
        digitalWrite(PIN_DRAIN, LOW);
    }
}

static void enter_running(bool resuming) {
    g_wm.state = WM_STATE_RUNNING;
    g_wm.stop_press_count = 0;
    g_wm.stop_window_ms = 0;

    if (!resuming) {
        /* Unconditional Zero-Refund: Money reset to 0 */
        g_wm.balance_cents = 0;
        g_wm.remaining_sec = WM_CYCLE_DURATION_SEC;
    }

    blinker_set(&blink_rled, HAL_LED_OFF);
    blinker_set(&blink_bled, HAL_LED_BLINK_1HZ); /* BLED Blinking 1Hz */
    update_running_actuators();
}

static void enter_paused(void) {
    g_wm.state = WM_STATE_PAUSED;
    g_wm.stop_press_count = 0;
    g_wm.stop_window_ms = 0;

    /* Actuators suspended, but remaining_sec continues counting down! */
    g_wm.motor = HAL_MOTOR_OFF;
    g_wm.drain_pump = false;
    digitalWrite(PIN_MTR_AGITATE, LOW);
    digitalWrite(PIN_MTR_SPIN, LOW);
    digitalWrite(PIN_DRAIN, LOW);

    blinker_set(&blink_bled, HAL_LED_ON); /* BLED Solid ON during Pause */
}

static void enter_error(void) {
    g_wm.state = WM_STATE_ERROR;
    g_wm.stop_press_count = 0;
    g_wm.stop_window_ms = 0;
    g_wm.motor = HAL_MOTOR_OFF;
    g_wm.drain_pump = false;
    g_wm.door_lock = false;

    digitalWrite(PIN_MTR_AGITATE, LOW);
    digitalWrite(PIN_MTR_SPIN, LOW);
    digitalWrite(PIN_DRAIN, LOW);
    digitalWrite(PIN_DOOR_LOCK, LOW);

    blinker_set(&blink_bled, HAL_LED_OFF);
    blinker_set(&blink_rled, HAL_LED_BLINK_2HZ); /* RLED Blinking 2Hz */
}

static void handle_coin(uint32_t amount) {
    if (g_wm.state == WM_STATE_STANDBY) {
        g_wm.balance_cents += amount;
        if (g_wm.balance_cents >= WM_COIN_THRESHOLD_CENTS) {
            enter_ready();
        }
    } else if (g_wm.state == WM_STATE_READY) {
        /* Surplus accepted without refund */
        g_wm.balance_cents += amount;
    }
}

static void handle_stop_button(void) {
    if (g_wm.state == WM_STATE_STANDBY) return;

    if (g_wm.stop_press_count == 0) {
        /* 1st press: start 1.5s window */
        g_wm.stop_press_count = 1;
        g_wm.stop_window_ms = WM_DOUBLE_PRESS_WINDOW_MS;
    } else {
        /* 2nd press within 1.5s: Force Stop! */
        g_wm.stop_press_count = 0;
        g_wm.stop_window_ms = 0;
        enter_standby();
    }
}

static void dispatch_event(wm_event_t evt) {
    if (evt == WM_EVT_FAULT_OCCURRED) {
        if (g_wm.state != WM_STATE_ERROR) {
            enter_error();
        }
        return;
    }

    switch (g_wm.state) {
        case WM_STATE_STANDBY:
            if (evt == WM_EVT_COIN_10) handle_coin(10);
            else if (evt == WM_EVT_COIN_20) handle_coin(20);
            else if (evt == WM_EVT_COIN_50) handle_coin(50);
            break;

        case WM_STATE_READY:
            if (evt == WM_EVT_COIN_10) handle_coin(10);
            else if (evt == WM_EVT_COIN_20) handle_coin(20);
            else if (evt == WM_EVT_COIN_50) handle_coin(50);
            else if (evt == WM_EVT_BTN_RUN) enter_running(false);
            else if (evt == WM_EVT_BTN_STOP) handle_stop_button();
            break;

        case WM_STATE_RUNNING:
            if (evt == WM_EVT_BTN_PAUSE) enter_paused();
            else if (evt == WM_EVT_BTN_STOP) handle_stop_button();
            else if (evt == WM_EVT_TIMER_TICK_1S) {
                if (g_wm.remaining_sec > 1) {
                    g_wm.remaining_sec--;
                    update_running_actuators();
                } else {
                    enter_standby();
                }
            }
            break;

        case WM_STATE_PAUSED:
            if (evt == WM_EVT_BTN_RUN) enter_running(true);
            else if (evt == WM_EVT_BTN_STOP) handle_stop_button();
            else if (evt == WM_EVT_TIMER_TICK_1S) {
                /* Timer still ticks down in PAUSED! */
                if (g_wm.remaining_sec > 1) {
                    g_wm.remaining_sec--;
                } else {
                    enter_standby();
                }
            }
            break;

        case WM_STATE_ERROR:
            if (evt == WM_EVT_FAULT_CLEARED) {
                enter_standby();
            }
            break;
    }
}

/* --- LCD Refresh Engine --- */
static void update_lcd(void) {
    lcd.setCursor(0, 0);
    switch (g_wm.state) {
        case WM_STATE_STANDBY:
            lcd.print("ST:STANDBY   $0.");
            if (g_wm.balance_cents < 10) lcd.print("0");
            lcd.print(g_wm.balance_cents);
            break;

        case WM_STATE_READY:
            lcd.print("ST:READY     $");
            lcd.print(g_wm.balance_cents / 100);
            lcd.print(".");
            if ((g_wm.balance_cents % 100) < 10) lcd.print("0");
            lcd.print(g_wm.balance_cents % 100);
            break;

        case WM_STATE_RUNNING:
        case WM_STATE_PAUSED: {
            uint32_t m = g_wm.remaining_sec / 60;
            uint32_t s = g_wm.remaining_sec % 60;
            lcd.print(g_wm.state == WM_STATE_RUNNING ? "RUN  " : "PAUS ");
            if (m < 10) lcd.print("0");
            lcd.print(m);
            lcd.print(":");
            if (s < 10) lcd.print("0");
            lcd.print(s);
            lcd.print(" [");
            lcd.print(g_wm.motor == HAL_MOTOR_SPIN ? "SPN]" : "WSH]");
            break;
        }

        case WM_STATE_ERROR:
            lcd.print("FAULT: DOOR OPEN");
            break;
    }

    lcd.setCursor(0, 1);
    if (g_wm.state == WM_STATE_ERROR) {
        lcd.print("CLOSE LID & REST");
    } else {
        lcd.print("M:");
        lcd.print(g_wm.motor == HAL_MOTOR_AGITATE ? "AGIT " :
                  g_wm.motor == HAL_MOTOR_SPIN    ? "SPIN " : "OFF  ");
        lcd.print("P:");
        lcd.print(g_wm.drain_pump ? "1 " : "0 ");
        lcd.print("LCK:");
        lcd.print(g_wm.door_lock ? "1" : "0");
    }
}

/* --- Setup & Super-Loop --- */
void setup() {
    Wire.begin();
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WASHING MACHINE");
    lcd.setCursor(0, 1);
    lcd.print("CO3053 - HCMUT");
    delay(1200);
    lcd.clear();

    btn_init(&btn_run, PIN_BTN_RUN);
    btn_init(&btn_pause, PIN_BTN_PAUSE);
    btn_init(&btn_stop, PIN_BTN_STOP);
    btn_init(&btn_c10, PIN_COIN_10);
    btn_init(&btn_c20, PIN_COIN_20);
    btn_init(&btn_c50, PIN_COIN_50);

    pinMode(PIN_SW_DOOR, INPUT_PULLUP);

    blinker_init(&blink_rled, PIN_RLED);
    blinker_init(&blink_bled, PIN_BLED);

    pinMode(PIN_MTR_AGITATE, OUTPUT);
    pinMode(PIN_MTR_SPIN, OUTPUT);
    pinMode(PIN_DRAIN, OUTPUT);
    pinMode(PIN_DOOR_LOCK, OUTPUT);

    enter_standby();
}

static uint32_t last_ms = 0;
static uint32_t ms_accumulator = 0;
static uint32_t lcd_refresh_timer = 0;

void loop() {
    uint32_t current_ms = millis();
    uint32_t delta = current_ms - last_ms;
    if (delta == 0) return;
    last_ms = current_ms;

    /* Update input button debouncers */
    btn_update(&btn_run, delta);
    btn_update(&btn_pause, delta);
    btn_update(&btn_stop, delta);
    btn_update(&btn_c10, delta);
    btn_update(&btn_c20, delta);
    btn_update(&btn_c50, delta);

    /* Update LED blinkers */
    blinker_tick(&blink_rled, delta);
    blinker_tick(&blink_bled, delta);

    /* Double-press window decrement */
    if (g_wm.stop_window_ms > 0) {
        if (g_wm.stop_window_ms > delta) {
            g_wm.stop_window_ms -= delta;
        } else {
            g_wm.stop_window_ms = 0;
            g_wm.stop_press_count = 0;
        }
    }

    /* Event Dispatching */
    if (btn_was_pressed(&btn_run))   dispatch_event(WM_EVT_BTN_RUN);
    if (btn_was_pressed(&btn_pause)) dispatch_event(WM_EVT_BTN_PAUSE);
    if (btn_was_pressed(&btn_stop))  dispatch_event(WM_EVT_BTN_STOP);
    if (btn_was_pressed(&btn_c10))   dispatch_event(WM_EVT_COIN_10);
    if (btn_was_pressed(&btn_c20))   dispatch_event(WM_EVT_COIN_20);
    if (btn_was_pressed(&btn_c50))   dispatch_event(WM_EVT_COIN_50);

    /* Check door switch fault: LOW means switch closed to GND (Fault triggered) */
    if (digitalRead(PIN_SW_DOOR) == LOW) {
        dispatch_event(WM_EVT_FAULT_OCCURRED);
    } else if (g_wm.state == WM_STATE_ERROR) {
        dispatch_event(WM_EVT_FAULT_CLEARED);
    }

    /* 1-second system tick */
    ms_accumulator += delta;
    if (ms_accumulator >= 1000) {
        ms_accumulator -= 1000;
        dispatch_event(WM_EVT_TIMER_TICK_1S);
    }

    /* Periodic LCD update (every 200ms) */
    lcd_refresh_timer += delta;
    if (lcd_refresh_timer >= 200) {
        lcd_refresh_timer = 0;
        update_lcd();
    }
}
