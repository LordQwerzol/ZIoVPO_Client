#include "Scanner.h"
#include <fstream>
#include <algorithm>
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

std::wstring toString(ObjectType type) noexcept {
    switch (type) {
        case ObjectType::PE:           return L"PE";
        case ObjectType::JAVASCRIPT:   return L"JAVASCRIPT";
        case ObjectType::NET_ASSEMBLY: return L"NET_ASSEMBLY";
        case ObjectType::JAVA_CLASS:   return L"JAVA_CLASS";
        default:                       return L"UNKNOWN";
    }
}

Scanner& Scanner::instance() {
    static Scanner instance;
    return instance;
}

std::vector<uint8_t> Scanner::computeSha256(const uint8_t* data, size_t len) {
    std::vector<uint8_t> hash(32, 0);
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (!CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return hash;
    }
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return hash;
    }
    if (!CryptHashData(hHash, data, (DWORD)len, 0)) {
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        return hash;
    }
    DWORD hashLen = 32;
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashLen, 0)) {
    }
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    return hash;
}

ObjectType Scanner::detectTypeByExtension(const std::wstring& path) {
    size_t dotPos = path.find_last_of(L'.');
    if (dotPos == std::wstring::npos) return ObjectType::UNKNOWN;
    std::wstring ext = path.substr(dotPos + 1);
    for (auto& c : ext) c = towlower(c);
    if (ext == L"exe" || ext == L"dll" || ext == L"sys") return ObjectType::PE;
    if (ext == L"js") return ObjectType::JAVASCRIPT;
    if (ext == L"class") return ObjectType::JAVA_CLASS;
    return ObjectType::UNKNOWN;
}

bool Scanner::checkRecord(const AvRecord& record,
                          const uint8_t* buffer, size_t bufferSize,
                          size_t pos,
                          ObjectType expectedType,
                          ThreatInfo& outThreat) const {
    // 3.3.1 Тип объекта
    if (record.objectType != static_cast<uint32_t>(expectedType))
        return false;

    // 3.3.2 Проверка диапазона (включительно)
    if (pos < record.offsetBegin || pos > record.offsetEnd)
        return false;

    // 3.3.3 Длина дополнительных байт (сигнатура без префикса)
    uint32_t extraLen = record.signatureLength - 8;
    if (pos + 8 + extraLen > bufferSize)
        return false;  // недостаточно данных

    // 3.3.4 Формируем буфер: префикс (8 байт) + дополнительные байты
    std::vector<uint8_t> fullSig(8 + extraLen);
    memcpy(fullSig.data(), buffer + pos, 8);
    if (extraLen > 0) {
        memcpy(fullSig.data() + 8, buffer + pos + 8, extraLen);
    }
    std::vector<uint8_t> computedHash = computeSha256(fullSig.data(), fullSig.size());

    // 3.3.5 Сравнение хешей
    if (computedHash.size() != record.signatureHash.size() ||
        memcmp(computedHash.data(), record.signatureHash.data(), computedHash.size()) != 0)
        return false;

    // Запись подошла – заполняем результат
    outThreat.threatName = std::wstring(record.threatName.begin(), record.threatName.end());
    outThreat.objectTypeString = toString(static_cast<ObjectType>(record.objectType));
    return true;
}

bool Scanner::scanBuffer(const uint8_t* data, size_t size,
                         ObjectType expectedType,
                         std::vector<ThreatInfo>& outThreats) {
    const auto& db = DatabaseLoader::instance().getDatabase();
    if (db.empty()) return false;

    // Сканирование с позиции 0, сдвиг на 1 байт
    for (size_t pos = 0; pos + 8 <= size; ++pos) {  // Требование 3.5
        uint64_t prefix = *reinterpret_cast<const uint64_t*>(data + pos);
        auto it = db.find(prefix);
        if (it == db.end()) continue;

        for (const auto& record : it->second) {
            ThreatInfo threat;
            threat.filePath = L""; // будет заполнено вызывающим методом
            if (checkRecord(record, data, size, pos, expectedType, threat)) {
                outThreats.push_back(threat); // По требованию 3.6 прекращаем сканирование при первом же совпадении
                return true;
            }
        }
    }
    return false;
}

bool Scanner::scanFile(const std::wstring& filePath,
                       ObjectType expectedType,
                       std::vector<ThreatInfo>& outThreats) {
    // Если тип не указан, определяем по расширению
    if (expectedType == ObjectType::UNKNOWN) {
        expectedType = detectTypeByExtension(filePath);
        if (expectedType == ObjectType::UNKNOWN) {
            return false;
        }
    }
    // Чтение файла
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION) {
        } else {
        }
        return false;
    }
    // Получение размера файла
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return false;
    }
    if (fileSize.QuadPart == 0) {
        CloseHandle(hFile);
        return false; // пустой файл не сканируем
    }
    // Получение потока байтов из файла
    HANDLE hMapping = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        return false;
    }
    const uint8_t* mappedData = (const uint8_t*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!mappedData) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return false;
    }

    std::vector<ThreatInfo> threats;
    bool found = scanBuffer(mappedData, static_cast<size_t>(fileSize.QuadPart), expectedType, threats);

    // Копируем найденные угрозы, добавляя путь
    for (auto& t : threats) {
        t.filePath = filePath;
        outThreats.push_back(t);
    }

    UnmapViewOfFile(mappedData);
    CloseHandle(hMapping);
    CloseHandle(hFile);
    return found;
}

bool Scanner::scanFolder(const std::wstring& folderPath,
                         ObjectType expectedType,
                         std::vector<ThreatInfo>& outThreats) {
    // Обход папки (получение всех файлов в папке)
    std::wstring searchPath = folderPath + L"\\*";
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err != ERROR_ACCESS_DENIED && err != ERROR_FILE_NOT_FOUND) {
        }
        return false;
    }
    // Сканирование содержимого файлов
    bool anyThreat = false;
    do {
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
            continue;

        std::wstring fullPath = folderPath + L"\\" + findData.cFileName;
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            anyThreat |= scanFolder(fullPath, expectedType, outThreats);
        } else {
            std::vector<ThreatInfo> fileThreats;
            if (scanFile(fullPath, expectedType, fileThreats)) {
                anyThreat = true;
                outThreats.insert(outThreats.end(), fileThreats.begin(), fileThreats.end());
            }
        }
    } while (FindNextFileW(hFind, &findData));
    FindClose(hFind);
    return anyThreat;
}
