#include "filter_interval_number.h"

namespace esphome {
namespace broan {

void FilterIntervalNumber::control(float value)
{
	// value is in days; optimistic publish only if the write actually queued
	if( this->parent_->setFilterInterval( (uint32_t) value ) )
		this->publish_state( value );
}

}  // namespace broan
}  // namespace esphome
