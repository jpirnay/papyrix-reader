# Makefile for CrossPoint Reader firmware
# Wraps PlatformIO commands for convenience

.PHONY: all build build-release upload upload-release flash flash-release \
        clean format check monitor size erase build-fs upload-fs sleep-screen help

# Default target
all: help

# Build targets
build:
	pio run

build-release:
	pio run -e gh_release

# Upload targets
upload:
	pio run --target upload

upload-release:
	pio run -e gh_release --target upload

# Aliases
flash: upload

flash-release: upload-release

# Clean
clean:
	pio run --target clean

# Code quality
format:
	./bin/clang-format-fix

check:
	pio check

# Device/debug
monitor:
	pio device monitor

size:
	pio run --target size

erase:
	pio run --target erase

# Filesystem
build-fs:
	pio run --target buildfs

upload-fs:
	pio run --target uploadfs

# Image conversion
sleep-screen:
ifdef INPUT
ifdef OUTPUT
	python3 scripts/create_sleep_screen_image.py $(INPUT) $(OUTPUT) $(ARGS)
else
	@echo "Usage: make sleep-screen INPUT=<image> OUTPUT=<bmp> [ARGS='--dither --bits 8']"
endif
else
	@echo "Usage: make sleep-screen INPUT=<image> OUTPUT=<bmp> [ARGS='--dither --bits 8']"
	@echo "Example: make sleep-screen INPUT=photo.jpg OUTPUT=sleep.bmp"
endif

# Help
help:
	@echo "CrossPoint Reader Makefile"
	@echo ""
	@echo "Build:"
	@echo "  make build          - Build dev firmware (default)"
	@echo "  make build-release  - Build release firmware"
	@echo "  make clean          - Clean build artifacts"
	@echo ""
	@echo "Flash:"
	@echo "  make upload         - Build and flash dev firmware"
	@echo "  make upload-release - Build and flash release firmware"
	@echo "  make flash          - Alias for upload"
	@echo ""
	@echo "Code quality:"
	@echo "  make format         - Format code with clang-format"
	@echo "  make check          - Run cppcheck static analysis"
	@echo ""
	@echo "Device:"
	@echo "  make monitor        - Open serial monitor"
	@echo "  make size           - Show firmware size"
	@echo "  make erase          - Erase device flash"
	@echo ""
	@echo "Filesystem:"
	@echo "  make build-fs       - Build SPIFFS filesystem"
	@echo "  make upload-fs      - Upload SPIFFS to device"
	@echo ""
	@echo "Image conversion:"
	@echo "  make sleep-screen INPUT=<image> OUTPUT=<bmp> [ARGS='...']"
	@echo "                      - Convert image to sleep screen format"
	@echo "  Options: --orientation portrait|landscape, --bits 2|4|8, --dither, --fit contain|cover|stretch"
