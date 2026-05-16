#include "DatabaseLoader.h"
#include <fstream>
#include <cstring>

DatabaseLoader& DatabaseLoader::instance() {
    static DatabaseLoader instance;
    return instance;
}

bool DatabaseLoader::load(const std::string& filepath) {
    m_loaded = false;

    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }

    // 1. Чтение заголовка
    DbHeader header;
    ifs.read(reinterpret_cast<char*>(&header.magic), sizeof(header.magic));
    ifs.read(reinterpret_cast<char*>(&header.version), sizeof(header.version));
    ifs.read(reinterpret_cast<char*>(&header.timestamp), sizeof(header.timestamp));
    ifs.read(reinterpret_cast<char*>(&header.recordCount), sizeof(header.recordCount));

    if (ifs.fail()) {
        return false;
    }

    if (header.magic != 0x44535F56 || header.version != 1) {
        return false;
    }

    m_timestamp = header.timestamp;
    m_recordCount = header.recordCount;
    m_db.clear();

    // 2. Чтение записей
    for (uint32_t i = 0; i < header.recordCount; ++i) {
        AvRecord record;

        // Базовые поля
        ifs.read(reinterpret_cast<char*>(&record.prefix), sizeof(record.prefix));
        ifs.read(reinterpret_cast<char*>(&record.signatureLength), sizeof(record.signatureLength));
        ifs.read(reinterpret_cast<char*>(&record.offsetBegin), sizeof(record.offsetBegin));
        ifs.read(reinterpret_cast<char*>(&record.offsetEnd), sizeof(record.offsetEnd));
        ifs.read(reinterpret_cast<char*>(&record.objectType), sizeof(record.objectType));

        // threatName
        uint16_t threatNameLen = 0;
        ifs.read(reinterpret_cast<char*>(&threatNameLen), sizeof(threatNameLen));
        if (threatNameLen > 0) {
            std::vector<char> buffer(threatNameLen);
            ifs.read(buffer.data(), threatNameLen);
            record.threatName.assign(buffer.data(), threatNameLen);
        } else {
            record.threatName.clear();
        }

        // UUID
        ifs.read(reinterpret_cast<char*>(record.uuid.data()), record.uuid.size());

        // signatureHash
        uint32_t hashLen = 0;
        ifs.read(reinterpret_cast<char*>(&hashLen), sizeof(hashLen));
        if (hashLen > 0) {
            record.signatureHash.resize(hashLen);
            ifs.read(reinterpret_cast<char*>(record.signatureHash.data()), hashLen);
        } else {
            record.signatureHash.clear();
        }

        // avRecordSignature
        uint32_t avSigLen = 0;
        ifs.read(reinterpret_cast<char*>(&avSigLen), sizeof(avSigLen));
        if (avSigLen > 0) {
            record.avRecordSignature.resize(avSigLen);
            ifs.read(reinterpret_cast<char*>(record.avRecordSignature.data()), avSigLen);
        } else {
            record.avRecordSignature.clear();
        }

        if (ifs.fail()) {
            return false;
        }

        m_db[record.prefix].push_back(std::move(record));
    }

    size_t totalRecords = 0;
    for (const auto& pair : m_db) {
        totalRecords += pair.second.size();
    }
    if (totalRecords != m_recordCount) {
        return false;
    }

    m_loaded = true;
    return true;
}