
#include "snfm.h"

int main(int argc, char *argv[])
{
  for (int i = 0; i < argc; i++)
  {
    std::cout << argv[i] << std::endl;
  }

  if (argc < 2)
  {
    std::cout << "bad arg count";
    return 1;
  }

  std::string rom_path = argv[1];
  std::string device_directory = argc > 2 ? argv[2] : "/.sni";

  SNIConnection sni;

  auto device_filter =
      [](const DevicesResponse::Device &device) -> bool
  {
    auto caps = device.capabilities();
    return HasCapability(device, DeviceCapability::MakeDirectory) &&
        HasCapability(device, DeviceCapability::PutFile) &&
        HasCapability(device, DeviceCapability::BootFile);
  };
  std::cout << "refreshing devices" << std::endl;
  sni.refreshDevices(device_filter);
  auto uri = sni.getFirstDeviceUri();
  if (!uri)
  {
    std::cout << "no uri" << std::endl;
    return 1;
  }

  sni.makeDirectory(*uri, device_directory);

  std::optional<std::filesystem::path> put_path = sni.putFile(*uri, rom_path, device_directory);
  if (!put_path)
  {
    std::cout << "didn't put file" << std::endl;
    return 1;
  }
  sni.bootFile(*uri, *put_path);

  return 0;
}
