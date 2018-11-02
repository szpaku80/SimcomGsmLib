#include <WiFi.h>
#include <SimcomGsmLib.h>
#include <GsmDebugHelpers.h>
#include <OperatorNameHelper.h>
#include <MappingHelpers.h>
#include <SSD1306.h>
#include "Gui.h"
#include "ConnectionDataValidator.h"
#include <Wire.h>

void UpdateBaudRate(int baudRate)
{
	Serial2.end();	
	Serial2.setRxBufferSize(2000);
	Serial2.begin(baudRate, SERIAL_8N1, 14, 12, false);
}
SimcomGsm gsm(Serial2, UpdateBaudRate);

SSD1306 display(188, 4, 15); // 60 or 188

Gui gui(display);
ConnectionDataValidator connectionValidator;
bool justConnectedToModem = true;
void OnLog(const char* gsmLog)
{
	Serial.print("[GSM]");
	Serial.println(gsmLog);
}
int receivedBytes = 0;

void OnDataReceived(uint8_t mux, FixedStringBase &data)
{
	if (connectionValidator.HasError())
	{
		return;
	}
	receivedBytes += data.length();
//	Serial.printf(" #####  Data received: %s\n", data.c_str());
	for (int i = 0; i < data.length(); i++)
	{
		connectionValidator.ValidateIncomingByte(data[i], i, receivedBytes);
	}
}

void setup()
{
	pinMode(16, OUTPUT);
	pinMode(2, OUTPUT);

	digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
	delay(500);
	digitalWrite(16, HIGH);

	gsm.Logger().LogAtCommands = true;
	gsm.Logger().OnLog(OnLog);
	Serial.begin(500000);
	Wire.begin(4, 15);
	for (int i = 0; i < 255; i++)
	{
		Wire.beginTransmission(i);
		if (Wire.endTransmission() == 0)
		{
			Serial.printf("Found I2C device at: %d\n", i);
		}
	}

	gui.init();
	gsm.OnDataReceived(OnDataReceived);
}

void loop()
{
	gui.Clear();
	

	if (connectionValidator.HasError())
	{
		auto error = connectionValidator.GetError();
		gui.DisplayError(error);
		delay(500);
		return;
	}


	if (!gsm.EnsureModemConnected(460800))
	{
		FixedString20 error = "No shield";
		gui.DisplayError(error);
		delay(200);
		return;
	}

	if (justConnectedToModem)
	{
		justConnectedToModem = false;

		FixedString20 info = "Restart modem";
		gui.DisplayError(info);
		gsm.FlightModeOn();
		gsm.FlightModeOff();
		gui.Clear();
	}

	SimState simStatus;
	if (gsm.GetSimStatus(simStatus) == AtResultType::Success)
	{
		if (simStatus != SimState::Ok)
		{
			gui.DisplaySimError(simStatus);
			delay(1000);
			return;
		}
	}

	int16_t signalQuality;
	BatteryStatus batteryInfo;
	FixedString20 operatorName;
	IncomingCallInfo callInfo;
	GsmRegistrationState gsmRegStatus;
	GsmIp ipAddress;
	SimcomIpState ipStatus;

	if (gsm.GetSignalQuality(signalQuality) == AtResultType::Timeout)
	{
		return;
	}
	if (gsm.GetBatteryStatus(batteryInfo) == AtResultType::Timeout)
	{
		return;
	}
	if (OperatorNameHelper::GetRealOperatorName(gsm, operatorName) == AtResultType::Timeout)
	{
		return;
	}
	if (gsm.GetIncomingCall(callInfo) == AtResultType::Timeout)
	{
		return;
	}
	if (gsm.GetIpState(ipStatus) == AtResultType::Timeout)
	{
		return;
	}

	auto getIpResult = gsm.GetIpAddress(ipAddress);
	if (getIpResult == AtResultType::Timeout)
	{
		return;
	}
	bool hasIpAddress = getIpResult == AtResultType::Success;	
	if (gsm.GetRegistrationStatus(gsmRegStatus) == AtResultType::Timeout)
	{
		return;
	}

	bool hasCipmux;
	if (gsm.GetCipmux(hasCipmux) == AtResultType::Success)
	{
		if (!hasCipmux)
		{
			Serial.println("Cipmux disabled, attempting to enable");
			if (gsm.SetCipmux(true) == AtResultType::Error)
			{
				Serial.println("Failed to set cipmux");
				gsm.Cipshut();
				gsm.SetCipmux(true);
				
			}
		}
	}

	bool hasManualRxGet;
	if (gsm.GetRxMode(hasManualRxGet) == AtResultType::Success)
	{
		if (!hasManualRxGet)
		{
			Serial.println("Manual RxGet disabled, attempting to enable");
			if (gsm.SetRxMode(true) == AtResultType::Error)
			{
				Serial.println("Failed to set rx get");
				gsm.Cipshut();
				gsm.SetCipmux(true);
			}
		}
	}


	if (!hasIpAddress)
	{
		gsm.Cipshut();
	}

	if (gsmRegStatus == GsmRegistrationState::Roaming || gsmRegStatus == GsmRegistrationState::HomeNetwork)
	{
		if (!hasIpAddress)
		{
			gui.Clear();
			display.setFont(ArialMT_Plain_10);
			gsm.SetApn("virgin-internet", "", "");
			display.drawString(0, 0, "Connecting to gprs..");
			display.display();
			delay(400);
			gsm.AttachGprs();
		}
	}
	

	ConnectionInfo info;

	if (hasIpAddress)
	{
		if (gsm.GetConnectionInfo(0, info) == AtResultType::Success)
		{
			Serial.printf("Conn info: bearer=%d, ctx=%d,proto=%s endpoint = [%s:%d] state = [%s]\n",
				info.Mux, info.Bearer,
				ProtocolToStr(info.Protocol), info.RemoteAddress.ToString().c_str(),

				info.Port, ConnectionStateToStr(info.State));

			if (info.State == ConnectionState::Closed || info.State == ConnectionState::Initial)
			{
				Serial.printf("Trying to connect...\n");
				receivedBytes = 0;
				connectionValidator.SetJustConnected();
				gsm.BeginConnect(ProtocolType::Tcp, 0, "conti.ml", 12668);
			}
		}
		else
		{
			Serial.println("Connection info failed");
		}
	}

	gui.drawBattery(batteryInfo.Percent, batteryInfo.Voltage);
	gui.drawGsmInfo(signalQuality, gsmRegStatus, operatorName);

	

	if (hasIpAddress)
	{
		gui.DisplayIp(ipAddress);
	}
	gui.DisplayBlinkIndicator();

	if (hasIpAddress)
	{
		ReadDataFromConnection();
	}

	if (hasIpAddress)
	{
		FixedString50 receivedBytesStr;
		receivedBytesStr.appendFormat("received: %d b", receivedBytes);
		display.setColor(OLEDDISPLAY_COLOR::WHITE);

		display.drawString(0, 64 - 22, ConnectionStateToStr(info.State));
		display.drawString(0, 64 - 12, receivedBytesStr.c_str());
	}

	if (gsm.GarbageOnSerialDetected())
	{
		FixedString20 error("UART garbage !!!");
		gui.DrawFramePopup(error, 40, 5);
		Serial.println("Draw garbage detected pupup");
	}

	gui.DisplayIncomingCall(callInfo);

	
	
	display.display();

	gsm.wait(1000);
}

void ReadDataFromConnection()
{
	FixedString200 buffer;
	while (gsm.Read(0, buffer) == AtResultType::Success)
	{
		if (buffer.length() == 0)
		{
			return;
		}
		OnDataReceived(0, buffer);
		buffer.clear();
	}
}
