#include "LittleFS.h"
#include <iostream>

LittleFSMock LittleFS;

size_t File::write(const uint8_t* buf, size_t size) {
    auto& data = LittleFS.storage[path];
    data.insert(data.end(), buf, buf + size);
    return size;
}

size_t File::read(uint8_t* buf, size_t size) {
    if (LittleFS.storage.count(path)) {
        auto& data = LittleFS.storage[path];
        // We need an offset to handle sequential reads
        static std::map<std::string, size_t> offsets;
        size_t& offset = offsets[path];
        size_t read_bytes = 0;
        for (size_t i = 0; i < size && (i + offset) < data.size(); i++) {
            buf[i] = data[i+offset];
            read_bytes++;
        }
        offset += read_bytes;
        // Reset offset if we reached end or if it's explicitly closed (mock close resets all)
        if (offset >= data.size()) offset = 0;
        return read_bytes;
    }
    return 0;
}

size_t File::size() {
    if (LittleFS.storage.count(path)) {
        return LittleFS.storage[path].size();
    }
    return 0;
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
