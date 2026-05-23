#!/usr/bin/env bash
# ==============================================================================
# install.sh
# Project Acheron macOS & Linux Bootstrap Installer
#
# Usage:
# curl -fsSL https://raw.githubusercontent.com/Harish020904/Acheron/main/bootstrap/install.sh | bash
# ==============================================================================

set -e # Exit immediately on error

# ANSI Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

OS="$(uname -s)"

function write_step() {
    echo -e "\n${CYAN}[ACHERON] $1${NC}"
}

function write_info() {
    echo -e "  -> $1"
}

function write_success() {
    echo -e "${GREEN}  [OK] $1${NC}"
}

function write_error_and_exit() {
    echo -e "${RED}  [ERROR] $1${NC}"
    exit 1
}

# ==============================================================================
# STEP 1: Verify Permissions
# ==============================================================================
write_step "Initializing Acheron Bootstrap ($OS)"
if [ "$EUID" -eq 0 ]; then
    write_error_and_exit "Please do not run this script as root/sudo. Package managers and vcpkg will fail or mess up permissions."
fi

# ==============================================================================
# STEP 2: Validate System Requirements
# ==============================================================================
write_step "Validating System Requirements"

if [ "$OS" == "Darwin" ]; then
    TOTAL_RAM_GB=$(($(sysctl -n hw.memsize) / 1024 / 1024 / 1024))
    FREE_SPACE_GB=$(df -g / | awk 'NR==2 {print $4}')
else
    # Linux (Ubuntu/Debian)
    TOTAL_RAM_GB=$(awk '/MemTotal/ {printf "%.0f", $2/1024/1024}' /proc/meminfo)
    FREE_SPACE_GB=$(df -BG / | awk 'NR==2 {print $4}' | sed 's/G//')
fi

write_info "Detected RAM: ${TOTAL_RAM_GB}GB"
if [ "$TOTAL_RAM_GB" -lt 8 ]; then
    write_error_and_exit "Acheron requires at least 8GB of RAM to compile and run."
fi

write_info "Free Disk Space (/): ${FREE_SPACE_GB}GB"
if [ "$FREE_SPACE_GB" -lt 10 ]; then
    write_error_and_exit "Acheron requires at least 10GB of free space on / for toolchains."
fi

write_success "System meets minimum requirements."

# ==============================================================================
# STEP 3: Install Toolchain
# ==============================================================================
write_step "Installing Toolchain Dependencies"

if [ "$OS" == "Darwin" ]; then
    if ! command -v xcode-select -p &>/dev/null; then
        write_info "Installing Xcode Command Line Tools..."
        xcode-select --install
        write_error_and_exit "Please follow the GUI prompts to install Xcode CLT, then run this script again."
    fi
    write_success "Xcode Command Line Tools installed."

    if ! command -v brew &>/dev/null; then
        write_info "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        eval "$(/opt/homebrew/bin/brew shellenv 2>/dev/null || /usr/local/bin/brew shellenv 2>/dev/null)"
    fi
    write_success "Homebrew installed."

    write_info "Installing cmake, ninja, vulkan-headers, vulkan-loader, git..."
    brew install cmake ninja vulkan-headers vulkan-loader git
else
    write_info "Requesting sudo password to install Linux packages via apt..."
    sudo apt-get update -y
    sudo apt-get install -y cmake ninja-build git build-essential curl unzip tar pkg-config
    
    write_info "Installing X11, Wayland, and Vulkan development libraries required for raylib/engine..."
    sudo apt-get install -y libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libgl1-mesa-dev libvulkan-dev vulkan-tools
fi

write_success "Toolchain installed."

# ==============================================================================
# STEP 4: Install vcpkg
# ==============================================================================
write_step "Configuring vcpkg"
VCPKG_DIR="$HOME/vcpkg"

if [ ! -f "$VCPKG_DIR/vcpkg" ]; then
    rm -rf "$VCPKG_DIR"
    write_info "Cloning vcpkg to $VCPKG_DIR..."
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
    
    write_info "Bootstrapping vcpkg..."
    cd "$VCPKG_DIR"
    ./bootstrap-vcpkg.sh -disableMetrics
    
    write_info "Integrating vcpkg..."
    ./vcpkg integrate install
    write_success "vcpkg configured."
else
    write_success "vcpkg already installed at $VCPKG_DIR."
fi

export VCPKG_ROOT="$VCPKG_DIR"

# ==============================================================================
# STEP 5: Clone or Update Acheron Repository
# ==============================================================================
write_step "Fetching Acheron Source Code"
CURRENT_DIR="$(pwd)"
REPO_URL="https://github.com/Harish020904/Acheron.git"

if [ -d "$CURRENT_DIR/.git" ]; then
    TARGET_DIR="$CURRENT_DIR"
    write_info "Executing from existing repository at $TARGET_DIR"
    write_success "Repository located."
else
    TARGET_DIR="$HOME/Acheron"
    if [ -d "$TARGET_DIR/.git" ]; then
        write_info "Repository exists at $TARGET_DIR. Pulling latest changes..."
        cd "$TARGET_DIR"
        git pull origin main
        write_success "Repository updated."
    else
        write_info "Cloning Acheron to $TARGET_DIR..."
        git clone "$REPO_URL" "$TARGET_DIR"
        write_success "Repository cloned."
    fi
fi

# ==============================================================================
# STEP 6: Generate Build Files
# ==============================================================================
write_step "Generating Build Files"
cd "$TARGET_DIR"

write_info "Running CMake generation (Ninja)..."
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
write_success "Build files generated."

# ==============================================================================
# STEP 7: Build the Engine
# ==============================================================================
write_step "Building Acheron Engine"
cmake --build build --config Release
write_success "Engine built successfully."

# ==============================================================================
# STEP 8: Stage Runtime Assets
# ==============================================================================
write_step "Staging Runtime Assets"
BIN_DIR="$TARGET_DIR/build/bin"

for asset in assets configs shaders localization; do
    if [ -d "$TARGET_DIR/$asset" ]; then
        write_info "Staging $asset..."
        mkdir -p "$BIN_DIR/$asset"
        cp -r "$TARGET_DIR/$asset/"* "$BIN_DIR/$asset/" 2>/dev/null || true
    fi
done
write_success "Assets staged."

# ==============================================================================
# STEP 9: Run the Engine
# ==============================================================================
write_step "Launching Acheron..."
EXE_PATH="$BIN_DIR/acheron"

if [ -f "$EXE_PATH" ]; then
    if [ "$OS" == "Darwin" ] && command -v xattr &>/dev/null; then
        xattr -d com.apple.quarantine "$EXE_PATH" 2>/dev/null || true
    fi
    cd "$TARGET_DIR" # Run from root
    
    write_success "Acheron launched successfully!"
    "$EXE_PATH"
else
    write_error_and_exit "Executable not found at $EXE_PATH."
fi

echo -e "\n${CYAN}========================================================${NC}"
echo -e "${CYAN} Bootstrap Complete! Environment is fully configured.${NC}"
echo -e "${CYAN}========================================================\n${NC}"
