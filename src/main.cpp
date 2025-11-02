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
#include "driver/ledc.h"

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
                    ->enable_ota("ThisIsMyOTAPassword")
                    ->get_app();

  // Level Channel 1
  auto Lc1 = new SKValueListener<float>("environment.outside.mast.level_channel_1");
  Lc1->connect_to(new LambdaConsumer<float>([](float value) {
    ledcWrite(1, static_cast<uint8_t>(value * 255)); // Write value (0-1 scaled to 0-255)
  }));

  // Level Channel 2
  auto Lc2 = new SKValueListener<float>("environment.outside.mast.level_channel_2");
  Lc2->connect_to(new LambdaConsumer<float>([](float value) {
    ledcWrite(2, static_cast<uint8_t>(value * 255)); // Write value (0-1 scaled to 0-255)
  }));

  // Level Channel 3
  auto Lc3 = new SKValueListener<float>("environment.outside.mast.level_channel_3");
  Lc3->connect_to(new LambdaConsumer<float>([](float value) {
    ledcWrite(3, static_cast<uint8_t>(value * 255)); // Write value (0-1 scaled to 0-255)
  }));



  // To avoid garbage collecting all shared pointers created in setup(),
  // loop from here.
  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }
