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

### Compiling tests

Several tests have been implemented for:
 - performing a raw signal acquisition (test\_acq.cpp)
 - filtering a raw signal acquisition (test\_filter.cpp)
 - test the BERT implementation (test\_bert\_encode\_decode.cpp)
 - run a BERT transmitter (test\_bert\_tx.cpp)
 - run a BERT receiver and display statistics (test\_bert\_rx.cpp)
 - perform the demodulation steps on a raw acquisition file (test\_demod.cpp)
 - Transmit a pure tone (test\_tone.cpp)

To compile a test, run `make {test_name}` and execute the resulting binary file.
You can also run `make tests` to compile all the tests at once.

## Execute the program

The compiled program is in the `build` folder and is called `M17Netd`
It requires superuser rights to run (because it must create a tun interface). It also expects a path to a config file as only argument. File `example.toml` at the root of the directory contains an example configuration.

Example: `sudo ./M17Netd ../example.toml`
