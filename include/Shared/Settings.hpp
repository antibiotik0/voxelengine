// =============================================================================
// VOXEL ENGINE - SETTINGS LOADER
// Simple TOML-like config parser
// =============================================================================
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>

namespace voxel {

class Settings {
public:
    // Load settings from file
    bool load(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::printf("[Settings] Failed to open: %s\n", filepath.c_str());
            return false;
        }

        std::string line;
        std::string current_section;

        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Trim whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            // Section header [section]
            if (line[0] == '[') {
                size_t end = line.find(']');
                if (end != std::string::npos) {
                    current_section = line.substr(1, end - 1);
                }
                continue;
            }

            // Key = value
            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // Trim key
            size_t key_end = key.find_last_not_of(" \t");
            if (key_end != std::string::npos) {
                key = key.substr(0, key_end + 1);
            }

            // Trim value
            size_t val_start = value.find_first_not_of(" \t");
            if (val_start != std::string::npos) {
                value = value.substr(val_start);
            }
            size_t val_end = value.find_last_not_of(" \t\r\n");
            if (val_end != std::string::npos) {
                value = value.substr(0, val_end + 1);
            }

            // Remove quotes from strings
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            // Store with section prefix
            std::string full_key = current_section.empty() ? key : current_section + "." + key;
            m_values[full_key] = value;
        }

        std::printf("[Settings] Loaded %zu settings from %s\n", m_values.size(), filepath.c_str());
        return true;
    }

    // Get string value
    std::string get_string(const std::string& key, const std::string& default_val = "") const {
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            return it->second;
        }
        return default_val;
    }

    // Get float value
    float get_float(const std::string& key, float default_val = 0.0f) const {
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            char* end = nullptr;
            float val = std::strtof(it->second.c_str(), &end);
            if (end != it->second.c_str()) {
                return val;
            }
        }
        return default_val;
    }

    // Get int value
    int get_int(const std::string& key, int default_val = 0) const {
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            char* end = nullptr;
            long val = std::strtol(it->second.c_str(), &end, 10);
            if (end != it->second.c_str()) {
                return static_cast<int>(val);
            }
        }
        return default_val;
    }

    // Get bool value
    bool get_bool(const std::string& key, bool default_val = false) const {
        auto it = m_values.find(key);
        if (it != m_values.end()) {
            const std::string& v = it->second;
            return v == "true" || v == "1" || v == "yes";
        }
        return default_val;
    }

    // Check if key exists
    bool has(const std::string& key) const {
        return m_values.find(key) != m_values.end();
    }

    // Singleton access
    static Settings& instance() {
        static Settings s;
        return s;
    }

private:
    std::unordered_map<std::string, std::string> m_values;
};

} // namespace voxel
