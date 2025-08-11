#include "broan.h"

namespace esphome {
namespace broan { // Change 'broan' to match your component name


void BroanComponent::setup()
{
	//uart::UARTDevice::setup();
	Component::setup();
	esp_log_level_set("broan", ESP_LOG_DEBUG);

	m_vecHeader.reserve(5);
}


void BroanComponent::loop()
{
	while ( true )
	{
		if( !readHeader() ) break;
		if( !readMessage() ) break;
	}

	if( !readHeader() )
		runRequests();
}

bool BroanComponent::readHeader()
{
	if( m_bHaveHeader )
	{
		//ESP_LOGD("broan", "Recycling header (good)");
		return true;
	}

	if( available() < 5 )
		return false;

	for (uint8_t i = 0; i < 5; i++) {
		m_vecHeader[i] = read();
		if( i == 0 && m_vecHeader[i] != 0x01 )
		{
			ESP_LOGW("broan", "Alignment: Unexpected %02X in position %i", m_vecHeader[i], i);
			return false;
		}

		if( i == 3 && m_vecHeader[i] != 0x01 )
		{
			ESP_LOGW("broan", "Alignment: Unexpected %02X in position %i", m_vecHeader[i], i);
			return false;
		}
	}

	uint8_t head = m_vecHeader[0];
	if ( m_vecHeader[1] > 32 || m_vecHeader[2] > 32 || m_vecHeader[4] > 0x7F)
	{
		ESP_LOGW("broan", "Alignment: Unexpected %02X %02X %02X %02X %02X",
			m_vecHeader[0], m_vecHeader[1], m_vecHeader[2], m_vecHeader[3], m_vecHeader[4]);
		return false;
	}

	m_bHaveHeader = true;

	return true;
}

void BroanComponent::writeRegisters( const std::vector<BroanField_t> &values )
{
	std::vector<uint8_t> message;


	message.push_back(0x40); // Write

	for( BroanField_t value : values )
	{
		message.push_back( value.m_nOpcodeHigh );
		message.push_back( value.m_nOpcodeLow );
		uint8_t len = value.m_nType == BroanFieldType::Byte ? 0x01 : 0x04;
		message.push_back( len );
		for( int i=0; i<len; i++ )
			message.push_back( value.m_value.m_rgBytes[i] );
	}

	send( message );
}

bool BroanComponent::readMessage()
{
	uint8_t target = m_vecHeader[1];
	uint8_t sender = m_vecHeader[2];
	int len = m_vecHeader[4];

	if( !m_bHaveHeader )
		return false;

	if( available() < len + 2 )
	{
		//ESP_LOGD("broan", "Waiting for rest of packet to show up in buffer (Want %i have %i)", len + 2, available() );
		return false;
	}

	m_bHaveHeader = false;

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
	uint8_t expected_checksum = calculateChecksum(sender, target, message);
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

	handleMessage(sender, target, message);

	return true;
}

void BroanComponent::handleMessage(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message)
{
	if (target != m_nClientAddress) return;
	int m_nType = message[0];
	switch (m_nType)
	{
		case 0x02:
		{
			// Respond to ping
			std::vector<uint8_t> reply = {0x03};
			reply.insert(reply.end(), message.begin() + 1, message.end());
			send(reply);
			ESP_LOGD("broan","0x02 Ping");
			m_bERVReady = true;

			break;
		}
		case 0x04:
		{
			// Heartbeat
			//ESP_LOGD("broan","Got 0x04 heartbeat");
			send({0x05});
			m_nNextPing = millis() + 100 ;
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
			// Log unhandled m_nType
			ESP_LOGW("broan", "Unhandled m_nType %02X", m_nType);
			break;
		}
	}
}

void BroanComponent::runRequests()
{
	uint32_t time = millis();

	// Ping the ERV (Is this actually needed?)
	if( m_nNextPing > 0 && millis() > m_nNextPing )
	{
		send({0x04});
		m_nNextPing = 0;

		if( m_nNextQuery == 0 )
			m_nNextQuery = millis() + 1000;
	}

	// Request new data
	if( m_nNextQuery > 0 && time > m_nNextQuery )
	{
		m_nNextQuery = time + 500;

		m_vecFields[FanMode].m_bStale = true;
		m_vecFields[FanSpeed].m_bStale = true;
		m_vecFields[FanSpeedB].m_bStale = true;

		std::vector<unsigned char> request;
		request.push_back(0x20);
		int count = 0;
		// Only check 10 fields at a time. Not sure if this is actually useful?
		// We don't know the message length limit yet.
		for( int i=0; i<BROAN_NUM_FIELDS; i++ )
		{
			if( !m_vecFields[i].m_bStale )
				continue;

			count++;

			m_vecFields[i].m_bStale = false;

			request.push_back( m_vecFields[i].m_nOpcodeHigh );
			request.push_back( m_vecFields[i].m_nOpcodeLow );
		}

		if( request.size() > 0 )
			send(request);
		//ESP_LOGD("broan", "Sending 0x20 request..." );

	}
}

void BroanComponent::parseBroanFields(const std::vector<uint8_t>& message)
{
    size_t i = 1;
	bool bPublish = false;

    while (i < message.size())
    {
        uint8_t m_nOpcodeHigh = message[i++];
        uint8_t m_nOpcodeLow  = message[i++];
		size_t len = message[i++];
		bool bFound = false;

		for( int j=0; j<BROAN_NUM_FIELDS; j++)
		{
			BroanField_t &field = m_vecFields[j];
			if( field.m_nOpcodeHigh == m_nOpcodeHigh && field.m_nOpcodeLow == m_nOpcodeLow )
			{
				uint32_t oldVal = field.m_value.m_nValue;
     			for (size_t b = 0; b < len; ++b)
            		field.m_value.m_rgBytes[b] = static_cast<char>(message[i+b]);

				bFound = true;
				if( oldVal == field.m_value.m_nValue )
					break;

				switch(j)
				{
					case BroanField::FanMode:
					{
						std::string strMode;
						switch( field.m_value.m_chValue )
						{
							case 0x09: strMode = "min"; break;
							case 0x0a: strMode = "max"; break;
							case 0x0b: strMode = "manual"; break;
							default: strMode = "off"; break;
						}

						fan_mode_select_->publish_state( strMode );
					}
					break;

					case BroanField::FanSpeed:
					{
						float flAdjusted = remap( field.m_value.m_flValue, 32.f, 175.f, 0.f, 100.f );
						//fan_mode_speed_->publish_state(flAdjusted);
					}
					break;
				}



				switch( field.m_nType )
				{
					case BroanFieldType::Byte:
						ESP_LOGD("broan","%02X%02X is now Byte  %02X", m_nOpcodeHigh, m_nOpcodeLow, m_vecFields[j].m_value.m_chValue );
						break;
					case BroanFieldType::Int:
						ESP_LOGD("broan","%02X%02X is now Int %i", m_nOpcodeHigh, m_nOpcodeLow, m_vecFields[j].m_value.m_nValue );
						break;
					case BroanFieldType::Float:
						ESP_LOGD("broan","%02X%02X is now Float %f", m_nOpcodeHigh, m_nOpcodeLow, m_vecFields[j].m_value.m_flValue );
						break;
				}

				break;
			}
		}

		if( !bFound )
			ESP_LOGW("broan","Got unexpected field response for opcode %02X%02X", m_nOpcodeHigh, m_nOpcodeLow );

        i += len;
    }

	//publishState();
}

void BroanComponent::send(const std::vector<uint8_t>& msg)
{
	uint8_t header = 0x01;
	uint8_t alignment = 0x01;
	uint8_t footer = 0x04;
	write(header);
	write(m_nServerAddress);
	write(m_nClientAddress);
	write(alignment);
	write((uint8_t)msg.size());
	for (auto b : msg) write(b);
	write(calculateChecksum(m_nClientAddress, m_nServerAddress, msg));
	write(footer);
}

uint8_t BroanComponent::calculateChecksum(uint8_t sender, uint8_t receiver, const std::vector<uint8_t>& message)
{
	uint8_t total = 0x01 + sender + receiver + 0x01 + message.size();
	for (uint8_t b : message) total += b;
	return 0xFF & (0 - (total - 1));
}


}
}