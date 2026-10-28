// Bring-up step 6: test each servo and find the "panel flat" angle.
// Power the servos from the 5 V rail with the 470 uF capacitor fitted first.
//
// Type a number 0-180 in the Serial Monitor (newline ending) to move the servo
// there. Find the angle where the panel is flat and put it in
// TILT_SERVO_CENTER in firmware/solar_tracker/config.h.
// Change SERVO_PIN to 19 to test the wiper.
#include <ESP32Servo.h>

const int SERVO_PIN = 18;   // 18 = tilt, 19 = wiper
Servo servo;

void setup() {
  Serial.begin(115200);
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, 500, 2400);
  servo.write(90);
  Serial.println("Servo at 90. Type an angle 0-180 and press Enter.");
}

void loop() {
  if (Serial.available()) {
    int angle = Serial.parseInt();
    while (Serial.available()) Serial.read();
    angle = constrain(angle, 0, 180);
    servo.write(angle);
    Serial.printf("Moved to %d\n", angle);
  }
}
