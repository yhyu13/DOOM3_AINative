#pragma once

#include <cstdint>
#include <string>

namespace q4 {

// Writes tightly packed or row-pitched RGBA8 bytes as a PNG through WIC.
// COM must be initialized on the calling thread.
bool WritePngRgba8(const std::string& path, uint32_t width, uint32_t height,
                   const uint8_t* pixels, uint32_t rowPitch, std::string* error);

}  // namespace q4
