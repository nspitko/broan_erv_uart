#include "broan.h"

namespace esphome {
namespace broan { // Change 'broan' to match your component name


void BroanComponent::setup()
{
	//uart::UARTDevice::setup();
	Component::setup();
	//esp_log_level_set("broan", ESP_LOG_DEBUG);

	for( int i=0; i<BroanField::MAX_FIELDS; i++ )
		m_vecFields[i].markDirty();

#ifdef USE_SENSOR
	// Seed the flow/speed sensors to 0 so they read 0 (not "unknown") after a
	// boot that lands in an intermittent-off window, where the ERV stops
	// returning the CFM/RPM registers. Real polls overwrite this immediately.
	if( supply_cfm_sensor_ )  supply_cfm_sensor_->publish_state(0);
	if( exhaust_cfm_sensor_ ) exhaust_cfm_sensor_->publish_state(0);
	if( supply_rpm_sensor_ )  supply_rpm_sensor_->publish_state(0);
	if( exhaust_rpm_sensor_ ) exhaust_rpm_sensor_->publish_state(0);
#endif

  	if(flow_control_pin_)
    	this->flow_control_pin_->setup();
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
	ESP_LOGCONFIG("broan", "Broan:");
	if(flow_control_pin_)
	{
		char buffer[255];
		this->flow_control_pin_->dump_summary( buffer, 255 );
		ESP_LOGCONFIG("broan", "Flow Control Pin: %s", buffer );
	}
}

float BroanComponent::get_setup_priority() const
{
  // After UART bus
  return setup_priority::BUS - 1.0f;
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

bool BroanComponent::writeRegisters( const std::vector<BroanField_t> &values )
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

	return queueMessage( message );
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

void BroanComponent::handleMessage(uint8_t sender, uint8_t target, const std::vector<uint8_t>& message)
{
	// A zero-length payload can pass the checksum; don't index into it.
	if( message.empty() )
		return;

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
			// (i+1 bound: an even-sized payload would otherwise read one past the end)
			for( size_t i=1; i+1<message.size(); i+=2)
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
			ESP_LOGW("broan", "%s", format_hex_pretty(message).c_str());
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

bool BroanComponent::queueMessage(std::vector<uint8_t>& message)
{
	if( m_vecSendQueue.size() > 20 )
	{
		ESP_LOGW("broan","Dropping queued message: Stack is full. (Tried to queue %02X)",message[0]);
		return false;
	}
	m_vecSendQueue.push_back(message);
	return true;
}


// Decode the raw fault register (17 00) into a human-readable status using the
// service-manual E/W code table. ASSUMES the register holds the printed E/W code
// number; 0xFFFFFFFF (and 0) = no fault. Encoding is unverified until the first
// real fault — the raw fault_code sensor is kept for exactly that check.
static std::string decodeBroanFault( uint32_t code )
{
	switch( code )
	{
		case 0xFFFFFFFFu:
		case 0:  return "OK";
		// Dampers
		case 1:  return "E01 Supply damper range";
		case 2:  return "E02 Supply damper timeout";
		case 3:  return "E03 Supply damper";
		case 5:  return "E05 Exhaust damper range";
		case 6:  return "E06 Exhaust damper timeout";
		case 7:  return "E07 Exhaust damper";
		case 9:  return "E09 Recirculation damper range";
		case 10: return "E10 Recirculation damper timeout";
		case 11: return "E11 Recirculation damper";
		// Airflow (E and W share these numbers + descriptions)
		case 22: return "E22 Supply airflow";
		case 32: return "E32 Exhaust airflow";
		// Supply motor
		case 23: return "E23 Supply motor over-current";
		case 24: return "E24 Supply motor over-voltage";
		case 25: return "E25 Supply motor under-voltage";
		case 26: return "E26 Supply motor over-temp";
		case 27: return "E27 Supply motor foc duration";
		case 28: return "E28 Supply motor speed feedback";
		case 29: return "E29 Supply motor startup";
		// Exhaust motor
		case 33: return "E33 Exhaust motor over-current";
		case 34: return "E34 Exhaust motor over-voltage";
		case 35: return "E35 Exhaust motor under-voltage";
		case 36: return "E36 Exhaust motor over-temp";
		case 37: return "E37 Exhaust motor foc duration";
		case 38: return "E38 Exhaust motor speed feedback";
		case 39: return "E39 Exhaust motor startup";
		// Thermistors / board
		case 40: return "E40 Outside air thermistor";
		case 41: return "E41 Distribution air thermistor";
		case 42: return "E42 PCBA thermistor";
		case 43: return "E43 PCBA over-temp";
		// Wall control
		case 50: return "E50 Wall control comms lost";
		case 51: return "E51 Wall control sensor";
		// Protection / warning-only numbers
		case 60: return "E60 Protection mode";
		case 52: return "W52 Initial setting incomplete";
		case 61: return "W61 Electronics overheating (protection)";
	}
	char buf[32];
	snprintf( buf, sizeof(buf), "Fault code %u", (unsigned)code );
	return buf;
}

// Decode the warning register (1A 00) — same numeric space as the fault codes
// but W-prefixed on the unit's display. Confirmed 2026-07-01 by forcing W22 +
// W32 (blocked airflow): the register alternates through concurrent warnings.
static std::string decodeBroanWarning( uint32_t code )
{
	switch( code )
	{
		case 0xFFFFFFFFu:
		case 0:  return "OK";
		case 22: return "W22 Supply airflow";
		case 32: return "W32 Exhaust airflow";
		case 40: return "W40 Outside air thermistor";
		case 52: return "W52 Initial setting incomplete";
		case 61: return "W61 Electronics overheating (protection)";
	}
	char buf[32];
	snprintf( buf, sizeof(buf), "Warning code %u", (unsigned)code );
	return buf;
}

// Decode the executing-airflow-state register (07 20), mapped 2026-07-02 by
// mode-cycling against live CFM: 0 = fans idle, nonzero = air actually moving,
// value = the running profile. Min/smart/recirculate values not yet observed —
// they fall through to "Running (code N)"; extend this table as HA history
// captures them (the raw Active Mode Code sensor keeps the number).
static std::string decodeBroanActiveMode( uint8_t code )
{
	switch( code )
	{
		case 0: return "Idle";
		case 2: return "Max";
		case 3: return "Turbo";
		case 4: return "Intermittent";
	}
	char buf[24];
	snprintf( buf, sizeof(buf), "Running (code %u)", (unsigned)code );
	return buf;
}

void BroanComponent::parseBroanFields(const std::vector<uint8_t>& message)
{
    size_t i = 1;
	bool bPublish = false;

    // Bounds: the checksum is a plain 8-bit sum over the received bytes, so it
    // says nothing about the inner TLV structure being sane — validate every
    // triplet header and its claimed length before touching the payload.
    while (i + 3 <= message.size())
    {
        uint8_t nOpcodeHigh = message[i++];
        uint8_t nOpcodeLow  = message[i++];
		size_t len = message[i++];
		uint32_t nDataPos = i;

		if( len > message.size() - nDataPos )
		{
			ESP_LOGW("broan", "Truncated field %02X%02X: len %u exceeds payload; dropping rest of frame",
				nOpcodeHigh, nOpcodeLow, (unsigned)len);
			break;
		}

		i += len;

		// String identity registers (02 00 / 02 60) carry ASCII that does not
		// fit the 4-byte field union — handle + skip them before the copy below.
		if( ( nOpcodeHigh == 0x02 && nOpcodeLow == 0x00 ) ||
		    ( nOpcodeHigh == 0x02 && nOpcodeLow == 0x60 ) )
		{
#ifdef USE_TEXT_SENSOR
			// data() + offset stays valid at len==0 (unlike &message[nDataPos]);
			// skip the re-publish when unchanged — these registers never change,
			// and TextSensor::publish_state has no dedup of its own.
			std::string s( reinterpret_cast<const char*>(message.data() + nDataPos), len );
			if( nOpcodeLow == 0x00 && firmware_text_sensor_ &&
				( !firmware_text_sensor_->has_state() || firmware_text_sensor_->state != s ) )
				firmware_text_sensor_->publish_state( s );
			if( nOpcodeLow == 0x60 && model_text_sensor_ &&
				( !model_text_sensor_->has_state() || model_text_sensor_->state != s ) )
				model_text_sensor_->publish_state( s );
#endif
			continue;
		}

		// 3-byte version registers (01 00 firmware, 01 60 hardware). Bytes are
		// PLAIN DECIMAL, confirmed 2026-07-02 against the unit's boot screen
		// (MAJ 001 / MIN 101 / REV 040): 01 65 28 => "1.101.40". (The first
		// guess was BCD/hex "%x" which showed 1.65.28 — wrong.)
		if( ( nOpcodeHigh == 0x01 && nOpcodeLow == 0x00 && len >= 3 ) ||
		    ( nOpcodeHigh == 0x01 && nOpcodeLow == 0x60 && len >= 3 ) )
		{
#ifdef USE_TEXT_SENSOR
			char buf[24];
			snprintf( buf, sizeof(buf), "%u.%u.%u",
				message[nDataPos], message[nDataPos+1], message[nDataPos+2] );
			if( nOpcodeLow == 0x00 && firmware_version_text_sensor_ &&
				( !firmware_version_text_sensor_->has_state() || firmware_version_text_sensor_->state != buf ) )
				firmware_version_text_sensor_->publish_state( buf );
			if( nOpcodeLow == 0x60 && hardware_rev_text_sensor_ &&
				( !hardware_rev_text_sensor_->has_state() || hardware_rev_text_sensor_->state != buf ) )
				hardware_rev_text_sensor_->publish_state( buf );
#endif
			continue;
		}

#ifdef DUMP_GROUP
		// Diagnostic: raw dump of every register in the target group, each pass.
		// Falls through (no continue) so mapped fields in the group still parse —
		// otherwise a 0x50 dump would kill the Power/CFM-min-max entities.
		if( nOpcodeLow == DUMP_GROUP )
		{
			union { uint8_t b[4]; uint32_t u; float f; } v = {};
			for( size_t b = 0; b < len && b < 4; ++b )
				v.b[b] = message[nDataPos + b];
			if( len == 0 )
				ESP_LOGD("broan","DUMP %02X%02X len=0 (void)", nOpcodeHigh, nOpcodeLow);
			else if( len == 1 )
				ESP_LOGD("broan","DUMP %02X%02X byte=%u (0x%02X)", nOpcodeHigh, nOpcodeLow, v.b[0], v.b[0]);
			else if( len == 4 )
				ESP_LOGD("broan","DUMP %02X%02X int=%u float=%f hex=%02X%02X%02X%02X", nOpcodeHigh, nOpcodeLow, v.u, v.f, v.b[0], v.b[1], v.b[2], v.b[3]);
			else
				ESP_LOGD("broan","DUMP %02X%02X len=%u hex=%s", nOpcodeHigh, nOpcodeLow, (unsigned)len, format_hex_pretty(&message[nDataPos], len).c_str());
		}
#endif

		uint32_t unField = lookupFieldIndex(nOpcodeHigh, nOpcodeLow);
		if( unField == INVALID_FIELD )
		{
			// Log/track registers not in the known map so the
			// SCAN_UNKNOWN dump actually surfaces them (was previously dropped).
			handleUnknownField(nOpcodeHigh, nOpcodeLow, len, nDataPos, message);
			continue;
		}

		BroanField_t *pField = &m_vecFields[unField];

		uint32_t oldVal = pField->m_value.m_nValue;
		// b < 4: never let a wire-supplied len overflow the 4-byte union into
		// the poll-rate/last-update members behind it.
		for (size_t b = 0; b < len && b < 4; ++b)
			pField->m_value.m_rgBytes[b] = static_cast<char>(message[nDataPos+b]);

		// If we just wrote this field, ignore read-backs that don't yet reflect
		// our value (stale in-flight reads) until it matches or the window ends —
		// otherwise the HA entity flips back to the old value momentarily.
		{
			auto itPend = m_pendingWrites.find( unField );
			if( itPend != m_pendingWrites.end() )
			{
				bool expired = (int32_t)( millis() - itPend->second.expiry ) >= 0;
				if( !expired && pField->m_value.m_nValue != itPend->second.value )
				{
					pField->m_value.m_nValue = oldVal;  // drop this stale read
					continue;
				}
				m_pendingWrites.erase( itPend );  // confirmed (or gave up)
			}
		}

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
					case BroanFanMode::Ovr: strMode = "ovr"; break;
					case BroanFanMode::Intermittent: strMode = "int"; break;
					case BroanFanMode::Min: strMode = "min"; break;
					case BroanFanMode::Max: strMode = "max"; break;
					case BroanFanMode::Manual: strMode = "manual"; break;
					case BroanFanMode::Turbo: strMode = "turbo"; break;
					case BroanFanMode::Humidity: strMode = "humidity"; break;
					case BroanFanMode::Recirculate: strMode = "recirculate"; break;
					case BroanFanMode::Smart: strMode = "smart"; break;
					case BroanFanMode::Away: strMode = "away"; break;

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

			case BroanField::Uptime:
				if( !uptime_sensor_ )
					continue;

				uptime_sensor_->publish_state(pField->m_value.m_nValue);
			break;

			// Diagnostic 0xE0 fields — unidentified; expose raw floats so HA
			// history can reveal whether they track temp / humidity / runtime.
			case BroanField::UnknownC:  // 07E0
				if( aux_07e0_sensor_ )
					aux_07e0_sensor_->publish_state(pField->m_value.m_flValue);
			break;
			case BroanField::UnknownA:  // 08E0
				if( aux_08e0_sensor_ )
					aux_08e0_sensor_->publish_state(pField->m_value.m_flValue);
			break;
			case BroanField::UnknownB:  // 09E0
				if( aux_09e0_sensor_ )
					aux_09e0_sensor_->publish_state(pField->m_value.m_flValue);
			break;

			// Read-only diagnostic: fault code. The wire idles at 0xFFFFFFFF
			// (= no fault), which a float sensor would render as 4294967296 —
			// publish 0 for the idle sentinel so the HA graph stays usable.
			case BroanField::FaultCode:  // 17 00
				if( fault_code_sensor_ )
					fault_code_sensor_->publish_state(
						pField->m_value.m_nValue == 0xFFFFFFFFu ? 0 : pField->m_value.m_nValue );
#ifdef USE_TEXT_SENSOR
				if( fault_status_text_sensor_ )
					fault_status_text_sensor_->publish_state( decodeBroanFault(pField->m_value.m_nValue) );
#endif
			break;

			// Warning register (1A 00): idle 0xFFFFFFFF published as 0, like the
			// fault code. With multiple warnings active it cycles between their
			// codes every poll, so HA history shows all of them.
			case BroanField::WarningCode:
			{
				uint32_t code = pField->m_value.m_nValue;
				if( warning_code_sensor_ )
					warning_code_sensor_->publish_state( code == 0xFFFFFFFFu ? 0 : code );
#ifdef USE_TEXT_SENSOR
				if( warning_status_text_sensor_ )
					warning_status_text_sensor_->publish_state( decodeBroanWarning(code) );
#endif
			}
			break;

			// Base fan mode (02 20): the BroanFanMode enum value the unit falls
			// back to after a turbo/ovr overlay expires. Raw code, diagnostic.
			case BroanField::BaseModeCode:
				if( base_mode_code_sensor_ )
					base_mode_code_sensor_->publish_state(pField->m_value.m_chValue);
			break;

			// Executing airflow state (07 20): the fans-running source of truth.
			// 0 = idle, nonzero = air moving (value = running profile).
			case BroanField::ActiveModeCode:
			{
				uint8_t code = pField->m_value.m_chValue;
				if( active_mode_code_sensor_ )
					active_mode_code_sensor_->publish_state(code);
#ifdef USE_BINARY_SENSOR
				if( fans_running_binary_sensor_ )
					fans_running_binary_sensor_->publish_state(code != 0);
#endif
#ifdef USE_TEXT_SENSOR
				if( active_mode_text_sensor_ )
					active_mode_text_sensor_->publish_state( decodeBroanActiveMode(code) );
#endif
			}
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

			// Fan Speed % depends on three registers (Medium value against the
			// Min/Max range) that can arrive in any order — recompute whenever
			// any of them lands; publishFanSpeed() no-ops until all are sane.
			// (Previously this remapped on Medium alone: at boot Min/Max were
			// still 0, the divide-by-zero published +inf, and the change-gate
			// above meant it was never corrected.)
			case BroanField::CFMIn_Medium:
			case BroanField::CFMIn_Min:
			case BroanField::CFMIn_Max:
				publishFanSpeed();
			break;

			case BroanField::IntModeDuration:
				if( !intermittent_period_number_ )
					continue;

				// Register is seconds; HA shows minutes.
				intermittent_period_number_->publish_state(pField->m_value.m_nValue / 60 );
			break;

			case BroanField::FilterInterval:
				if( !filter_interval_number_ )
					continue;

				// Seconds -> Days
				filter_interval_number_->publish_state(pField->m_value.m_nValue / ( 60 * 60 * 24 ) );
			break;

			case BroanField::OverrideDuration:
				if( !override_duration_number_ )
					continue;

				// Seconds -> Minutes
				override_duration_number_->publish_state(pField->m_value.m_nValue / 60 );
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

#ifdef USE_NUMBER
		// Push current value back to any CFM setpoint number entity bound to this field.
		{
			auto itNum = cfm_numbers_.find( unField );
			if( itNum != cfm_numbers_.end() )
				itNum->second->publish_state( pField->m_value.m_flValue );
		}
#endif

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

#ifdef USE_NUMBER
void BroanComponent::publishFanSpeed()
{
	if( !fan_speed_number_ )
		return;

	float flMin = m_vecFields[CFMIn_Min].m_value.m_flValue;
	float flMax = m_vecFields[CFMIn_Max].m_value.m_flValue;
	float flMed = m_vecFields[CFMIn_Medium].m_value.m_flValue;

	// All three registers must have real values (they arrive in any order at
	// boot) and the range must be sane before remap — no divide-by-zero infs.
	if( !(flMin > 0.f) || !(flMax > flMin) || !(flMed > 0.f) )
		return;

	float flAdjusted = remap( flMed, flMin, flMax, 0.f, 100.f );
	fan_speed_number_->publish_state( std::max(0.f, std::min(100.f, flAdjusted)) );
}
#endif

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
 	if(flow_control_pin_)
    	flow_control_pin_->digital_write(true);

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

 	if(flow_control_pin_)
    	flow_control_pin_->digital_write(false);
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

#ifdef DUMP_GROUP
		m_nGroupCursor = DUMP_GROUP;  // pin the sweep to one group
#endif

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
					case 0xE0: m_nGroupCursor = 0x00; break;
						case 0x00: m_nGroupCursor = 0x10; break;
						case 0x10: m_nGroupCursor = 0x20; break;
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
