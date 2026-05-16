#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <string_view>
#include "DatabaseLoader.h"

class Scanner {
public:

    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;

    static Scanner& instance();

    // Основные методы сканирования
    bool scanBuffer(const uint8_t* data, size_t size,
                    ObjectType expectedType,
                    std::vector<ThreatInfo>& outThreats);

    bool scanFile(const std::wstring& filePath,
                  ObjectType expectedType,
                  std::vector<ThreatInfo>& outThreats);

    bool scanFolder(const std::wstring& folderPath,
                    ObjectType expectedType,
                    std::vector<ThreatInfo>& outThreats);

private:
    Scanner() = default;
    ~Scanner() = default;

    static std::vector<uint8_t> computeSha256(const uint8_t* data, size_t len);

    bool checkRecord(const AvRecord& record,
                     const uint8_t* buffer, size_t bufferSize,
                     size_t pos,
                     ObjectType expectedType,
                     ThreatInfo& outThreat) const;

    ObjectType detectTypeByExtension(const std::wstring& path);
};