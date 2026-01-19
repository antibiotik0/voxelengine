#pragma once

#include "imgui.h"
#include <cstdint>
#include <string>
#include <memory>

struct DebugOverlayData {
    // Position & Raycast Info
    int64_t world_x = 0;
    int64_t world_y = 0;
    int64_t world_z = 0;
    
    int32_t chunk_x = 0;
    int32_t chunk_y = 0;
    int32_t chunk_z = 0;
    
    int32_t local_x = 0;
    int32_t local_y = 0;
    int32_t local_z = 0;
    
    // Target Block Info
    bool has_target = false;
    int64_t target_world_x = 0;
    int64_t target_world_y = 0;
    int64_t target_world_z = 0;
    uint8_t target_type = 0;
    int32_t target_normal_x = 0;
    int32_t target_normal_y = 0;
    int32_t target_normal_z = 0;
    
    // Performance
    float fps = 0.0f;
    float frame_time_ms = 0.0f;
    
    // Rendering
    uint32_t meshes_rebuilt = 0;
    uint32_t chunk_count = 0;
    
    // Physics & Player State
    float player_x = 0.0f;
    float player_y = 0.0f;
    float player_z = 0.0f;
    
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;
    float velocity_z = 0.0f;
    
    bool on_ground = false;
};

class ImGuiDebugOverlay {
public:
    ImGuiDebugOverlay() : m_visible(false) {}
    
    void init() {
        // Context is already initialized in main.cpp
    }
    
    void shutdown() {
        // Context is destroyed in main.cpp
    }
    
    void begin_frame() {
        ImGui::NewFrame();
    }
    
    void render(const DebugOverlayData& data) {
        if (!m_visible) return;
        
        // F3 Debug Overlay - Top Left Corner
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.85f);
        
        if (ImGui::Begin("Debug Overlay (F3)", &m_visible, 
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
            
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Voxel Engine Debug");
            ImGui::Separator();
            
            // === PERFORMANCE ===
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Performance");
            ImGui::Text("FPS: %.1f", static_cast<double>(data.fps));
            ImGui::Text("Frame Time: %.2f ms", static_cast<double>(data.frame_time_ms));
            ImGui::Text("Meshes Rebuilt: %u", data.meshes_rebuilt);
            ImGui::Text("Chunks Loaded: %u", data.chunk_count);
            
            ImGui::Separator();
            
            // === PLAYER POSITION ===
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Player Position");
            ImGui::Text("World: (%lld, %lld, %lld)", 
                       static_cast<long long>(data.world_x), 
                       static_cast<long long>(data.world_y), 
                       static_cast<long long>(data.world_z));
            ImGui::Text("Chunk: (%d, %d, %d)", 
                       data.chunk_x, data.chunk_y, data.chunk_z);
            ImGui::Text("Local: (%d, %d, %d)", 
                       data.local_x, data.local_y, data.local_z);
            
            ImGui::Separator();
            
            // === PLAYER PHYSICS ===
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Player Physics");
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", 
                       static_cast<double>(data.player_x), 
                       static_cast<double>(data.player_y), 
                       static_cast<double>(data.player_z));
            ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", 
                       static_cast<double>(data.velocity_x), 
                       static_cast<double>(data.velocity_y), 
                       static_cast<double>(data.velocity_z));
            ImGui::Text("On Ground: %s", data.on_ground ? "Yes" : "No");
            
            ImGui::Separator();
            
            // === TARGET BLOCK ===
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Target Block");
            if (data.has_target) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), 
                                  "Block: (%lld, %lld, %lld)", 
                                  static_cast<long long>(data.target_world_x), 
                                  static_cast<long long>(data.target_world_y), 
                                  static_cast<long long>(data.target_world_z));
                ImGui::Text("Type: %u", data.target_type);
                ImGui::Text("Face Normal: (%d, %d, %d)", 
                           data.target_normal_x, data.target_normal_y, data.target_normal_z);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No block targeted");
            }
            
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Press F3 to toggle | F4 for physics");
        }
        ImGui::End();
    }
    
    void end_frame() {
        ImGui::Render();
    }
    
    void toggle_visibility() {
        m_visible = !m_visible;
    }
    
    bool is_visible() const {
        return m_visible;
    }
    
    void set_visible(bool visible) {
        m_visible = visible;
    }
    
    // Get ImGui IO for input handling if needed
    ImGuiIO& get_io() {
        return ImGui::GetIO();
    }

private:
    bool m_visible;
};
