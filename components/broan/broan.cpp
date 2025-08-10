#include "broan.h"

namespace esphome {
namespace broan { // Change 'broan' to match your component name


void Broan::setup()
{
	//uart::UARTDevice::setup();
	Component::setup();
	esp_log_level_set("broan", ESP_LOG_DEBUG);

	header.reserve(5);
}


void Broan::loop()
{
	while ( true )
	{
		if( !read_header() ) break;
		if( !read_message() ) break;
	}

	if( !read_header() )
		runRequests();
}

bool Broan::read_header()
{
	if( haveHeader )
	{
		//ESP_LOGD("broan", "Recycling header (good)");
		return true;
	}

	if( available() < 5 )
		return false;

	for (uint8_t i = 0; i < 5; i++) {
		header[i] = read();
		if( i == 0 && header[i] != 0x01 )
		{
			ESP_LOGW("broan", "Alignment: Unexpected %02X in position %i", header[i], i);
			return false;
		}

		if( i == 3 && header[i] != 0x01 )
		{
			ESP_LOGW("broan", "Alignment: Unexpected %02X in position %i", header[i], i);
			return false;
		}
	}

	uint8_t head = header[0];
	if ( header[1] > 32 || header[2] > 32 || header[4] > 0x7F)
	{
		ESP_LOGW("broan", "Alignment: Unexpected %02X %02X %02X %02X %02X",
			header[0], header[1], header[2], header[3], header[4]);
		return false;
	}

	haveHeader = true;

	return true;
}

bool Broan::read_message()
{
	uint8_t target = header[1];
	uint8_t sender = header[2];
	int len = header[4];

	if( !haveHeader )
		return false;

	if( available() < len + 2 )
	{
		//ESP_LOGD("broan", "Waiting for rest of packet to show up in buffer (Want %i have %i)", len + 2, available() );
		return false;
	}

	haveHeader = false;

	std::vector<uint8_t> message(len);

	for (uint8_t i = 0; i < len; i++) {
		if (!available())
		{
			ESP_LOGE("broan", "Exhausted ring buffer somehow");
			return false;
		}

		message[i] = read();
	}

	uint8_t checksum = read();
	uint8_t expected_checksum = calculate_checksum(sender, target, message);
	if (checksum != expected_checksum)
	{
		ESP_LOGE("broan", "Checksum mismatch: got %02X, expected %02X", checksum, expected_checksum);
		return false;
	}

	uint8_t footer = read();
	if (footer != 0x04)
	{
		ESP_LOGE("broan", "Missing 0x04 footer, incomplete read??");
		return false;
	}

	handle_message(sender, target, message);

	return true;
}

void Broan::handle_message(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message)
{
	if (target != my_address) return;
	int type = message[0];
	switch (type)
	{
		case 0x02:
		{
			// Respond to ping
			std::vector<uint8_t> reply = {0x03};
			reply.insert(reply.end(), message.begin() + 1, message.end());
			send(reply);
			ESP_LOGD("broan","0x02 Ping");
			erv_ready = true;

			break;
		}
		case 0x04:
		{
			// Heartbeat
			//ESP_LOGD("broan","Got 0x04 heartbeat");
			send({0x05});
			next_ping = millis() + 100 ;
			//send({0x04});
			break;
		}
		case 0x05:
			//ESP_LOGD("broan","Got 0x05 heartbeat response");
			break;

		case 0x40:
		{
			ESP_LOGD("broan","0x40 message");
			//send({0x41, 0x00, 0x50, 0x00});
			break;
		}
		case 0x21:
		{
			//ESP_LOGD("broan","0x21 response");

			parseBroanFields(message);

			break;
		}
		default:
		{
			// Log unhandled type
			ESP_LOGW("broan", "Unhandled type %02X", type);
			break;
		}
	}
}

void Broan::runRequests()
{
	uint32_t time = millis();

	// Ping the ERV (Is this actually needed?)
	if( next_ping > 0 && millis() > next_ping )
	{
		send({0x04});
		next_ping = 0;

		if( next_query == 0 )
			next_query = millis() + 1000;
	}

	// Request new data
	if( next_query > 0 && time > next_query )
	{
		next_query = time + 5000;
		std::vector<unsigned char> request;
		request.push_back(0x20);
		int count = 0;
		while( count++ < 10 )
		{
			query_cursor++;
			if( query_cursor >= BROAN_NUM_FIELDS)
				query_cursor = 0;

			request.push_back( fields[query_cursor].opcodeHigh );
			request.push_back( fields[query_cursor].opcodeLow );
		}
		send(request);
		//ESP_LOGD("broan", "Sending 0x20 request..." );

	}
}

void Broan::parseBroanFields(const std::vector<uint8_t>& message)
{
    size_t i = 1;

    while (i < message.size())
    {
        uint8_t opcodeHigh = message[i++];
        uint8_t opcodeLow  = message[i++];
		size_t len = message[i++];
		bool bFound = false;

		for( int j=0; j<BROAN_NUM_FIELDS; j++)
		{
			if( fields[j].opcodeHigh == opcodeHigh && fields[j].opcodeLow == opcodeLow )
			{
				uint32_t oldVal = fields[j].value.intVal;
     			for (size_t b = 0; b < len; ++b)
            		fields[j].value.bytes[b] = static_cast<char>(message[i+b]);

				bFound = true;
				if( oldVal == fields[j].value.intVal )
					break;

				switch( fields[j].type )
				{
					case BroanFieldType::Byte:
						ESP_LOGD("broan","%02X%02X is now Byte  %02X", opcodeHigh, opcodeLow, fields[j].value.byteVal );
						break;
					case BroanFieldType::Int:
						ESP_LOGD("broan","%02X%02X is now Int %i", opcodeHigh, opcodeLow, fields[j].value.intVal );
						break;
					case BroanFieldType::Float:
						ESP_LOGD("broan","%02X%02X is now Float %f", opcodeHigh, opcodeLow, fields[j].value.floatVal );
						break;
				}

				break;
			}
		}

		if( !bFound )
			ESP_LOGW("broan","Got unexpected field response for opcode %02X%02X", opcodeHigh, opcodeLow );

        i += len;
    }
}

void Broan::send(const std::vector<uint8_t>& msg)
{
	uint8_t header = 0x01;
	uint8_t alignment = 0x01;
	uint8_t footer = 0x04;
	write(header);
	write(server_address);
	write(my_address);
	write(alignment);
	write((uint8_t)msg.size());
	for (auto b : msg) write(b);
	write(calculate_checksum(my_address, server_address, msg));
	write(footer);
}

uint8_t Broan::calculate_checksum(uint8_t sender, uint8_t receiver, const std::vector<uint8_t>& message)
{
	uint8_t total = 0x01 + sender + receiver + 0x01 + message.size();
	for (uint8_t b : message) total += b;
	return 0xFF & (0 - (total - 1));
}

int Broan::peek_byte()
{
	// *ESPHome's UARTDevice does not provide a real peek; to realign, skip until 0x01 header is found
	// In an advanced version, implement a small local buffer to support peeking functionality
	return read();
}


}
}