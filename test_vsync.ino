// Quick VSync test - just flash VSync pin to see if it's connected
#define PIN_VSYNC 5

void setup() {
  pinMode(PIN_VSYNC, OUTPUT);
  Serial.begin(115200);
  Serial.println("VSync Test - Pin 5 should flash at 2 Hz");
}

void loop() {
  digitalWriteFast(PIN_VSYNC, HIGH);
  delay(250);
  digitalWriteFast(PIN_VSYNC, LOW);
  delay(250);
  Serial.println("Toggle");
}
