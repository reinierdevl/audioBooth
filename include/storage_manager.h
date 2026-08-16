#pragma once

#include <FS.h>

enum class StorageSource {
    None,
    Internal,
    SdCard,
};

// Selects SD or FFat once. Call exactly once from setup().
bool beginStorage();

StorageSource storageSource();
fs::FS *activeFileSystem();
bool storageAvailable();
bool contentLayoutValid();
void markStorageUnavailable();
const char *storageSourceName();

