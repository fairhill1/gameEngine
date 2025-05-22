#!/bin/bash

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Set the dynamic library path to include SDL3
export DYLD_LIBRARY_PATH="$SCRIPT_DIR/sdl_src/SDL-release-3.2.14/build:$DYLD_LIBRARY_PATH"
echo "Set DYLD_LIBRARY_PATH to: $DYLD_LIBRARY_PATH"

# Run the game
echo "Running game..."
"$SCRIPT_DIR/build/MyFirstCppGame"