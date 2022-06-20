#include "sni.pb.h"
#include "sni.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <string_view>
#include <functional>
#include <optional>
#include <filesystem>
#include <fstream>

class SNIConnection
{
public:
  SNIConnection(const std::string &address = "localhost:8191")
      : channel_{grpc::CreateChannel(address, grpc::InsecureChannelCredentials())}, devices_stub_{Devices::NewStub(channel_)}, filesystem_stub_{DeviceFilesystem::NewStub(channel_)}
  {
  }

  void refreshDevices(const std::function<bool(const DevicesResponse::Device &)> &filter = nullptr)
  {
    DevicesRequest request;
    DevicesResponse device_response;

    grpc::ClientContext context;

    auto status = devices_stub_->ListDevices(&context, request, &device_response);

    // proto::repeated_field -> std::vector
    devices_ = decltype(devices_){device_response.devices().begin(), device_response.devices().end()};
    // filter out anything that doesn't meet the filter
    // this is not the cleanest way to do this
    if (filter)
    {
      devices_.erase(std::remove_if(devices_.begin(), devices_.end(), std::not_fn(filter)), devices_.end());
    }
  }

  std::optional<std::string> getFirstDeviceUri()
  {
    if (devices_.empty())
    {
      return {};
    }
    return devices_.begin()->uri();
  }

  void makeDirectory(const std::string &uri, const std::string &directory)
  {
    grpc::ClientContext context;
    MakeDirectoryRequest request;
    {
      request.set_uri(uri);
      request.set_path(directory);
    }
    MakeDirectoryResponse response;
    std::cout << request.DebugString() << std::endl;
    filesystem_stub_->MakeDirectory(&context, request, &response);
    std::cout << response.DebugString() << std::endl;

    // this can fail and that's ok
  }

  std::optional<std::filesystem::path> putFile(const std::string &uri, const std::filesystem::path &local_path, const std::filesystem::path device_directory)
  {

    std::filesystem::path filename = local_path.filename();

    std::filesystem::path device_path = device_directory / filename;

    std::ifstream file(local_path, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
      // uh
      std::cout << "couldn't open file " << local_path << std::endl;
      return {};
    }

    PutFileRequest request;
    {
      request.set_uri(uri);
      request.set_path(device_path.generic_string());

      *request.mutable_data() = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

      assert(request.data().size() == std::filesystem::file_size(local_path));
    }

    std::cout << request.uri() << " put file of size " << request.data().size() << " at " << device_path.generic_string() << std::endl;
    PutFileResponse response;
    grpc::ClientContext context;
    filesystem_stub_->PutFile(&context, request, &response);
    std::cout << response.DebugString() << std::endl;
    if (response.path().empty())
    {
      std::cout << "no response path" << std::endl;
      return {};
    }
    return response.path();
  }

  void bootFile(const std::string &uri, const std::filesystem::path &file_path)
  {
    BootFileRequest request;
    request.set_uri(uri);
    request.set_path(file_path.generic_string());
    BootFileResponse response;
    grpc::ClientContext context;
    filesystem_stub_->BootFile(&context, request, &response);
    std::cout << response.DebugString() << std::endl;
  }

private:
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<Devices::Stub> devices_stub_;

  std::unique_ptr<DeviceFilesystem::Stub> filesystem_stub_;

  std::vector<DevicesResponse::Device> devices_;
};

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
    return std::find(caps.begin(), caps.end(), DeviceCapability::MakeDirectory) != caps.end() &&
           std::find(caps.begin(), caps.end(), DeviceCapability::PutFile) != caps.end() &&
           std::find(caps.begin(), caps.end(), DeviceCapability::BootFile) != caps.end();
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
}
