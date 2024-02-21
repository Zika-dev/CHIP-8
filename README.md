<div align="center">
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="MIT License"></a>
</div>

# CHIP-8
CHIP-8 emulator made in C.

## Arguments

```sh
CHIP-8 [options] <rom-file>
```
- `[options]`: Includes optional arguments
- `<rom-file>`: File containing instructions for the CHIP-8. These can be found at multiple sources such as [here](https://github.com/kripod/chip8-roms)

**Options**
<ol>
  <li><code>-s &lt;speed&gt;</code> Sets the processors clock speed to a specific integer, default is 1000Hz</li>
  <li><code>-d</code> Enables debugging mode</li>
  <li><code>-m</code> Disables audio</li>
  <li><code>-mem &lt;start-addr&gt; &lt;end-addr&gt;</code> Performs a memory dump on the provided rom from <code>&lt;start-addr&gt;</code> to <code>&lt;end-addr&gt;</code>. Addresses can be both decimal and hex (starting with 0x)</li>
</ol>

## Building

### Prerequisites

Requires the [SDL 2](https://github.com/libsdl-org/SDL) library

### On Windows

1. Generate Visual Studio project
```sh
cmake .
```
2. Open and build the Visual Studio project

### On Ubuntu

1. Install the SDL 2 library
```sh
sudo apt install libsdl2-dev
```

2. Generate build files
```sh
cmake .
```

3. Compile and link
```sh
make
```
