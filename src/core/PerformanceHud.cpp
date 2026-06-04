#include "core/PerformanceHud.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>

namespace sr {

namespace {

Color fromBGRA(std::uint32_t value)
{
    return {
        static_cast<std::uint8_t>((value >> 16) & 0xff),
        static_cast<std::uint8_t>((value >> 8) & 0xff),
        static_cast<std::uint8_t>(value & 0xff),
        static_cast<std::uint8_t>((value >> 24) & 0xff),
    };
}

std::uint8_t blendChannel(std::uint8_t source, std::uint8_t target, std::uint8_t alpha)
{
    const int inverseAlpha = 255 - static_cast<int>(alpha);
    return static_cast<std::uint8_t>(
        (static_cast<int>(source) * static_cast<int>(alpha) + static_cast<int>(target) * inverseAlpha) / 255);
}

void blendPixel(Framebuffer& framebuffer, int x, int y, Color color)
{
    if (x < 0 || y < 0 || x >= framebuffer.width() || y >= framebuffer.height()) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(framebuffer.width()) + static_cast<std::size_t>(x);
    if (color.a == 255) {
        framebuffer.pixels()[index] = color.toBGRA();
        return;
    }

    const Color target = fromBGRA(framebuffer.pixels()[index]);
    const Color blended {
        blendChannel(color.r, target.r, color.a),
        blendChannel(color.g, target.g, color.a),
        blendChannel(color.b, target.b, color.a),
        255,
    };
    framebuffer.pixels()[index] = blended.toBGRA();
}

void fillRect(Framebuffer& framebuffer, int left, int top, int width, int height, Color color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    const int right = std::min(framebuffer.width(), left + width);
    const int bottom = std::min(framebuffer.height(), top + height);
    const int startX = std::max(0, left);
    const int startY = std::max(0, top);
    for (int y = startY; y < bottom; ++y) {
        for (int x = startX; x < right; ++x) {
            blendPixel(framebuffer, x, y, color);
        }
    }
}

std::array<std::uint8_t, 7> glyphRows(char character)
{
    switch (character) {
    case '0':
        return { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 };
    case '1':
        return { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 };
    case '2':
        return { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 };
    case '3':
        return { 0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110 };
    case '4':
        return { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 };
    case '5':
        return { 0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110 };
    case '6':
        return { 0b01110, 0b10000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 };
    case '7':
        return { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 };
    case '8':
        return { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 };
    case '9':
        return { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00001, 0b01110 };
    case 'A':
        return { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 };
    case 'B':
        return { 0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110 };
    case 'C':
        return { 0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110 };
    case 'D':
        return { 0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110 };
    case 'E':
        return { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111 };
    case 'F':
        return { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 };
    case 'G':
        return { 0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01110 };
    case 'H':
        return { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 };
    case 'I':
        return { 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 };
    case 'J':
        return { 0b00111, 0b00010, 0b00010, 0b00010, 0b10010, 0b10010, 0b01100 };
    case 'K':
        return { 0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001 };
    case 'L':
        return { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 };
    case 'M':
        return { 0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001 };
    case 'N':
        return { 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001 };
    case 'O':
        return { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 };
    case 'P':
        return { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 };
    case 'Q':
        return { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 };
    case 'R':
        return { 0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001 };
    case 'S':
        return { 0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110 };
    case 'T':
        return { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100 };
    case 'U':
        return { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 };
    case 'V':
        return { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100 };
    case 'W':
        return { 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010 };
    case 'X':
        return { 0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001 };
    case 'Y':
        return { 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100 };
    case 'Z':
        return { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111 };
    case '.':
        return { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100 };
    case ':':
        return { 0b00000, 0b01100, 0b01100, 0b00000, 0b01100, 0b01100, 0b00000 };
    case '-':
        return { 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000 };
    case '/':
        return { 0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000 };
    default:
        return { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000 };
    }
}

void drawGlyph(Framebuffer& framebuffer, int left, int top, char character, Color color, int scale)
{
    const std::array<std::uint8_t, 7> rows = glyphRows(character);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            const std::uint8_t mask = static_cast<std::uint8_t>(1u << (4 - column));
            if ((rows[static_cast<std::size_t>(row)] & mask) == 0) {
                continue;
            }

            fillRect(framebuffer, left + column * scale, top + row * scale, scale, scale, color);
        }
    }
}

std::string toUpperAscii(std::string text)
{
    for (char& character : text) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return text;
}

void drawText(Framebuffer& framebuffer, int left, int top, const std::string& text, Color color, int scale)
{
    int x = left;
    for (char character : toUpperAscii(text)) {
        if (character != ' ') {
            drawGlyph(framebuffer, x, top, character, color, scale);
        }
        x += 6 * scale;
        if (x >= framebuffer.width()) {
            break;
        }
    }
}

std::string formatDouble(double value, int precision)
{
    char buffer[32] = {};
    const char* pattern = precision == 2 ? "%.2f" : "%.1f";
    std::snprintf(buffer, sizeof(buffer), pattern, value);
    return buffer;
}

std::string formatMilliseconds(double value)
{
    return formatDouble(std::max(0.0, value), value < 10.0 ? 2 : 1) + " MS";
}

std::string formatCount(std::uint64_t value)
{
    char buffer[32] = {};
    if (value >= 1000000000ull) {
        std::snprintf(buffer, sizeof(buffer), "%.2fB", static_cast<double>(value) / 1000000000.0);
    } else if (value >= 1000000ull) {
        std::snprintf(buffer, sizeof(buffer), "%.2fM", static_cast<double>(value) / 1000000.0);
    } else if (value >= 1000ull) {
        std::snprintf(buffer, sizeof(buffer), "%.1fK", static_cast<double>(value) / 1000.0);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
    }
    return buffer;
}

void drawBar(Framebuffer& framebuffer, int left, int top, int width, int height, double value, double maxValue, Color color)
{
    fillRect(framebuffer, left, top, width, height, { 32, 38, 50, 210 });
    if (width <= 2 || height <= 2 || maxValue <= 0.0) {
        return;
    }

    const double ratio = std::clamp(value / maxValue, 0.0, 1.0);
    const int fillWidth = static_cast<int>(std::round(ratio * static_cast<double>(width - 2)));
    fillRect(framebuffer, left + 1, top + 1, fillWidth, height - 2, color);
}

void drawTimedRow(
    Framebuffer& framebuffer,
    int textX,
    int& y,
    int barX,
    int barWidth,
    int lineHeight,
    int scale,
    const std::string& label,
    double value,
    double maxValue,
    Color textColor,
    Color barColor)
{
    drawText(framebuffer, textX, y, label + " " + formatMilliseconds(value), textColor, scale);
    drawBar(framebuffer, barX, y + scale, barWidth, std::max(3, 5 * scale), value, maxValue, barColor);
    y += lineHeight;
}

double smoothValue(double previous, double current)
{
    if (!std::isfinite(current)) {
        return previous;
    }
    constexpr double blend = 0.18;
    return previous + (current - previous) * blend;
}

} // namespace

void PerformanceMonitor::submit(const FrameStats& stats)
{
    if (!initialized_) {
        smoothed_ = stats;
        initialized_ = true;
        return;
    }

    const double previousShadowMilliseconds = smoothed_.renderer.shadowPassMilliseconds;
    const double previousMainMilliseconds = smoothed_.renderer.mainPassMilliseconds;
    smoothed_.framesPerSecond = smoothValue(smoothed_.framesPerSecond, stats.framesPerSecond);
    smoothed_.frameMilliseconds = smoothValue(smoothed_.frameMilliseconds, stats.frameMilliseconds);
    smoothed_.updateMilliseconds = smoothValue(smoothed_.updateMilliseconds, stats.updateMilliseconds);
    smoothed_.renderMilliseconds = smoothValue(smoothed_.renderMilliseconds, stats.renderMilliseconds);
    smoothed_.hudMilliseconds = smoothValue(smoothed_.hudMilliseconds, stats.hudMilliseconds);
    smoothed_.presentMilliseconds = smoothValue(smoothed_.presentMilliseconds, stats.presentMilliseconds);
    smoothed_.framebufferWidth = stats.framebufferWidth;
    smoothed_.framebufferHeight = stats.framebufferHeight;
    smoothed_.renderModeName = stats.renderModeName;
    smoothed_.renderer = stats.renderer;
    smoothed_.renderer.shadowPassMilliseconds = smoothValue(
        previousShadowMilliseconds,
        stats.renderer.shadowPassMilliseconds);
    smoothed_.renderer.mainPassMilliseconds = smoothValue(
        previousMainMilliseconds,
        stats.renderer.mainPassMilliseconds);
}

bool PerformanceMonitor::hasStats() const
{
    return initialized_;
}

const FrameStats& PerformanceMonitor::displayStats() const
{
    return smoothed_;
}

void drawPerformanceHud(Framebuffer& framebuffer, const FrameStats& stats)
{
    if (framebuffer.width() <= 0 || framebuffer.height() <= 0) {
        return;
    }

    const int scale = framebuffer.width() >= 760 && framebuffer.height() >= 430 ? 2 : 1;
    const int margin = 6 * scale;
    const int panelWidth = std::min(framebuffer.width() - margin * 2, scale == 2 ? 456 : 304);
    const int lineHeight = 9 * scale;
    const int rowCount = 12;
    const int panelHeight = std::min(framebuffer.height() - margin * 2, 22 * scale + lineHeight * rowCount);
    if (panelWidth <= 0 || panelHeight <= 0) {
        return;
    }

    const int panelX = margin;
    const int panelY = margin;
    const int textX = panelX + 8 * scale;
    int y = panelY + 8 * scale;
    const int barX = textX + 132 * scale;
    const int barWidth = std::max(16 * scale, panelX + panelWidth - barX - 8 * scale);

    fillRect(framebuffer, panelX, panelY, panelWidth, panelHeight, { 9, 13, 20, 212 });
    fillRect(framebuffer, panelX, panelY, panelWidth, 2 * scale, { 110, 196, 255, 235 });

    const Color titleColor { 178, 230, 255, 255 };
    const Color textColor { 230, 235, 228, 255 };
    const Color mutedColor { 176, 185, 194, 255 };
    const Color updateColor { 118, 214, 151, 245 };
    const Color renderColor { 255, 193, 106, 245 };
    const Color shadowColor { 170, 146, 255, 245 };
    const Color mainColor { 255, 128, 145, 245 };
    const Color presentColor { 116, 203, 230, 245 };

    const double frameMax = std::max(1.0, stats.frameMilliseconds);
    const double renderMax = std::max(1.0, stats.renderMilliseconds);

    drawText(framebuffer, textX, y, "CPU RASTERIZER PERF", titleColor, scale);
    y += lineHeight;
    drawText(
        framebuffer,
        textX,
        y,
        "FPS " + formatDouble(std::max(0.0, stats.framesPerSecond), 1) + "  FRAME " + formatMilliseconds(stats.frameMilliseconds),
        textColor,
        scale);
    y += lineHeight;
    drawTimedRow(framebuffer, textX, y, barX, barWidth, lineHeight, scale, "UPDATE", stats.updateMilliseconds, frameMax, mutedColor, updateColor);
    drawTimedRow(framebuffer, textX, y, barX, barWidth, lineHeight, scale, "RENDER", stats.renderMilliseconds, frameMax, mutedColor, renderColor);
    drawTimedRow(
        framebuffer,
        textX,
        y,
        barX,
        barWidth,
        lineHeight,
        scale,
        "SHADOW",
        stats.renderer.shadowPassMilliseconds,
        renderMax,
        mutedColor,
        shadowColor);
    drawTimedRow(framebuffer, textX, y, barX, barWidth, lineHeight, scale, "MAIN", stats.renderer.mainPassMilliseconds, renderMax, mutedColor, mainColor);
    drawTimedRow(framebuffer, textX, y, barX, barWidth, lineHeight, scale, "PRESENT", stats.presentMilliseconds, frameMax, mutedColor, presentColor);
    drawText(
        framebuffer,
        textX,
        y,
        "TRI IN " + formatCount(stats.renderer.inputTriangles) + "  RAST " + formatCount(stats.renderer.rasterizedTriangles),
        textColor,
        scale);
    y += lineHeight;
    drawText(
        framebuffer,
        textX,
        y,
        "SHADOW TRI " + formatCount(stats.renderer.shadowTriangles) + "  DRAWS " + formatCount(stats.renderer.drawCommands),
        textColor,
        scale);
    y += lineHeight;
    drawText(
        framebuffer,
        textX,
        y,
        "PIX SHADED " + formatCount(stats.renderer.shadedPixels) + "  WRITTEN " + formatCount(stats.renderer.colorPixelsWritten),
        textColor,
        scale);
    y += lineHeight;
    drawText(framebuffer, textX, y, "SHADOW Z WRITES " + formatCount(stats.renderer.shadowDepthWrites), textColor, scale);
    y += lineHeight;
    drawText(
        framebuffer,
        textX,
        y,
        "MODE " + stats.renderModeName + "  " + formatCount(static_cast<std::uint64_t>(stats.framebufferWidth)) + "X"
            + formatCount(static_cast<std::uint64_t>(stats.framebufferHeight)),
        mutedColor,
        scale);
}

} // namespace sr
