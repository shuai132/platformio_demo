#include <HardwareSerial.h>
#include <IPv6Address.h>
#include <WiFiAP.h>

static WiFiAPClass wiFiAPClass;

void setup() {
  Serial.begin(115200);
  pinMode(BLINK_LED, OUTPUT);
  wiFiAPClass.softAP("ESP", "88888888");
}

void loop() {
  delay(2000);

  Serial.print("AP IP address: ");
  Serial.println(wiFiAPClass.softAPIP());

  Serial.print("softAP Broadcast IP: ");
  Serial.println(wiFiAPClass.softAPBroadcastIP());

  Serial.print("softAP NetworkID: ");
  Serial.println(wiFiAPClass.softAPNetworkID());

  Serial.print("softAP SubnetCIDR: ");
  Serial.println(wiFiAPClass.softAPSubnetCIDR());

  Serial.print("softAP Hostname: ");
  Serial.println(wiFiAPClass.softAPgetHostname());

  Serial.print("softAP macAddress: ");
  Serial.println(wiFiAPClass.softAPmacAddress());

  Serial.print("softAP StationNum: ");
  Serial.println(wiFiAPClass.softAPgetStationNum());
}
