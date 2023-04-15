set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

set(gRPC_PROTOBUF_PROVIDER package)
set(gRPC_PROTOBUF_PACKAGE_TYPE CONFIG)

set(CMAKE_REQUIRED_LINK_OPTIONS "-arch;arm64")