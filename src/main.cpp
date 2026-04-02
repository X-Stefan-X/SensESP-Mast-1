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
#include "sensesp/transforms/linear.h"
#include "sensesp_app_builder.h"
#include "CalypsoNimBLE.h"
#include "SHT85.h"


#include <Arduino.h>

using namespace sensesp;

#define SHT85_ADDRESS       0x44
SHT85 sht(SHT85_ADDRESS);

SKOutput<float>* sk_temp;
SKOutput<float>* sk_hum;

void read_sht85() {
  sht.read();

  float t_C = sht.getTemperature();
  float h_percent = sht.getHumidity();

  float t_K = t_C + 273.15F;
  float h_ratio = h_percent / 100.0F;

  sk_temp->set_input(t_K);
  sk_hum->set_input(h_ratio); 
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

  //Calypso BLE
  auto* calypso_sensor = new CalypsoWindSensor(500);
  calypso_sensor->start(); 

  calypso_sensor->speed_ms.connect_to(new SKOutputFloat("environment.wind.speedApparent", "/Calypso/AWS"));
  calypso_sensor->angle_rad.connect_to(new SKOutputFloat("environment.wind.angleApparent", "/Calypso Wind/angle"));
  calypso_sensor->soc.connect_to(new Linear(0.01, 0.0))->connect_to(new SKOutputFloat("electrical.batteries.99.capacity.stateOfCharge", "/Calypso Wind/battery SOC"));
  // Roll, Pitch und Kompass sind nur verfügbar wenn Sensoren aktiviert wurden (calypso->activate_sensors(true))
  //calypso_sensor->roll_deg.connect_to(new SKOutputFloat("environment.outside.mast.roll", "/Calypso/roll"));
  //calypso_sensor->pitch_deg.connect_to(new SKOutputFloat("environment.outside.mast.pitch", "/Calypso/pitch"));
  //calypso_sensor->compass_deg.connect_to(new SKOutputFloat("environment.outside.mast.compass", "/Calypso/compass"));

  // Calypso Gerätestatus → Signal K (0=sleep, 1=low power, 2=normal)
  auto* sk_calypso_status = new SKOutput<float>(
      "electrical.outside.mast.calypso.status",
      "/Calypso/status",
      new SKMetadata("", "Calypso Gerätestatus (0=sleep, 1=low power, 2=normal)")
  );

  // Calypso Datenrate → Signal K (aktuell gelesener Wert in Hz)
  auto* sk_calypso_datarate = new SKOutput<float>(
      "electrical.outside.mast.calypso.dataRate",
      "/Calypso/dataRate",
      new SKMetadata("Hz", "Calypso Datenrate (1, 4 oder 8 Hz)")
  );

  // Status und Datenrate alle 30s vom Gerät lesen und publizieren (nur wenn verbunden)
  auto* calypso_ble = CalypsoBLE::getInstance();
  event_loop()->onRepeat(30000, [calypso_ble, sk_calypso_status, sk_calypso_datarate]() {
    if (calypso_ble->ConnectionStatus == 1) {
      uint8_t status = calypso_ble->read_status();
      if (status != 0xFF) sk_calypso_status->set_input((float)status);
      uint8_t rate = calypso_ble->read_data_rate();
      if (rate != 0xFF) sk_calypso_datarate->set_input((float)rate);
    }
  });

  // Datenrate von Signal K aus setzen (erlaubte Werte: 1, 4, 8)
  auto* Lrate = new SKValueListener<float>("electrical.outside.mast.calypso.setDataRate", CHANGE, "Calypso setDataRate");
  auto* Lrate_Consumer = new LambdaConsumer<float>([calypso_ble](float value) {
    uint8_t rate = (uint8_t)value;
    if (rate == 1 || rate == 4 || rate == 8) {
      calypso_ble->set_data_rate(rate);
    } else {
      debugW("Ungültige Datenrate: %.0f – erlaubt sind 1, 4 oder 8 Hz", value);
    }
  });
  Lrate->connect_to(Lrate_Consumer);


  //SHT85

  sht.begin();

  sk_temp = new SKOutput<float>(
      "environment.outside.temperature",
      "/sensors/sht85/temperature",
      new SKMetadata("K", "Inside temperature")
  );

  sk_hum = new SKOutput<float>(
      "environment.outside.humidity",
      "/sensors/sht85/humidity",
      new SKMetadata("ratio", "Inside relative humidity")
  );

  event_loop()->onRepeat(30000, []() { read_sht85(); });


  // To avoid garbage collecting all shared pointers created in setup(),
  // loop from here.
  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }
