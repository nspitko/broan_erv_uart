#include "cfm_number.h"

namespace esphome {
namespace broan {

void CFMNumber::control(float value)
{
	// Optimistic publish, but only if the write actually queued — otherwise HA
	// would show a value that was never sent (and never corrected, since the
	// unchanged register's re-reads dedup away).
	if( this->parent_->setCFM( this->field_, value ) )
		this->publish_state( value );
}

}  // namespace broan
}  // namespace esphome
