#!/bin/bash

# Set build directory
BUILD_DIR="build"

# Get absolute path of input file if provided
INPUT_FILE=""
if [ -n "$1" ]; then
  # Convert to absolute path
  INPUT_FILE=$(realpath "$1")
  # Check if the file exists
  if [ ! -f "$INPUT_FILE" ]; then
    echo "Error: Input file not found: $1"
    echo "Continuing with build only..."
    INPUT_FILE=""
  else
    echo "Input file: $INPUT_FILE"
  fi
fi

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
  mkdir "$BUILD_DIR"
fi

# Navigate to build directory
cd "$BUILD_DIR"

# Build with CMake
echo "Configuring the project with CMake..."
cmake ..

# Compile the project
echo "Compiling the project..."
make

# Check if compilation was successful
if [ $? -eq 0 ]; then
  echo "Compilation successful."
  
  # Check if a file was provided as an argument
  if [ -n "$INPUT_FILE" ]; then
    echo "Running the application with file: $INPUT_FILE"
    # Run the application with the file path provided
    ./TS-PARSER "$INPUT_FILE"
  else
    echo "No valid input file provided. Usage: ./build_and_run.sh <input_file>"
    echo "Example: ./build_and_run.sh /path/to/your/file.ts"
  fi
else
  echo "Compilation failed."
fi
