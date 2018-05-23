# waylandeglinfo

Small utility to print the EGL config / info on wayland

## Building

* To build you can simply run something like:

```
gcc $(pkg-config --cflags --libs wayland-client egl glesv2 wayland-egl wayland-server) waylandes2info.c -o waylandes2info
```

* Or you can use the provided **CMake** build system.
```
mkdir build
cd build
cmake ..
make && make install
```
