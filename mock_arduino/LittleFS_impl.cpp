#include "LittleFS.h"
#include <iostream>

LittleFSMock LittleFS;

void File::write(uint8_t* buf, size_t size) {
    auto& data = LittleFS.storage[path];
    data.insert(data.end(), buf, buf + size);
}

void File::read(uint8_t* buf, size_t size) {
    if (LittleFS.storage.count(path)) {
        auto& data = LittleFS.storage[path];
        // We need an offset to handle sequential reads, but simple mock just does this:
        static size_t offset = 0;
        for (size_t i = 0; i < size && (i + offset) < data.size(); i++) buf[i] = data[i+offset];
        offset += size;
        if (offset >= data.size()) offset = 0;
    }
}

File LittleFSMock::open(const char* path, const char* mode) {
    std::string s_mode(mode);
    std::string s_path(path);
    if (s_mode == "w") {
        LittleFS.storage[s_path].clear();
        return File(s_path, true);
    }
    if (storage.count(s_path)) return File(s_path, false);
    return File();
}
