/*
* Apple System Management Control (SMC) Tool
* Copyright (C) 2006 devnull
* Portions Copyright (C) 2013 Michael Wilber
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "smc.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFString.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#include <mach/mach_port.h>

/*
 The SMC-related functions for FanSpeed_getData are from
 https://github.com/hholtmann/smcFanControl/tree/master/smc-command
 Some function were modified and the caching system was removed (we
 only check every second anyway.)
 */

io_connect_t g_conn = 0;
OSSpinLock g_keyInfoSpinLock = 0;

UInt32 _strtoul(char *str, int size, int base) {
	UInt32 total = 0;
	int i;
	
	for (i = 0; i < size; i++)
	{
		if (base == 16)
			total += str[i] << (size - 1 - i) * 8;
		else
			total += ((unsigned char) (str[i]) << (size - 1 - i) * 8);
	}
	return total;
}

void _ultostr(char *str, UInt32 val) {
	str[0] = '\0';
	sprintf(str, "%c%c%c%c",
			(unsigned int) val >> 24,
			(unsigned int) val >> 16,
			(unsigned int) val >> 8,
			(unsigned int) val);
}

float _strtof(unsigned char *str, int size, int e) {
	float total = 0;
	int i;
	
	for (i = 0; i < size; i++)
	{
		if (i == (size - 1))
			total += (str[i] & 0xff) >> e;
		else
			total += str[i] << (size - 1 - i) * (8 - e);
	}
	
	total += (str[size-1] & 0x03) * 0.25;
	
	return total;
}

kern_return_t SMCCall2(int index, SMCKeyData_t *inputStructure, SMCKeyData_t *outputStructure,io_connect_t conn) {
	size_t   structureInputSize;
	size_t   structureOutputSize;
	structureInputSize = sizeof(SMCKeyData_t);
	structureOutputSize = sizeof(SMCKeyData_t);
	
	return IOConnectCallStructMethod(conn, index, inputStructure, structureInputSize, outputStructure, &structureOutputSize);
}

kern_return_t SMCGetKeyInfo(UInt32 key, SMCKeyData_keyInfo_t* keyInfo, io_connect_t conn) {
	SMCKeyData_t inputStructure;
	SMCKeyData_t outputStructure;
	kern_return_t result = kIOReturnSuccess;
	
	OSSpinLockLock(&g_keyInfoSpinLock);
	
	memset(&inputStructure, 0, sizeof(inputStructure));
	memset(&outputStructure, 0, sizeof(outputStructure));
	
	inputStructure.key = key;
	inputStructure.data8 = SMC_CMD_READ_KEYINFO;
	
	result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure, conn);
	if (result == kIOReturnSuccess)
		*keyInfo = outputStructure.keyInfo;
	
	OSSpinLockUnlock(&g_keyInfoSpinLock);
	
	return result;
}

kern_return_t SMCReadKey2(UInt32Char_t key, SMCVal_t *val, io_connect_t conn) {
	kern_return_t result;
	SMCKeyData_t  inputStructure;
	SMCKeyData_t  outputStructure;
	
	memset(&inputStructure, 0, sizeof(SMCKeyData_t));
	memset(&outputStructure, 0, sizeof(SMCKeyData_t));
	memset(val, 0, sizeof(SMCVal_t));
	
	inputStructure.key = _strtoul(key, 4, 16);
	sprintf(val->key, key);
	
	result = SMCGetKeyInfo(inputStructure.key, &outputStructure.keyInfo, conn);
	if (result != kIOReturnSuccess)
	{
		return result;
	}
	
	val->dataSize = outputStructure.keyInfo.dataSize;
	_ultostr(val->dataType, outputStructure.keyInfo.dataType);
	inputStructure.keyInfo.dataSize = val->dataSize;
	inputStructure.data8 = SMC_CMD_READ_BYTES;
	
	result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure,conn);
	if (result != kIOReturnSuccess)
	{
		return result;
	}
	
	memcpy(val->bytes, outputStructure.bytes, sizeof(outputStructure.bytes));
	
	return kIOReturnSuccess;
}

kern_return_t SMCReadKey(UInt32Char_t key, SMCVal_t *val) {
	return SMCReadKey2(key, val, g_conn);
}

kern_return_t SMCOpen(io_connect_t *conn) {
	kern_return_t result;
	mach_port_t   masterPort;
	io_iterator_t iterator;
	io_object_t   device;
	
	IOMasterPort(MACH_PORT_NULL, &masterPort);
	
	CFMutableDictionaryRef matchingDictionary = IOServiceMatching("AppleSMC");
	result = IOServiceGetMatchingServices(masterPort, matchingDictionary, &iterator);
	if (result != kIOReturnSuccess)
	{
		/* "Error: IOServiceGetMatchingServices() = %08x\n", result */
		return 1;
	}
	
	device = IOIteratorNext(iterator);
	IOObjectRelease(iterator);
	if (device == 0)
	{
		/* "Error: no SMC found\n" */
		return 1;
	}
	
	result = IOServiceOpen(device, mach_task_self(), 0, conn);
	IOObjectRelease(device);
	if (result != kIOReturnSuccess)
	{
		/* "Error: IOServiceOpen() = %08x\n", result */
		return 1;
	}
	
	return kIOReturnSuccess;
}

void smc_init() {
	SMCOpen(&g_conn);
}

kern_return_t SMCClose(io_connect_t conn) {
	return IOServiceClose(conn);
}

void smc_close() {
	SMCClose(g_conn);
}

float getSmcValue(char *key) {
	smc_init();
	
	SMCVal_t      val;
	UInt32Char_t  key_;
	float out;
	
	snprintf(key_, 5, key);
	SMCReadKey(key_, &val);
	out = ((SInt16)ntohs(*(UInt16*)val.bytes)) / 256.0;
	
	smc_close();
	
	return out;
}

float *getSmcValues(char *keys[], size_t n) {
	smc_init();
	
	SMCVal_t      val;
	UInt32Char_t  key_;
	float *out = malloc(n*sizeof(float));
	
	for (int i = 0; i < n; i++) {
		snprintf(key_, 5, keys[i]);
		SMCReadKey(key_, &val);
		out[i] = ((SInt16)ntohs(*(UInt16*)val.bytes))*1.0 / 256.0;
	}
	
	smc_close();
	
	return out;
}

//void Power_getData(double* level, int* state) {
//	smc_init();
//
//	SMCVal_t      val;
//	UInt32Char_t  key;
//
//	sprintf(key, "PP0R");
//	SMCReadKey(key, &val);
//	*level = ((SInt16)ntohs(*(UInt16*)val.bytes)) / 256.0;
//
//	smc_close();
//
//	*state = 0;
//}
//

kern_return_t SMCWriteKey2(SMCVal_t writeVal, io_connect_t conn)
{
	kern_return_t result;
	SMCKeyData_t  inputStructure;
	SMCKeyData_t  outputStructure;
	
	SMCVal_t      readVal;
	
	result = SMCReadKey2(writeVal.key, &readVal,conn);
	if (result != kIOReturnSuccess)
		return result;
	
	if (readVal.dataSize != writeVal.dataSize)
		return kIOReturnError;
	
	memset(&inputStructure, 0, sizeof(SMCKeyData_t));
	memset(&outputStructure, 0, sizeof(SMCKeyData_t));
	
	inputStructure.key = _strtoul(writeVal.key, 4, 16);
	inputStructure.data8 = SMC_CMD_WRITE_BYTES;
	inputStructure.keyInfo.dataSize = writeVal.dataSize;
	memcpy(inputStructure.bytes, writeVal.bytes, sizeof(writeVal.bytes));
	result = SMCCall2(KERNEL_INDEX_SMC, &inputStructure, &outputStructure,conn);
	
	if (result != kIOReturnSuccess)
		return result;
	return kIOReturnSuccess;
}

kern_return_t SMCWriteKey(SMCVal_t writeVal)
{
	return SMCWriteKey2(writeVal, g_conn);
}

//void allFans() {
//	SMCVal_t val_force = {
//		.key = "FS! ",
//		.dataSize = 2,
//		.dataType = {0},
//		.bytes = "\x00\x03",
//	};
//
//	SMCVal_t val_auto = {
//		.key = "FS! ",
//		.dataSize = 2,
//		.dataType = {0},
//		.bytes = "\x00\x03",
//	};
//
//	smc_init();
//
//	kern_return_t result;
//	SMCVal_t      val;
//	UInt32Char_t  key;
//	int           totalFans, i;
//
//	result = SMCReadKey("FNum", &val);
//	if (result != kIOReturnSuccess)
//		return;
//	totalFans = _strtoul((char *)val.bytes, val.dataSize, 10);
//
//	SMCWriteKey(val_force);
//	for (i = 0; i < totalFans; i++) {
//		sprintf(key, "F%dMx", i);
//		SMCReadKey(key, &val);
//
//		sprintf(val.key, "F%dTg", i);
//		SMCWriteKey(val);
//	}
//
//	smc_close();
//}

void blinkSIL() {
	smc_init();
	
	SMCVal_t val = {
		.key = "LSOO",
		.dataSize = 1,
		.dataType = "flag",
		.bytes = "\x00",
	};
	
	useconds_t t = 0.1*1000*1000;
	while (TRUE) {
		val.bytes[0] = 1;
		SMCWriteKey(val);
		usleep(t);
		
		val.bytes[0] = 0;
		SMCWriteKey(val);
		usleep(t);
	}
	
//	smc_close();
}

//void f_5500() {
//	SMCVal_t val0 = {
//		.key = "FS! ",
//		.dataSize = 2,
//		.dataType = {0},
//		.bytes = "\x00\x03",
//	};
//	SMCVal_t val1 = {
//		.key = "F0Tg",
//		.dataSize = 2,
//		.dataType = {0},
//		.bytes = "\x55\xf0",
//	};
//
//	smc_init();
//	SMCWriteKey(val0);
//	SMCWriteKey(val1);
//	smc_close();
//}
//
