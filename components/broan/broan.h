#pragma once

#include "esphome.h"
#include <deque>
#include <map>
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

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
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

// Diagnostic register scanner — enable via platformio_options build_flags, e.g.:
//   -DSCAN_UNKNOWN=1     brute-force read every register, log the unmapped ones
//   -DDUMP_GROUP=0x50    ALSO PINS the sweep to that one group, logging every value
//                        each pass (change detection in other groups is off while set)
// Left off by default so production configs stay clean.
//#define SCAN_UNKNOWN 1
//#define DUMP_GROUP 0x22
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

enum BroanFanMode
{
	Off = 0x01,
	Ovr = 0x02,
	Intermittent = 0x08,
	Recirculate = 0x06,
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

	// 08 E0 / 09 E0 — airstream humidity (decoded 2026-07-01: jump ~9 pts when
	// airflow starts, collapse together to house RH when fans stop). 08E0 leads,
	// so it's the likely intake side in summer; intake/exhaust split provisional.
	UnknownA,   // 08 E0 — humidity (provisional: intake)
	UnknownB,   // 09 E0 — humidity (provisional: exhaust)

	// Added via ESP-side register scan (2026-07-01)
	FilterInterval, // 09 30 — configured filter life in seconds (120 days default)
	Firmware,       // 02 00 — ASCII app name ("am_main")
	Model,          // 02 60 — ASCII model code ("AM1G4")
	FirmwareVersion,// 01 00 — 3-byte major.minor.patch (next to "am_main")
	HardwareRev,    // 01 60 — 3-byte major.minor.patch (next to "AM1G4")
	OverrideDuration, // 01 22 — override (OVR) length in seconds (1200 = "OVR 20M")
	UnknownC,       // 07 E0 — PCBA (board) thermistor (~32 C; E42/W61 in service manual)

	// Read-only diagnostic from upstream's VTSPEEDW scan (added 2026-07-01)
	FaultCode,      // 17 00 — fault/error code; idles 0xFFFFFFFF (all-ones) = no fault

	// Warning register, CONFIRMED 2026-07-01 by forced W22+W32: showed 22 then
	// alternated 22<->32 while both airflow warnings were active (the register
	// cycles through concurrent warnings). Idles 0xFFFFFFFF like FaultCode.
	// (12 00 also idles all-ones but did NOT react — role unknown, not polled.)
	WarningCode,    // 1A 00 — active W code; idles 0xFFFFFFFF = no warning

	// Decoded 2026-07-02 by mode-cycling with HA history:
	//  02 20 = BASE fan mode (BroanFanMode enum value). Tracks off/min/max/int
	//          but NOT turbo — turbo is an overlay; this is the mode the unit
	//          falls back to when it expires.
	//  07 20 = EXECUTING airflow state: 0 = fans idle, nonzero = air actually
	//          moving, value encodes the running profile (2=max, 3=turbo,
	//          4=int-venting; min/smart/recirc values TBD — see
	//          decodeBroanActiveMode). THE fans-running indicator: during a
	//          60 s "min" test the fans never spun and it correctly stayed 0.
	//  (01 20 was a mode-CHANGE pulse — flashed the new mode enum for one poll
	//  then returned to 0. Event register, no lasting state; dropped.)
	// Both legitimately read 0, so their m_value is seeded 0xFF below —
	// otherwise the oldVal==newVal change-gate swallows the first publish
	// (the bug that faked out the old 08 20 "DamperState" entity).
	BaseModeCode,
	ActiveModeCode,

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
	SUB_SENSOR(uptime)
	SUB_SENSOR(aux_07e0)
	SUB_SENSOR(aux_08e0)
	SUB_SENSOR(aux_09e0)
	SUB_SENSOR(fault_code)
	SUB_SENSOR(warning_code)
	SUB_SENSOR(base_mode_code)
	SUB_SENSOR(active_mode_code)
#endif

#ifdef USE_TEXT_SENSOR
	SUB_TEXT_SENSOR(model)
	SUB_TEXT_SENSOR(firmware)
	SUB_TEXT_SENSOR(firmware_version)
	SUB_TEXT_SENSOR(hardware_rev)
	SUB_TEXT_SENSOR(fault_status)
	SUB_TEXT_SENSOR(warning_status)
	SUB_TEXT_SENSOR(active_mode)
#endif

#ifdef USE_BINARY_SENSOR
	SUB_BINARY_SENSOR(fans_running)
#endif

#ifdef USE_SELECT
	SUB_SELECT(fan_mode)
#endif

#ifdef USE_NUMBER
	SUB_NUMBER(fan_speed)
	SUB_NUMBER(humidity_setpoint)
	SUB_NUMBER(intermittent_period)
	SUB_NUMBER(filter_interval)
	SUB_NUMBER(override_duration)
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


		// Interesting fields found by scan (07/08/09 E0 decoded 2026-07-01)
		{ 0x08, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // UnknownA — airstream humidity (provisional: intake)
		{ 0x09, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // UnknownB — airstream humidity (provisional: exhaust)

		// Added via ESP-side register scan (2026-07-01). Order matches the enum.
		{ 0x09, 0x30, BroanFieldType::Int,   {0}, UPDATE_RATE_SLOW }, // FilterInterval — configured filter life (seconds)
		{ 0x02, 0x00, BroanFieldType::Void,  {0}, UPDATE_RATE_SLOW }, // Firmware — ASCII string, handled specially
		{ 0x02, 0x60, BroanFieldType::Void,  {0}, UPDATE_RATE_SLOW }, // Model    — ASCII string, handled specially
		{ 0x01, 0x00, BroanFieldType::Void,  {0}, UPDATE_RATE_SLOW }, // FirmwareVersion — 3-byte version, handled specially
		{ 0x01, 0x60, BroanFieldType::Void,  {0}, UPDATE_RATE_SLOW }, // HardwareRev     — 3-byte version, handled specially
		{ 0x01, 0x22, BroanFieldType::Int,   {0}, UPDATE_RATE_SLOW }, // OverrideDuration — OVR length (seconds)
		{ 0x07, 0xE0, BroanFieldType::Float, {0}, UPDATE_RATE_FAST }, // UnknownC (07E0) — PCBA board thermistor (~32 C; E42/W61)
		{ 0x17, 0x00, BroanFieldType::Int,   {0}, UPDATE_RATE_SLOW }, // FaultCode — 0xFFFFFFFF = no fault
		{ 0x1A, 0x00, BroanFieldType::Int,   {0}, UPDATE_RATE_FAST }, // WarningCode — cycles through active W codes; 0xFFFFFFFF = none. FAST: warnings are transient

		// Seeded 0xFF (not {0}) so the first read — legitimately 0 — differs
		// from the initial value and publishes past the change-gate.
		{ 0x02, 0x20, BroanFieldType::Byte, {{'\xFF','\xFF','\xFF','\xFF'}}, UPDATE_RATE_FAST }, // BaseModeCode — BroanFanMode enum; excludes turbo overlay
		{ 0x07, 0x20, BroanFieldType::Byte, {{'\xFF','\xFF','\xFF','\xFF'}}, UPDATE_RATE_FAST }, // ActiveModeCode — 0 = fans idle; nonzero = running profile

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
		{ 0x08, 0x20, BroanFieldType::Byte, {0} }, // Controller WRITE-target only: 0 entering SMART, 1 in continuous modes. Reads never answered on AM1G4 (tried 2026-07-01) — don't promote as a sensor.
*/
	// NB: 17 00 above was promoted to FaultCode (2026-07-01).
	};

	// uart overrides
	void setup() override;
	void loop() override;
	void dump_config() override;
	float get_setup_priority() const override;

public:
	// Setup
	void set_flow_control_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

	// Control API. Setters return false when the write was dropped (send queue
	// full) so callers know not to optimistically publish a value never sent.
	void setFanMode( std::string mode );
	void setFanSpeed( float speed );
	void resetFilter();
	void setHumidityControl( bool enable );
	void setHumiditySetpoint( float humidity );
	void setCurrentHumidity( float humidity );
	void setIntermittentPeriod( uint32_t period );
	bool setFilterInterval( uint32_t days );
	bool setOverrideDuration( uint32_t minutes );

	// Write a single CFM setpoint field (by BroanField index) — enables
	// independent supply/exhaust (balanced/unbalanced) control per speed.
	bool setCFM( uint32_t field, float cfm );

	// RE/debug: write one raw byte to an arbitrary register (doesn't need to be
	// in m_vecFields). Used from scan-build template numbers to poke candidate
	// registers (e.g. the 0D22/0E22 per-mode speed-selector hunt, 2026-07-02).
	bool pokeByteRegister( uint8_t opcodeHigh, uint8_t opcodeLow, uint8_t value )
	{
		BroanField_t f = { opcodeHigh, opcodeLow, BroanFieldType::Byte, {0}, 0 };
		f.m_value.m_chValue = value;
		ESP_LOGW( "broan", "POKE %02X%02X <= %u", opcodeHigh, opcodeLow, value );
		return writeRegisters( { f } );
	}
#ifdef USE_NUMBER
	// Called from codegen so we can push the current value back to the entity.
	void register_cfm_number( uint32_t field, number::Number *n ) { cfm_numbers_[field] = n; }
#endif

	// Record that we just wrote `raw` (4-byte union value) to `field`, so the
	// parser ignores stale read-backs until the field reflects it (or ~5s).
	void notePendingWrite( uint32_t field, uint32_t raw ) { m_pendingWrites[field] = { raw, (uint32_t)(millis() + 5000) }; }

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

	uint8_t m_vecHeader[5] = {0};
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
	bool writeRegisters( const std::vector<BroanField_t> &values );

#ifdef USE_NUMBER
	// Recompute + publish the Fan Speed % from CFMIn_Medium against the
	// CFMIn_Min/Max range; no-op until all three registers hold sane values.
	void publishFanSpeed();
#endif

	float remap(float flIn, float flInMin, float flInMax, float flOutMin, float flOutMax) {
  		return (flIn - flInMin) * (flOutMax - flOutMin) / (flInMax - flInMin) + flOutMin;
	}

	BroanField_t* lookupField( uint8_t opcodeHigh, uint8_t opcodeLow );
	uint32_t lookupFieldIndex( uint8_t opcodeHigh, uint8_t opcodeLow );
	void handleUnknownField(uint32_t nOpcodeHigh, uint32_t nOpcodeLow, uint8_t len, uint32_t i, const std::vector<uint8_t>& message );

	bool queueMessage(std::vector<uint8_t>& message);


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

#ifdef USE_NUMBER
	std::map<uint32_t, number::Number*> cfm_numbers_;
#endif

	struct PendingWrite { uint32_t value; uint32_t expiry; };
	std::map<uint32_t, PendingWrite> m_pendingWrites;
};

}  // namespace broan
}  // namespace esphome
