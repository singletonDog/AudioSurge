#pragma once

#include "wasapi_output.h"
#include <string>
#include <vector>

#ifndef INITGUID
#endif

std::vector<DeviceInfo> EnumerateRenderDevices();
std::string GetDefaultRenderDeviceId();
bool SetDefaultRenderDevice(const std::string& deviceId, std::string* errorMessage = nullptr);
std::string GetDeviceId(IMMDevice* device);
std::string GetDeviceFriendlyName(IMMDevice* device);
