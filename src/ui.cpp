#include "ui.h"
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <vector>

// STB TrueType for font loading
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// Define the vertex layout
bgfx::VertexLayout UIVertex::ms_layout;

bool FontAtlas::init(const char* fontPath, float size) {
    fontSize = size;
    
    // Try to load the Old Standard TT font
    const char* systemFont = "src/OldStandardTT-Regular.ttf";
    if (fontPath) systemFont = fontPath;
    
    // Read font file
    std::ifstream fontFile(systemFont, std::ios::binary | std::ios::ate);
    if (!fontFile.is_open()) {
        printf("Failed to open font file: %s, using fallback patterns\n", systemFont);
        return initFallbackFont();
    }
    
    std::streamsize fontFileSize = fontFile.tellg();
    fontFile.seekg(0, std::ios::beg);
    printf("Font file opened: %s, size: %ld bytes\n", systemFont, fontFileSize);
    
    std::vector<uint8_t> fontBuffer(fontFileSize);
    if (!fontFile.read(reinterpret_cast<char*>(fontBuffer.data()), fontFileSize)) {
        printf("Failed to read font file, using fallback patterns\n");
        return initFallbackFont();
    }
    fontFile.close();
    printf("Font file read successfully\n");
    
    // Check font file magic number for debugging
    if (fontFileSize >= 4) {
        printf("Font header bytes: %02X %02X %02X %02X\n", 
               fontBuffer[0], fontBuffer[1], fontBuffer[2], fontBuffer[3]);
    }
    
    // Initialize STB TrueType - try different offsets for font collections
    stbtt_fontinfo fontInfo;
    int offset = stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0);
    printf("STB font offset for index 0: %d\n", offset);
    
    if (offset < 0 || !stbtt_InitFont(&fontInfo, fontBuffer.data(), offset)) {
        printf("STB TrueType failed to parse font data (offset=%d), using fallback patterns\n", offset);
        return initFallbackFont();
    }
    printf("STB TrueType successfully initialized with offset %d!\n", offset);
    
    // Calculate font scale
    float scale = stbtt_ScaleForPixelHeight(&fontInfo, size);
    
    // Create larger atlas for real fonts
    atlasWidth = 512;
    atlasHeight = 512;
    std::vector<uint8_t> fontData(atlasWidth * atlasHeight, 0);
    
    // Pack characters into atlas
    int currentX = 1, currentY = 1;
    int rowHeight = 0;
    
    for (int c = 32; c < 127; c++) {
        int width, height, xoff, yoff;
        uint8_t* bitmap = stbtt_GetCodepointBitmap(&fontInfo, 0, scale, c, &width, &height, &xoff, &yoff);
        
        if (!bitmap) {
            // Handle missing glyphs
            Glyph glyph = {};
            glyph.advance = size * 0.5f; // Default advance
            glyphs[c] = glyph;
            continue;
        }
        
        // Check if we need to move to next row
        if (currentX + width >= atlasWidth - 1) {
            currentX = 1;
            currentY += rowHeight + 1;
            rowHeight = 0;
        }
        
        // Make sure we don't overflow atlas
        if (currentY + height >= atlasHeight - 1) {
            printf("Font atlas overflow, stopping at character %d\n", c);
            stbtt_FreeBitmap(bitmap, nullptr);
            break;
        }
        
        // Copy bitmap to atlas
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int atlasIndex = (currentY + y) * atlasWidth + (currentX + x);
                fontData[atlasIndex] = bitmap[y * width + x];
            }
        }
        
        // Get advance width
        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(&fontInfo, c, &advanceWidth, &leftSideBearing);
        
        // Store glyph info
        Glyph glyph;
        glyph.atlasX = currentX;
        glyph.atlasY = currentY;
        glyph.atlasWidth = width;
        glyph.atlasHeight = height;
        glyph.x = (float)currentX / atlasWidth;
        glyph.y = (float)currentY / atlasHeight;
        glyph.width = (float)width / atlasWidth;
        glyph.height = (float)height / atlasHeight;
        glyph.bearingX = xoff;
        glyph.bearingY = yoff;
        glyph.advance = advanceWidth * scale;
        
        glyphs[c] = glyph;
        
        // Update position
        currentX += width + 1;
        rowHeight = std::max(rowHeight, height);
        
        stbtt_FreeBitmap(bitmap, nullptr);
    }
    
    // Create BGFX texture
    const bgfx::Memory* mem = bgfx::copy(fontData.data(), fontData.size());
    texture = bgfx::createTexture2D(atlasWidth, atlasHeight, false, 1, bgfx::TextureFormat::R8,
                                   BGFX_TEXTURE_NONE | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT, mem);
    
    printf("Real font atlas created: %s, %d characters, %dx%d texture\n", 
           systemFont, (int)glyphs.size(), atlasWidth, atlasHeight);
    return bgfx::isValid(texture);
}

bool FontAtlas::initFallbackFont() {
    // Fallback to simple bitmap patterns if TTF loading fails
    atlasWidth = 128;
    atlasHeight = 128;
    
    unsigned char fontData[128 * 128];
    memset(fontData, 0, sizeof(fontData));
    
    // Create simple bitmap patterns for basic characters
    for (int c = 32; c < 127; c++) {
        int charX = (c - 32) % 16;
        int charY = (c - 32) / 16;
        
        Glyph glyph;
        glyph.atlasX = charX * 8;
        glyph.atlasY = charY * 8;
        glyph.atlasWidth = 8;
        glyph.atlasHeight = 8;
        glyph.x = (float)glyph.atlasX / atlasWidth;
        glyph.y = (float)glyph.atlasY / atlasHeight;
        glyph.width = 8.0f / atlasWidth;
        glyph.height = 8.0f / atlasHeight;
        glyph.bearingX = 0;
        glyph.bearingY = 8;
        glyph.advance = c == ' ' ? 4 : 7;
        
        glyphs[c] = glyph;
        
        // Create simple patterns for common characters
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                int atlasX = charX * 8 + x;
                int atlasY = charY * 8 + y;
                uint8_t pixel = 0;
                
                if (c == ' ') {
                    pixel = 0; // Space is transparent
                } else if (c >= '0' && c <= '9') {
                    // Numbers - simple filled rectangles with holes
                    if ((x >= 1 && x <= 5) && (y >= 1 && y <= 6)) {
                        if ((y == 1 || y == 6) || (x == 1 || x == 5)) {
                            pixel = 255;
                        }
                    }
                } else if (c >= 'A' && c <= 'Z') {
                    // Letters - simple filled patterns
                    if (x >= 1 && x <= 5 && y >= 1 && y <= 6) {
                        pixel = 255;
                    }
                } else if (c >= 'a' && c <= 'z') {
                    // Lowercase - smaller patterns
                    if (x >= 1 && x <= 4 && y >= 3 && y <= 6) {
                        pixel = 255;
                    }
                } else {
                    // Other characters - simple dots/patterns
                    if (x == 3 && y == 3) pixel = 255;
                }
                
                fontData[atlasY * 128 + atlasX] = pixel;
            }
        }
    }
    
    // Create BGFX texture
    const bgfx::Memory* mem = bgfx::copy(fontData, sizeof(fontData));
    texture = bgfx::createTexture2D(atlasWidth, atlasHeight, false, 1, bgfx::TextureFormat::R8,
                                   BGFX_TEXTURE_NONE, mem);
    
    printf("Fallback font atlas created: %dx%d texture\n", atlasWidth, atlasHeight);
    return bgfx::isValid(texture);
}

void FontAtlas::destroy() {
    if (bgfx::isValid(texture)) {
        bgfx::destroy(texture);
        texture = BGFX_INVALID_HANDLE;
    }
    glyphs.clear();
}

const Glyph* FontAtlas::getGlyph(char c) const {
    auto it = glyphs.find(c);
    if (it != glyphs.end()) {
        return &it->second;
    }
    // Return space character as fallback
    auto spaceIt = glyphs.find(' ');
    return spaceIt != glyphs.end() ? &spaceIt->second : nullptr;
}

bool UIRenderer::init(const char* fontPath) {
    printf("UI: Initializing UI renderer...\n");
    
    // Initialize vertex layout
    UIVertex::init();
    
    // Initialize font atlas
    if (!m_fontAtlas.init(fontPath, 16.0f)) {
        printf("UI: Failed to initialize font atlas\n");
        return false;
    }
    
    // Using transient buffers for UI rendering (no need to create dynamic buffers)
    
    // Create uniforms
    m_texColorUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(m_texColorUniform)) {
        printf("UI: Failed to create texture uniform\n");
        return false;
    }
    
    // Load shaders (we'll create simple ones for now)
    m_textProgram = loadProgram("vs_ui_text", "fs_ui_text");
    m_panelProgram = loadProgram("vs_ui_panel", "fs_ui_panel");
    
    m_currentTexture = BGFX_INVALID_HANDLE;
    m_isTextMode = false;
    
    bool success = bgfx::isValid(m_textProgram) && bgfx::isValid(m_panelProgram);
    printf("UI: Initialization %s (text program valid: %d, panel program valid: %d)\n", 
           success ? "SUCCESS" : "FAILED", 
           bgfx::isValid(m_textProgram), 
           bgfx::isValid(m_panelProgram));
    
    return success;
}

void UIRenderer::destroy() {
    m_fontAtlas.destroy();
    
    if (bgfx::isValid(m_textProgram)) bgfx::destroy(m_textProgram);
    if (bgfx::isValid(m_panelProgram)) bgfx::destroy(m_panelProgram);
    if (bgfx::isValid(m_texColorUniform)) bgfx::destroy(m_texColorUniform);
}

void UIRenderer::begin(float screenWidth, float screenHeight) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_vertices.clear();
    m_indices.clear();
    m_currentTexture = BGFX_INVALID_HANDLE;
    m_isTextMode = false;
    
    // Set up UI view like ImGui does
    bgfx::setViewName(UI_VIEW_ID, "UI");
    bgfx::setViewMode(UI_VIEW_ID, bgfx::ViewMode::Sequential);
    
    // Clear depth only (not color, so 3D scene shows through)
    bgfx::setViewClear(UI_VIEW_ID, BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
    
    // Create orthographic projection matrix for UI coordinates
    float ortho[16];
    const bgfx::Caps* caps = bgfx::getCaps();
    bx::mtxOrtho(ortho, 0.0f, screenWidth, screenHeight, 0.0f, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
    bgfx::setViewTransform(UI_VIEW_ID, nullptr, ortho);
    bgfx::setViewRect(UI_VIEW_ID, 0, 0, uint16_t(screenWidth), uint16_t(screenHeight));
    
}

void UIRenderer::end() {
    flushBatch();
}

void UIRenderer::text(float x, float y, const char* text, uint32_t color, float scale) {
    if (!text || !*text) return;
    
    // Switch to text mode if needed
    if (!m_isTextMode || m_currentTexture.idx != m_fontAtlas.texture.idx) {
        flushBatch();
        m_currentTexture = m_fontAtlas.texture;
        m_isTextMode = true;
    }
    
    float currentX = x;
    float charHeight = m_fontAtlas.fontSize * scale;
    
    for (const char* c = text; *c; c++) {
        const Glyph* glyph = m_fontAtlas.getGlyph(*c);
        if (!glyph) continue;
        
        if (*c == ' ') {
            currentX += glyph->advance * scale;
            continue;
        }
        
        float charWidth = glyph->atlasWidth * scale;
        float charX = currentX + glyph->bearingX * scale;
        // Fix baseline - adjust Y by a consistent font baseline offset
        float charY = y + (m_fontAtlas.fontSize * 0.8f) + glyph->bearingY * scale;
        
        // Add quad for this character
        addQuad(charX, charY, charWidth, charHeight, 
                glyph->x, glyph->y, glyph->x + glyph->width, glyph->y + glyph->height, 
                color);
        
        currentX += glyph->advance * scale;
    }
}

void UIRenderer::textCentered(float x, float y, const char* text, uint32_t color, float scale) {
    float width = getTextWidth(text, scale);
    this->text(x - width * 0.5f, y, text, color, scale);
}

void UIRenderer::panel(float x, float y, float width, float height, uint32_t color) {
    // Switch to panel mode if needed
    if (m_isTextMode || bgfx::isValid(m_currentTexture)) {
        flushBatch();
        m_currentTexture = BGFX_INVALID_HANDLE;
        m_isTextMode = false;
    }
    
    // Add quad with no texture (solid color)
    addQuad(x, y, width, height, 0, 0, 1, 1, color);
}

void UIRenderer::panelBordered(float x, float y, float width, float height, uint32_t fillColor, uint32_t borderColor, float borderWidth) {
    // Draw fill
    panel(x + borderWidth, y + borderWidth, width - 2 * borderWidth, height - 2 * borderWidth, fillColor);
    
    // Draw borders
    panel(x, y, width, borderWidth, borderColor); // Top
    panel(x, y + height - borderWidth, width, borderWidth, borderColor); // Bottom
    panel(x, y, borderWidth, height, borderColor); // Left
    panel(x + width - borderWidth, y, borderWidth, height, borderColor); // Right
}

float UIRenderer::getTextWidth(const char* text, float scale) const {
    if (!text || !*text) return 0.0f;
    
    float width = 0.0f;
    for (const char* c = text; *c; c++) {
        const Glyph* glyph = m_fontAtlas.getGlyph(*c);
        if (glyph) {
            width += glyph->advance * scale;
        }
    }
    return width;
}

float UIRenderer::getTextHeight(float scale) const {
    return m_fontAtlas.fontSize * scale;
}

void UIRenderer::flushBatch() {
    if (m_vertices.empty()) return;
    
    // Render batch to GPU
    
    // Use transient buffers like the working direct code
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer tib;
    
    bgfx::allocTransientVertexBuffer(&tvb, m_vertices.size(), UIVertex::ms_layout);
    bgfx::allocTransientIndexBuffer(&tib, m_indices.size());
    
    if (!tvb.data || !tib.data) {
        m_vertices.clear();
        m_indices.clear();
        return;
    }
    
    // Copy data to transient buffers
    bx::memCopy(tvb.data, m_vertices.data(), m_vertices.size() * sizeof(UIVertex));
    bx::memCopy(tib.data, m_indices.data(), m_indices.size() * sizeof(uint16_t));
    
    // Set vertex and index buffers
    bgfx::setVertexBuffer(0, &tvb);
    bgfx::setIndexBuffer(&tib);
    
    // Set up render state - match the working direct code exactly
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
    state |= BGFX_STATE_DEPTH_TEST_ALWAYS;
    bgfx::setState(state);
    
    // Submit rendering command
    if (m_isTextMode && bgfx::isValid(m_currentTexture)) {
        bgfx::setTexture(0, m_texColorUniform, m_currentTexture);
        bgfx::submit(UI_VIEW_ID, m_textProgram);
    } else {
        bgfx::submit(UI_VIEW_ID, m_panelProgram);
    }
    
    // Clear for next batch
    m_vertices.clear();
    m_indices.clear();
}

void UIRenderer::addQuad(float x, float y, float width, float height, float u0, float v0, float u1, float v1, uint32_t color) {
    // Use screen coordinates directly (orthographic projection handles the conversion)
    float x0 = x;
    float y0 = y;
    float x1 = x + width;
    float y1 = y + height;
    
    
    uint16_t baseIndex = (uint16_t)m_vertices.size();
    
    // Add vertices with screen coordinates (orthographic projection will handle conversion)
    m_vertices.push_back({x0, y0, 0.0f, u0, v0, color}); // Top-left
    m_vertices.push_back({x1, y0, 0.0f, u1, v0, color}); // Top-right
    m_vertices.push_back({x1, y1, 0.0f, u1, v1, color}); // Bottom-right
    m_vertices.push_back({x0, y1, 0.0f, u0, v1, color}); // Bottom-left
    
    // Add indices (two triangles)
    m_indices.push_back(baseIndex + 0);
    m_indices.push_back(baseIndex + 1);
    m_indices.push_back(baseIndex + 2);
    m_indices.push_back(baseIndex + 0);
    m_indices.push_back(baseIndex + 2);
    m_indices.push_back(baseIndex + 3);
}

bgfx::ProgramHandle UIRenderer::loadProgram(const char* vsName, const char* fsName) {
    char vsPath[256];
    char fsPath[256];
    snprintf(vsPath, sizeof(vsPath), "shaders/metal/%s.bin", vsName);
    snprintf(fsPath, sizeof(fsPath), "shaders/metal/%s.bin", fsName);
    
    printf("UI: Loading shader program %s/%s\n", vsName, fsName);
    printf("UI: VS path: %s\n", vsPath);
    printf("UI: FS path: %s\n", fsPath);
    
    // Load vertex shader
    FILE* vsFile = fopen(vsPath, "rb");
    if (!vsFile) {
        printf("Failed to open vertex shader: %s\n", vsPath);
        return BGFX_INVALID_HANDLE;
    }
    
    fseek(vsFile, 0, SEEK_END);
    long vsSize = ftell(vsFile);
    fseek(vsFile, 0, SEEK_SET);
    
    const bgfx::Memory* vsMem = bgfx::alloc(vsSize);
    fread(vsMem->data, 1, vsSize, vsFile);
    fclose(vsFile);
    
    bgfx::ShaderHandle vs = bgfx::createShader(vsMem);
    if (!bgfx::isValid(vs)) {
        printf("Failed to create vertex shader from %s\n", vsPath);
        return BGFX_INVALID_HANDLE;
    }
    
    // Load fragment shader
    FILE* fsFile = fopen(fsPath, "rb");
    if (!fsFile) {
        printf("Failed to open fragment shader: %s\n", fsPath);
        bgfx::destroy(vs);
        return BGFX_INVALID_HANDLE;
    }
    
    fseek(fsFile, 0, SEEK_END);
    long fsSize = ftell(fsFile);
    fseek(fsFile, 0, SEEK_SET);
    
    const bgfx::Memory* fsMem = bgfx::alloc(fsSize);
    fread(fsMem->data, 1, fsSize, fsFile);
    fclose(fsFile);
    
    bgfx::ShaderHandle fs = bgfx::createShader(fsMem);
    if (!bgfx::isValid(fs)) {
        printf("Failed to create fragment shader from %s\n", fsPath);
        bgfx::destroy(vs);
        return BGFX_INVALID_HANDLE;
    }
    
    // Create program
    bgfx::ProgramHandle program = bgfx::createProgram(vs, fs, true);
    if (!bgfx::isValid(program)) {
        printf("UI: Failed to create shader program %s/%s\n", vsName, fsName);
        return BGFX_INVALID_HANDLE;
    }
    
    printf("UI: Successfully created shader program %s/%s with handle %d\n", vsName, fsName, program.idx);
    return program;
}