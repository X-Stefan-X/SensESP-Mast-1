#include "CalypsoNimBLE.h"
#include "NimBLEDevice.h"
#include <cstring>

namespace sensesp {

// ─── Static member definitions ───────────────────────────────────────────────

ScanCallbacks   scanCallbacks;
ClientCallbacks clientCallbacks;

const NimBLEAdvertisedDevice* CalypsoBLE::advDevice  = nullptr;
bool     CalypsoBLE::doConnect  = false;
uint32_t CalypsoBLE::scanTimeMs = 5000;
CalypsoBLE* CalypsoBLE::instance_ = nullptr;

ScanCallbacks::~ScanCallbacks()   {}
ClientCallbacks::~ClientCallbacks() {}

// ─── Singleton ───────────────────────────────────────────────────────────────

CalypsoBLE* CalypsoBLE::getInstance() {
  if (instance_ == nullptr) {
    instance_ = new CalypsoBLE();
  }
  return instance_;
}

// ─── Constructor ─────────────────────────────────────────────────────────────

CalypsoBLE::CalypsoBLE()
    : device_name_(""),
      pClient_(nullptr),
      wind_speed_(0), wind_dir_(0), battery_(0),
      temp_(0), roll_(0), pitch_(0), compass_(0) {}

// ─── BLE Callbacks ───────────────────────────────────────────────────────────

void ClientCallbacks::onConnect(NimBLEClient* pClient) {
  debugI("Connected.");
}

void ClientCallbacks::onDisconnect(NimBLEClient* pClient, int reason) {
  debugW("%s Disconnected, reason=%d – restarting scan",
         pClient->getPeerAddress().toString().c_str(), reason);
  NimBLEDevice::getScan()->start(5000, false, true);
}

void ScanCallbacks::onResult(const NimBLEAdvertisedDevice* advertisedDevice) {
  debugI("Advertised Device found: %s", advertisedDevice->toString().c_str());
  if (advertisedDevice->getAddress().toString() == "d7:f6:cd:3d:f4:14") {
    debugI("Found Calypso device: %s", advertisedDevice->toString().c_str());
    NimBLEDevice::getScan()->stop();
    CalypsoBLE* instance = CalypsoBLE::getInstance();
    CalypsoBLE::advDevice = advertisedDevice;
    instance->connectToCalypso();
  }
}

void ScanCallbacks::onScanEnd(const NimBLEScanResults& scanResults, int reason) {
  debugI("Scan ended, reason=%d, count=%d – restarting", reason, scanResults.getCount());
  NimBLEDevice::getScan()->start(CalypsoBLE::scanTimeMs, false, true);
}

// ─── Notify Callback ─────────────────────────────────────────────────────────

void CalypsoBLE::notifyCallback(NimBLERemoteCharacteristic* pChr,
                                uint8_t* pData, size_t length, bool isNotify) {
  // Format laut Developer Manual 1.8 (UUID 0x2A39):
  // Byte 0-1: Wind Speed  (uint16 LE, /100 → m/s)
  // Byte 2-3: Wind Dir    (uint16 LE, 1° steps, 0–360°)
  // Byte 4:   Battery     (×10 → %)
  // Byte 5:   Temperature (−100 → °C)
  // Byte 6:   Roll        (−90 → °, range −90..+90)
  // Byte 7:   Pitch       (−90 → °, range −90..+90)
  // Byte 8-9: eCompass    (uint16 LE, 360 − value → °)
  if (length < 10) return;

  uint16_t ws_raw  = pData[0] + (pData[1] << 8);
  wind_speed_ = ws_raw / 100.0f;

  uint16_t dir_raw = pData[2] + (pData[3] << 8);
  wind_dir_   = (float)dir_raw;

  battery_    = pData[4] * 10.0f;
  temp_       = (float)pData[5] - 100.0f;
  roll_       = (float)pData[6] - 90.0f;
  pitch_      = (float)pData[7] - 90.0f;

  uint16_t cmp_raw = pData[8] + (pData[9] << 8);
  compass_    = 360.0f - (float)cmp_raw;

  debugV("Calypso: speed=%.2f m/s, dir=%.0f°, bat=%.0f%%, temp=%.1f°C, "
         "roll=%.0f°, pitch=%.0f°, compass=%.0f°",
         wind_speed_, wind_dir_, battery_, temp_, roll_, pitch_, compass_);
}

// ─── Private Helper ──────────────────────────────────────────────────────────

NimBLERemoteCharacteristic* CalypsoBLE::get_data_service_char(const char* uuid) {
  if (!pClient_ || !pClient_->isConnected()) {
    debugE("Not connected – cannot access characteristic %s", uuid);
    return nullptr;
  }
  NimBLERemoteService* svc = pClient_->getService("180D");
  if (!svc) {
    debugE("Data Service 180D not found");
    return nullptr;
  }
  NimBLERemoteCharacteristic* chr = svc->getCharacteristic(uuid);
  if (!chr) {
    debugE("Characteristic %s not found", uuid);
  }
  return chr;
}

// ─── Connect ─────────────────────────────────────────────────────────────────

bool CalypsoBLE::connectToCalypso() {
  debugI("Connecting...");
  ConnectionStatus = 2;
  pClient_ = nullptr;
  DeviceInfo di;
  debugI("ServiceUUID: %s", CalypsoBLE::advDevice->getServiceUUID().toString().c_str());

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

  if (!pClient_) {
    if (NimBLEDevice::getCreatedClientCount() >= NIMBLE_MAX_CONNECTIONS) {
      debugE("Max clients reached.");
      ConnectionStatus = 0;
      return false;
    }
    pClient_ = NimBLEDevice::createClient();
    debugI("New client created.");
    pClient_->setClientCallbacks(&clientCallbacks, false);
    ConnectionStatus = 3;

    if (!pClient_->connect(advDevice)) {
      NimBLEDevice::deleteClient(pClient_);
      debugE("Failed to connect, client deleted.");
      ConnectionStatus = 0;
      return false;
    }
  }

  if (!pClient_->isConnected()) {
    if (!pClient_->connect(advDevice)) {
      debugE("Failed to connect.");
      ConnectionStatus = 0;
      return false;
    }
  }

  debugI("Connected to: %s  RSSI: %d",
         pClient_->getPeerAddress().toString().c_str(), pClient_->getRssi());
  ConnectionStatus = 1;

  pSvc_  = nullptr;
  pChr_  = nullptr;
  pDesc_ = nullptr;

  // ── Data Service: subscribe to wind data notifications (0x2A39) ──
  pSvc_ = pClient_->getService("180D");
  if (pSvc_) {
    pChr_ = pSvc_->getCharacteristic("2A39");
  }
  if (pChr_) {
    if (pChr_->canNotify()) {
      if (!pChr_->subscribe(true, [this](NimBLERemoteCharacteristic* pChr,
                                         uint8_t* pData, size_t length, bool isNotify) {
            this->notifyCallback(pChr, pData, length, isNotify);
          })) {
        debugE("Failed to subscribe to notifications.");
        pClient_->disconnect();
        ConnectionStatus = 0;
        return false;
      }
      debugI("Subscribed to wind data notifications.");
    }
  } else {
    debugE("Data Service 180D or characteristic 2A39 not found.");
    ConnectionStatus = 0;
  }

  // ── Device Information Service (0x180A) ──
  pSvc_ = pClient_->getService("180A");
  if (pSvc_) {
    pChr_ = pSvc_->getCharacteristic("2A29"); // Manufacturer
    if (pChr_ && pChr_->canRead()) {
      di.manufacturer = pChr_->readValue();
      debugI("Manufacturer: %s", di.manufacturer.c_str());
    }
    pChr_ = pSvc_->getCharacteristic("2A24"); // Model Number
    if (pChr_ && pChr_->canRead()) {
      di.model = pChr_->readValue();
      debugI("Model: %s", di.model.c_str());
    }
    pChr_ = pSvc_->getCharacteristic("2A26"); // Firmware Revision
    if (pChr_ && pChr_->canRead()) {
      di.firmware = pChr_->readValue();
      debugI("Firmware: %s", di.firmware.c_str());
    }
  } else {
    debugW("Device Information Service 180A not found.");
  }

  debugI("Connection setup complete.");
  ConnectionStatus = 1;
  return true;
}

// ─── BLE Init & Scan ─────────────────────────────────────────────────────────

void CalypsoBLE::begin() {
  NimBLEDevice::init("sensesp-mast1");
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

// ─── Configuration Functions ─────────────────────────────────────────────────

uint8_t CalypsoBLE::read_status() {
  auto* chr = get_data_service_char("A001");
  if (!chr || !chr->canRead()) return 0xFF;
  std::string val = chr->readValue();
  uint8_t status = val.empty() ? 0xFF : (uint8_t)val[0];
  debugI("Device status: 0x%02X (%s)", status,
         status == 0x00 ? "sleep" : status == 0x01 ? "low power" : "normal");
  return status;
}

uint8_t CalypsoBLE::read_data_rate() {
  auto* chr = get_data_service_char("A002");
  if (!chr || !chr->canRead()) return 0xFF;
  std::string val = chr->readValue();
  uint8_t rate = val.empty() ? 0xFF : (uint8_t)val[0];
  debugI("Data rate: 0x%02X (%dHz)", rate, rate);
  return rate;
}

bool CalypsoBLE::set_data_rate(uint8_t rate) {
  auto* chr = get_data_service_char("A002");
  if (!chr || !chr->canWrite()) {
    debugE("Data rate characteristic not writable.");
    return false;
  }
  uint8_t value[1] = { rate };
  if (!chr->writeValue(value, 1, true)) {
    debugE("Failed to write data rate.");
    return false;
  }
  debugI("Data rate set to 0x%02X (%dHz)", rate, rate);
  return true;
}

bool CalypsoBLE::activate_sensors(bool on) {
  auto* chr = get_data_service_char("A003");
  if (!chr || !chr->canWrite()) {
    debugE("Sensors characteristic not writable.");
    return false;
  }
  uint8_t value[1] = { on ? (uint8_t)0x01 : (uint8_t)0x00 };
  if (!chr->writeValue(value, 1, true)) {
    debugE("Failed to write sensors characteristic.");
    return false;
  }
  debugI("Clinometer/eCompass sensors: %s", on ? "ON" : "OFF");
  return true;
}

uint16_t CalypsoBLE::read_angle_offset() {
  auto* chr = get_data_service_char("A007");
  if (!chr || !chr->canRead()) return 0xFFFF;
  std::string val = chr->readValue();
  if (val.size() < 2) return 0xFFFF;
  uint16_t offset = (uint8_t)val[0] + ((uint8_t)val[1] << 8);
  debugI("Angle offset: %d°", offset);
  return offset;
}

bool CalypsoBLE::write_angle_offset(uint16_t offset) {
  auto* chr = get_data_service_char("A007");
  if (!chr || !chr->canWrite()) {
    debugE("Angle offset characteristic not writable.");
    return false;
  }
  uint8_t value[2] = { (uint8_t)(offset & 0xFF), (uint8_t)(offset >> 8) };
  if (!chr->writeValue(value, 2, true)) {
    debugE("Failed to write angle offset.");
    return false;
  }
  debugI("Angle offset set to %d°", offset);
  return true;
}

bool CalypsoBLE::read_compass_calibration() {
  auto* chr = get_data_service_char("A008");
  if (!chr || !chr->canRead()) return false;
  std::string val = chr->readValue();
  bool calibrating = !val.empty() && (uint8_t)val[0] == 0x01;
  debugI("eCompass calibration mode: %s", calibrating ? "ON" : "OFF");
  return calibrating;
}

bool CalypsoBLE::write_compass_calibration(bool calibrate) {
  auto* chr = get_data_service_char("A008");
  if (!chr || !chr->canWrite()) {
    debugE("Compass calibration characteristic not writable.");
    return false;
  }
  uint8_t value[1] = { calibrate ? (uint8_t)0x01 : (uint8_t)0x00 };
  if (!chr->writeValue(value, 1, true)) {
    debugE("Failed to write compass calibration.");
    return false;
  }
  debugI("eCompass calibration: %s", calibrate ? "started" : "saved to flash");
  return true;
}

float CalypsoBLE::read_wind_speed_correction() {
  auto* chr = get_data_service_char("A009");
  if (!chr || !chr->canRead()) return 1.0f;
  std::string val = chr->readValue();
  if (val.size() < 4) return 1.0f;
  float factor;
  memcpy(&factor, val.data(), 4);
  debugI("Wind speed correction factor: %.4f", factor);
  return factor;
}

bool CalypsoBLE::write_wind_speed_correction(float factor) {
  auto* chr = get_data_service_char("A009");
  if (!chr || !chr->canWrite()) {
    debugE("Wind speed correction characteristic not writable.");
    return false;
  }
  uint8_t value[4];
  memcpy(value, &factor, 4);
  if (!chr->writeValue(value, 4, true)) {
    debugE("Failed to write wind speed correction.");
    return false;
  }
  debugI("Wind speed correction factor set to %.4f", factor);
  return true;
}

bool CalypsoBLE::reset_device() {
  auto* chr = get_data_service_char("A00A");
  if (!chr || !chr->canWrite()) {
    debugE("Reset characteristic not writable.");
    return false;
  }
  uint8_t value[1] = { 0x01 };
  if (!chr->writeValue(value, 1, true)) {
    debugE("Failed to write reset command.");
    return false;
  }
  debugW("Device reset initiated – device needs ~3 minutes to recover.");
  return true;
}

// ─── CalypsoWindSensor ───────────────────────────────────────────────────────

CalypsoWindSensor::CalypsoWindSensor(unsigned int read_interval_ms)
    : calypso_(CalypsoBLE::getInstance()),
      interval_ms_(read_interval_ms) {}

void CalypsoWindSensor::start() {
  event_loop()->onRepeat(interval_ms_, [this]() { this->update(); });
}

void CalypsoWindSensor::update() {
  float v       = calypso_->wind_speed();
  float dir_deg = calypso_->wind_dir_deg();
  float bat     = calypso_->battery();
  float t       = calypso_->temperature();
  float r       = calypso_->roll();
  float p       = calypso_->pitch();
  float cmp     = calypso_->compass();

  // Convert wind direction degrees to radians, normalise to -π..+π
  float ang = dir_deg * (3.14159265f / 180.0f);
  if (ang > 3.14159265f) {
    ang -= 2.0f * 3.14159265f;
  }

  speed_ms.set(v);
  angle_rad.set(ang);
  temp_C.set(t);
  soc.set(bat);
  roll_deg.set(r);
  pitch_deg.set(p);
  compass_deg.set(cmp);
}

} // namespace sensesp
