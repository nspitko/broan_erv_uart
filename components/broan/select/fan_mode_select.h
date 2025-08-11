#pragma once

#include "esphome/components/select/select.h"
#include "../broan.h"

namespace esphome {
namespace broan {

class FanModeSelect : public select::Select, public Parented<BroanComponent> {
public:
	FanModeSelect() = default;

protected:
	void control(const std::string &value) override;
};

}  // namespace broan
}  // namespace esphome