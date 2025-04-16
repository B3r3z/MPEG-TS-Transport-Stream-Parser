# MPEG-TS Transport Stream Parser

This project is a simple MPEG-TS transport stream parser that reads a binary `.ts` file, analyzes the packet headers, and outputs the results to a `.txt` file.

## Features
- Parses MPEG-TS packet headers.
- Outputs header analysis to a single `.txt` file (`analysis_output.txt`).
- Supports binary-to-text conversion for debugging purposes.

## Requirements
- C++17 or later.
- A compiler such as `g++` or `clang++`.
- A valid MPEG-TS file for input.

## Usage

### Build and Run
1. Clone the repository:
   ```bash
   git clone <repository_url>
   cd lab02_MPEG-TS-S1-TRANSPORT_STREAM_PARSER_HEADER
   ```

2. Make the build script executable:
   ```bash
   chmod +x build_and_run.sh
   ```

3. Run the build and parser script:
   ```bash
   ./build_and_run.sh <input_file.ts>
   ```

   Example:
   ```bash
   ./build_and_run.sh example_new.ts
   ```

4. The output will be saved in `analysis_output.txt`.

### File Structure
- **TS_parser.cpp**: Main program logic.
- **tsTransportStream.h / tsTransportStream.cpp**: Header and implementation for MPEG-TS packet parsing.
- **tsCommon.h**: Common utilities and definitions.
- **build_and_run.sh**: Script to build and run the project.
- **.gitignore**: Git ignore file for temporary and build files.

## Example Output
The `analysis_output.txt` file will contain lines like:
```
0000000000 SB: 0x47 E: 0 S: 1 T: 0 PID: 0x0000 TSC: 0 AFC: 1 CC: 0
0000000001 SB: 0x47 E: 0 S: 0 T: 0 PID: 0x0011 TSC: 0 AFC: 1 CC: 1
...
```

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.

## Author
Bartosz