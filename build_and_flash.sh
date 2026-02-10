#!/bin/bash
set -e

echo "========================================"
echo "  Marine Gateway - Build & Flash"
echo "========================================"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    echo "Error: platformio.ini not found. Run this script from the project root."
    exit 1
fi

# Step 1: Build React Dashboard
echo -e "${BLUE}[1/5] Building React dashboard...${NC}"
cd web-dashboard
npm run build
cd ..
echo -e "${GREEN}✓ Dashboard built${NC}"

# Step 2: Copy to data folder
echo -e "${BLUE}[2/5] Copying dashboard to data/www...${NC}"
rm -rf data/www/*
mkdir -p data/www
cp -r web-dashboard/dist/* data/www/
echo -e "${GREEN}✓ Dashboard copied${NC}"

# Step 3: Build firmware
echo -e "${BLUE}[3/5] Building firmware...${NC}"
pio run
echo -e "${GREEN}✓ Firmware built${NC}"

# Step 4: Upload filesystem
echo -e "${BLUE}[4/5] Uploading filesystem to ESP32...${NC}"
pio run -t uploadfs
echo -e "${GREEN}✓ Filesystem uploaded${NC}"

# Step 5: Upload firmware
echo -e "${BLUE}[5/5] Uploading firmware to ESP32...${NC}"
pio run -t upload
echo -e "${GREEN}✓ Firmware uploaded${NC}"

echo ""
echo "========================================"
echo -e "${GREEN}  Build & Flash Complete!${NC}"
echo "========================================"
echo ""
echo "Next steps:"
echo "1. Open serial monitor: pio device monitor"
echo "2. The ESP32 will create an AP: 'MarineGateway-XXXXXX'"
echo "3. Connect and configure WiFi at: http://192.168.4.1"
echo ""
