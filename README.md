# GDMP-Server

Server software for the mod

# Build Instruction

## MacOS

```sh
brew install openssl protobuf
cmake . -DCMAKE_OSX_ARCHITECTURES=x86_64 -DGEODE_TARGET_PLATFORM=MacOS
make
```

## Windows

```cmd
vcpkg install --triplet=x86-windows-static openssl protobuf
cmake .
make
```

# Credits

- [GD Programming Discord](https://discord.gg/jEwtDBK) - They help me alot, They made this possible
- [ReplayBot](https://github.com/matcool/ReplayBot) - Lots of reference and resources