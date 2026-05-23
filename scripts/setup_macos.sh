#!/usr/bin/env bash
# ==============================================================================
# setup_macos.sh
# Automated Environment Setup for Project Acheron (macOS)
# ==============================================================================

# ANSI Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN} Project Acheron - macOS Setup Script ${NC}"
echo -e "${CYAN}========================================${NC}"
echo ""

set -e # Exit immediately if a command exits with a non-zero status

# ------------------------------------------------------------------------------
# 1. Verify Xcode Command Line Tools
# ------------------------------------------------------------------------------
echo -e "${YELLOW}[1/5] Verifying Xcode Command Line Tools...${NC}"
if xcode-select -p &>/dev/null; then
    echo -e "${GREEN}Xcode Command Line Tools installed at $(xcode-select -p)${NC}"
else
    echo -e "${RED}ERROR: Xcode Command Line Tools not found.${NC}"
    echo -e "Installing now... Follow the GUI prompts, then re-run this script."
    xcode-select --install
    exit 1
fi

# ------------------------------------------------------------------------------
# 2. Verify Homebrew
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}[2/5] Verifying Homebrew Installation...${NC}"
if command -v brew &>/dev/null; then
    echo -e "${GREEN}Homebrew found: $(brew --version | head -n 1)${NC}"
else
    echo -e "${RED}Homebrew not found. Installing Homebrew...${NC}"
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    echo -e "${GREEN}Homebrew installed successfully.${NC}"
    # Source brew to current shell for immediately subsequent commands
    eval "$(/opt/homebrew/bin/brew shellenv 2>/dev/null || /usr/local/bin/brew shellenv 2>/dev/null)"
fi

# ------------------------------------------------------------------------------
# 3. Install CMake and Dependencies
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}[3/5] Installing CMake and System Dependencies...${NC}"
echo "Installing cmake, ninja..."
brew install cmake ninja

if command -v cmake &>/dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n 1)
    echo -e "${GREEN}CMake verified: $CMAKE_VERSION${NC}"
else
    echo -e "${RED}ERROR: CMake installation failed.${NC}"
    exit 1
fi

# Note on SDL2/raylib for macOS
echo -e "\n${CYAN}NOTE: SDL2 / raylib dependencies are automatically managed via CMake FetchContent.${NC}"
echo -e "${CYAN}They will be securely downloaded and compiled during your first build.${NC}"

# ------------------------------------------------------------------------------
# 4. Verify Clang Compiler
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}[4/5] Verifying Clang Compiler...${NC}"
if command -v clang++ &>/dev/null; then
    CLANG_VERSION=$(clang++ --version | head -n 1)
    echo -e "${GREEN}Compiler verified: $CLANG_VERSION${NC}"
else
    echo -e "${RED}ERROR: clang++ not found.${NC}"
    exit 1
fi

# ------------------------------------------------------------------------------
# 5. Create Build Folders
# ------------------------------------------------------------------------------
echo -e "\n${YELLOW}[5/5] Creating Build Directories...${NC}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

DIRS=(
    "$PROJECT_ROOT/build"
    "$PROJECT_ROOT/build/bin"
    "$PROJECT_ROOT/build/obj"
    "$PROJECT_ROOT/build/temp"
)

for dir in "${DIRS[@]}"; do
    if [ ! -d "$dir" ]; then
        mkdir -p "$dir"
        echo -e "${GREEN}Created directory: $dir${NC}"
    else
        echo -e "\033[1;30mDirectory already exists: $dir${NC}"
    fi
done

echo -e "\n${CYAN}========================================${NC}"
echo -e "${CYAN} Setup Complete! You are ready to build. ${NC}"
echo -e "${CYAN} Run: ./scripts/build_macos.sh ${NC}"
echo -e "${CYAN}========================================${NC}"
