/*
* Calypso Ultrasonic BLE – Developer Manual 1.8
*
* Services:
*   0x180A  Device Information Service
*   0x180D  Data Service
*
* Data Service Characteristics:
*   0x2A39  Notify  – Wind data stream (speed, direction, battery, temp, roll, pitch, compass)
*   0xA001  Read    – Device status (0=sleep, 1=low power, 2=normal)
*   0xA002  R/W     – Data rate (0x01=1Hz, 0x04=4Hz, 0x08=8Hz)
*   0xA003  R/W     – Sensors on/off (clinometer + eCompass, 0x01=on, 0x00=off)
*   0xA007  R/W     – Wind direction angle offset (uint16)
*   0xA008  R/W     – eCompass calibration (0x01=calibrate, 0x00=normal)
*   0xA009  R/W     – Wind speed correction factor (float32, optional)
*   0xA00A  R/W     – Reset device (write 0x01; needs ~3 min to recover)
*
* Notes:
*   - Firmware 0.47 and above required.
*   - Device starts at 4Hz, motion sensors disabled after each connect.
*   - <20% battery: low power mode, max 1Hz, no motion sensors.
*   - <10% battery: sleep mode, BLE advertising only.
*   - Disconnecting BLE turns off all sensors automatically.
*/
#pragma once
#include <memory>
#include "sensesp/sensors/sensor.h"
#include "sensesp/system/startable.h"
#include "sensesp/system/observable.h"
#include "NimBLEDevice.h"

namespace sensesp {

struct DeviceInfo {
  std::string manufacturer;
  std::string model;
  std::string firmware;
};

class CalypsoBLE {
 public:
  CalypsoBLE();
  void begin();
  bool connectToCalypso();
  void notifyCallback(NimBLERemoteCharacteristic* pChr, uint8_t* pData, size_t len, bool isNotify);

  // Configuration – Data Rate (0x01=1Hz, 0x04=4Hz, 0x08=8Hz)
  uint8_t read_data_rate();
  bool    set_data_rate(uint8_t rate);

  // Configuration – Device status (0=sleep, 1=low power, 2=normal)
  uint8_t read_status();

  // Configuration – Clinometer + eCompass sensors
  bool activate_sensors(bool on);

  // Configuration – Wind direction mounting offset (0–360, uint16)
  uint16_t read_angle_offset();
  bool     write_angle_offset(uint16_t offset);

  // Configuration – eCompass calibration
  bool read_compass_calibration();
  bool write_compass_calibration(bool calibrate);

  // Configuration – Wind speed correction factor (optional, float32, default 1.0)
  float read_wind_speed_correction();
  bool  write_wind_speed_correction(float factor);

  // Reset device (write 0x01 to 0xA00A; device needs ~3 min to recover)
  bool reset_device();

  static bool     doConnect;
  static uint32_t scanTimeMs;
  static const NimBLEAdvertisedDevice* advDevice;
  static CalypsoBLE* getInstance();

  int ConnectionStatus;

  // Getters for latest notify values
  float wind_speed()  const { return wind_speed_; }   // m/s
  float wind_dir_deg() const { return wind_dir_; }    // 0–360°
  float battery()     const { return battery_; }      // 0–100 %
  float temperature() const { return temp_; }         // °C
  float roll()        const { return roll_; }         // -90–+90°
  float pitch()       const { return pitch_; }        // -90–+90°
  float compass()     const { return compass_; }      // 0–360°

 protected:
  std::string device_name_;
  NimBLEClient*                 pClient_;
  NimBLERemoteCharacteristic*   pChr_;
  NimBLERemoteDescriptor*       pDesc_;
  NimBLERemoteService*          pSvc_;

 private:
  static CalypsoBLE* instance_;

  // Helper: get a characteristic from the Data Service (0x180D)
  NimBLERemoteCharacteristic* get_data_service_char(const char* uuid);

  // Latest parsed measurements
  float wind_speed_;
  float wind_dir_;
  float battery_;
  float temp_;
  float roll_;
  float pitch_;
  float compass_;
};


class ScanCallbacks : public NimBLEScanCallbacks {
 public:
  ScanCallbacks() {}
  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override;
  void onScanEnd(const NimBLEScanResults& scanResults, int reason) override;
  CalypsoBLE* calypso_;
  virtual ~ScanCallbacks();
};

class ClientCallbacks : public NimBLEClientCallbacks {
 public:
  ClientCallbacks(CalypsoBLE* calypso = nullptr) : calypso_(calypso) {}
  void onConnect(NimBLEClient* pClient) override;
  void onDisconnect(NimBLEClient* pClient, int reason) override;
  CalypsoBLE* calypso_;
  virtual ~ClientCallbacks();
};

extern ScanCallbacks  scanCallbacks;
extern ClientCallbacks clientCallbacks;


class CalypsoWindSensor : public Startable, public Observable {
 public:
  CalypsoWindSensor(unsigned int read_interval_ms);

  ObservableValue<float> speed_ms;
  ObservableValue<float> angle_rad;
  ObservableValue<float> temp_C;
  ObservableValue<float> soc;
  ObservableValue<float> roll_deg;
  ObservableValue<float> pitch_deg;
  ObservableValue<float> compass_deg;

  void start() override;

 private:
  void update();
  CalypsoBLE*  calypso_;
  unsigned int interval_ms_;
};

} // namespace sensesp
