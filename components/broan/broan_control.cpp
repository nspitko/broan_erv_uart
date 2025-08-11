#include "broan.h"

namespace esphome {
namespace broan {

void BroanComponent::setFanMode( std::string mode )
{
	uint8_t value = 0x01;

	if( mode == "min")
		value = 0x09;
	else if (mode == "max" )
		value = 0x0a;
	else if( mode == "manual" )
		value = 0x0b;
	else
		value = 0x01;


	std::vector<BroanField_t> vecFields;
	vecFields.push_back( m_vecFields[FanMode].copyForUpdate( value ) );

	m_vecFields[FanMode].m_bStale = true;

	writeRegisters( vecFields );

}

void BroanComponent::setFanSpeed( float input )
{
	//return;
	float value = remap( input, 0.f, 100.f, 32.f, 175.f );

	std::vector<BroanField_t> vecFields;

	vecFields.push_back( m_vecFields[FanSpeed].copyForUpdate( value ) );
	vecFields.push_back( m_vecFields[FanSpeedB].copyForUpdate( value ) );

	m_vecFields[FanSpeed].m_bStale = true;
	m_vecFields[FanSpeedB].m_bStale = true;

	writeRegisters( vecFields );

}

}  // namespace broan
}  // namespace esphome