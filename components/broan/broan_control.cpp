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
	else if( mode == "int" )
		value = 0x08;
	else if( mode == "turbo" )
		value = 0x0c;
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


void BroanComponent::setFanSpeedCFM( BroanFanMode mode, BroanCFMMode direction, float flTargetCFM )
{
	std::vector<BroanField_t> vecFields;


	switch( mode )
	{
		case BroanFanMode::Max:
		{
			if( ( direction & BroanCFMMode::Input ) != 0 )
				vecFields.push_back( m_vecFields[CFMIn_Max].copyForUpdate( flTargetCFM ) );
			if( ( direction & BroanCFMMode::Output ) != 0 )
				vecFields.push_back( m_vecFields[CFMOut_Max].copyForUpdate( flTargetCFM ) );
		}
		break;

		case BroanFanMode::Min:
		{
			if( ( direction & BroanCFMMode::Input ) != 0 )
				vecFields.push_back( m_vecFields[CFMIn_Max].copyForUpdate( flTargetCFM ) );
			if( ( direction & BroanCFMMode::Output ) != 0 )
				vecFields.push_back( m_vecFields[CFMOut_Max].copyForUpdate( flTargetCFM ) );
		}
		break;


		default:
			ESP_LOGW("broan","Unhandled: Setting fan speed limits for  mode %02X", mode );

	}

	writeRegisters( vecFields );

}

}  // namespace broan
}  // namespace esphome