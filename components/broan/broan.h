#pragma once

#include "esphome.h"
#include "esphome/core/component.h"

#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#ifdef USE_BUTTON
#include "esphome/components/button/button.h"
#endif

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#include "esphome/components/uart/uart.h"


namespace esphome {
namespace broan {

#define CONTROL_TIMEOUT 5000
#define UPDATE_RATE 1000
#define HEARTBEAT_RATE 10000

#define UPDATE_RATE_FAST 10000 // 10 seconds
#define UPDATE_RATE_SLOW 60000 // 1 minute
#define UPDATE_RATE_NEVER 0xFFFFFFFF

#define MAX_REQUEST_SIZE 10
#define INVALID_FIELD 0xFFFFFF

#define FILTER_LIFE_MAX 7884000

//#define SCAN_UNKNOWN 1
//#define LISTEN_ONLY 1

template<typename T>
concept BroanFieldTypes = 	std::is_same_v<T, float> ||
							std::is_same_v<T, uint8_t> ||
							std::is_same_v<T, uint32_t>;

enum BroanFieldType
{
	Float,
	Int,
	Byte,
	Void,
};

enum BroanCFMMode
{
	Input = 1 >> 0,
	Output = 1 >> 1,
	Both = BroanCFMMode::Input | BroanCFMMode::Output,
};

enum BroanFanMode
{
	Off = 0x01,
	Ovr = 0x02,
	Intermittent = 0x08,
	Min = 0x09,
	Max = 0x0a,
	Smart = 0x11,
	Manual = 0x0b,
	Turbo = 0x0c,
	Humidity = 0x0d,
	Away = 0x0F, // "OTH", no idea what this actually does?
};

enum BroanField
{
	// Control
	FanMode = 0,
	HumidityControl,
	IntModeDuration,
	TargetHumidityA, // Set both to same value per VTSPEEDW
	TargetHumidityB,

	// Info
	Uptime, // In seconds?
	Wattage,
	TemperatureIn,
	TemperatureOut,
	SupplyCFM,
	ExhaustCFM,
	SupplyRPM,
	ExhaustRPM,

	// Speeds
	CFMIn_Medium,
	CFMOut_Medium,
	CFMIn_Max,
	CFMOut_Max,
	CFMIn_Min,
	CFMOut_Min,

	// Input
	Heartbeat, // Weird void value that controllers ping every 10s
	ControllerHumidity,
	ControllerTemperature,

	// Maintenance
	FilterReset, // Set to 1 to reset
	FilterLife, // default 7884000 / 3 months

	// Unknown fields that look interesting but aren't understood nor read by controllers
	UnknownA,
	UnknownB,

	MAX_FIELDS,
};

struct BroanField_t
{
	uint8_t m_nOpcodeHigh;
	uint8_t m_nOpcodeLow;

	uint8_t m_nType;

	union {
		char m_rgBytes[4];
		float m_flValue;
		uint32_t m_nValue;
		uint8_t m_chValue;
	} m_value;

	uint32_t m_unPollRate = UPDATE_RATE_SLOW;
	uint32_t m_unLastUpdate = 0;

	// Totally safe blind copy of the incoming value.
	BroanField_t copyForUpdate(BroanFieldTypes auto const &newVal) const
	{
		BroanField_t copy = *this;

		size_t len = (m_nType == static_cast<uint8_t>(BroanFieldType::Byte)) ? 1 : 4;
		std::memcpy(copy.m_value.m_rgBytes, &newVal, len);

		return copy;
	}

	void markDirty()
	{
		m_unLastUpdate = millis() - m_unPollRate;
	}

};

class BroanComponent : public Component, public uart::UARTDevice
{

#ifdef USE_SENSOR
	SUB_SENSOR(power)
	SUB_SENSOR(temperature)
	SUB_SENSOR(temperature_out)
	SUB_SENSOR(filter_life)
	SUB_SENSOR(supply_cfm)
	SUB_SENSOR(exhaust_cfm)
	SUB_SENSOR(supply_rpm)
	SUB_SENSOR(exhaust_rpm)
#endif

#ifdef USE_SELECT
	SUB_SELECT(fan_mode)
#endif

#ifdef USE_NUMBER
	SUB_NUMBER(fan_speed)
	SUB_NUMBER(humidity_setpoint)
	SUB_NUMBER(intermittent_period)
#endif

#ifdef USE_BUTTON
  SUB_BUTTON(filter_reset)
#endif

#ifdef USE_SWITCH
  SUB_SWITCH(humidity_control)
#endif

public:
	const uint8_t m_nServerAddress = 0x10;
	const uint8_t m_nClientAddress = 0x12;

	bool m_bWaitForRemote = false;

	BroanField_t m_vecFields[BroanField::MAX_FIELDS] = {
		// Known fields
		// Control
		{ 0x00, 0x20, BroanFieldType::Byte, {0}, UPDATE_RATE_FAST }, // FanMode
		{ 0x0F, 0x22, BroanFieldType::Byte, {0}, UPDATE_RATE_SLOW }, // Humidity control on/off
		{ 0x02, 0x22, BroanFieldType::Int, {0}, UPDATE_RATE_SLOW }, // INT mode on time (seconds, OFF time will be what remains of an hour)
		{ 0x0C, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Target humidity?
		{ 0x0A, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // Target humidity? (These are set together)


		// Info
		{ 0x14, 0x00, BroanFieldType::Int, {0}, UPDATE_RATE_SLOW }, // Uptime (Seconds)
		{ 0x23, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Power draw (Watts)
		{ 0x01, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Temperature sensor (In)
		{ 0x03, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Temperature sensor (Out)
		{ 0x05, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Intake CFM
		{ 0x06, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Exhaust CFM
		{ 0x03, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Intake RPM
		{ 0x04, 0x10, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // Exhaust RPM

		// Speeds
		{ 0x06, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // MED target CFM in.
		{ 0x08, 0x22, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // MED target CFM out.
		{ 0x0E, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MAX target CFM in.
		{ 0x0F, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MAX target CFM out.
		{ 0x0A, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MIN target CFM in.
		{ 0x0B, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_SLOW }, // MIN target CFM out.

		//Input
		{ 0x00, 0x50, BroanFieldType::Void, {0}, UPDATE_RATE_NEVER }, // Unknown. Controllers regularly write this. Some kind of heartbeat maybe?
		{ 0x04, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Controller Humidity (Write only)
		{ 0x05, 0x50, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Controller temperature (Write only)

		// Maintenance
		{ 0x01, 0x30, BroanFieldType::Byte, {0}, UPDATE_RATE_SLOW }, // Set to 0x01 to reset filter
		{ 0x08, 0x30, BroanFieldType::Int, {0}, UPDATE_RATE_SLOW }, // Number of seconds until filter needs reset. Set along side reset byte


		// Interesting fields found by scan
		{ 0x08, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Unknown. Seems to change a lot. 38.943115 / 1109116352 (Does not correlate with fan speed)
		{ 0x09, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_NEVER }, // Unknown. Seems to change a lot. 36.360962 / 1108439456 (Same as above)

/*
		// Unknown fields scanned by the VTSPEEDW
		{ 0x02, 0x30, BroanFieldType::Byte, {0}, UPDATE_RATE_SLOW }, // Unknown. 1. Set to 0 in TURBO mode
		{ 0x0A, 0x22, BroanFieldType::Float, {0} }, // Unknown. 40 / 00002042
		{ 0x0E, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x0C, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x0B, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x0A, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x09, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x08, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x07, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x06, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x05, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x04, 0x21, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x02, 0x20, BroanFieldType::Byte, {0} }, // Unknown. Set to 8 when in INT mode.
		{ 0x17, 0x00, BroanFieldType::Int, {0} }, // Unknown. NaN / ffffffff
		{ 0x00, 0x30, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x00, 0x22, BroanFieldType::Int, {0} }, // Unknown. 14400 / 40380000
		{ 0x07, 0x50, BroanFieldType::Int, {0} }, // Unknown. VTSPEEDW often sets this to -1
		{ 0x03, 0x20, BroanFieldType::Byte, {0} }, // Unknown. Set to 0 when entering INT mode
		{ 0x08, 0x20, BroanFieldType::Byte, {0} }, // Unknown. Set to 0 when entering SMART mode, set to 1 in continuous modes.
*/
	};

	// uart overrides
	void setup() override;
	void loop() override;
	void dump_config() override;
	float get_setup_priority() const override;

public:
	// Setup
	void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

	// Control API
	void setFanMode( std::string mode );
	void setFanSpeed( float speed );
	void setFanSpeedCFM( BroanFanMode mode, BroanCFMMode direction, float flTargetCFM );
	void resetFilter();
	void setHumidityControl( bool enable );
	void setHumiditySetpoint( float humidity );
	void setCurrentHumidity( float humidity );
	void setIntermittentPeriod( uint32_t period );

private:

	uint32_t m_nLastHadControl = 0;
	uint32_t m_unLastHeartbeat = 0; // Next time to send heartbeat

	bool m_bERVReady = false;

#ifdef SCAN_UNKNOWN
	// Field scanner
	uint32_t m_nNextScan = 0;
	uint8_t m_nFieldCursor = 0;
	uint8_t m_nGroupCursor = 0x20;
	std::map<uint16_t, BroanField_t> m_vecFieldData;
#endif

	std::vector<uint8_t> m_vecHeader;
	bool m_bHaveHeader = false;

	bool m_bHaveControl = false;
	bool m_bExpectingReply = false;
	bool m_bHaveSentMessage = false;

	std::deque<std::vector<uint8_t>> m_vecSendQueue;


private:
	// Internal
	bool readHeader();
	bool readMessage();
	void handleMessage(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message);
	void send(const std::vector<uint8_t>& msg);
	uint8_t calculateChecksum(uint8_t sender, uint8_t receiver, const std::vector<uint8_t>& message);
	void replyIfAllowed();
	void runTasks();
	void parseBroanFields(const std::vector<uint8_t>& message);
	void writeRegisters( const std::vector<BroanField_t> &values );

	float remap(float flIn, float flInMin, float flInMax, float flOutMin, float flOutMax) {
  		return (flIn - flInMin) * (flOutMax - flOutMin) / (flInMax - flInMin) + flOutMin;
	}

	BroanField_t* lookupField( uint8_t opcodeHigh, uint8_t opcodeLow );
	uint32_t lookupFieldIndex( uint8_t opcodeHigh, uint8_t opcodeLow );
	void handleUnknownField(uint32_t nOpcodeHigh, uint32_t nOpcodeLow, uint8_t len, uint32_t i, const std::vector<uint8_t>& message );

	void queueMessage(std::vector<uint8_t>& message);


protected:
	// esphome glue
	std::string fan_mode_{};
	
	float fan_speed_{0.f};
	float power_{0.f};
	float temperature_{0.f};
	float temperature_out_{0.f};
	float supply_cfm_{0.f};
	float exhaust_cfm_{0.f};
	float supply_rpm_{0.f};
	float exhaust_rpm_{0.f};

	uint32_t filter_life_{0};

	GPIOPin *flow_control_pin_{nullptr};
};

}  // namespace broan
}  // namespace esphome
