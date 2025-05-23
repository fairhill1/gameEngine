#pragma once

#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <string>
#include <vector>
#include <unordered_map>

// Font glyph information for SDF rendering
struct Glyph {
    float x, y, width, height;     // Position in atlas (normalized 0-1)
    float bearingX, bearingY;      // Glyph metrics
    float advance;                 // Horizontal advance
    int atlasX, atlasY;           // Pixel coordinates in atlas
    int atlasWidth, atlasHeight;   // Pixel size in atlas
};

// Font atlas containing SDF texture and glyph data
struct FontAtlas {
    bgfx::TextureHandle texture;
    std::unordered_map<char, Glyph> glyphs;
    int atlasWidth, atlasHeight;
    float fontSize;
    
    bool init(const char* fontPath, float size);
    void destroy();
    const Glyph* getGlyph(char c) const;
};

// UI vertex format for text and panels
struct UIVertex {
    float x, y, z;        // Position (3 floats to match shader)
    float u, v;           // Texture coordinates
    uint32_t color;       // Vertex color
    
    static void init() {
        ms_layout
            .begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)  // Changed to 3 floats
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();
    }
    
    static bgfx::VertexLayout ms_layout;
};

// Main UI rendering system
class UIRenderer {
private:
    FontAtlas m_fontAtlas;
    
    // Rendering resources
    bgfx::ProgramHandle m_textProgram;
    bgfx::ProgramHandle m_panelProgram;
    bgfx::UniformHandle m_texColorUniform;
    
    // Vertex/index data for batching (using transient buffers)
    std::vector<UIVertex> m_vertices;
    std::vector<uint16_t> m_indices;
    
    // Screen dimensions for UI coordinate system
    float m_screenWidth, m_screenHeight;
    
    // Current rendering state
    bgfx::TextureHandle m_currentTexture;
    bool m_isTextMode;
    
    // UI view ID for separate rendering pass
    static const bgfx::ViewId UI_VIEW_ID = 10;
    
public:
    bool init(const char* fontPath = nullptr);
    void destroy();
    
    // Frame management
    void begin(float screenWidth, float screenHeight);
    void end();
    
    // Text rendering
    void text(float x, float y, const char* text, uint32_t color = 0xFFFFFFFF, float scale = 1.0f);
    void textCentered(float x, float y, const char* text, uint32_t color = 0xFFFFFFFF, float scale = 1.0f);
    
    // Panel rendering
    void panel(float x, float y, float width, float height, uint32_t color = 0x80000000);
    void panelBordered(float x, float y, float width, float height, uint32_t fillColor = 0x80000000, uint32_t borderColor = 0xFFFFFFFF, float borderWidth = 1.0f);
    
    // Utility functions
    float getTextWidth(const char* text, float scale = 1.0f) const;
    float getTextHeight(float scale = 1.0f) const;
    
    
private:
    void flushBatch();
    void addQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1, uint32_t color);
    bgfx::ProgramHandle loadProgram(const char* vsName, const char* fsName);
};

// Color utilities
namespace UIColors {
    constexpr uint32_t WHITE = 0xFFFFFFFF;
    constexpr uint32_t BLACK = 0xFF000000;
    constexpr uint32_t RED = 0xFFFF0000;
    constexpr uint32_t GREEN = 0xFF00FF00;
    constexpr uint32_t BLUE = 0xFF0000FF;
    constexpr uint32_t YELLOW = 0xFFFFFF00;
    constexpr uint32_t CYAN = 0xFF00FFFF;
    constexpr uint32_t MAGENTA = 0xFFFF00FF;
    constexpr uint32_t GRAY = 0xFF808080;
    constexpr uint32_t DARK_GRAY = 0xFF404040;
    constexpr uint32_t LIGHT_GRAY = 0xFFC0C0C0;
    
    // Game-specific colors
    constexpr uint32_t PANEL_BG = 0xE0202020;      // Semi-transparent dark
    constexpr uint32_t PANEL_BORDER = 0xFF606060;   // Light gray border
    constexpr uint32_t TEXT_NORMAL = 0xFFE0E0E0;    // Light gray text
    constexpr uint32_t TEXT_HIGHLIGHT = 0xFF00FFFF; // Cyan highlight
    constexpr uint32_t TEXT_WARNING = 0xFFFFFF00;   // Yellow warning
    constexpr uint32_t TEXT_ERROR = 0xFFFF4040;     // Red error
}