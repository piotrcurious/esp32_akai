#include "LittleFS.h"

LittleFSMock LittleFS;

void File::write(uint8_t* buf, size_t size) {
    LittleFS.storage[path].assign(buf, buf + size);
}

void File::read(uint8_t* buf, size_t size) {
    if (LittleFS.storage.count(path)) {
        auto& data = LittleFS.storage[path];
        for (size_t i = 0; i < size && i < data.size(); i++) buf[i] = data[i];
    }
}

File LittleFSMock::open(const char* path, const char* mode) {
    std::string s_mode(mode);
    if (s_mode == "w" || storage.count(path)) return File(path, s_mode == "w");
    return File();
}
