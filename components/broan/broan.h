#pragma once

#include "esphome.h"
#include "esphome/core/component.h"

#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

#include "esphome/components/uart/uart.h"


namespace esphome {
namespace broan {

#define BROAN_NUM_FIELDS 27
#define CONTROL_TIMEOUT 5000

template<typename T>
concept BroanFieldTypes = 	std::is_same_v<T, float> ||
							std::is_same_v<T, uint8_t> ||
							std::is_same_v<T, uint32_t>;

enum BroanFieldType
{
	Float,
	Int,
	Byte,
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
	Intermittent = 0x08,
	Min = 0x09,
	Max = 0x0a,
	Manual = 0x0b,
	Turbo = 0x0c,

};

enum BroanField
{
	FanMode = 0, // 0x0A for max, 0x09 for min, 0x0B for variable, and 0x01 for off
	Uptime = 1, // In seconds?
	FanSpeed = 2, // Fan speed
	FanSpeedB = 3, // Also Fan speed?
	CFMIn_Max = 4,
	CFMOut_Max = 5,
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

	bool m_bPoll = false;
	bool m_bStale = true;


	// Totally safe blind copy of the incoming value.
	BroanField_t copyForUpdate(BroanFieldTypes auto const &newVal) const
	{
		BroanField_t copy = *this;

		size_t len = (m_nType == static_cast<uint8_t>(BroanFieldType::Byte)) ? 1 : 4;
		std::memcpy(copy.m_value.m_rgBytes, &newVal, len);

		return copy;
	}

};

class BroanComponent : public Component, public uart::UARTDevice
{

#ifdef USE_SELECT
	SUB_SELECT(fan_mode)
#endif

#ifdef USE_NUMBER
	SUB_NUMBER(fan_speed)
#endif

public:
	const uint8_t m_nServerAddress = 0x10;
	const uint8_t m_nClientAddress = 0x12;

	BroanField_t m_vecFields[BROAN_NUM_FIELDS] = {
		// Known fields
		{ 0x00, 0x20, BroanFieldType::Byte, {0}, true }, // FanMode
		{ 0x14, 0x00, BroanFieldType::Int, {0} }, // Uptime (Seconds)
		{ 0x08, 0x22, BroanFieldType::Float, {0}, true }, // Fan speed 32 - 175
		{ 0x06, 0x22, BroanFieldType::Float, {0}, true }, // Fan speed 32 - 175 .. Unsure how these are different, related to INT mode?
		{ 0x0F, 0x50, BroanFieldType::Float, {0}, true }, // MAX target CFM in.
		{ 0x0E, 0x50, BroanFieldType::Float, {0}, true }, // MAX target CFM out.
		{ 0x0B, 0x50, BroanFieldType::Float, {0}, true }, // MIN target CFM in.
		{ 0x0A, 0x50, BroanFieldType::Float, {0}, true }, // MIN target CFM out.



		// Unknown fields

		{ 0x02, 0x30, BroanFieldType::Byte, {0}, true }, // Unknown. 1. Set to 0 in TURBO mode
		{ 0x0F, 0x22, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
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
		{ 0x07, 0x50, BroanFieldType::Int, {0} }, // Unknown. Remote regularly sets this to -1
		{ 0x03, 0x20, BroanFieldType::Byte, {0} }, // Unknown. Set to 0 when entering INT mode
	};

	// uart overrides
	void setup() override;
	void loop() override;

public:
	// Control API
	void setFanMode( std::string mode );
	void setFanSpeed( float speed );
	void setFanSpeedCFM( BroanFanMode mode, BroanCFMMode direction, float flTargetCFM );

private:

	uint32_t m_nLastHadControl = 0;
	uint32_t m_nNextQuery = 0;

	bool m_bERVReady = false;
	uint8_t m_nQueryCursor = 0;

	std::vector<uint8_t> m_vecHeader;
	bool m_bHaveHeader = false;

	bool m_bHaveControl = false;
	bool m_bExpectingReply = false;

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

	void queueMessage(std::vector<uint8_t>& message);


protected:
	// esphome glue
	std::string fan_mode_{};
	float fan_speed_{0.f};


};

}  // namespace broan
}  // namespace esphome