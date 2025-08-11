#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace broan {

#define BROAN_NUM_FIELDS 25

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

enum BroanFanMode
{
	Off,
	Min,
	Max,
	Custom,
};

enum BroanField
{
	FanMode = 18, // 0x0A for max, 0x09 for min, 0x0B for variable, and 0x01 for off
	Uptime = 20, // In seconds?
	FanSpeed = 23, // Fan speed
	FanSpeedB = 7, // Also Fan speed?
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

  SUB_SELECT(fan_mode)

public:
	uint8_t m_nServerAddress = 0x10;
	uint8_t m_nClientAddress = 0x11;

	BroanField_t m_vecFields[BROAN_NUM_FIELDS] = {
		{ 0x0F, 0x50, BroanFieldType::Float, {0} }, // Upper CFM?. 175 / 00002f43
		{ 0x0E, 0x50, BroanFieldType::Float, {0} }, // Upper CFM?. 175 / 00002f43
		{ 0x0B, 0x50, BroanFieldType::Float, {0} }, // Lower CFM?. 32 / 00000042
		{ 0x0A, 0x50, BroanFieldType::Float, {0} }, // Lower CFM?. 32 / 00000042
		{ 0x02, 0x30, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x0F, 0x22, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x0A, 0x22, BroanFieldType::Float, {0} }, // Unknown. 40 / 00002042
		{ 0x06, 0x22, BroanFieldType::Float, {0} }, // Unknown. 175 / 00002f43
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
		{ 0x02, 0x20, BroanFieldType::Byte, {0} }, // Unknown. 1 / 01
		{ 0x00, 0x20, BroanFieldType::Byte, {0} }, // Unknown. 20 / 14
		{ 0x14, 0x00, BroanFieldType::Int, {0} },
		{ 0x17, 0x00, BroanFieldType::Int, {0} }, // Unknown. NaN / ffffffff
		{ 0x00, 0x30, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x08, 0x22, BroanFieldType::Float, {0} }, // Unknown. 175 / 00002f43
		{ 0x00, 0x22, BroanFieldType::Int, {0} }, // Unknown. 14400 / 40380000
	};


	uint32_t m_nNextPing = 0;
	uint32_t m_nNextQuery = 0;
	bool m_bERVReady = false;
	uint8_t m_nQueryCursor = 0;



	std::vector<uint8_t> m_vecHeader;
	bool m_bHaveHeader = false;

	// uart overrides
	void setup() override;
	void loop() override;

public:
	// get/set
	void setFanMode( std::string mode );
	void setFanSpeed( float speed );


private:
	// Internal
	bool readHeader();
	bool readMessage();
	void handleMessage(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message);
	void send(const std::vector<uint8_t>& msg);
	uint8_t calculateChecksum(uint8_t sender, uint8_t receiver, const std::vector<uint8_t>& message);
	void runRequests();
	void parseBroanFields(const std::vector<uint8_t>& message);
	void writeRegisters( const std::vector<BroanField_t> &values );

	float remap(float flIn, float flInMin, float flInMax, float flOutMin, float flOutMax) {
  		return (flIn - flInMin) * (flOutMax - flOutMin) / (flInMax - flInMin) + flOutMin;
	}

public:
	void set_fan_mode( std::string mode ) {}

protected:
	std::string fan_mode_{};

};

}  // namespace broan
}  // namespace esphome