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


#include <Arduino.h>

using namespace sensesp;




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
                    ->enable_system_info_sensors()
                    ->enable_ota("ThisIsMyOTAPassword")
                    ->get_app();



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
  ledcAttachPin(D3, 3);
  auto Lc4 = new SKValueListener<float>("electrical.outside.mast.channel.4.value", CHANGE, "Channel 4");
  auto* Lc4_Consumer = new LambdaConsumer<float>([](float value) {
    ledcWrite(3, value);
    debugI("Channel 4: %f", value);
  });
  Lc4->connect_to(Lc4_Consumer);

  


  // To avoid garbage collecting all shared pointers created in setup(),
  // loop from here.
  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }
