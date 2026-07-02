#include "intermittent_period_number.h"

namespace esphome {
namespace broan {

void IntermittentPeriodNumber::control(float value)
{
	// HA shows minutes; the ERV register (02 22) is in seconds.
	this->parent_->setIntermittentPeriod( (uint32_t)(value * 60.0f) );
}

}  // namespace broan
}  // namespace esphome