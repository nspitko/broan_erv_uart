#pragma once

#include "esphome/components/number/number.h"
#include "../broan.h"

class BroanComponent;

namespace esphome {
namespace broan {

class HumiditySetpointNumber : public number::Number, public Parented<BroanComponent> {
 public:
  HumiditySetpointNumber() = default;
  
 protected:
  void control(float value) override;
};

}  // namespace broan
}  // namespace esphome