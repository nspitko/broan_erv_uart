#include "override_duration_number.h"

namespace esphome {
namespace broan {

void OverrideDurationNumber::control(float value)
{
	// value is in minutes; optimistic publish only if the write actually queued
	if( this->parent_->setOverrideDuration( (uint32_t) value ) )
		this->publish_state( value );
}

}  // namespace broan
}  // namespace esphome
