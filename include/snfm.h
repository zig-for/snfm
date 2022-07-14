#pragma once

#include "sni.pb.h"
#include "sni.grpc.pb.h"

#include <grpcpp/grpcpp.h>
#include <iostream>
#include <functional>
#include <optional>
#include <filesystem>
#include <fstream>


static grpc::ChannelArguments GetChannelArgs()
{
    grpc::ChannelArguments ca;
    ca.SetMaxReceiveMessageSize(-1);
    return ca;
}

bool HasCapability(const DevicesResponse::Device& device, DeviceCapability cap);

class SNIConnection
{
public:
    SNIConnection(const std::string& address = "localhost:8191") // "172.27.16.1:8191"
        : channel_{ grpc::CreateCustomChannel(address, grpc::InsecureChannelCredentials(), GetChannelArgs()) }
        , devices_stub_{ Devices::NewStub(channel_) }
        , filesystem_stub_{ DeviceFilesystem::NewStub(channel_) }
        , device_stub_{DeviceControl::NewStub(channel_)}
        , device_memory_stub_{ DeviceMemory::NewStub(channel_) }
    {

    }

    void refreshDevices(const std::function<bool(const DevicesResponse::Device&)>& filter = nullptr)
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

    std::vector<std::string> getDeviceUris()
    {
        std::vector<std::string> out;
        for (const auto& device : devices_)
        {
            out.push_back(device.uri());
        }
        return out;
    }

    std::optional<DevicesResponse::Device> getDevice(const std::string& uri)
    {
        for (const auto& device : devices_)
        {
            if (device.uri() == uri)
            {
                return device;
            }
        }
        return {};
    }

    ReadDirectoryResponse readDirectory(const std::string& uri, const std::string& directory)
    {
        grpc::ClientContext context;
        ReadDirectoryRequest request;
        {
            request.set_uri(uri);
            request.set_path(directory);
        }
        ReadDirectoryResponse response;
        std::cout << request.DebugString() << std::endl;
        filesystem_stub_->ReadDirectory(&context, request, &response);
        return response;
    }

    void makeDirectory(const std::string& uri, const std::string& directory)
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

    std::optional<std::filesystem::path> putFile(const std::string& uri, const std::filesystem::path& local_path, const std::filesystem::path device_directory)
    {
        std::filesystem::path filename = local_path.filename();

        std::filesystem::path device_path = device_directory / filename;

        std::ifstream file(local_path, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
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
        auto status = filesystem_stub_->PutFile(&context, request, &response);

        if (!status.ok())
        {
            std::cerr << status.error_message();
            return {};
        }
        if (response.path().empty())
        {
            std::cout << "no response path" << std::endl;
            return {};
        }
        return response.path();
    }

    std::optional<std::vector<uint8_t>> getFile(const std::string& uri, const std::filesystem::path& device_path)
    {
        std::filesystem::path filename = device_path.filename();

        GetFileRequest request;
        {
            request.set_uri(uri);
            request.set_path(device_path.generic_string());
        }

        GetFileResponse response;
        grpc::ClientContext context;
        auto status = filesystem_stub_->GetFile(&context, request, &response);

        if (!status.ok())
        {
            std::cerr << status.error_message();
            return {};
        }
        
        return { { response.data().begin(), response.data().end() } };
    }
    std::optional<std::filesystem::path> getFile(const std::string& uri, const std::filesystem::path& device_path, std::filesystem::path local_path, bool is_filename = false)
    {
        auto data = getFile(uri, device_path);
        if (!data)
        {
            return {};
        }
        if (!is_filename)
        {
            std::filesystem::path filename = device_path.filename();
            local_path /= filename;
        }
        std::ofstream of(local_path, std::ios::out | std::ios::binary);
        std::copy(data->cbegin(), data->cend(),
            std::ostream_iterator<uint8_t>(of));
        return local_path;
    }


    void bootFile(const std::string& uri, const std::filesystem::path& file_path)
    {
        BootFileRequest request;
        request.set_uri(uri);
        request.set_path(file_path.generic_string());
        BootFileResponse response;
        grpc::ClientContext context;
        filesystem_stub_->BootFile(&context, request, &response);
    }
    bool deleteFile(const std::string& uri, const std::filesystem::path& file_path)
    {
        RemoveFileRequest request;
        request.set_uri(uri);
        request.set_path(file_path.generic_string());
        RemoveFileResponse response;
        grpc::ClientContext context;
        grpc::Status status = filesystem_stub_->RemoveFile(&context, request, &response);
        return status.ok();
    }

    bool renameFile(const std::string& uri, const std::filesystem::path& from, const std::filesystem::path& to)
    {
        RenameFileRequest request;
        request.set_uri(uri);
        request.set_path(from.generic_string());

        assert(to.has_root_directory());
        assert(from.has_root_directory());
        assert(from.parent_path() != from);
        std::filesystem::path a(from.parent_path());
        std::filesystem::path b(from.filename());
        std::filesystem::path c(to.lexically_relative(a));
        request.set_newfilename(to.lexically_relative(from.parent_path()).generic_string());
        RenameFileResponse response;
        grpc::ClientContext context;
        grpc::Status status = filesystem_stub_->RenameFile(&context, request, &response);
        return status.ok();
    }

    void resetToMenu(const std::string& uri)
    {
        ResetToMenuRequest request;
        request.set_uri(uri);
        ResetToMenuResponse response;
        grpc::ClientContext context;
        device_stub_->ResetToMenu(&context, request, &response);
    }

    void resetGame(const std::string& uri)
    {
        ResetSystemRequest request;
        request.set_uri(uri);
        ResetSystemResponse response;
        grpc::ClientContext context;
        device_stub_->ResetSystem(&context, request, &response);
    }

    MemoryMapping detectMapping(const std::string& uri)
    {
        DetectMemoryMappingRequest request;
        request.set_uri(uri);
        DetectMemoryMappingResponse response;
        grpc::ClientContext context;
        device_memory_stub_->MappingDetect(&context, request, &response);
        return response.memorymapping();
    }

    std::vector<std::byte> readMemory(const std::string& uri, AddressSpace address_space, uint32_t address, uint32_t size, MemoryMapping mapping = MemoryMapping::Unknown)
    {
        SingleReadMemoryRequest request;
        request.set_uri(uri);
        request.mutable_request()->set_requestaddressspace(address_space);
        request.mutable_request()->set_requestaddress(address);
        request.mutable_request()->set_size(size);
        request.mutable_request()->set_requestmemorymapping(mapping);
        SingleReadMemoryResponse response;
        grpc::ClientContext context;
        device_memory_stub_->SingleRead(&context, request, &response);
        const std::string& data = response.response().data();
        std::vector<std::byte> bytes;
        bytes.reserve(data.size());
        std::transform(std::begin(data), std::end(data), std::back_inserter(bytes), [](char c) {
            return std::byte(c);
            });
        return bytes;
    }

    std::vector<std::byte> readControllerState(const std::string& uri, MemoryMapping mapping = MemoryMapping::Unknown)
    {
        return readMemory(uri, AddressSpace::FxPakPro, 0xF50000 + 0xDA2, 8, mapping);
    }
    void writeMemory(const std::string& uri, AddressSpace address_space, uint32_t address, const std::vector<std::byte>& data, MemoryMapping mapping = MemoryMapping::Unknown)
    {
        SingleWriteMemoryRequest request;
        request.set_uri(uri);
        request.mutable_request()->set_requestaddressspace(address_space);
        request.mutable_request()->set_requestaddress(address);

        std::string s;
        s.resize(data.size());
        for (int i = 0; i < data.size(); i++)
        {
            s[i] = char(data[i]);
        }
        request.mutable_request()->set_data(s);
        request.mutable_request()->set_requestmemorymapping(mapping);
        SingleWriteMemoryResponse response;
        grpc::ClientContext context;
        device_memory_stub_->SingleWrite(&context, request, &response);
        
        return;
    }


private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<Devices::Stub> devices_stub_;

    std::unique_ptr<DeviceFilesystem::Stub> filesystem_stub_;
    std::unique_ptr<DeviceControl::Stub> device_stub_;
    std::unique_ptr<DeviceMemory::Stub> device_memory_stub_;


    std::vector<DevicesResponse::Device> devices_;
};
