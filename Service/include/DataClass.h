#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <string_view>

// Заголовок файла (первые байты)
struct DbHeader {
    uint32_t magic = 0x44535F56; // "DS_V" например
    uint32_t version = 1;        // версия формата
    uint64_t timestamp;          // дата базы в Unix millis (для отображения в GUI)
    uint32_t recordCount;        // количество сигнатур
};
// Поля одной сигнатуры 
struct AvRecord {
    uint64_t prefix;
    uint32_t signatureLength;
    std::vector<uint8_t> signatureHash;
    uint64_t offsetBegin;
    uint64_t offsetEnd;
    uint32_t objectType;
    std::vector<uint8_t> avRecordSignature;
    std::string threatName;
    std::array<uint8_t, 16> uuid;
};
// Перечисление 
enum class ObjectType : uint32_t {
    UNKNOWN = 0,
    PE = 1,
    JAVASCRIPT = 2,
    NET_ASSEMBLY = 3,
    JAVA_CLASS = 4
};

struct ThreatInfo {
        std::wstring filePath;
        std::wstring threatName;
        std::wstring objectTypeString;
};