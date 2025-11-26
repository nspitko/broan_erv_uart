#include "broan.h"

namespace esphome {
namespace broan { // Change 'broan' to match your component name


void BroanComponent::setup()
{
	//uart::UARTDevice::setup();
	Component::setup();
	//esp_log_level_set("broan", ESP_LOG_DEBUG);

	m_vecHeader.reserve(5);

	for( int i=0; i<BroanField::MAX_FIELDS; i++ )
		m_vecFields[i].markDirty();

	// DEFL: Copied from modbus
  	if (this->flow_control_pin_ != nullptr) {
    	this->flow_control_pin_->setup();
  	}
}


void BroanComponent::loop()
{
	while ( true )
	{
		if( !readHeader() ) break;
		bool bRead = readMessage();
		if( !bRead ) break;
	}

	replyIfAllowed();

	runTasks();
}

void BroanComponent::dump_config()
{
	LOG_PIN("broan", "Flow control pin (from uart): ", this->flow_control_pin_);
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
	if ( m_vecHeader[1] > 32 || m_vecHeader[2] > 32 )
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

	queueMessage( message );
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

	for (uint8_t i = 0; i < len; i++)
	{
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

void esp_log_vector_hex(const char* tag, const std::vector<uint8_t>& message) {
    if (message.empty()) {
        ESP_LOGW(tag, "Message vector is empty");
        return;
    }
    std::string hex_string;
    for (size_t i = 0; i < message.size(); ++i) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", message[i]);
        hex_string += buf;
        // Optional: newline every 16 bytes for readability
        if ((i + 1) % 16 == 0) {
            ESP_LOGW(tag, "%s", hex_string.c_str());
            hex_string.clear();
        }
    }
    if (!hex_string.empty()) {
        ESP_LOGW(tag, "%s", hex_string.c_str());
    }
}

void BroanComponent::handleMessage(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message)
{
	if( target == m_nServerAddress )
	{
		if( message[0] == 0x03 )
			m_bWaitForRemote = false;
	}
#ifndef LISTEN_ONLY
	if (target != m_nClientAddress) return;
#endif

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
			// Flow control
			m_nLastHadControl = millis();
			m_bHaveControl = true;
			m_bExpectingReply = false;
			// ERV won't re-ping us if we drop, so just assume if we're getting flow
			// control messages it's ready for us to start feeding it data.
			m_bERVReady = true;

			// Ack that we have control. We'll send any queued messages then release with 0x04
			send({ 0x05 });
			//ESP_LOGD("broan","Got flow control");
			break;
		}
		case 0x05:
			// ERV has confirmed it has control, no-op
			break;

		case 0x41:
		{
			// set register ACK, mark all fields dirty
			for( int i=1; i<message.size(); i+=2)
			{
				BroanField_t *pField = lookupField(message[i], message[i+1]);
				if( !pField )
				{
					ESP_LOGW("broan", "Got write response for unknown field %02X %02X", message[i], message[i+1]);
					continue;
				}
				pField->markDirty();
			}
			m_bExpectingReply = false;


			break;
		}
		case 0x21:
		{
			// Request register response
			parseBroanFields(message);
			m_bExpectingReply = false;

			break;
		}
#ifdef LISTEN_ONLY
		case 0x20:
			break;
#endif
		default:
		{
			// Log unhandled m_nType
			ESP_LOGW("broan", "Unhandled m_nType %02X", m_nType);
			esp_log_vector_hex("broan", message );
			break;
		}
	}
}

void BroanComponent::replyIfAllowed()
{
	uint32_t time = millis();
	if( m_nLastHadControl + CONTROL_TIMEOUT < time )
	{
		ESP_LOGW("broan","ERV has not yielded control in over %ims, communication has likely failed. Please restart the device.", CONTROL_TIMEOUT);
		m_bERVReady = false;
		m_nLastHadControl = time;
	}

	if( !m_bHaveControl || m_bExpectingReply )
		return;

	if( m_vecSendQueue.size() > 0 )
	{
		send( m_vecSendQueue.front() );
		m_vecSendQueue.pop_front();
		m_bExpectingReply = true;
		m_bHaveSentMessage = true;
		return;
	}

	if( m_bHaveControl && !m_bExpectingReply && m_vecSendQueue.size() == 0 )
	{
		// Release control.
		send( { 0x04 } );
		m_bHaveControl = false;
		m_bERVReady = true;
		m_bHaveSentMessage = false;
		return;
	}

}

void BroanComponent::queueMessage(std::vector<uint8_t>& message)
{
	if( m_vecSendQueue.size() > 20 )
	{
		ESP_LOGW("broan","Dropping queued message: Stack is full. (Tried to queue %02X)",message[0]);
		return;
	}
	m_vecSendQueue.push_back(message);
}


void BroanComponent::parseBroanFields(const std::vector<uint8_t>& message)
{
    size_t i = 1;
	bool bPublish = false;

    while (i < message.size())
    {
        uint8_t nOpcodeHigh = message[i++];
        uint8_t nOpcodeLow  = message[i++];
		size_t len = message[i++];
		uint32_t nDataPos = i;

		i += len;

		uint32_t unField = lookupFieldIndex(nOpcodeHigh, nOpcodeLow);
		if( unField == INVALID_FIELD )
			continue;

		BroanField_t *pField = &m_vecFields[unField];
		if( !pField )
		{
			handleUnknownField(nOpcodeHigh, nOpcodeLow, len, nDataPos, message);
			continue;
		}

		uint32_t oldVal = pField->m_value.m_nValue;
		for (size_t b = 0; b < len; ++b)
			pField->m_value.m_rgBytes[b] = static_cast<char>(message[nDataPos+b]);
	
		if( oldVal == pField->m_value.m_nValue )
			continue;

		switch(unField)
		{
#ifdef USE_SELECT
			case BroanField::FanMode:
			{
				if( !fan_mode_select_ )
					continue;

				std::string strMode;
				switch( pField->m_value.m_chValue )
				{
					case 0x02: strMode = "ovr"; break;
					case 0x08: strMode = "int"; break;
					case 0x09: strMode = "min"; break;
					case 0x0a: strMode = "max"; break;
					case 0x0b: strMode = "manual"; break;
					case 0x0c: strMode = "turbo"; break;
					case 0x0d: strMode = "humidity"; break;

					default: strMode = "off"; break;
				}

				fan_mode_select_->publish_state( strMode );
			}
			break;
#endif
#ifdef USE_SENSOR
			case BroanField::Wattage:
				if( !power_sensor_ )
					continue;

				power_sensor_->publish_state(pField->m_value.m_flValue);
			break;

			case BroanField::FilterLife:
				if( !filter_life_sensor_ )
					continue;

				// Seconds -> Days
				filter_life_sensor_->publish_state(pField->m_value.m_nValue / ( 60 * 60 * 24 ) );
			break;

			case BroanField::TemperatureIn:
				if( !temperature_sensor_ )
					continue;

				temperature_sensor_->publish_state(pField->m_value.m_flValue);
			break;

			case BroanField::SupplyCFM:
				if( !supply_cfm_sensor_ )
					continue;

				supply_cfm_sensor_->publish_state(pField->m_value.m_flValue);
			break;

			case BroanField::ExhaustCFM:
				if( !exhaust_cfm_sensor_ )
					continue;

				exhaust_cfm_sensor_->publish_state(pField->m_value.m_flValue);
			break;

			case BroanField::SupplyRPM:
				if( !supply_rpm_sensor_ )
					continue;

				supply_rpm_sensor_->publish_state(pField->m_value.m_flValue);
			break;

			case BroanField::ExhaustRPM:
				if( !exhaust_rpm_sensor_ )
					continue;

				exhaust_rpm_sensor_->publish_state(pField->m_value.m_flValue);
			break;
				
			case BroanField::TemperatureOut:
			{
				// @todo: We should stop querying NaN fields...
				if( !temperature_out_sensor_ || std::isnan( pField->m_value.m_flValue ) )
					continue;

				temperature_out_sensor_->publish_state(pField->m_value.m_flValue);		
			}
			break;

#endif	
#ifdef USE_NUMBER
			case BroanField::TargetHumidityA:
				if( !humidity_setpoint_number_ )
					continue;
				humidity_setpoint_number_->publish_state(pField->m_value.m_flValue);
			break;

			// @todo: We don't support unbalanced values here currently....
			case BroanField::CFMIn_Medium:
			{
				if( !fan_speed_number_ )
					continue;

				float flMin = m_vecFields[CFMIn_Min].m_value.m_flValue;
				float flMax = m_vecFields[CFMIn_Max].m_value.m_flValue;
				float flAdjusted = remap( pField->m_value.m_flValue, flMin, flMax, 0.f, 100.f );
				fan_speed_number_->publish_state(flAdjusted);
			}
			break;

			case BroanField::IntModeDuration:
				if( !intermittent_period_number_ )
					continue;

				intermittent_period_number_->publish_state(pField->m_value.m_nValue /* / 1000 */ );
			break;
#endif
#ifdef USE_SWITCH
			case BroanField::HumidityControl:
				if( !humidity_control_switch_ )
						continue;
				
				humidity_control_switch_->publish_state(pField->m_value.m_chValue==1);
			break;
	#endif
		}

		switch( pField->m_nType )
		{
			case BroanFieldType::Byte:
				ESP_LOGD("broan","%02X%02X is now Byte %02X", nOpcodeHigh, nOpcodeLow, pField->m_value.m_chValue );
				break;
			case BroanFieldType::Int:
				ESP_LOGD("broan","%02X%02X is now Int %i", nOpcodeHigh, nOpcodeLow, pField->m_value.m_nValue );
				break;
			case BroanFieldType::Float:
				ESP_LOGD("broan","%02X%02X is now Float %f", nOpcodeHigh, nOpcodeLow, pField->m_value.m_flValue );
				break;
			case BroanFieldType::Void:
				ESP_LOGD("broan","%02X%02X is not set", nOpcodeHigh, nOpcodeLow );
				break;
		}
    }

}

void BroanComponent::handleUnknownField(uint32_t nOpcodeHigh, uint32_t nOpcodeLow, uint8_t len, uint32_t i, const std::vector<uint8_t>& message )
{
#ifdef SCAN_UNKNOWN
	uint16_t kv = ( nOpcodeHigh << 8 ) | nOpcodeLow;
	if( m_vecFieldData.contains( kv ) )
	{
		BroanField_t copy = m_vecFieldData[ kv ];

		for (size_t b = 0; b < len && b < 4; ++b)
			m_vecFieldData[kv].m_value.m_rgBytes[b] = static_cast<char>(message[i+b]);

		if( m_vecFieldData[kv].m_value.m_nValue != copy.m_value.m_nValue )
		{


			if( len == 4)
				ESP_LOGD("broan","%02X%02X field is unmapped. Value: %f / %i -->  %f / %i", nOpcodeHigh, nOpcodeLow,
					copy.m_value.m_flValue, copy.m_value.m_nValue,
					m_vecFieldData[kv].m_value.m_flValue, m_vecFieldData[kv].m_value.m_nValue ) ;
			else if (len == 1)
				ESP_LOGD("broan","%02X%02X field is unmapped. Value: %f / %i -->  %f / %i", nOpcodeHigh, nOpcodeLow,
					copy.m_value.m_flValue, copy.m_value.m_nValue,
					m_vecFieldData[kv].m_value.m_flValue, m_vecFieldData[kv].m_value.m_nValue ) ;
		}
	}
	else
#endif
	{
		BroanField_t newField;
		newField.m_nOpcodeHigh = nOpcodeHigh;
		newField.m_nOpcodeLow = nOpcodeLow;
		newField.m_nType = len == 4 ? BroanFieldType::Float : BroanFieldType::Byte;

		for (size_t b = 0; b < len && b < 4; ++b)
			newField.m_value.m_rgBytes[b] = static_cast<char>(message[i+b]);


		if( len == 4)
			ESP_LOGD("broan","%02X%02X field is unmapped. Value: %f / %i", nOpcodeHigh, nOpcodeLow, newField.m_value.m_flValue, newField.m_value.m_nValue );
		else if( len == 1 )
			ESP_LOGD("broan","%02X%02X field is unmapped. Value: %i", nOpcodeHigh, nOpcodeLow, newField.m_value.m_chValue);
		else
			ESP_LOGD("broan","%02X%02X has unhandled field length %i: %s", nOpcodeHigh, nOpcodeLow, len, format_hex_pretty(&message[i], len).c_str() );
#ifdef SCAN_UNKNOWN
		m_vecFieldData[kv] = newField;
#endif

	}

}

void BroanComponent::send(const std::vector<uint8_t>& vecMessage)
{
#ifndef LISTEN_ONLY

	// DEFL Copied from modbus
 	if (this->flow_control_pin_ != nullptr)
	{
    	this->flow_control_pin_->digital_write(true);
		ESP_LOGD("broan","FC high");
	}

	uint8_t header = 0x01;
	uint8_t alignment = 0x01;
	uint8_t footer = 0x04;
	write(header);
	write(m_nServerAddress);
	write(m_nClientAddress);
	write(alignment);
	write((uint8_t)vecMessage.size());
	for (auto b : vecMessage) write(b);
	write(calculateChecksum(m_nClientAddress, m_nServerAddress, vecMessage));
	write(footer);

	flush();

	// DEFL Copied from modbus
 	if (this->flow_control_pin_ != nullptr)
	{
    	this->flow_control_pin_->digital_write(false);
		ESP_LOGD("broan","FC low");
	}

#endif
}

uint8_t BroanComponent::calculateChecksum(uint8_t sender, uint8_t receiver, const std::vector<uint8_t>& message)
{
	uint8_t total = 0x01 + sender + receiver + 0x01 + message.size();
	for (uint8_t b : message) total += b;
	return 0xFF & (0 - (total - 1));
}

BroanField_t* BroanComponent::lookupField( uint8_t opcodeHigh, uint8_t opcodeLow )
{
	uint32_t unField = lookupFieldIndex( opcodeHigh, opcodeLow );
	if( unField != INVALID_FIELD )
	{
		return &m_vecFields[unField];
	}

	return nullptr;
}

uint32_t BroanComponent::lookupFieldIndex( uint8_t opcodeHigh, uint8_t opcodeLow )
{
	for( int i=0; i<BroanField::MAX_FIELDS; i++ )
	{
		BroanField_t *pField = &m_vecFields[i];
		if( pField->m_nOpcodeHigh == opcodeHigh && pField->m_nOpcodeLow == opcodeLow )
			return i;
	}

	return INVALID_FIELD;
}

void BroanComponent::runTasks()
{
	uint32_t time = millis();

	if( m_bERVReady )
	{
		//ESP_LOGD("broan", "Reading values" );

		std::vector<unsigned char> vecRequest;

		int nCount = 0;
		for( int i=0; i<BroanField::MAX_FIELDS && nCount < MAX_REQUEST_SIZE; i++ )
		{
			if( m_vecFields[i].m_unPollRate == UPDATE_RATE_NEVER || time - m_vecFields[i].m_unLastUpdate < m_vecFields[i].m_unPollRate )
				continue;

			nCount++;
			m_vecFields[i].m_unLastUpdate = time;

			if( vecRequest.size() == 0 )
				vecRequest.push_back(0x20);

			vecRequest.push_back( m_vecFields[i].m_nOpcodeHigh );
			vecRequest.push_back( m_vecFields[i].m_nOpcodeLow );
		}

		if( vecRequest.size() > 0 )
		{
			queueMessage(vecRequest);
		}
	}


	if( time - m_unLastHeartbeat > HEARTBEAT_RATE )
	{
		m_unLastHeartbeat = time;
		std::vector<unsigned char> vecRequest;
		vecRequest.push_back(0x40);
		vecRequest.push_back(0x00);
		vecRequest.push_back(0x50);
		vecRequest.push_back(0x00);

		queueMessage(vecRequest);
	}

#ifdef SCAN_UNKNOWN

	if( m_nNextScan == 0 )
		m_nNextScan	= time + 15000;

	if( m_bERVReady && time > m_nNextScan )
	{
		m_nNextScan = time + 100;

		std::vector<unsigned char> vecRequest;
		vecRequest.push_back(0x20);

		for( int i=0; i<15;i++)
		{
			vecRequest.push_back(m_nFieldCursor);
			vecRequest.push_back(m_nGroupCursor);


			if( m_nFieldCursor == 0xFF)
			{

				switch( m_nGroupCursor )
				{
					case 0x20: m_nGroupCursor = 0x21; break;
					case 0x21: m_nGroupCursor = 0x22; break;
					case 0x22: m_nGroupCursor = 0x30; break;
					case 0x30: m_nGroupCursor = 0x40; break;
					case 0x40: m_nGroupCursor = 0x50; break;
					case 0x50: m_nGroupCursor = 0x60; break;
					case 0x60: m_nGroupCursor = 0xE0; break;
					case 0xE0: m_nGroupCursor = 0x20; break;
					//case 0xF0: m_nGroupCursor = 0x20; break;
				}
				//ESP_LOGD("broan","Brute force: Group is now %02X ", m_nGroupCursor );
			}
			m_nFieldCursor++;
		}

		queueMessage(vecRequest);

	}
#endif
}



}
}
