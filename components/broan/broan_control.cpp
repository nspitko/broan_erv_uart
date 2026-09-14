#include "broan.h"

namespace esphome {
namespace broan {

void BroanComponent::setFanMode(std::string mode) {
  uint8_t value = 0x01;

  if (mode == "min")
    value = BroanFanMode::Min;
  else if (mode == "max")
    value = BroanFanMode::Max;
  else if (mode == "manual")
    value = BroanFanMode::Manual;
  else if (mode == "int")
    value = BroanFanMode::Intermittent;
  else if (mode == "turbo")
    value = BroanFanMode::Turbo;
  else if (mode == "humidity")
    value = BroanFanMode::Humidity;
  else if (mode == "recirculate")
    value = BroanFanMode::Recirculate;
  else if (mode == "smart")
    value = BroanFanMode::Smart;
  else
    value = BroanFanMode::Off;

  std::vector<BroanField_t> vecFields;
  vecFields.push_back(m_vecFields[FanMode].copyForUpdate(value));

  m_vecFields[FanMode].markDirty();

  writeRegisters(vecFields);
}

void BroanComponent::setFanSpeed(float input) {
  // return;
  float flMin = m_vecFields[CFMIn_Min].m_value.m_flValue;
  float flMax = m_vecFields[CFMIn_Max].m_value.m_flValue;
  if (flMin == 0 || flMax == 0) {
    ESP_LOGE("broan", "Failed to set fan speed: Invalid min/max state");
    return;
  }
  float value = remap(input, 0.f, 100.f, flMin, flMax);

  std::vector<BroanField_t> vecFields;

  vecFields.push_back(m_vecFields[CFMIn_Medium].copyForUpdate(value));
  vecFields.push_back(m_vecFields[CFMOut_Medium].copyForUpdate(value));

  m_vecFields[CFMIn_Medium].markDirty();
  m_vecFields[CFMOut_Medium].markDirty();

  writeRegisters(vecFields);
}

void BroanComponent::setFanSpeedCFM(BroanFanMode mode, BroanCFMMode direction, float flTargetCFM) {
  std::vector<BroanField_t> vecFields;

  switch (mode) {
    case BroanFanMode::Max: {
      if ((direction & BroanCFMMode::Input) != 0)
        vecFields.push_back(m_vecFields[CFMIn_Max].copyForUpdate(flTargetCFM));
      if ((direction & BroanCFMMode::Output) != 0)
        vecFields.push_back(m_vecFields[CFMOut_Max].copyForUpdate(flTargetCFM));
    } break;

    case BroanFanMode::Min: {
      if ((direction & BroanCFMMode::Input) != 0)
        vecFields.push_back(m_vecFields[CFMIn_Max].copyForUpdate(flTargetCFM));
      if ((direction & BroanCFMMode::Output) != 0)
        vecFields.push_back(m_vecFields[CFMOut_Max].copyForUpdate(flTargetCFM));
    } break;

    default:
      ESP_LOGW("broan", "Unhandled: Setting fan speed limits for  mode %02X", mode);
  }

  writeRegisters(vecFields);
}

// Sends new filter life to ERV in three steps (to mimic wall controller)
// 1. Write new filter life with FilterLifeStage (09:30)
// 2. Write FilterReset=1 + filter life in (08:30)
// 3. Write new filter life again, alone
// Not sure why new filter life needs to be repeated three times
// and if all this is necessary.
void BroanComponent::resetFilter() {
  uint32_t unNewFilterLife = FILTER_LIFE_MAX;

  std::vector<BroanField_t> vecStage;
  vecStage.push_back(m_vecFields[FilterLifeStage].copyForUpdate(unNewFilterLife));
  writeRegisters(vecStage);

  std::vector<BroanField_t> vecFields;
  uint8_t unFilterReset = 1;

  vecFields.push_back(m_vecFields[FilterLife].copyForUpdate(unNewFilterLife));
  vecFields.push_back(m_vecFields[FilterReset].copyForUpdate(unFilterReset));

  m_vecFields[FilterReset].markDirty();
  m_vecFields[FilterLife].markDirty();

  writeRegisters(vecFields);

  std::vector<BroanField_t> vecConfirm;
  vecConfirm.push_back(m_vecFields[FilterLife].copyForUpdate(unNewFilterLife));
  m_vecFields[FilterLife].markDirty();
  writeRegisters(vecConfirm);
}

void BroanComponent::setHumidityControl(bool enable) {
  std::vector<BroanField_t> vecFields;

  uint8_t value = 0;

  if (enable) {
    value = 0x01;
  }

  vecFields.push_back(m_vecFields[HumidityControl].copyForUpdate(value));

  m_vecFields[HumidityControl].markDirty();

  writeRegisters(vecFields);
}

void BroanComponent::setHumiditySetpoint(float humidity) {
  std::vector<BroanField_t> vecFields;

  vecFields.push_back(m_vecFields[TargetHumidityA].copyForUpdate(humidity));
  vecFields.push_back(m_vecFields[TargetHumidityB].copyForUpdate(humidity));

  m_vecFields[TargetHumidityA].markDirty();
  m_vecFields[TargetHumidityB].markDirty();

  writeRegisters(vecFields);
}

void BroanComponent::setCurrentHumidity(float humidity) {
  std::vector<BroanField_t> vecFields;

  ESP_LOGI("broan_control", "Set current humidity: %0.1f%%", humidity);

  vecFields.push_back(m_vecFields[ControllerHumidity].copyForUpdate(humidity));
  m_vecFields[ControllerHumidity].markDirty();

  writeRegisters(vecFields);
}

void BroanComponent::setIntermittentPeriod(uint32_t period) {
  std::vector<BroanField_t> vecFields;

  // S -> MS
  // period *= 1000;

  ESP_LOGI("broan_control", "Set int period: %i", period);

  vecFields.push_back(m_vecFields[IntModeDuration].copyForUpdate(period));
  m_vecFields[IntModeDuration].markDirty();

  writeRegisters(vecFields);
}

}  // namespace broan
}  // namespace esphome
