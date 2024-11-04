# M17Netd
Create IP links over M17

## Dependencies

In order to build this program, you'll need the following dependencies:

- `liquid-sdr`
- `pkg-config`
- `fftw3` (actually `fftw3f` for floats)

## Compilation instructions

- Clone the repository: `git clone git@github.com:mdiepart/M17Netd.git`
- Enter the repository: `cd M17Netd`
- Init the submodules: `git submodule init && git submodule update`
- Create a build directory: `mkdir build && cd build`
- Configure the project:
    - `cmake .. -DCMAKE_BUILD_TYPE=Debug` for debug mode compilation
    or
    - `cmake .. -DCMAKE_BUILD_TYPE=Release` for release mode compilation
- Compile the project: `make`

## Execute the program

The compiled program is in the `build` folder and is called `M17Netd`
It requires superuser rights to run (because it must create a tun interface). It also expects a path to a config file as only argument. File `example.toml` at the root of the directory contains an example configuration.

Example: `sudo ./M17Netd ../example.toml`