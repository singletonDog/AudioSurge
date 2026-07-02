#pragma once

#include "wasapi_output.h"
#include <mmdeviceapi.h>
#include <string>
#include <vector>

#ifndef INITGUID
#endif

std::vector<DeviceInfo> EnumerateRenderDevices();
std::string GetDefaultRenderDeviceId();
std::string GetDeviceId(IMMDevice* device);
std::string GetDeviceFriendlyName(IMMDevice* device);
