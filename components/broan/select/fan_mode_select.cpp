#include "fan_mode_select.h"

namespace esphome {
namespace broan {

void FanModeSelect::control(const std::string &value)
{
	this->parent_->setFanMode( value );
}

}  // namespace ld2410
}  // namespace esphome