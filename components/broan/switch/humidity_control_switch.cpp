#include "humidity_control_switch.h"

namespace esphome {
namespace broan {

void HumidityControlSwitch::write_state(bool state) {
  this->parent_->setHumidityControl(state);
}

}  // namespace broan
}  // namespace esphome