#pragma once

#include "esphome/components/number/number.h"
#include "../broan.h"

class BroanComponent;

namespace esphome {
namespace broan {

// A number entity bound to one CFM setpoint register (by BroanField index).
// Writing it sends that single field, so supply and exhaust can be set
// independently (balanced or unbalanced) per speed.
class CFMNumber : public number::Number, public Parented<BroanComponent> {
 public:
  CFMNumber() = default;
  void set_field(uint32_t field) { this->field_ = field; }
  uint32_t get_field() const { return this->field_; }

 protected:
  void control(float value) override;
  uint32_t field_{0};
};

}  // namespace broan
}  // namespace esphome
