#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace broan {

#define BROAN_NUM_FIELDS 25

enum BroanFieldType
{
	Float,
	Int,
	Byte,
};

enum BroanField
{
	Uptime = 20,
};

struct BroanField_t
{
	uint8_t opcodeHigh;
	uint8_t opcodeLow;

	uint8_t type;

	union {
		char bytes[4];
		float floatVal;
		uint32_t intVal;
		uint8_t byteVal;
	} value;

};

class Broan : public Component, public uart::UARTDevice
{
public:
	Broan(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}  // Note the correct namespace

	uint8_t server_address = 0x10;
	uint8_t my_address = 0x11;

	BroanField_t fields[BROAN_NUM_FIELDS] = {
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
		{ 0x14, 0x00, BroanFieldType::Int, {0} }, // Uptime (Seconds)
		{ 0x17, 0x00, BroanFieldType::Int, {0} }, // Unknown. NaN / ffffffff
		{ 0x00, 0x30, BroanFieldType::Byte, {0} }, // Unknown. 0 / 00
		{ 0x08, 0x22, BroanFieldType::Float, {0} }, // Unknown. 175 / 00002f43
		{ 0x00, 0x22, BroanFieldType::Int, {0} }, // Unknown. 14400 / 40380000
	};


	uint32_t next_ping = 0;
	uint32_t next_query = 0;
	bool erv_ready = false;
	uint8_t query_cursor = 0;



	std::vector<uint8_t> header;
	bool haveHeader = false;

	// uart overrides
	void setup() override;
	void loop() override;

	// Internal
	bool read_header();
	bool read_message();
	void handle_message(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message);
	void send(const std::vector<uint8_t>& msg);
	int peek_byte();
	uint8_t calculate_checksum(uint8_t sender, uint8_t receiver, const std::vector<uint8_t>& message);
	void runRequests();
	void parseBroanFields(const std::vector<uint8_t>& message);
};

}  // namespace broan
}  // namespace esphome