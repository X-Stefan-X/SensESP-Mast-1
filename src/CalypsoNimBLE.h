/*
* Functions:
* GetDeviceInformations
* ReadStatus
* ReadDataRate
* WriteDataRate
* ActivateClinometerCompass
* ReadAngleOffset
* WriteAngleOffset
* ReadCompassCalibrationCharacteristic
* WriteCompassCalibrationCharacteristic
* ReadWindSpeedCorrectionCharacteristic
* WriteWindSpeedCorrectinCharacteristic
* ResetToFirwareDefaults
* GetNotifyDataService
* 
* 
* Rember the Notes:
* - Firmware versions compatible: 0.47 and above
* - All the measures are referenced to the orientation mark, in the side of the device. The mark is orientated to the bow of the boat.
* - The device enter en low power mode to values <20% of the battery level o Only 1Hz rate available, and motion sensors disabled.
* - The device enter in sleep mode to values <10% of the battery level o Only Bluetooth advertising
* - When you connect with the device, always starts at 4Hz rate and motion sensors disabled.
* - You have a wind direction offset(0xA007), to compensate the mounting of the device.
* - The characteristics (0xA00B and 0xA00C) are reserved. And not used.
* - You can reset the device using the 0xA00A characteristic, sending a 0x01, the device need 3 minuts to reset.
* - Use the motion sensors and the high data rate 8hz with caution, the device increase the power consuption considerably.
* - The Wind Speed Correction characteristic 0xA009 is an optional characteristic, not used in a normal use of the device.
* - When you disconnect the bluetooth link, automaticaly switch off all the sensors.
* 
* 
*/
#pragma once
#include "CalypsoNimBLE.h"
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
    // mehr....
  };
 

class CalypsoBLE {
 public:
  CalypsoBLE() ;
  void begin();
  void on_data_notify(NimBLERemoteCharacteristic* pChr, uint8_t* pData, size_t len, bool isNotify);
  bool set_data_rate(uint8_t rate);
  DeviceInfo getDeviceInfo();
  static bool doConnect;
  static uint32_t scanTimeMs;
  bool connectToCalypso();
  static const NimBLEAdvertisedDevice* advDevice;
  void notifyCallback(NimBLERemoteCharacteristic* pChr, uint8_t* pData, size_t len, bool isNotify);
  static CalypsoBLE* getInstance();
  int ConnectionStatus;

  float wind_speed() const { return wind_speed_; }      // m/s
  float wind_dir_deg() const { return wind_dir_; }      // 0..359
  float battery() const { return battery_; }            // 0..100 %
  float temperature() const { return temp_; }           // °K

 protected:
  std::string device_name_;
  NimBLEClient* pClient_;
  NimBLERemoteCharacteristic* pChr_;
  NimBLERemoteDescriptor* pDesc_;
  NimBLERemoteService* pSvc_;

private:
  static CalypsoBLE* instance_;
  

  // Parsed measurements
  float wind_speed_;
  float wind_dir_;
  float battery_;
  float temp_;
};

class ScanCallbacks : public NimBLEScanCallbacks {
  public: 
  ScanCallbacks() {};

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


extern ScanCallbacks scanCallbacks;
extern ClientCallbacks clientCallbacks;


class CalypsoWindSensor : public Startable, public Observable {
 public:
  CalypsoWindSensor(unsigned int read_interval_ms);

  ObservableValue<float> speed_ms;
  ObservableValue<float> angle_rad;
  ObservableValue<float> temp_C;
  ObservableValue<float> soc;

  void start() override;

 private:
  void update();
  CalypsoBLE* calypso_;
  unsigned int interval_ms_;
};




} // namespace sensesp
