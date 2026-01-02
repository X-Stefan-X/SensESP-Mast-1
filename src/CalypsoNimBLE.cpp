#include "CalypsoNimBLE.h"
#include "NimBLEDevice.h"

namespace sensesp
{
  ScanCallbacks::~ScanCallbacks(){}
  ClientCallbacks::~ClientCallbacks(){}

  ScanCallbacks scanCallbacks;
  ClientCallbacks clientCallbacks;
  const NimBLEAdvertisedDevice* CalypsoBLE::advDevice = nullptr;
  bool CalypsoBLE::doConnect = false;
  uint32_t CalypsoBLE::scanTimeMs = 5000;
  CalypsoBLE* CalypsoBLE::instance_ = nullptr;


CalypsoBLE::CalypsoBLE()
    : device_name_(""),
      pClient_(nullptr),
      wind_speed_(0),
      wind_dir_(0),
      battery_(0),
      temp_(0) {
        //
      } 

CalypsoBLE* CalypsoBLE::getInstance() {
    if (instance_ == nullptr) {
        instance_ = new CalypsoBLE();
    }
    return instance_;
}

void ClientCallbacks::onConnect(NimBLEClient* pClient) {
  // Handle connection event
  debugI("Connected.");
}
void ClientCallbacks::onDisconnect(NimBLEClient* pClient, int reason) {
  // Handle disconnection event
  debugW("%s Disconnected, reason: = %d - Starting scan\n", pClient->getPeerAddress().toString().c_str(), reason);
  NimBLEDevice::getScan()->start(5000, false, true);
}
void ScanCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
  debugI("Advertised Device found: %s", advertisedDevice->toString().c_str());
  if (advertisedDevice->getAddress().toString() == "d7:f6:cd:3d:f4:14") {
  //if (advertisedDevice->isAdvertisingService(NimBLEUUID("180d"))) { // Check for the Calypso service UUID
    debugI("Found Calypso device: %s", advertisedDevice->toString().c_str());
    NimBLEDevice::getScan()->stop();
    CalypsoBLE* instance = CalypsoBLE::getInstance();
    CalypsoBLE::advDevice = advertisedDevice; // Store the device for later use
    instance->connectToCalypso();
  }
}
void ScanCallbacks::onScanEnd(const NimBLEScanResults& scanResults, int reason) {
  debugI("Scan ended, reason: %d, device count: %d; Restarting scan", reason, scanResults.getCount());
  NimBLEDevice::getScan()->start(CalypsoBLE::scanTimeMs, false, true);
}

void CalypsoBLE::notifyCallback(NimBLERemoteCharacteristic* pChr, uint8_t* pData, size_t length, bool isNotify) {
  // Erwartetes Format laut PDF: [WindSpeed_L, WindSpeed_H, WindDir, Battery, Temp, Roll, Pitch, Compass_H, Compass_L]
  if (length < 9) return;
  uint16_t ws_raw = pData[0] + (pData[1] << 8); // Hex/100 → m/s
  wind_speed_ = ws_raw / 100.0f;
  wind_dir_ = pData[2]; // 1° steps
  battery_ = (pData[3] / 10.0f); // 10% steps (Byte 0-10)
  temp_ = (int8_t)pData[4]; // Byte als signed int
  debugV("NotifyCallback recieved: ws_raw = %f, wind_speed = %f, wind_dir = %f, battery = %f, temp = %f", ws_raw, wind_speed_, wind_dir_, battery_);
}

bool CalypsoBLE::connectToCalypso() {
   debugI("Connecting...");
   ConnectionStatus = 2;
   pClient_ = nullptr;
   DeviceInfo* di = new DeviceInfo();
   debugI("ServiceUUID: %s", CalypsoBLE::advDevice->getServiceUUID().toString());

   if (NimBLEDevice::getCreatedClientCount()) {
    pClient_ = NimBLEDevice::getClientByPeerAddress(CalypsoBLE::advDevice->getAddress());
    if (pClient_) {
      if (!pClient_->connect(advDevice, false)) {
        debugI("Reconnect failed.");
        ConnectionStatus = 0;
        return false;
      }
      debugI("Reconnected client");
    } else {
      pClient_ = NimBLEDevice::getDisconnectedClient();
    }
   }
   // No Client to reuse - > creat new
   if (!pClient_) {
    if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
      debugE("Max clients reached! No more connections available.");
      ConnectionStatus = 0;
      return false;
    }
    pClient_ = NimBLEDevice::createClient();
    debugI("New Client created.");
    pClient_->setClientCallbacks(&clientCallbacks, false);
    //pClient_->setConnectionParams(12, 120, 10, 2000);
    //pClient_->setConnectTimeout(5000);
    ConnectionStatus = 3;
    debugI("Connection Parameters set.");


    if (!pClient_->connect(advDevice)) {
      NimBLEDevice::deleteClient(pClient_);
      debugE("Failed to connect, deleted client.");
      ConnectionStatus = 0;
      return false;
    } else {
      debugI("pClient Connected.");
    }
   }
   if (!pClient_->isConnected()) {
    if (!pClient_->connect(advDevice)) {
      debugE("Failed to connect.");
      ConnectionStatus = 0;
      return false;
    } else
    {
      debugI("pClient isConnected.");
    }
   }
   debugI("Connected to: %s RSSI: %d", pClient_->getPeerAddress().toString().c_str(), pClient_->getRssi());
   ConnectionStatus = 1;

   pSvc_ = nullptr;
   pChr_ = nullptr;
   pDesc_ = nullptr;

   pSvc_ = pClient_->getService("180D"); // Data Service UUID
   if (pSvc_) {
    pChr_ = pSvc_->getCharacteristic("180D");
   }
   if (pChr_) {
    if (pChr_->canNotify()) {
      if (pChr_->subscribe(true, [this](NimBLERemoteCharacteristic* pChr, uint8_t* pData, size_t length, bool isNotify) {
  this->notifyCallback(pChr, pData, length, isNotify);
})
) {
        debugE("Failed to subscribe to notifications");
        pClient_->disconnect();
        ConnectionStatus = 0;
        return false;
      }
    }
   } else {
    debugE("180D Service not found.");
    ConnectionStatus = 0;
   }
   pSvc_ = pClient_->getService("180A"); // Device Information Service UUID
   if (pSvc_) {
    pChr_ = pSvc_->getCharacteristic("2A29"); // Manufacturer Name String");
    if (pChr_) {
      if (pChr_->canRead()) {
        di->manufacturer = pChr_->readValue();
        debugI("Manufacturer: %s", di->manufacturer.c_str());
      } else {
        debugE("2A29 Characteristic not readable.");
        ConnectionStatus = 0;
      }
      pChr_ = pSvc_->getCharacteristic("2A24"); // Model Number String
      if (pChr_) {
        if (pChr_->canRead()) {
          di->model = pChr_->readValue();
          debugI("Model: %s", di->model.c_str());
        } else {
          debugE("2A24 Characteristic not readable.");
          ConnectionStatus = 0;
        }
        pChr_ = pSvc_->getCharacteristic("2A26"); // Firmware Revision String
        if (pChr_) {
          if (pChr_->canRead()) {
            di->firmware = pChr_->readValue();
            debugI("Firmware: %s", di->firmware.c_str());
          } else {
            debugE("2A26 Characteristic not readable.");
            ConnectionStatus = 0;
          }
        }
      }
    }
   } else {
     debugE("180A Service not found.");
     ConnectionStatus = 0;
   }
   debugI("Done with this device.");
   ConnectionStatus = 1;
   return true;
}

void CalypsoBLE::begin() {
  NimBLEDevice::init("hier sollte der Sensesp Name stehen");
  NimBLEDevice::setPower(3);
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setScanCallbacks(&scanCallbacks, false);
  pScan->setInterval(100);
  pScan->setWindow(100);
  pScan->setActiveScan(true);
  ConnectionStatus = 0;
  pScan->start(CalypsoBLE::scanTimeMs);
  debugI("Scanning...");
}






bool CalypsoBLE::set_data_rate(uint8_t rate) {
  if (!pClient_ || !pClient_->isConnected()) {
    debugE("Nicht verbunden – kann Data Rate nicht setzen");
    return false;
  }

  NimBLERemoteService* pService = pClient_->getService("180D"); // Data Service UUID
  if (!pService) {
    debugE("Data Service nicht gefunden");
    return false;
  }

  // Vollständige UUID aus Manual: 0000a002-0000-1000-8000-00805f9b34fb
  NimBLERemoteCharacteristic* dataRateChar = pService->getCharacteristic("0000a002-0000-1000-8000-00805f9b34fb");
  if (!dataRateChar || !dataRateChar->canWrite()) {
    debugE("Data Rate Characteristic nicht verfügbar oder nicht schreibbar");
    return false;
  }

  uint8_t value[1] = { rate };
  if (dataRateChar->writeValue(value, 1, true)) {
    debugI("Data Rate erfolgreich auf 0x%02X gesetzt\n", rate);
    return true;
  } else {
    debugE("Fehler beim Schreiben der Data Rate");
    return false;
  }
}

CalypsoWindSensor::CalypsoWindSensor(unsigned int read_interval_ms)
    : calypso_(CalypsoBLE::getInstance()),
      interval_ms_(read_interval_ms) {}

void CalypsoWindSensor::start() {
  event_loop()->onRepeat(interval_ms_, [this]() { this->update(); });
}

void CalypsoWindSensor::update() {
  float v = calypso_->wind_speed();
  float dir_deg = calypso_->wind_dir_deg();
  float bat = calypso_->battery();
  float t = calypso_->temperature();

  float ang = dir_deg * (3.14159265f / 180.0f);
  if (ang > 3.14159265f) {
    ang -= 2.0f * 3.14159265f;
  }

  speed_ms.set(v);
  angle_rad.set(ang);
  temp_C.set(t);
  soc.set(bat);
}


} // namespace sensesp
