// Signal K application template file.
//
// This application demonstrates core SensESP concepts in a very
// concise manner. You can build and upload the application as is
// and observe the value changes on the serial port monitor.
//
// You can use this source file as a basis for your own projects.
// Remove the parts that are not relevant to you, and add your own code
// for external hardware libraries.

#include <memory>

#include "sensesp.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp_app_builder.h"
#include "Wire.h"

#include <Arduino.h>

using namespace sensesp;



typedef struct __attribute__((packed)) messagePack_t {
  float throttleLevel;
  float regenLevel;
  float RemoteBatLevel;
  bool regenEN;
  bool ecoEN;
  bool w2bMaster;
  bool MOB;
  bool MarkPosition;
  bool horn;
} messagePack_t;

messagePack_t Status{};

void ESPi2c_get_messagePack() {
  debugI("ESP i2c get messagePack");
  Wire.requestFrom(0x08, (uint8_t)sizeof(messagePack_t));
  if (Wire.available() == sizeof(messagePack_t)) {
    Wire.readBytes((uint8_t*)&Status, sizeof(messagePack_t));
    String s = "T:" + String(Status.throttleLevel, 2) +
             " R:" + String(Status.regenLevel, 2) +
             (Status.regenEN ? " R=1" : " R=0") +
             " B:" + String(Status.RemoteBatLevel, 2) +
             (Status.w2bMaster ? " M=1" : " M=0") +
             (Status.MOB ? " MOB!" : "") +
             (Status.ecoEN ? " E=1" : " E=0") +
             (Status.MarkPosition ? " Pos": "") + 
             (Status.horn ? " Horn" : "");
    debugI("i2c: %s", s.c_str());
  } else {
    debugE("No Answer!");
  }
}


// The setup function performs one-time application initialization.
void setup() {
  SetupLogging(ESP_LOG_DEBUG);

  // Construct the global SensESPApp() object
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    // Set a custom hostname for the app.
                    ->set_hostname("sensesp-mast1")
                    // Optionally, hard-code the WiFi and Signal K server
                    // settings. This is normally not needed.
                    //->set_wifi_client("My WiFi SSID", "my_wifi_password")
                    //->set_wifi_access_point("My AP SSID", "my_ap_password")
                    //->set_sk_server("192.168.10.3", 80)
                    ->enable_ota("ThisIsMyOTAPassword")
                    ->get_app();

  Wire.begin();

  // Level Channel 0
  ledcSetup(0, 8000, 8);
  ledcAttachPin(D0, 0);
  auto Lc1 = new SKValueListener<float>("electrical.outside.mast.channel.1.value", CHANGE, "Channel 1");
  auto* Lc1_Consumer = new LambdaConsumer<float>([](float value) {
    ledcWrite(0, value);
    debugI("Channel 1: %f", value);
  });
  Lc1->connect_to(Lc1_Consumer);

  // Level Channel 1
  ledcSetup(1, 8000, 8);
  ledcAttachPin(D1, 1);
  auto Lc2 = new SKValueListener<float>("electrical.outside.mast.channel.2.value", CHANGE, "Channel 2");
  auto* Lc2_Consumer = new LambdaConsumer<float>([](float value) {
    ledcWrite(1, value);
    debugI("Channel 2: %f", value);
  });
  Lc2->connect_to(Lc2_Consumer);

  // Level Channel 2
  ledcSetup(2, 8000, 8);
  ledcAttachPin(D2, 2);
  auto Lc3 = new SKValueListener<float>("electrical.outside.mast.channel.3.value", CHANGE, "Channel 3");
  auto* Lc3_Consumer = new LambdaConsumer<float>([](float value) {
    ledcWrite(2, value);
    debugI("Channel 3: %f", value);
  });
  Lc3->connect_to(Lc3_Consumer);

  // Level Channel 3
  ledcSetup(3, 8000, 8);
  ledcAttachPin(D3, 2);
  auto Lc4 = new SKValueListener<float>("electrical.outside.mast.channel.4.value", CHANGE, "Channel 4");
  auto* Lc4_Consumer = new LambdaConsumer<float>([](float value) {
    ledcWrite(2, value);
    debugI("Channel 4: %f", value);
  });
  Lc4->connect_to(Lc4_Consumer);

  ////////////////////////////////////////
  // i2c Repeatsensor Stuff
  int rs_interval = 1000;
  
  auto* throttle_sensor = new RepeatSensor<float>(rs_interval, [](){
    return Status.throttleLevel;
  });
  auto* throttle_out = new SKOutput<float>("electrical.motor.throttle.level", "/throttle_level");
  throttle_sensor->connect_to(throttle_out);

  auto* regen_sensor = new RepeatSensor<float>(rs_interval, [](){
    return Status.regenLevel;
  });
  auto* regen_out = new SKOutput<float>("electrical.motor.regen.level", "/regen_level");
  regen_sensor->connect_to(regen_out);

  auto* RemoteBatLevel_sensor = new RepeatSensor<float>(rs_interval, [](){
    return Status.RemoteBatLevel;
  });
  auto* RemoteBatLevel_out = new SKOutput<float>("electrical.motor.remote.Batlevel", "/RemoteBat_level");
  RemoteBatLevel_sensor->connect_to(RemoteBatLevel_out);

  auto* regenEN_sensor = new RepeatSensor<bool>(rs_interval, [](){
    return Status.regenEN;
  });
  auto* regenEN_out = new SKOutput<bool>("electrical.motor.regen.enable", "/regen_en");
  regen_sensor->connect_to(regen_out);

  auto* ecoEN_sensor = new RepeatSensor<bool>(rs_interval, [](){
    return Status.ecoEN;
  });
  auto* ecoEN_out = new SKOutput<bool>("electrical.motor.eco.enable", "/eco_en");
  ecoEN_sensor->connect_to(ecoEN_out);

  auto* w2bMaster_sensor = new RepeatSensor<bool>(rs_interval, [](){
    return Status.w2bMaster;
  });
  auto* w2bMaster_out = new SKOutput<bool>("electrical.motor.remote.w2bmaster", "/w2bmaster");
  w2bMaster_sensor->connect_to(w2bMaster_out);

  auto* MOB_sensor = new RepeatSensor<bool>(rs_interval, [](){
    return Status.MOB;
  });
  auto* MOB_out = new SKOutput<bool>("MOB", "/MOB");
  MOB_sensor->connect_to(MOB_out);

  auto* MarkPosition_sensor = new RepeatSensor<bool>(rs_interval, [](){
    return Status.MarkPosition;
  });
  auto* MarkPosition_out = new SKOutput<bool>("MarkPosition", "/MarkPosition");
  MarkPosition_sensor->connect_to(MarkPosition_out);

  auto* horn_sensor = new RepeatSensor<bool>(rs_interval, [](){
    return Status.horn;
  });
  auto* horn_out = new SKOutput<bool>("Horn", "/Horn");
  horn_sensor->connect_to(horn_out);

  event_loop()->onRepeat(1000, ESPi2c_get_messagePack);


  // To avoid garbage collecting all shared pointers created in setup(),
  // loop from here.
  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }
