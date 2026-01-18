// =============================================================================
// VOXEL ENGINE - FILE LOGGER
// =============================================================================
#pragma once

#include <fstream>
#include <string>
#include <sstream>
#include <cstdint>
#include <mutex>

namespace voxel {

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    bool open(const std::string& filename) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
        }
        m_file.open(filename, std::ios::out | std::ios::trunc);
        if (m_file.is_open()) {
            m_file << "=== VOXEL ENGINE LOG ===\n\n";
            m_file.flush();
            return true;
        }
        return false;
    }

    void close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    template<typename... Args>
    void log(const char* category, Args&&... args) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open()) return;

        m_file << "[" << category << "] ";
        ((m_file << args), ...);
        m_file << "\n";
        m_file.flush();
    }

    void log_separator() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open()) return;
        m_file << "----------------------------------------\n";
        m_file.flush();
    }

    // Log hex values
    void log_hex(const char* category, const char* label, const void* data, std::size_t size) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open()) return;

        const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
        m_file << "[" << category << "] " << label << ": ";
        for (std::size_t i = 0; i < size; ++i) {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X ", bytes[i]);
            m_file << hex;
        }
        m_file << "\n";
        m_file.flush();
    }

    // Log matrix
    void log_mat4(const char* category, const char* label, const float* m) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open()) return;

        m_file << "[" << category << "] " << label << ":\n";
        for (int row = 0; row < 4; ++row) {
            m_file << "  [";
            for (int col = 0; col < 4; ++col) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%8.4f", static_cast<double>(m[col * 4 + row]));
                m_file << buf;
                if (col < 3) m_file << ", ";
            }
            m_file << "]\n";
        }
        m_file.flush();
    }

private:
    Logger() = default;
    ~Logger() { close(); }

    std::ofstream m_file;
    std::mutex m_mutex;
};

// Convenience macros
#define LOG(...) voxel::Logger::instance().log(__VA_ARGS__)
#define LOG_SEP() voxel::Logger::instance().log_separator()
#define LOG_HEX(cat, label, data, size) voxel::Logger::instance().log_hex(cat, label, data, size)
#define LOG_MAT4(cat, label, m) voxel::Logger::instance().log_mat4(cat, label, m)

} // namespace voxel
