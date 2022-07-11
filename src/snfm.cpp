#include "snfm.h"

bool HasCapability(const DevicesResponse::Device& device, DeviceCapability cap)
{
    const auto& caps = device.capabilities();
    return std::find(caps.begin(), caps.end(), cap) != caps.end();
}