# Multi-Threaded Private Network Scanner

A high-performance, multi-threaded C23 network scanner designed to efficiently discover active hosts on private networks using parallel processing and optimized threading strategies.

## Features

- **Ultra-Fast Parallel Scanning**: Utilizes multiple threads to scan networks simultaneously
- **Intelligent Thread Management**: Automatically adjusts thread count based on system CPU cores
- **Comprehensive Network Coverage**: Scans common private IP ranges (Class A, B, C networks)
- **Multiple Scanning Modes**: From quick scans to comprehensive full-range discovery
- **Real-time Progress Tracking**: Live updates on scan progress and results
- **Memory-Aligned Data Structures**: Optimized for cache performance
- **Atomic Operations**: Thread-safe counters and statistics
- **Resource Management**: Proper cleanup and error handling

## System Requirements

- **Operating System**: Linux (tested on Fedora, should work on other distributions)
- **Compiler**: Clang with C23 support
- **Dependencies**: 
  - POSIX threads (pthread)
  - Standard C library
  - `ping` command available in PATH

## Building

The project uses a comprehensive Makefile with debug and release configurations:

### Debug Build (with sanitizers and debug symbols)
```bash
make debug
# or
make run-debug
```

### Release Build (optimized)
```bash
make release
# or
make run-release
```

### Clean Build Files
```bash
make clean
```

## Usage

Run the compiled binary and select from the available scanning modes:

```bash
./build/release/network_info
```

### Scanning Modes

1. **Parallel Scan of Common Private Networks (RECOMMENDED)**
   - Scans the most commonly used private IP ranges
   - Covers Class A (10.x.x.x), Class B (172.16-31.x.x), and Class C (192.168.x.x) networks
   - Optimized for typical home and office environments

2. **Ultra-Parallel Full 192.168.x.x Range**
   - Comprehensive scan of all 256 subnets in the 192.168.x.x range
   - Scans 65,024 IP addresses across 256 subnets
   - Maximum parallelization for fastest results

3. **Single Subnet Scan**
   - Targeted scan of a specific subnet range
   - Customizable host range (e.g., 192.168.1.1-254)
   - Ideal for focused network discovery

4. **Quick Parallel Scan**
   - Fast scan of the most likely networks
   - Covers 192.168.1.x, 192.168.0.x, 10.0.0.x, and 172.16.0.x
   - Perfect for quick network discovery

## Architecture

### Thread Management
- **Ping Threads**: 4x CPU cores (optimized for I/O-bound ping operations)
- **Subnet Threads**: Up to 16 parallel subnet scanners
- **Dynamic Batching**: Efficiently distributes work across available threads

### Memory Management
- **Aligned Allocations**: 64-byte aligned data structures for optimal cache performance
- **Atomic Operations**: Thread-safe counters using C11 atomics
- **Proper Cleanup**: Automatic resource deallocation and error handling

### Performance Optimizations
- **Cache-Friendly Design**: Aligned memory access patterns
- **Minimal System Calls**: Efficient ping command execution
- **Parallel Processing**: Multiple levels of parallelization
- **Progress Tracking**: Real-time feedback without performance impact

## Network Ranges Covered

### Common Class C Networks (192.168.x.x)
- 192.168.1.x, 192.168.0.x, 192.168.2.x, 192.168.3.x
- 192.168.10.x, 192.168.11.x, 192.168.20.x, 192.168.25.x
- 192.168.50.x, 192.168.100.x, 192.168.101.x, 192.168.200.x, 192.168.254.x

### Common Class B Networks (172.16-31.x.x)
- 172.16.0.x through 172.31.0.x ranges
- Focus on commonly used subnets

### Common Class A Networks (10.x.x.x)
- 10.0.0.x, 10.0.1.x, 10.0.2.x, 10.0.10.x
- 10.1.0.x, 10.1.1.x, 10.1.2.x, 10.1.10.x
- 10.2.0.x, 10.2.1.x, 10.10.0.x, 10.10.1.x
- 10.20.0.x, 10.100.0.x, 10.200.0.x, 10.254.0.x

### Localhost
- 127.0.0.x (loopback network)

## Output Format

The scanner provides detailed output including:
- Real-time progress updates per thread
- Active host discoveries with IP addresses
- Per-subnet summary statistics
- Overall scan statistics:
  - Total subnets scanned
  - Total hosts scanned
  - Total responders found
  - Scan duration
  - Scanning rate (hosts/second)
  - Parallel efficiency metrics

## Example Output

```
Multi-Threaded Private Network Scanner (C23 Optimized)
=====================================================
System: 8 CPU cores detected
Max ping threads: 32
Max subnet threads: 16

=== Common Class C Private Networks (Parallel Mode) ===
Scanning 15 subnets with up to 16 parallel threads...

[Thread 1] Scanning 192.168.1.1-254...
[Thread 2] Scanning 192.168.0.1-254...
[Thread 1] ✓ Host alive: 192.168.1.1
[Thread 1] ✓ Host alive: 192.168.1.254
[Thread 1] → 2 responders found in 192.168.1

========================================
        PARALLEL SCAN COMPLETE
========================================
Total subnets scanned: 47
Total hosts scanned: 11938
Total responders found: 15
Scan duration: 45 seconds
System cores utilized: 8
Average rate: 265.3 hosts/second
Parallel efficiency: 33.2x speedup
========================================
```

## Performance Characteristics

- **Typical Scan Speed**: 200-400 hosts/second (depends on network conditions)
- **Memory Usage**: ~10-50MB depending on scan scope
- **CPU Utilization**: Scales with available cores
- **Network Impact**: Minimal (single ping per host)

## Security Considerations

- **Network Scanning**: This tool performs active network scanning using ICMP ping
- **Permission Requirements**: May require elevated privileges on some systems
- **Network Policies**: Ensure compliance with your organization's network scanning policies
- **Rate Limiting**: Built-in threading limits prevent network flooding

## Troubleshooting

### Common Issues

1. **Permission Denied**: Some systems require root privileges for ping operations
   ```bash
   sudo ./build/release/network_info
   ```

2. **Thread Creation Failures**: Reduce thread limits if experiencing resource constraints
   - Modify `MAX_PING_THREADS` and `MAX_SUBNET_THREADS` in source code

3. **Slow Performance**: 
   - Check network connectivity
   - Verify ping command availability
   - Consider firewall/security software interference

### Build Issues

1. **C23 Support**: Ensure your clang version supports C23 features
2. **Missing Headers**: Install development packages for pthread and system headers
3. **Linking Errors**: Verify pthread library is available

## Development

### Code Structure
- `src/main.c`: Main implementation with all scanning logic
- `Makefile`: Build configuration with debug/release modes
- Thread-safe design using atomic operations and proper synchronization

### Contributing
- Follow existing code style and naming conventions
- Add appropriate error handling for new features
- Test with various network configurations
- Consider performance implications of changes

## License

This project is provided as-is for educational and network administration purposes. Use responsibly and in compliance with applicable laws and regulations.

## Author

Created as a high-performance network discovery tool showcasing modern C23 features and parallel programming techniques.
