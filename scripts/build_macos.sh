#!/usr/bin/env bash
# ==============================================================================
# build_macos.sh
# Automated Build Script for Project Acheron (macOS)
# ==============================================================================

# ANSI Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN} Project Acheron - macOS Build Script ${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

set -e # Exit immediately on error

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

# ------------------------------------------------------------------------------
# 1. CMake Configure
# ------------------------------------------------------------------------------
echo -e "${YELLOW}Configuring CMake project...${NC}"

# If ninja is installed, use the faster ninja preset, otherwise fallback to makefiles
if command -v ninja &>/dev/null; then
    PRESET="macos-ninja-release"
    echo "Ninja detected. Using faster preset: $PRESET"
else
    PRESET="macos-release"
    echo "Ninja not found. Using standard preset: $PRESET"
fi

if ! cmake --preset $PRESET; then
    echo -e "${RED}ERROR: CMake configuration failed.${NC}"
    echo -e "${RED}Please ensure you have run setup_macos.sh first.${NC}"
    exit 1
fi

# ------------------------------------------------------------------------------
# 2. CMake Build
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}Building project...${NC}"

if ! cmake --build build --config Release; then
    echo -e "${RED}ERROR: Build failed.${NC}"
    exit 1
fi

# ------------------------------------------------------------------------------
# 3. Final Validation
# ------------------------------------------------------------------------------
EXE_PATH="$PROJECT_ROOT/build/bin/acheron"

if [ -f "$EXE_PATH" ]; then
    echo -e "\n${GREEN}========================================${NC}"
    echo -e "${GREEN} Build Successful! ${NC}"
    echo -e "${GREEN} Executable located at: $EXE_PATH ${NC}"
    echo -e "${CYAN} Run it with: ./scripts/run_macos.sh ${NC}"
    echo -e "${GREEN}========================================${NC}"
else
    echo -e "\n${RED}ERROR: Build completed but executable was not found at expected path: $EXE_PATH${NC}"
    exit 1
fi
