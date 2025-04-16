#!/bin/bash

# Set build directory
BUILD_DIR="build"

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
  mkdir "$BUILD_DIR"
fi

# Navigate to build directory
cd "$BUILD_DIR"

# Compile the project
echo "Compiling the project..."
g++ -std=c++17 -Wall -Wextra -o TS_parser ../TS_parser.cpp ../tsTransportStream.cpp

# Check if compilation was successful
if [ $? -eq 0 ]; then
  echo "Compilation successful."
  echo "Running the application..."
  # Run the application with the first argument passed to the script
  ./TS_parser "$1"
else
  echo "Compilation failed."
fi
