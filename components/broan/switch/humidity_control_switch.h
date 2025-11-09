#pragma once

#include "esphome/components/switch/switch.h"
#include "../broan.h"

class BroanComponent;

namespace esphome {
namespace broan {

class HumidityControlSwitch : public switch_::Switch, public Parented<BroanComponent> {
 public:
  HumidityControlSwitch() = default;

 protected:
  void write_state(bool state) override;
};

}  // namespace broan
}  // namespace esphome