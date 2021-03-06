#pragma once

#include "SimcomAtCommands.h"
#include <FixedString.h>
#include <WString.h>


enum class GsmState
{
	Initial,
	NoShield,
	Initializing,
	SimError,
	SearchingForNetwork,
	RegistrationDenied,
	RegistrationUnknown,
	ConnectingToGprs,
	ConnectedToGprs,
};



class GsmModule
{
	SimcomAtCommands& _gsm;
	bool _justConnectedToModem;
	void ChangeState(GsmState newState)
	{
		if (_state != newState)
		{
			_gsm.Logger().Log(F("State changed %s -> %s"), StateToStr(_state), StateToStr(newState));
		}
		_state = newState;
	}
	GsmState _state;
	bool GetVariablesFromModem();
public:
	GsmState GetState()
	{
		return _state;
	}
	const __FlashStringHelper* StateToStr(GsmState state);
	int GarbageOnSerialDetected();
	int16_t signalQuality;
	BatteryStatus batteryInfo;
	FixedString20 operatorName;
	IncomingCallInfo callInfo;
	GsmRegistrationState gsmRegStatus;
	GsmIp ipAddress;
	SimcomIpState ipStatus;
	SimState simStatus;

	GsmModule(SimcomAtCommands &gsm);
	void Loop();
	void Wait(uint64_t delayInMs)
	{
		_gsm.wait(delayInMs);
	}
};

