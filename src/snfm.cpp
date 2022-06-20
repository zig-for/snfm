#include "sni.pb.h"
#include "sni.grpc.pb.h"

#include <grpcpp/grpcpp.h>

int main()
{
  const char address[] = "localhost:8191";
    
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
  auto stub = Devices::NewStub(channel);
//service Devices {
  // detect and list devices currently connected to the system:
  //rpc ListDevices(DevicesRequest) returns (DevicesResponse) {}


}