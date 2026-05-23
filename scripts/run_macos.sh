#!/usr/bin/env bash
# ==============================================================================
# run_macos.sh
# Automated Run Script for Project Acheron (macOS)
# ==============================================================================

RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXE_PATH="$PROJECT_ROOT/build/bin/acheron"

if [ ! -f "$EXE_PATH" ]; then
    echo -e "${RED}ERROR: Executable not found at $EXE_PATH${NC}"
    echo -e "${YELLOW}Please build the project first using: ./scripts/build_macos.sh${NC}"
    exit 1
fi

echo -e "${CYAN}Starting Acheron Simulation...${NC}"

# Remove quarantine attribute in case Gatekeeper blocks the newly compiled binary
if command -v xattr &>/dev/null; then
    xattr -d com.apple.quarantine "$EXE_PATH" 2>/dev/null || true
fi

# Set working directory to project root so assets load correctly
cd "$PROJECT_ROOT"

# Execute
"$EXE_PATH"
