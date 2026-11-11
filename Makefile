# Run `make` from this directory. Requires PICO_SDK_PATH to point to pico-sdk.
BUILD_DIR ?= build

.PHONY: all clean
all:
	cmake -S . -B $(BUILD_DIR) -DPICO_BOARD=pico2
	cmake --build $(BUILD_DIR) --target pico_hill_drive

clean:
	cmake -E rm -rf $(BUILD_DIR)
