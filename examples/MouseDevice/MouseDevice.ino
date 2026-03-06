#include <CatMouse.h>

CatMouse mouse;

// Define your Game ID. The Mouse will only respond to Cats with this ID.
const uint8_t gameID = 42;

// Runs when the Cat finds the Mouse on the current channel
void onMouseCaught(uint8_t channel) {
  Serial.printf("CAUGHT! The Cat found me on Channel: %d ! Fleeing...\n",
                channel);
}

void setup() {
  Serial.begin(115200);

  // Initialize as MOUSE with the Game ID.
  // It will hide on a random channel for anywhere between 100ms and 500ms
  // Squeaks every 2000ms (default) to let Cats discover it
  if (!mouse.beginAsMouse(gameID, 100, 500)) {
    Serial.println("Failed to initialize Mouse! Check ESP-NOW.");
    while (true) delay(1000);
  }

  // Register the callback for when the Cat scores a direct hit
  mouse.onHit(onMouseCaught);

  Serial.println("Mouse is hiding and occasionally squeaking...");
}

void loop() {
  // Calling update() automatically triggers hops and periodic Squeak broadcasts
  mouse.update();
}
