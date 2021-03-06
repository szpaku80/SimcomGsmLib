#ifndef _GSMLIBHELPERS_H
#define _GSMLIBHELPERS_H

#include <WString.h>
#include <SimcomGsmTypes.h>

const __FlashStringHelper* IpStatusToStr(SimcomIpState ipStatus);
const __FlashStringHelper* RegStatusToStr(GsmRegistrationState state);
const __FlashStringHelper* ProtocolToStr(ProtocolType protocol);
const __FlashStringHelper* ConnectionStateToStr(ConnectionState state);
void BinaryToString(FixedStringBase&source, FixedStringBase& target);

#endif