#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  // ssid, password, channel, hidden, max_connections
  WiFi.softAP("HeartsLAN", "hearts2026", 6, false, 8);
  Serial.print("AP up at ");
  Serial.println(WiFi.softAPIP());   // 192.168.4.1
}

void loop() {
  Serial.printf("Connected devices: %d\n", WiFi.softAPgetStationNum());
  delay(5000);
}