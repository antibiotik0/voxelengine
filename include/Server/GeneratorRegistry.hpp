// =============================================================================
// VOXEL ENGINE - GENERATOR REGISTRY
// Runtime-swappable world generator system
// =============================================================================
#pragma once

#include "WorldGenerator.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <vector>
#include <cstdio>

namespace voxel::server {

// =============================================================================
// GENERATOR FACTORY FUNCTION TYPE
// =============================================================================
using GeneratorFactory = std::function<std::unique_ptr<WorldGenerator>(std::uint64_t seed)>;

// =============================================================================
// GENERATOR REGISTRY (Singleton)
// Allows runtime registration and creation of world generators
// =============================================================================
class GeneratorRegistry {
public:
    // Singleton access
    static GeneratorRegistry& instance() {
        static GeneratorRegistry registry;
        return registry;
    }
    
    // =============================================================================
    // REGISTRATION
    // =============================================================================
    
    // Register a generator factory
    void register_generator(const std::string& name, GeneratorFactory factory) {
        m_factories[name] = std::move(factory);
        std::printf("[GeneratorRegistry] Registered generator: %s\n", name.c_str());
    }
    
    // Register with template (convenience)
    template<typename T>
    void register_generator(const std::string& name) {
        register_generator(name, [](std::uint64_t seed) -> std::unique_ptr<WorldGenerator> {
            auto gen = std::make_unique<T>();
            // If the generator has a seed setter, we could call it here
            return gen;
        });
    }
    
    // =============================================================================
    // CREATION
    // =============================================================================
    
    // Create generator by name
    [[nodiscard]] std::unique_ptr<WorldGenerator> create(
        std::string_view name, 
        std::uint64_t seed = 0
    ) const {
        auto it = m_factories.find(std::string(name));
        if (it != m_factories.end()) {
            return it->second(seed);
        }
        
        std::printf("[GeneratorRegistry] Unknown generator: %.*s\n", 
                    static_cast<int>(name.size()), name.data());
        return nullptr;
    }
    
    // =============================================================================
    // QUERY
    // =============================================================================
    
    // Check if generator exists
    [[nodiscard]] bool has_generator(const std::string& name) const {
        return m_factories.find(name) != m_factories.end();
    }
    
    // Get list of registered generator names
    [[nodiscard]] std::vector<std::string> list_generators() const {
        std::vector<std::string> names;
        names.reserve(m_factories.size());
        for (const auto& [name, _] : m_factories) {
            names.push_back(name);
        }
        return names;
    }
    
    // Get count of registered generators
    [[nodiscard]] std::size_t count() const noexcept {
        return m_factories.size();
    }
    
private:
    GeneratorRegistry() {
        // Register built-in generators
        register_defaults();
    }
    
    void register_defaults() {
        // Superflat - default
        register_generator("superflat", [](std::uint64_t seed) {
            auto config = SuperflatConfig::classic();
            config.seed = seed;
            return std::make_unique<SuperflatGenerator>(config);
        });
        
        // Superflat - stone world variant
        register_generator("stone_world", [](std::uint64_t seed) {
            auto config = SuperflatConfig::stone_world(32);
            config.seed = seed;
            return std::make_unique<SuperflatGenerator>(config);
        });
        
        // Superflat - deep stone
        register_generator("deep_stone", [](std::uint64_t seed) {
            auto config = SuperflatConfig::stone_world(64);
            config.seed = seed;
            return std::make_unique<SuperflatGenerator>(config);
        });
        
        // Flat grass (single grass layer)
        register_generator("flat_grass", [](std::uint64_t seed) {
            SuperflatConfig config;
            config.seed = seed;
            config.layers[0] = { VoxelType::STONE, 1 };
            config.layers[1] = { VoxelType::DIRT, 3 };
            config.layers[2] = { VoxelType::GRASS, 1 };
            config.layer_count = 3;
            return std::make_unique<SuperflatGenerator>(config);
        });
        
        // Water world (for testing fluids)
        register_generator("water_world", [](std::uint64_t seed) {
            SuperflatConfig config;
            config.seed = seed;
            config.layers[0] = { VoxelType::STONE, 1 };
            config.layers[1] = { VoxelType::SAND, 2 };
            config.layers[2] = { VoxelType::WATER, 4 };
            config.layer_count = 3;
            return std::make_unique<SuperflatGenerator>(config);
        });
    }
    
    std::unordered_map<std::string, GeneratorFactory> m_factories;
};

// =============================================================================
// CONVENIENCE FUNCTION
// =============================================================================

// Create a generator by name using the global registry
[[nodiscard]] inline std::unique_ptr<WorldGenerator> create_generator(
    const std::string& name, 
    std::uint64_t seed = 0
) {
    return GeneratorRegistry::instance().create(name, seed);
}

} // namespace voxel::server
