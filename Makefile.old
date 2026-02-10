.PHONY: all build flash dashboard firmware clean check help

# Default target
all: build flash

# Build everything
build: dashboard firmware

# Build React dashboard
dashboard:
	@echo "Building React dashboard..."
	cd web-dashboard && npm run build

# Build ESP32 firmware
firmware:
	@echo "Building firmware..."
	pio run

# Flash everything to ESP32
flash:
	@echo "Uploading filesystem..."
	pio run -t uploadfs
	@echo "Uploading firmware..."
	pio run -t upload

# Upload only filesystem
uploadfs:
	pio run -t uploadfs

# Upload only firmware
upload:
	pio run -t upload

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	pio run -t clean
	rm -rf data/www/*
	cd web-dashboard && rm -rf dist/

# Clean everything including dependencies
clean-all: clean
	@echo "Cleaning dependencies..."
	cd web-dashboard && rm -rf node_modules/

# Install dashboard dependencies
install:
	cd web-dashboard && npm install

# Open serial monitor
monitor:
	pio device monitor

# Check setup
check:
	@chmod +x check_setup.sh
	@./check_setup.sh

# Erase ESP32 flash
erase:
	pio run -t erase

# Show help
help:
	@echo "Marine Gateway - Makefile Commands"
	@echo ""
	@echo "  make all        - Build and flash everything (default)"
	@echo "  make build      - Build dashboard and firmware"
	@echo "  make flash      - Upload filesystem and firmware"
	@echo "  make dashboard  - Build React dashboard only"
	@echo "  make firmware   - Build ESP32 firmware only"
	@echo "  make uploadfs   - Upload filesystem only"
	@echo "  make upload     - Upload firmware only"
	@echo "  make monitor    - Open serial monitor"
	@echo "  make clean      - Clean build artifacts"
	@echo "  make clean-all  - Clean everything including node_modules"
	@echo "  make install    - Install dashboard dependencies"
	@echo "  make check      - Check development environment"
	@echo "  make erase      - Erase ESP32 flash"
	@echo "  make help       - Show this help"
