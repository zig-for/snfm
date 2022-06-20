# Super Nintendo File Manager

## Windows instructions

### First
`vcpkg install protobuf protobuf:x64-windows grpc`
`cmake -B build -S . "-DCMAKE_TOOLCHAIN_FILE=<YOUR_VCPKG_INSTALL_FOLDER>/scripts/buildsystems/vcpkg.cmake"`

### Then
`cmake --build build`

## Linux/MacOS instructions

Probably the same thing, but without having to mess with vcpkg.