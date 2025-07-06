# c_network_info

A simple C library and toolset for gathering and displaying network interface information on your system. This project is intended for C programmers and system administrators who need to inspect network adapters, addresses, and related statistics programmatically or from the command line.

## Features

- List all network interfaces
- Display IP addresses (IPv4 and IPv6)
- Show MAC addresses
- Display interface status (up/down)
- Show MTU and other interface statistics
- Lightweight and dependency-free (pure C)

## Getting Started

### Prerequisites

- GCC or compatible C compiler
- POSIX-compliant OS (Linux, BSD, macOS)
- Make (optional, for building examples/tools)

### Building

1. Clone the repository:
   ```sh
   git clone https://github.com/ragg967/c_network_info.git
   cd c_network_info
   ```

2. Build the library and example tools:
   ```sh
   make
   ```

   This will produce a static library (`libnetworkinfo.a`) and command-line tools in the `bin/` directory.

### Usage

#### As a Library

To use in your own C code:
```c
#include "network_info.h"

int main() {
    list_network_interfaces();
    return 0;
}
```

Compile with:
```sh
gcc -o myprog myprog.c -L. -lnetworkinfo
```

#### Command-Line Tool

After building, use the provided binary:
```sh
./bin/network_info
```
This will print network interface information to stdout.

## API Reference

See [network_info.h](./network_info.h) for function documentation and usage examples.

## Contributing

Contributions are welcome! Feel free to open issues or pull requests for improvements or bug fixes.

1. Fork the repo and create your branch
2. Make your changes and add tests if possible
3. Submit a pull request

## License

This project is licensed under the MIT License. See [LICENSE](./LICENSE) for details.

## Acknowledgements

- Inspired by common UNIX tools like `ifconfig` and `ip`
- Uses POSIX APIs for maximum compatibility

---

**Author:** [ragg967](https://github.com/ragg967)
