# Super Nintendo File Manager

## Windows instructions

### First
`vcpkg install protobuf:x64-windows grpc wxwidgets`
`cmake -B build -S . "-DCMAKE_TOOLCHAIN_FILE=<YOUR_VCPKG_INSTALL_FOLDER>/scripts/buildsystems/vcpkg.cmake"`

### Then
`cmake --build build`

## Linux/MacOS instructions

The same, but install the dependencies through git/apt. Good luck.

## Known issue:

Outputting files appends junk on the end. :)