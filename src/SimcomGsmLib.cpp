#include "SimcomGsmLib.h"
#include "GsmLibHelpers.h"

SimcomGsm::SimcomGsm(Stream& serial, UpdateBaudRateCallback updateBaudRateCallback) :
_serial(serial),
_parser(_dataBuffer, _parserContext, _logger),
IsAsync(false)
{
	lastDataWrite = 0;
	_updateBaudRateCallback = updateBaudRateCallback;
	_currentBaudRate = 0;
}
AtResultType SimcomGsm::GetSimStatus(SimState &simStatus)
{
	SendAt_P(AtCommand::Cpin, F("AT+CPIN?"));
	auto result = PopCommandResult();
	if (result == AtResultType::Success)
	{
		simStatus = _parserContext.SimStatus;
	}
	return result;
}

AtResultType SimcomGsm::GetRegistrationStatus(GsmRegistrationState& registrationStatus)
{	
	SendAt_P(AtCommand::Creg ,F("AT+CREG?"));

	auto result = PopCommandResult();
	if (result == AtResultType::Success)
	{
		registrationStatus = _parserContext.RegistrationStatus;
	}
	return result;
}
AtResultType SimcomGsm::GenericAt(int timeout, const __FlashStringHelper* command, ...)
{	
	_parser.SetCommandType(AtCommand::Generic);
	va_list argptr;
	va_start(argptr, command);

	FixedString200 buffer;
	buffer.appendFormatV(command, argptr);
	_logger.LogAt(F(" => %s"), buffer.c_str());
	_currentCommand = buffer;
	_serial.println(buffer.c_str());
	auto result = PopCommandResult(timeout);

	va_end(argptr);	
	return result;
}
void SimcomGsm::SendAt_P(AtCommand commnd, const __FlashStringHelper* command, ...)
{
	_parser.SetCommandType(commnd);

	va_list argptr;
	va_start(argptr, command);

	FixedString200 buffer;
	buffer.appendFormatV(command, argptr);
	_currentCommand = buffer;
	_logger.LogAt(F(" => %s"), buffer.c_str());
	_serial.println(buffer.c_str());

	va_end(argptr);
}
AtResultType SimcomGsm::GetOperatorName(FixedStringBase &operatorName, bool returnImsi)
{	
	SendAt_P(AtCommand::Cops, F("AT+COPS?"));
	_parserContext.OperatorName = &operatorName;

	auto result = PopCommandResult();
	if (result != AtResultType::Success)
	{
		return result;
	}

	if (_parserContext.IsOperatorNameReturnedInImsiFormat == returnImsi)
	{
		return result;
	}

	auto operatorFormat = returnImsi ? 2 : 0;	
	GenericAt(AT_DEFAULT_TIMEOUT, F("AT+COPS=3,%d"), operatorFormat);
	SendAt_P(AtCommand::Cops, F("AT+COPS?"));
	return PopCommandResult();
}
AtResultType SimcomGsm::FlightModeOn()
{
	return GenericAt(10000, F("AT+CFUN=0"));
}
AtResultType SimcomGsm::FlightModeOff()
{
	return GenericAt(10000, F("AT+CFUN=1"));
}
AtResultType SimcomGsm::SetRegistrationMode(RegistrationMode mode, const char *operatorName)
{	
	auto operatorFormat = _parserContext.IsOperatorNameReturnedInImsiFormat ? 2 : 0;
	// don't need to use AtCommand::Cops here, AT+COPS write variant returns OK/ERROR
	SendAt_P(AtCommand::Generic, F("AT+COPS=%d,%d,\"%s\""), mode, operatorFormat, operatorName);
	auto result = PopCommandResult(120000);
	return result;
}

AtResultType SimcomGsm::GetSignalQuality(int16_t& signalQuality)
{	
	_parserContext.CsqSignalQuality = &signalQuality;
	SendAt_P(AtCommand::Csq, F("AT+CSQ"));
	auto result = PopCommandResult();
	return result;
}

AtResultType SimcomGsm::GetBatteryStatus(BatteryStatus &batteryStatus)
{	
	_parserContext.BatteryInfo = &batteryStatus;
	SendAt_P(AtCommand::Cbc,F("AT+CBC"));
	return PopCommandResult();
}

AtResultType SimcomGsm::GetIpState(SimcomIpState &ipState)
{	
	_parserContext.IpState = &ipState;
	SendAt_P(AtCommand::Cipstatus, F("AT+CIPSTATUS"));
	return PopCommandResult();	
}

AtResultType SimcomGsm::GetIpAddress(GsmIp& ipAddress)
{	
	_parserContext.IpAddress = &ipAddress;
	SendAt_P(AtCommand::Cifsr, F("AT+CIFSR;E0"));
	return PopCommandResult();
}

AtResultType SimcomGsm::GetRxMode(bool& isRxManual)
{	
	SendAt_P(AtCommand::CipRxGet, F("AT+CIPRXGET?"));
	auto result = PopCommandResult();
	if (result == AtResultType::Success)
	{
		isRxManual = _parserContext.IsRxManual;
	}
	return result;
}

AtResultType SimcomGsm::SetRxMode(bool isRxManual)
{	
	SendAt_P(AtCommand::Generic, F("AT+CIPRXGET=%d"), isRxManual ? 1 : 0);

	return PopCommandResult();
}

AtResultType SimcomGsm::GetCipmux(bool& cipmux)
{	
	SendAt_P(AtCommand::Cipmux, F("AT+CIPMUX?"));
	auto result = PopCommandResult();
	if (result == AtResultType::Success)
	{
		cipmux = _parserContext.Cipmux;
	}
	return result;
}

AtResultType SimcomGsm::SetCipmux(bool cipmux)
{	
	SendAt_P(AtCommand::Generic, F("AT+CIPMUX=%d"), cipmux ? 1 : 0);
	_parserContext.Cipmux = cipmux;
	return PopCommandResult();	
}

AtResultType SimcomGsm::GetCipQuickSend(bool& cipqsend)
{
	SendAt_P(AtCommand::CipQsendQuery, F("AT+CIPQSEND?"));
	auto result = PopCommandResult();
	if (result == AtResultType::Success)
	{
		cipqsend = _parserContext.CipQSend;
	}
	return result;
}

AtResultType SimcomGsm::SetSipQuickSend(bool cipqsend)
{
	SendAt_P(AtCommand::Generic, F("AT+CIPQSEND=%d"), cipqsend ? 1: 0);
	return PopCommandResult();
}

AtResultType SimcomGsm::AttachGprs()
{	
	SendAt_P(AtCommand::Generic, F("AT+CIICR"));
	return PopCommandResult(60000);
}

AtResultType SimcomGsm::PopCommandResult()
{
	return PopCommandResult(AT_DEFAULT_TIMEOUT);
}
AtResultType SimcomGsm::PopCommandResult(int timeout)
{
	unsigned long start = millis();
	while(_parser.commandReady == false && (millis()-start) < (unsigned long)timeout)
	{
		if(_serial.available())
		{
			char c = _serial.read();
			_parser.FeedChar(c);
		}
	}

	auto commandResult = _parser.GetAtResultType();
	auto elapsedMs = millis() - start;	
	_logger.LogAt(F("    -- %d ms --"), elapsedMs);
	if (commandResult == AtResultType::Timeout)
	{
		_logger.Log(F("                      --- !!! '%s' - TIMEOUT!!! ---      "), _currentCommand.c_str(), elapsedMs);
	}
	if (commandResult == AtResultType::Error)
	{
		_logger.Log(F("                      --- !!! '%s' - ERROR!!! ---      "), _currentCommand.c_str(), elapsedMs);
	}
	return commandResult;
}
/*
Disables/enables echo on serial port
*/
AtResultType SimcomGsm::SetEcho(bool echoEnabled)
{	
	if (echoEnabled)
	{
		SendAt_P(AtCommand::Generic, F("ATE1"));
	}
	else
	{
		SendAt_P(AtCommand::Generic, F("ATE0"));
	}

	auto r = PopCommandResult();
	delay(100); // without 100ms wait, next command failed, idk wky
	return r;
}

AtResultType SimcomGsm::SetTransparentMode(bool transparentMode)
{	
	SendAt_P(AtCommand::Generic, F("AT+CIPMODE=%d"), transparentMode ? 1:0);
	return PopCommandResult();
}

AtResultType SimcomGsm::SetApn(const char *apnName, const char *username,const char *password )
{	
	SendAt_P(AtCommand::Generic, F("AT+CSTT=\"%s\",\"%s\",\"%s\""), apnName, username, password);
	return PopCommandResult();
}

AtResultType SimcomGsm::At()
{	
	SendAt_P(AtCommand::Generic, F("AT"));
	return PopCommandResult(30);
}

void SimcomGsm::OnDataReceived(DataReceivedCallback onDataReceived)
{
	_parser.OnDataReceived(onDataReceived);
}

AtResultType SimcomGsm::SetBaudRate(uint32_t baud)
{	
	SendAt_P(AtCommand::Generic, F("AT+IPR=%d"), baud);
	return PopCommandResult();
}
bool SimcomGsm::EnsureModemConnected(long requestedBaudRate)
{	
	auto atResult = At();

	int n = 1;
	if (_currentBaudRate != 0)
	{
		while (atResult != AtResultType::Success && n-- > 0)
		{
			delay(20);
			atResult = At();
		}
		if (atResult == AtResultType::Success || atResult == AtResultType::Error)
		{
			return true;
		}
	}
	_currentBaudRate = FindCurrentBaudRate();
	if (_currentBaudRate == 0)
	{
		return false;
	}

	_logger.Log(F("Found baud rate = %d"), _currentBaudRate);
	
	if (SetBaudRate(requestedBaudRate) != AtResultType::Success)
	{
		_logger.Log(F("Failed to update baud rate to %d"), requestedBaudRate);
		return false;
	}
	_currentBaudRate = requestedBaudRate;
	_logger.Log(F("Updated baud rate to = %d"), _currentBaudRate);

	At();

	if (SetEcho(false) != AtResultType::Success)
	{
		_logger.Log(F("Failed to set echo"));
		return false;	
	}
	_parser.ResetUartGarbageDetected();
	return true;	
}
AtResultType SimcomGsm::GetImei(FixedString20 &imei)
{	
	_parserContext.Imei = &imei;
	SendAt_P(AtCommand::Gsn, F("AT+GSN"));
	return PopCommandResult();
}

void SimcomGsm::wait(int ms)
{
	unsigned long start = millis();
	while ((millis() - start) <= (unsigned long)ms)
	{
		if (_serial.available())
		{
			auto c = _serial.read();
			_parser.FeedChar(c);
		}
	}
}

bool SimcomGsm::GarbageOnSerialDetected()
{
	return _parser.GarbageOnSerialDetected();
}

AtResultType SimcomGsm::SendSms(char *number, char *message)
{	
	SendAt_P(AtCommand::Generic, F("AT+CMGS=\"%s\""), number);
	
	uint64_t start = millis();
	// wait for >
	while (_serial.read() != '>')
		if (millis() - start > 200)
			return AtResultType::Error;
	_serial.print(message);
	_serial.print('\x1a');
	return PopCommandResult();
}
AtResultType SimcomGsm::SendUssdWaitResponse(char *ussd, FixedString150& response)
{
	_parserContext.UssdResponse = &response;
	SendAt_P(AtCommand::Cusd, F("AT+CUSD=1,\"%s\""), ussd);
	auto result = PopCommandResult(10000);
	return result;
}

int SimcomGsm::FindCurrentBaudRate()
{
	if (_updateBaudRateCallback == nullptr)
	{
		return 0;
	}
	int i = 0;
	int baudRate = 0;
	do
	{
		baudRate = _defaultBaudRates[i];
		_logger.Log(F("Trying baud rate: %d"), baudRate);
		_updateBaudRateCallback(baudRate);
		if (At() == AtResultType::Success)
		{
			_logger.Log(F(" Found baud rate: %d"), baudRate);
			return baudRate;
		}
		i++;
	} 
	while (_defaultBaudRates[i] != 0);

	return 0;
}

AtResultType SimcomGsm::Cipshut()
{	
	SendAt_P(AtCommand::Cipshut, F("AT+CIPSHUT"));
	return PopCommandResult(20000);
}

AtResultType SimcomGsm::Call(char *number)
{
	SendAt_P(AtCommand::Generic, F("ATD%s;"), number);
	return PopCommandResult();
}

AtResultType SimcomGsm::GetIncomingCall(IncomingCallInfo & callInfo)
{
	_parserContext.CallInfo = &callInfo;
	SendAt_P(AtCommand::Clcc, F("AT+CLCC"));
	const auto result = PopCommandResult();
	return result;
}

AtResultType SimcomGsm::Shutdown()
{	
	SendAt_P(AtCommand::Generic, F("AT+CPOWD=0"));
	return PopCommandResult();
}


AtResultType SimcomGsm::BeginConnect(ProtocolType protocol, uint8_t mux, const char *address, int port)
{	
	SendAt_P(AtCommand::Generic, 
		F("AT+CIPSTART=%d,\"%s\",\"%s\",\"%d\""),
		mux, ProtocolToStr(protocol), address, port);	
	return PopCommandResult(60000);
}

AtResultType SimcomGsm::Read(int mux, FixedStringBase& outputBuffer)
{
	_parserContext.CipRxGetBuffer = &outputBuffer;
	SendAt_P(AtCommand::CipRxGetRead,F("AT+CIPRXGET=2,%d,%d"), mux, outputBuffer.capacity());
	return PopCommandResult();
}

AtResultType SimcomGsm::Send(int mux, FixedStringBase& data)
{
	SendAt_P(AtCommand::CipSend, F("AT+CIPSEND=%d,%d"), mux, data.length());

	const auto promptResult = WaitForPrompt('>', 1000);
	if(promptResult  != AtResultType::Success)
	{
		return promptResult;
	}
	

	_serial.write(data.c_str(), data.length());
	return PopCommandResult();
}

AtResultType SimcomGsm::WaitForPrompt(char prompt, int timeout)
{
	const uint64_t start = millis();
	// wait for >
	while (millis() - start < timeout)
	{
		if (_serial.available())
		{
			const auto receivedChar = _serial.read();
			if (receivedChar == '\r' || receivedChar == '\n')
			{
				continue;
			}
			if (receivedChar == prompt)
			{
				return AtResultType::Success;
			}
			_logger.Log(F("Received wrong prompt: %c, expected = %c"), receivedChar, prompt);
			return AtResultType::Error;
		}
	}
	_logger.Log(F("Timed out waiting for prompt: %c"), prompt);
	return AtResultType::Timeout;
}

AtResultType SimcomGsm::CloseConnection(uint8_t mux)
{	
	SendAt_P(AtCommand::Cipclose, F("AT+CIPCLOSE=%d"), mux);
	return PopCommandResult();
}

AtResultType SimcomGsm::GetConnectionInfo(uint8_t mux, ConnectionInfo &connectionInfo)
{	
	_parserContext.CurrentConnectionInfo = &connectionInfo;
	SendAt_P(AtCommand::CipstatusSingleConnection, F("AT+CIPSTATUS=%d"), mux);
	return PopCommandResult();
}







































