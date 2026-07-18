#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace transitink {

struct EmbeddedCatalogAsset {
    const char* path;
    const uint8_t* data;
    std::size_t size;
    const char* sha256;
};

extern const char kEmbeddedCatalogRevision[];
extern const char kEmbeddedCatalogGeneratedAt[];
extern const EmbeddedCatalogAsset kEmbeddedCatalogAssets[];
extern const std::size_t kEmbeddedCatalogAssetCount;

}  // namespace transitink
