#include "fan_speed_number.h"

namespace esphome {
namespace broan {

void FanSpeedNumber::control(float value)
{
	this->parent_->setFanSpeed( value );
}

}  // namespace broan
}  // namespace esphome