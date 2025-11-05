# waylandeglinfo

Small utility to print the EGL config / info on wayland

## Building

* Use the provided **CMake** build system.
```
mkdir build
cd build
cmake ..
make && make install
```

## OpenGL and OpenGL ES versions

* There are two versions of the tool

 * waylandeglinfo-es : Uses OpenGL ES
 * waylandeglinfo-gl : Uses OpenGL (Desktop GL)

* The version of OpenGL to use defaults to 2.0 in both
 cases, but it can be changed at runtime with the flag
`--glver`
