#ifndef LITTLEFS_H
#define LITTLEFS_H

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <cstddef>

class File {
    std::string path;
    bool writing;
public:
    File() : writing(false) {}
    File(std::string p, bool w) : path(p), writing(w) {}
    operator bool() { return !path.empty(); }
    size_t write(const uint8_t* buf, size_t size);
    size_t read(uint8_t* buf, size_t size);
    size_t size();
    void close() {}
};

class LittleFSMock {
public:
    std::map<std::string, std::vector<uint8_t>> storage;
    bool begin(bool format) { return true; }
    File open(const char* path, const char* mode);
};

extern LittleFSMock LittleFS;

#endif
