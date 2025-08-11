#include "fan_mode_select.h"

namespace esphome {
namespace broan {

void BaudRateSelect::control(const std::string &value)
{
	BroanFanMode mode;

	switch( value )
	{
		case "min": mode = Min; break;
		case "max": mode = Max; break;
		case "manual": mode = Custom; break;
		default: mode = Off;
	}
	this->parent_->setFanMode( mode );
}

}  // namespace ld2410
}  // namespace esphome