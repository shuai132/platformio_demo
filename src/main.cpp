#include <HardwareSerial.h>

void setup() {
    Serial.begin(115200);
    pinMode(BLINK_LED, OUTPUT);
}

void loop() {
    Serial.println("loop...");
    digitalWrite(BLINK_LED, HIGH);
    delay(100);
    digitalWrite(BLINK_LED, LOW);
    delay(100);
}
