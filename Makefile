.PHONY: all build flash dashboard firmware clean check help setup-venv

# Virtual environment settings
VENV_DIR = .venv
VENV_BIN = $(VENV_DIR)/bin
PYTHON = $(VENV_BIN)/python
PIO = $(VENV_BIN)/pio

# Check if venv exists, if not create it
$(VENV_DIR):
	@echo "Creating Python virtual environment..."
	python3 -m venv $(VENV_DIR)
	@echo "Installing PlatformIO..."
	$(VENV_BIN)/pip install --upgrade pip
	$(VENV_BIN)/pip install platformio
	@echo "✓ Virtual environment ready"

# Setup virtual environment
setup-venv: $(VENV_DIR)

# Default target
all: setup-venv build flash monitor

# Build everything
build: setup-venv dashboard firmware

# Check if npm dependencies are installed
web-dashboard/node_modules:
	@echo "Installing npm dependencies..."
	cd web-dashboard && npm install

# Build React dashboard
dashboard: web-dashboard/node_modules
	@echo "Building React dashboard..."
	cd web-dashboard && npm run build

# Build ESP32 firmware
firmware: setup-venv
	@echo "Building firmware..."
	$(PIO) run

# Flash everything to ESP32
flash: setup-venv
	@echo "Uploading filesystem..."
	$(PIO) run -t uploadfs
	@echo "Uploading firmware..."
	$(PIO) run -t upload

# Upload only filesystem
uploadfs: setup-venv
	$(PIO) run -t uploadfs

# Upload only firmware
upload: setup-venv
	$(PIO) run -t upload

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@if [ -d $(VENV_DIR) ]; then $(PIO) run -t clean; fi
	rm -rf data/www/*
	cd web-dashboard && rm -rf dist/

# Clean everything including dependencies
clean-all: clean
	@echo "Cleaning dependencies..."
	cd web-dashboard && rm -rf node_modules/
	rm -rf $(VENV_DIR)
	@echo "✓ Cleaned venv and dependencies"

# Install dashboard dependencies
install: web-dashboard/node_modules
	@echo "✓ npm dependencies installed"

# Open serial monitor
monitor: setup-venv
	$(PIO) device monitor

# Check setup
check: setup-venv
	@chmod +x check_setup.sh
	@./check_setup.sh

# Erase ESP32 flash
erase: setup-venv
	$(PIO) run -t erase

# Show help
help:
	@echo "Marine Gateway - Makefile Commands"
	@echo ""
	@echo "  make setup-venv - Create Python venv and install PlatformIO"
	@echo "  make all        - Build and flash everything (default)"
	@echo "  make build      - Build dashboard and firmware"
	@echo "  make flash      - Upload filesystem and firmware"
	@echo "  make dashboard  - Build React dashboard only"
	@echo "  make firmware   - Build ESP32 firmware only"
	@echo "  make uploadfs   - Upload filesystem only"
	@echo "  make upload     - Upload firmware only"
	@echo "  make monitor    - Open serial monitor"
	@echo "  make clean      - Clean build artifacts"
	@echo "  make clean-all  - Clean everything including venv"
	@echo "  make install    - Install dashboard dependencies"
	@echo "  make check      - Check development environment"
	@echo "  make erase      - Erase ESP32 flash"
	@echo "  make help       - Show this help"

