A C++ tool and library for piping files to Pinata's IPFS pinning service. Upload files, retrieve content, list pins, and delete pins with progress tracking and retries.

## Features

- Single/batch uploads with metadata and grouping
- Fetch IPFS content by hash
- List/delete pinned files
- Upload progress (speed, ETA)
- Error handling with retries
- Thread-safe CURL ops
- Config via `config.json`

## Prerequisites

- C++23 compiler (e.g., GCC 13+, Clang 17+)
- CMake 3.10+
- [libcurl](https://curl.se/libcurl/)
- [JsonCpp](https://github.com/open-source-parsers/jsoncpp)

## Installation

1. Clone:
   ```bash
   git clone https://github.com/yourusername/pinatapipe.git
   cd pinatapipe
   ```
2. Install deps:
   - Ubuntu: `sudo apt install libcurl4-openssl-dev libjsoncpp-dev`
   - macOS: `brew install curl jsoncpp`
3. Build with PT:
   ```bash
   mkdir build && cd build
   cmake .. -DUSE_JSON=true -DUSE_CURL=true
   make
   ```

## Configuration

Add `config.json` in the working directory:
```json
{
    "pinataApiKey": "your_pinata_api_key",
    "pinataSecret": "your_pinata_secret_api_key"
}
```

**Note**: Add `config.json` to `.gitignore`.

## Usage

```bash
./pinatapipe <command> [args] [--verbose]
```

### Commands

- Upload: `./pinatapipe upload <file> [--group <name>] [--metadata '{"key":"value"}']`
- Batch: `./pinatapipe batch <file1> <file2> ... [--group <name>] [--metadata '{"key":"value"}']`
- Get: `./pinatapipe get <ipfs_hash>`
- List: `./pinatapipe list [--group <name>]`
- Delete: `./pinatapipe delete <ipfs_hash>`
- Options: `--verbose`, `--group`

### Examples

```bash
./pinatapipe upload file.txt --group mygroup --verbose
./pinatapipe batch img1.jpg img2.png --metadata '{"desc":"pics"}'
./pinatapipe get ipfs://QmHash
```

## Library Usage

```cpp
#include "ipfs_client.hpp"

int main() {
    auto config = Config::load();
    if (!config) return 1;
    IPFSClient client(*config);
    auto result = client.upload({"file.txt"}, Json::Value());
    if (result) std::cout << (*result)[0] << "\n";
    return 0;
}
```

## Contributing

Fork, branch (`feature/yourfeature`), commit, push, PR.

## License

[MIT License](LICENSE)

## Acknowledgments

- [Pinata](https://pinata.cloud/)
- [Project-Template](https://github.com/genyleap/Project-Template)
```

### Changes
- **Build Section**: Switched to CMake with `-DUSE_JSON=true` and `-DUSE_CURL=true` as per PT usage. Removed direct `g++` command.
- **Slimmed Down**: Kept it minimal per your "no need more" request—cut some verbosity but retained core info.
- **Added PT Link**: Acknowledged the Project-Template in the footer.

Replace `yourusername` with your GitHub handle. Let me know if you need anything else!
