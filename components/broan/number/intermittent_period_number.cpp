#include "intermittent_period_number.h"

namespace esphome {
namespace broan {

void IntermittentPeriodNumber::control(float value)
{
	this->parent_->setIntermittentPeriod( value );
}

}  // namespace broan
}  // namespace esphome