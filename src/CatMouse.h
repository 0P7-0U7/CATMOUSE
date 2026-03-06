#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

enum Role { ROLE_CAT_LISTENING, ROLE_CAT_HUNTING, ROLE_MOUSE, ROLE_NONE };

class CatMouse {
public:
  CatMouse();
  ~CatMouse();

  // Core setup with dynamic timing controls and Game IDs
  bool beginAsCat(uint8_t gameID, uint8_t *targetMac = nullptr,
                  unsigned long attackRateMs = 10);
  bool beginAsMouse(uint8_t gameID, unsigned long hideMinMs = 100,
                    unsigned long hideMaxMs = 500,
                    unsigned long squeakIntervalMs = 2000);

  // Tear down ESP-NOW and reset state
  void end();

  // Auto-Discovery: The Mouse public announcement
  void broadcastSqueak();

  // Put this in your main loop() to drive the timers
  void update();

  // Grab the current channel on demand
  uint8_t getChannel();

  // Scoring and Telemetry
  uint32_t getHitsScored() const { return hitsScored; }
  uint32_t getSweepsMissed() const { return sweepsMissed; }

  // Callback definitions (now passing the channel to your sketch)
  typedef void (*HitCallback)(uint8_t channel);
  typedef void (*MissCallback)(uint8_t channel);

  // Register your callbacks here
  void onHit(HitCallback cb);
  void onMiss(MissCallback cb);

private:
  volatile Role currentRole = ROLE_NONE;
  uint8_t gameID = 0xFF; // Used to isolate multiple games in the same area
  uint8_t mouseAddress[6];
  volatile uint8_t currentChannel = 1;

  // Scoring and Stamina
  volatile uint32_t hitsScored = 0;
  volatile uint32_t sweepsMissed = 0;

  // Dynamic Timing Variables
  unsigned long catAttackRateMs;
  volatile unsigned long catStaminaMs; // Increases as the cat sweeps down the spectrum
  unsigned long mouseHideMinMs;
  unsigned long mouseHideMaxMs;
  unsigned long mouseSqueakIntervalMs = 2000;
  volatile unsigned long currentMouseHideMs = 0;

  unsigned long lastHopTime = 0;
  unsigned long lastAttackTime = 0;
  unsigned long lastSqueakTime = 0;
  unsigned long lastListenHopTime = 0; // For Cat scanning while listening

  HitCallback hitCb = nullptr;
  MissCallback missCb = nullptr;

  void hopToRandomChannel();
  void nextChannel();
  void strike();

  // ESP-NOW requires static callback functions
  static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  static void onDataRecv(const esp_now_recv_info_t *recv_info,
                         const uint8_t *incomingData, int len);
#else
  static void onDataRecv(const uint8_t *mac, const uint8_t *incomingData,
                         int len);
#endif

  // Pointer to allow static callbacks to access class variables
  static CatMouse *instance;
};
