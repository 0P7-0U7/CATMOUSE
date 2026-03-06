#include <CatMouse.h>

CatMouse cat;

// Define your Game ID. Cats will only hunt Mice with matching IDs.
const uint8_t gameID = 42;

void onCatHit(uint8_t channel) {
  Serial.printf("HIT! Found Mouse on Channel: %d | Hits Scored: %d\n", channel,
                cat.getHitsScored());
}

void onCatMiss(uint8_t channel) {
  Serial.printf("Miss... Sweeping Channel: %d | Sweeps: %d\n", channel,
                cat.getSweepsMissed());
}

void setup() {
  Serial.begin(115200);

  // Initialize as CAT using just the Game ID (no MAC address)
  // The Cat will boot in LISTENING mode, scanning channels until it hears a Squeak
  if (!cat.beginAsCat(gameID, nullptr, 10)) {
    Serial.println("Failed to initialize Cat! Check ESP-NOW.");
    while (true) delay(1000);
  }

  // Register the result callbacks
  cat.onHit(onCatHit);
  cat.onMiss(onCatMiss);

  Serial.println("Cat is scanning channels, listening for the first Mouse squeak...");
}

void loop() {
  // Calling update() triggers listening, and eventually sweeps & stamina logic
  cat.update();
}
