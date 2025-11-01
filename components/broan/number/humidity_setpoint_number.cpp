#include "humidity_setpoint_number.h"

namespace esphome {
namespace broan {

void HumiditySetpointNumber::control(float value)
{
	this->parent_->setHumiditySetpoint( value );
}

}  // namespace broan
}  // namespace esphome