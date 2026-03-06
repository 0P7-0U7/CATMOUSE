#include "CatMouse.h"
#include "soc/soc.h"

CatMouse *CatMouse::instance = nullptr;

CatMouse::CatMouse() {
  // Enforce single instance — ESP-NOW only supports one set of static callbacks
  if (instance != nullptr) {
    log_e("CatMouse: Only one instance allowed! Previous instance overwritten.");
  }
  instance = this;
}

CatMouse::~CatMouse() { end(); }

void CatMouse::end() {
  esp_now_unregister_send_cb();
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  currentRole = ROLE_NONE;
  hitsScored = 0;
  sweepsMissed = 0;
  if (instance == this) {
    instance = nullptr;
  }
}

bool CatMouse::beginAsCat(uint8_t gameID_in, uint8_t *targetMac,
                          unsigned long attackRateMs) {
  this->gameID = gameID_in;
  catAttackRateMs = attackRateMs;
  catStaminaMs = attackRateMs; // Start stamina at peak
  hitsScored = 0;
  sweepsMissed = 0;

  // Prepare the radio safely for all architectures
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    log_e("CatMouse: esp_now_init() failed");
    return false;
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv); // Add recv to catch Squeaks

  if (targetMac == nullptr) {
    // Auto-Discovery Mode: Cat starts listening for a Squeak
    currentRole = ROLE_CAT_LISTENING;
    lastListenHopTime = millis();
    currentChannel = 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    // Add Broadcast as peer to hear squeaks
    esp_now_peer_info_t peerInfo = {};
    for (int i = 0; i < 6; i++)
      peerInfo.peer_addr[i] = 0xFF;
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      log_e("CatMouse: Failed to add broadcast peer");
      return false;
    }
  } else {
    // Direct Hunting Mode
    currentRole = ROLE_CAT_HUNTING;
    memcpy(mouseAddress, targetMac, 6);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mouseAddress, 6);
    peerInfo.channel = 0; // Force radio on the fly
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      log_e("CatMouse: Failed to add target peer");
      return false;
    }
  }

  return true;
}

bool CatMouse::beginAsMouse(uint8_t gameID_in, unsigned long hideMinMs,
                            unsigned long hideMaxMs,
                            unsigned long squeakIntervalMs) {
  currentRole = ROLE_MOUSE;
  this->gameID = gameID_in;
  mouseHideMinMs = hideMinMs;
  mouseHideMaxMs = hideMaxMs;
  mouseSqueakIntervalMs = squeakIntervalMs;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    log_e("CatMouse: esp_now_init() failed");
    return false;
  }

  esp_now_register_recv_cb(onDataRecv);

  // We also need send callback in case we broadcast Squeaks
  esp_now_register_send_cb(onDataSent);

  // Prepare pure broadcast peer for Squeaks
  esp_now_peer_info_t peerInfo = {};
  for (int i = 0; i < 6; i++)
    peerInfo.peer_addr[i] = 0xFF;
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    log_e("CatMouse: Failed to add broadcast peer");
    return false;
  }

  hopToRandomChannel();
  return true;
}

void CatMouse::update() {
  unsigned long currentMillis = millis();

  if (currentRole == ROLE_CAT_LISTENING) {
    // Scan channels while waiting for a Squeak from the Mouse
    if (currentMillis - lastListenHopTime >= 250) {
      lastListenHopTime = currentMillis;
      nextChannel();
    }
  } else if (currentRole == ROLE_CAT_HUNTING) {
    if (currentMillis - lastAttackTime >= catStaminaMs) {
      lastAttackTime = currentMillis;
      strike();
    }
  } else if (currentRole == ROLE_MOUSE) {
    if (currentMillis - lastHopTime >= currentMouseHideMs) {
      hopToRandomChannel(); // Mouse got nervous and moved
    }

    // Mouse occasionally squeaks to allow auto-discovery
    if (currentMillis - lastSqueakTime >= mouseSqueakIntervalMs) {
      lastSqueakTime = currentMillis;
      broadcastSqueak();
    }
  }
}

uint8_t CatMouse::getChannel() { return currentChannel; }

void CatMouse::onHit(HitCallback cb) { hitCb = cb; }

void CatMouse::onMiss(MissCallback cb) { missCb = cb; }

void CatMouse::strike() {
  // Transmit the unique Game ID instead of a generic byte
  esp_now_send(mouseAddress, &gameID, sizeof(gameID));
}

void CatMouse::broadcastSqueak() {
  static const uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  // Squeak contains the gameID so Cats know if it's their target
  esp_now_send(broadcastMac, &gameID, sizeof(gameID));
}

void CatMouse::hopToRandomChannel() {
  currentChannel = random(1, 14); // Wi-Fi channels 1 through 13
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

  // Pick a new random duration within the user's range for this specific
  // channel
  currentMouseHideMs = random(mouseHideMinMs, mouseHideMaxMs + 1);

  lastHopTime = millis();
}

void CatMouse::nextChannel() {
  currentChannel++;
  if (currentChannel > 13)
    currentChannel = 1;
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
}

// ---------------------------------------------------------
// STATIC ESP-NOW CALLBACKS
// ---------------------------------------------------------

void CatMouse::onDataSent(const uint8_t *mac_addr,
                          esp_now_send_status_t status) {
  if (!instance)
    return;

  if (instance->currentRole == ROLE_CAT_HUNTING) {
    if (status == ESP_NOW_SEND_SUCCESS) {
      // HIT!
      instance->hitsScored++;
      instance->catStaminaMs = 5; // FRENZY MODE! Fast strikes!
      if (instance->hitCb)
        instance->hitCb(instance->currentChannel);
    } else {
      // MISS!
      instance->sweepsMissed++;
      // The Cat gets tired the longer it sweeps, slowing down
      if (instance->catStaminaMs < 50)
        instance->catStaminaMs += 1;

      if (instance->missCb)
        instance->missCb(instance->currentChannel);
      instance->nextChannel();
    }
  }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void CatMouse::onDataRecv(const esp_now_recv_info_t *recv_info,
                          const uint8_t *incomingData, int len) {
  const uint8_t *mac = recv_info->src_addr;
#else
void CatMouse::onDataRecv(const uint8_t *mac, const uint8_t *incomingData,
                          int len) {
#endif
  if (!instance)
    return;

  // Ignore invalid packets or those belonging to a different Game ID
  if (len < 1 || incomingData[0] != instance->gameID)
    return;

  if (instance->currentRole == ROLE_MOUSE) {
    // The Mouse was hit! Trigger user code, then flee.
    if (instance->hitCb)
      instance->hitCb(instance->currentChannel);
    instance->hopToRandomChannel();
  } else if (instance->currentRole == ROLE_CAT_LISTENING) {
    // The Listening Cat heard a valid Squeak! Lock its MAC and Hunt!
    memcpy(instance->mouseAddress, mac, 6);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, instance->mouseAddress, 6);
    peerInfo.channel = 0; // Force radio on the fly
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(instance->mouseAddress)) {
      esp_now_add_peer(&peerInfo);
    }

    // Begin the hunt
    instance->currentRole = ROLE_CAT_HUNTING;
  }
}
