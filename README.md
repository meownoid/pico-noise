# Brown noise generator for Raspberry Pi Pico

Brown noise generator for Raspberry Pi Pico which works with the [Pimoroni Pico Audio](https://pimoroni.com/picoaudio).

## Installation

First install the compiler. If you're using macOS, install the ARM GCC toolchain using Homebrew:

```bash
brew install gcc-arm-embedded
```

On Debian-based Linux distributions, install the required packages using:

```bash
apt install cmake gcc-arm-none-eabi build-essential
```

If you're using a different Linux distribution, ensure you have cmake and gcc-arm-none-eabi installed via your package manager.

Since this project relies on the Raspberry Pi Pico SDK, Pico Extras, and Pimoroni’s library, you need to clone these dependencies first.

**Pico SDK**

```bash
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk
git submodule update --init
cd ../
```

**Pico Extras**

```bash
git clone https://github.com/raspberrypi/pico-extras
```

**Pimoroni libraries**

```bash
git clone https://github.com/pimoroni/pimoroni-pico
```

Ensure the necessary environment variables are set. If you are using VS Code, you can add them to `settings.json`:

```json
{
    "cmake.configureSettings": {
        "PICO_SDK_PATH": "/path/to/pico-sdk",
        "PIMORONI_PICO_PATH": "/path/to/pimoroni-pico",
        "PICO_SDK_POST_LIST_DIRS": "/path/to/pico-extras"
    }
}
```

Alternatively, for Unix-based systems, you can export them in your terminal session or `.bashrc`/`.zshrc` file:

```bash
export PICO_SDK_PATH="/path/to/pico-sdk",
export PIMORONI_PICO_PATH="/path/to/pimoroni-pico",
export PICO_SDK_POST_LIST_DIRS="/path/to/pico-extras",
```

You can optionally set the installation directory to the Pico storage device:

```bash
export CMAKE_INSTALL_PREFIX="/Volumes/RPI-RP2/"
```

Now you can build and install the project using CMake.
