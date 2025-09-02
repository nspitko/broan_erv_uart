#include "filter_reset_button.h"

namespace esphome {
namespace broan {

void FilterResetButton::press_action()
{
	this->parent_->resetFilter();
}

}  // namespace broan
}  // namespace esphome