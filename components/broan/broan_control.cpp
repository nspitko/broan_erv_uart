#include "broan.h"

namespace esphome {
namespace broan {

void BroanComponent::setFanMode( std::string mode )
{
	// OVR is a hardware "hard override" (dry-contact). Forcing it in software
	// wedges the unit until its timer expires, so ignore it as a settable mode
	// and just re-read the real mode back to the entity.
	if( mode == "ovr" )
	{
		ESP_LOGW("broan_control", "Ignoring software set of OVR (hardware override only)");
		m_vecFields[FanMode].markDirty();
		return;
	}

	uint8_t value = 0x01;

	if( mode == "min")
		value = BroanFanMode::Min;
	else if (mode == "max" )
		value = BroanFanMode::Max;
	else if( mode == "manual" )
		value = BroanFanMode::Manual;
	else if( mode == "int" )
		value = BroanFanMode::Intermittent;
	else if( mode == "turbo" )
		value = BroanFanMode::Turbo;
	else if( mode == "humidity" )
		value = BroanFanMode::Humidity;
	else if( mode == "ovr" )
		value = BroanFanMode::Ovr;
	else if( mode == "recirculate" )
		value = BroanFanMode::Recirculate;
	else if( mode == "smart" )
		value = BroanFanMode::Smart;
	else if( mode == "away" )
		value = BroanFanMode::Away;
	else
		value = BroanFanMode::Off;


	std::vector<BroanField_t> vecFields;
	vecFields.push_back( m_vecFields[FanMode].copyForUpdate( value ) );

	m_vecFields[FanMode].markDirty();

	writeRegisters( vecFields );

}

void BroanComponent::setFanSpeed( float input )
{
	// Scale each side across its OWN min/max range so an unbalanced setup
	// (supply != exhaust, e.g. the factory 75/70 MED) keeps its offset instead
	// of being silently rebalanced every time the slider moves.
	float flInMin  = m_vecFields[CFMIn_Min].m_value.m_flValue;
	float flInMax  = m_vecFields[CFMIn_Max].m_value.m_flValue;
	float flOutMin = m_vecFields[CFMOut_Min].m_value.m_flValue;
	float flOutMax = m_vecFields[CFMOut_Max].m_value.m_flValue;
	if( !(flInMin > 0.f) || !(flInMax > flInMin) || !(flOutMin > 0.f) || !(flOutMax > flOutMin) )
	{
		ESP_LOGE("broan","Failed to set fan speed: Invalid min/max state");
		return;
	}
	float flInValue  = remap( input, 0.f, 100.f, flInMin, flInMax );
	float flOutValue = remap( input, 0.f, 100.f, flOutMin, flOutMax );

	std::vector<BroanField_t> vecFields;
	vecFields.push_back( m_vecFields[CFMIn_Medium].copyForUpdate( flInValue ) );
	vecFields.push_back( m_vecFields[CFMOut_Medium].copyForUpdate( flOutValue ) );

	// Queue first: if the send queue is full the write never happens, so don't
	// record a pending write (which would suppress genuine read-backs).
	if( !writeRegisters( vecFields ) )
		return;

	uint32_t rawIn, rawOut;
	std::memcpy( &rawIn,  &flInValue,  sizeof(rawIn) );
	std::memcpy( &rawOut, &flOutValue, sizeof(rawOut) );
	notePendingWrite( CFMIn_Medium, rawIn );
	notePendingWrite( CFMOut_Medium, rawOut );

	m_vecFields[CFMIn_Medium].markDirty();
	m_vecFields[CFMOut_Medium].markDirty();
}

void BroanComponent::resetFilter()
{
	std::vector<BroanField_t> vecFields;

	uint32_t unNewFilterLife = FILTER_LIFE_MAX;
	// Register doc (broan.h): "Set to 0x01 to reset filter". Upstream wrote 0
	// here, contradicting its own comment; the FilterLife=MAX write alongside
	// masks the difference in HA. Verify against the unit on the next real
	// filter reset — if the change-filter indicator doesn't clear, revisit.
	uint8_t unFilterReset = 1;

	vecFields.push_back( m_vecFields[FilterLife].copyForUpdate( unNewFilterLife ) );
	vecFields.push_back( m_vecFields[FilterReset].copyForUpdate( unFilterReset ) );

	m_vecFields[FilterReset].markDirty();
	m_vecFields[FilterLife].markDirty();

	writeRegisters( vecFields );
}

void BroanComponent::setHumidityControl( bool enable ) {
	std::vector<BroanField_t> vecFields;

	uint8_t value = 0;

	if (enable) {
		value = 0x01;
	}

	vecFields.push_back( m_vecFields[HumidityControl].copyForUpdate( value ) );

	m_vecFields[HumidityControl].markDirty();

	writeRegisters( vecFields );
}

void BroanComponent::setHumiditySetpoint( float humidity ) {
	std::vector<BroanField_t> vecFields;

	vecFields.push_back( m_vecFields[TargetHumidityA].copyForUpdate( humidity ) );
	vecFields.push_back( m_vecFields[TargetHumidityB].copyForUpdate( humidity ) );

	if( !writeRegisters( vecFields ) )
		return;

	uint32_t raw;
	std::memcpy( &raw, &humidity, sizeof(raw) );
	notePendingWrite( TargetHumidityA, raw );
	notePendingWrite( TargetHumidityB, raw );

	m_vecFields[TargetHumidityA].markDirty();
	m_vecFields[TargetHumidityB].markDirty();
}

void BroanComponent::setCurrentHumidity( float humidity ) {
	std::vector<BroanField_t> vecFields;
  
	ESP_LOGI("broan_control", "Set current humidity: %0.1f%%", humidity);

	vecFields.push_back( m_vecFields[ControllerHumidity].copyForUpdate( humidity ) );
	m_vecFields[ControllerHumidity].markDirty();

	writeRegisters( vecFields );
}

void BroanComponent::setIntermittentPeriod( uint32_t period ) {
	std::vector<BroanField_t> vecFields;

	// S -> MS
	//period *= 1000;
  
	ESP_LOGI("broan_control", "Set int period: %i", period);

	vecFields.push_back( m_vecFields[IntModeDuration].copyForUpdate( period ) );

	if( !writeRegisters( vecFields ) )
		return;

	notePendingWrite( IntModeDuration, period );
	m_vecFields[IntModeDuration].markDirty();
}

bool BroanComponent::setFilterInterval( uint32_t days )
{
	uint32_t seconds = days * 86400u;
	ESP_LOGI("broan_control", "Set filter interval: %u days (%u s)", days, seconds);

	std::vector<BroanField_t> vecFields;
	vecFields.push_back( m_vecFields[FilterInterval].copyForUpdate( seconds ) );

	if( !writeRegisters( vecFields ) )
		return false;

	notePendingWrite( FilterInterval, seconds );
	m_vecFields[FilterInterval].markDirty();
	return true;
}

bool BroanComponent::setOverrideDuration( uint32_t minutes )
{
	uint32_t seconds = minutes * 60u;
	ESP_LOGI("broan_control", "Set override duration: %u min (%u s)", minutes, seconds);

	std::vector<BroanField_t> vecFields;
	vecFields.push_back( m_vecFields[OverrideDuration].copyForUpdate( seconds ) );

	if( !writeRegisters( vecFields ) )
		return false;

	notePendingWrite( OverrideDuration, seconds );
	m_vecFields[OverrideDuration].markDirty();
	return true;
}

bool BroanComponent::setCFM( uint32_t field, float cfm )
{
	if( field >= BroanField::MAX_FIELDS )
		return false;

	ESP_LOGI("broan_control", "Set CFM field %u -> %.1f", field, cfm);

	std::vector<BroanField_t> vecFields;
	vecFields.push_back( m_vecFields[field].copyForUpdate( cfm ) );

	if( !writeRegisters( vecFields ) )
		return false;

	uint32_t raw;
	std::memcpy( &raw, &cfm, sizeof(raw) );
	notePendingWrite( field, raw );
	m_vecFields[field].markDirty();
	return true;
}

}  // namespace broan
}  // namespace esphome
