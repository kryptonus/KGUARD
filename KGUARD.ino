#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// ---- hardware config ----
// AI-Thinker ESP32-CAM pinout
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define BUZZER_PIN 13

// ---- detection tuning ----
// how much a single pixel must change to count as "moved"
#define MOTION_THRESHOLD 50
// how many such pixels trigger the alarm
#define CHANGED_PIXELS_LIMIT 1800
// how many pixels we sample per frame (QVGA JPEG ~10KB)
#define SAMPLE_SIZE 10000

// ---- globals ----
uint8_t* prevFrame   = nullptr;
bool alarmActive     = false;
bool buzzerState     = false;
unsigned long lastBuzzerToggle = 0;


// beep pattern: 500ms on, 500ms off while alarm is active
void handleBuzzer() {
  if (!alarmActive) {
    ledcWrite(BUZZER_PIN, 0);
    buzzerState = false;
    return;
  }
  unsigned long now = millis();
  if (now - lastBuzzerToggle > 500) {
    buzzerState = !buzzerState;
    ledcWrite(BUZZER_PIN, buzzerState ? 200 : 0);
    lastBuzzerToggle = now;
  }
}


// compare current frame against previous one
// returns true if enough pixels changed significantly
bool detectBoiling() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;

  bool boiling = false;

  if (prevFrame != nullptr) {
    int changed = 0;
    int limit = min((int)fb->len, SAMPLE_SIZE);

    for (int i = 0; i < limit; i++) {
      if (abs((int)fb->buf[i] - (int)prevFrame[i]) > MOTION_THRESHOLD)
        changed++;
    }

    Serial.printf("changed pixels: %d\n", changed);
    boiling = (changed > CHANGED_PIXELS_LIMIT);
  }

  // first run: allocate buffer
  if (prevFrame == nullptr)
    prevFrame = (uint8_t*)malloc(fb->len);

  memcpy(prevFrame, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return boiling;
}


void beep(int times) {
  for (int i = 0; i < times; i++) {
    ledcWrite(BUZZER_PIN, 200);
    delay(120);
    ledcWrite(BUZZER_PIN, 0);
    delay(120);
  }
}


void initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = Y2_GPIO_NUM;
  cfg.pin_d1       = Y3_GPIO_NUM;
  cfg.pin_d2       = Y4_GPIO_NUM;
  cfg.pin_d3       = Y5_GPIO_NUM;
  cfg.pin_d4       = Y6_GPIO_NUM;
  cfg.pin_d5       = Y7_GPIO_NUM;
  cfg.pin_d6       = Y8_GPIO_NUM;
  cfg.pin_d7       = Y9_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_pclk     = PCLK_GPIO_NUM;
  cfg.pin_vsync    = VSYNC_GPIO_NUM;
  cfg.pin_href     = HREF_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.frame_size   = FRAMESIZE_QVGA;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  cfg.fb_location  = CAMERA_FB_IN_DRAM;
  cfg.jpeg_quality = 15;
  cfg.fb_count     = 1;

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("camera init failed: 0x%x\n", err);
    while (true) delay(1000); // halt
  }
  Serial.println("camera ok");
}


void captureReference() {
  // give the sensor time to auto-expose properly
  Serial.println("waiting for exposure to settle...");
  delay(3000);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("failed to grab reference frame");
    return;
  }

  prevFrame = (uint8_t*)malloc(fb->len);
  memcpy(prevFrame, fb->buf, fb->len);
  esp_camera_fb_return(fb);
  Serial.println("reference frame saved. watching now.");
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
  Serial.begin(115200);
  Serial.println("\n-- MilkGuard --");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  initCamera();

  // buzzer uses LEDC channel 1, camera already claimed channel 0
  ledcAttachChannel(BUZZER_PIN, 2000, 8, 1);
  beep(1); // single beep = camera alive

  captureReference();
  beep(2); // double beep = ready to watch
}


void loop() {
  handleBuzzer();

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 2000) {
    lastCheck = millis();

    bool boiling = detectBoiling();

    if (boiling && !alarmActive) {
      Serial.println(">> BOILING DETECTED");
      alarmActive = true;
    } else if (!boiling && alarmActive) {
      Serial.println(">> settled down");
      alarmActive = false;
    }
  }

  delay(10);
}