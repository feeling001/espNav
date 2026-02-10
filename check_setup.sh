#!/bin/bash

echo "========================================"
echo "  Marine Gateway - Setup Checker"
echo "========================================"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

errors=0

# Virtual environment settings
VENV_DIR=".venv"
VENV_PIO="${VENV_DIR}/bin/pio"

# Check Python 3
echo -n "Checking Python 3... "
if command -v python3 &> /dev/null; then
    version=$(python3 --version)
    echo -e "${GREEN}✓ Found $version${NC}"
    
    # Check version >= 3.6
    major=$(python3 -c 'import sys; print(sys.version_info.major)')
    minor=$(python3 -c 'import sys; print(sys.version_info.minor)')
    if [ "$major" -lt 3 ] || ([ "$major" -eq 3 ] && [ "$minor" -lt 6 ]); then
        echo -e "  ${RED}✗ Python 3.6+ required${NC}"
        errors=$((errors + 1))
    fi
else
    echo -e "${RED}✗ Not found${NC}"
    echo "  Install Python 3.6+"
    errors=$((errors + 1))
fi

# Check venv module
echo -n "Checking Python venv module... "
if python3 -m venv --help &> /dev/null; then
    echo -e "${GREEN}✓ Available${NC}"
else
    echo -e "${RED}✗ Not found${NC}"
    echo "  Install: apt install python3-venv (Debian/Ubuntu)"
    errors=$((errors + 1))
fi

# Check if venv exists
echo -n "Checking virtual environment... "
if [ -d "${VENV_DIR}" ]; then
    echo -e "${GREEN}✓ Found at ${VENV_DIR}${NC}"
    
    # Check PlatformIO in venv
    echo -n "Checking PlatformIO in venv... "
    if [ -f "${VENV_PIO}" ]; then
        version=$(${VENV_PIO} --version 2>/dev/null || echo "unknown")
        echo -e "${GREEN}✓ Found $version${NC}"
    else
        echo -e "${YELLOW}⚠ Not found in venv${NC}"
        echo "  Run: make setup-venv"
    fi
else
    echo -e "${YELLOW}⚠ Not found${NC}"
    echo "  Run: make setup-venv (will be created automatically)"
fi

# Check Node.js
echo -n "Checking Node.js... "
if command -v node &> /dev/null; then
    version=$(node --version)
    echo -e "${GREEN}✓ Found $version${NC}"
    
    # Check version >= 18
    major=$(echo $version | cut -d'.' -f1 | sed 's/v//')
    if [ "$major" -lt 18 ]; then
        echo -e "  ${YELLOW}⚠ Warning: Node.js 18+ recommended${NC}"
    fi
else
    echo -e "${RED}✗ Not found${NC}"
    echo "  Install from: https://nodejs.org/"
    errors=$((errors + 1))
fi

# Check npm
echo -n "Checking npm... "
if command -v npm &> /dev/null; then
    echo -e "${GREEN}✓ Found $(npm --version)${NC}"
else
    echo -e "${RED}✗ Not found${NC}"
    errors=$((errors + 1))
fi

# Check project structure
echo ""
echo "Checking project structure..."

check_file() {
    if [ -f "$1" ]; then
        echo -e "  ${GREEN}✓${NC} $1"
    else
        echo -e "  ${RED}✗${NC} $1 (missing)"
        errors=$((errors + 1))
    fi
}

check_dir() {
    if [ -d "$1" ]; then
        echo -e "  ${GREEN}✓${NC} $1/"
    else
        echo -e "  ${RED}✗${NC} $1/ (missing)"
        errors=$((errors + 1))
    fi
}

# Core files
check_file "platformio.ini"
check_file "partitions.csv"
check_file "build_and_flash.sh"
check_file "Makefile"

# Firmware
check_dir "include"
check_dir "src"
check_file "src/main.cpp"
check_file "include/config.h"

# Dashboard
check_dir "web-dashboard"
check_file "web-dashboard/package.json"
check_file "web-dashboard/vite.config.js"
check_file "web-dashboard/src/App.jsx"

echo ""
echo "Checking npm dependencies..."
if [ -d "web-dashboard/node_modules" ]; then
    echo -e "  ${GREEN}✓ node_modules exists${NC}"
else
    echo -e "  ${YELLOW}⚠ node_modules missing${NC}"
    echo "    Run: cd web-dashboard && npm install"
    echo "    Or:  make install"
fi

echo ""
echo "========================================"

if [ $errors -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo ""
    echo "You're ready to build. Run:"
    echo "  make all          # Build and flash everything"
    echo "  make setup-venv   # Setup venv only (automatic on first make)"
    echo ""
    echo "The virtual environment will be created automatically"
    echo "in ${VENV_DIR}/ on first build."
else
    echo -e "${RED}✗ Found $errors error(s)${NC}"
    echo ""
    echo "Please fix the errors above before building."
    exit 1
fi

echo "========================================"

