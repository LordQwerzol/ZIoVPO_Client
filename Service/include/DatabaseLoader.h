#pragma once
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include "DataClass.h"

using AvDatabase = std::map<uint64_t, std::vector<AvRecord>>;

class DatabaseLoader {
public:

    DatabaseLoader(const DatabaseLoader&) = delete;
    DatabaseLoader& operator=(const DatabaseLoader&) = delete;

    static DatabaseLoader& instance();

    bool load(const std::string& filepath);

    bool isLoaded() const { return m_loaded; }

    const AvDatabase& getDatabase() const { return m_db; }

    uint64_t getTimestamp() const { return m_timestamp; }
    uint32_t getRecordCount() const { return m_recordCount; }

private:
    DatabaseLoader() = default;
    ~DatabaseLoader() = default;

    AvDatabase m_db;
    uint64_t m_timestamp = 0;
    uint32_t m_recordCount = 0;
    bool m_loaded = false;
};