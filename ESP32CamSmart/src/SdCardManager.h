/*
 * SdCardManager - SD 卡管理
 * 支持：文件浏览、上传、下载、删除、空间统计
 */

#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"

class SdCardManager {
public:
    SdCardManager() : _initialized(false), _totalBytes(0), _usedBytes(0) {}

    bool begin() {
        DEBUG_PRINTLN_TAG("SD", "初始化SD卡...");

        SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

        // 依次尝试不同频率（高速失败时降速），空白卡自动建 FAT
        uint32_t freqs[] = {20000000, 10000000, 5000000, 2500000};
        bool ok = false;
        for (int i = 0; i < 4; i++) {
            ok = SD.begin(SD_CS_PIN, SPI, freqs[i], "/sd", 5, true);
            if (ok) {
                DEBUG_PRINTF("[SD] %lu MHz 挂载成功\r\n", freqs[i] / 1000000);
                break;
            }
            DEBUG_PRINTF("[SD] %lu MHz 挂载失败\r\n", freqs[i] / 1000000);
            delay(100);
        }

        if (!ok) {
            DEBUG_PRINTF("[SD] SD卡初始化失败，卡类型: %s（请检查卡是否插好/已格式化）\r\n",
                getCardTypeName().c_str());
            return false;
        }

        _initialized = true;
        updateSpace();

        DEBUG_PRINTF("[SD] 初始化成功，类型: %s\r\n", getCardTypeName().c_str());
        DEBUG_PRINTF("[SD] 总空间: %s\r\n", formatBytes(_totalBytes).c_str());
        DEBUG_PRINTF("[SD] 已用: %s\r\n", formatBytes(_usedBytes).c_str());
        DEBUG_PRINTF("[SD] 可用: %s\r\n", formatBytes(_totalBytes - _usedBytes).c_str());

        return true;
    }

    String getCardTypeName() {
        switch (SD.cardType()) {
            case CARD_MMC:  return "MMC";
            case CARD_SD:   return "SD";
            case CARD_SDHC: return "SDHC";
            default:        return "未知";
        }
    }

    bool isInitialized() { return _initialized; }

    // 获取空间信息
    size_t getTotalBytes() { return _totalBytes; }
    size_t getUsedBytes() { return _usedBytes; }
    size_t getFreeBytes() { return _totalBytes - _usedBytes; }

    void updateSpace() {
        if (!_initialized) return;
        _totalBytes = SD.totalBytes();
        _usedBytes = SD.usedBytes();
    }

    // 列出目录（限制条目数，避免大目录耗尽堆）
    String listDir(String path, int levels = 1) {
        if (!_initialized) return "[]";

        File root = SD.open(path);
        if (!root || !root.isDirectory()) return "[]";

        String json;
        json.reserve(1024);  // 预留一次，减少反复扩容产生的堆碎片
        json = "[";

        bool first = true;
        int count = 0;
        const int MAX_ITEMS = 100;  // 上限，防止目录过大时 String 无限增长

        File file = root.openNextFile();
        while (file) {
            if (count >= MAX_ITEMS) break;
            count++;

            if (!first) json += ',';
            first = false;

            const char* name = file.name();
            bool isDir = file.isDirectory();

            json += F("{\"name\":\"");
            json += name;
            json += F("\",\"isDir\":");
            json += isDir ? F("true") : F("false");

            if (!isDir) {
                char sz[48];
                snprintf(sz, sizeof(sz), ",\"size\":%lu", (unsigned long)file.size());
                json += sz;
            }

            json += F(",\"path\":\"");
            json += joinPath(path, String(name));
            json += F("\"}");

            file.close();
            file = root.openNextFile();
        }

        root.close();
        json += ']';
        return json;
    }

    // 读取文件
    bool readFile(String path, uint8_t* buffer, size_t bufferSize, size_t& bytesRead) {
        if (!_initialized) return false;

        File f = SD.open(path, FILE_READ);
        if (!f) return false;

        bytesRead = f.read(buffer, bufferSize);
        f.close();
        return true;
    }

    // 写入文件
    bool writeFile(String path, const uint8_t* data, size_t len) {
        if (!_initialized) return false;

        File f = SD.open(path, FILE_WRITE);
        if (!f) return false;

        size_t written = f.write(data, len);
        f.close();

        updateSpace();
        return written == len;
    }

    // 追加写入
    bool appendFile(String path, const uint8_t* data, size_t len) {
        if (!_initialized) return false;

        File f = SD.open(path, FILE_APPEND);
        if (!f) return false;

        size_t written = f.write(data, len);
        f.close();

        updateSpace();
        return written == len;
    }

    // 删除文件
    bool deleteFile(String path) {
        if (!_initialized) return false;
        bool ok = SD.remove(path);
        if (ok) updateSpace();
        return ok;
    }

    // 创建目录
    bool makeDir(String path) {
        if (!_initialized) return false;
        return SD.mkdir(path);
    }

    // 格式化SD卡（快速格式化：删除根目录下所有文件与目录）
    bool format() {
        if (!_initialized) return false;
        DEBUG_PRINTLN_TAG("SD", "正在格式化（清空所有文件）...");
        _deleteFailed = false;
        deleteRecursive("/");
        updateSpace();
        DEBUG_PRINTF("[SD] 格式化完成，可用: %s\r\n",
            formatBytes(_totalBytes - _usedBytes).c_str());
        return !_deleteFailed;
    }

    // 删除目录（必须为空）
    bool removeDir(String path) {
        if (!_initialized) return false;
        return SD.rmdir(path);
    }

    // 检查文件是否存在
    bool exists(String path) {
        if (!_initialized) return false;
        return SD.exists(path);
    }

    // 获取文件大小
    size_t fileSize(String path) {
        if (!_initialized) return 0;
        File f = SD.open(path, FILE_READ);
        if (!f) return 0;
        size_t s = f.size();
        f.close();
        return s;
    }

    // 打开文件（用于流式读取）
    File openFile(String path, const char* mode = FILE_READ) {
        if (!_initialized) return File();
        return SD.open(path, mode);
    }

    String formatBytes(size_t bytes) {
        if (bytes < 1024) return String(bytes) + " B";
        if (bytes < 1024 * 1024) return String(bytes / 1024.0f, 1) + " KB";
        if (bytes < 1024 * 1024 * 1024) return String(bytes / (1024.0f * 1024.0f), 1) + " MB";
        return String(bytes / (1024.0f * 1024.0f * 1024.0f), 2) + " GB";
    }

private:
    bool _initialized;
    bool _deleteFailed = false;
    size_t _totalBytes;
    size_t _usedBytes;

    String joinPath(const String& dir, const String& name) {
        if (name.startsWith("/")) return name;
        if (dir == "/" || dir.isEmpty()) return "/" + name;
        return dir + "/" + name;
    }

    void deleteRecursive(String path) {
        File root = SD.open(path);
        if (!root || !root.isDirectory()) return;
        File file = root.openNextFile();
        while (file) {
            String filePath = joinPath(path, String(file.name()));
            if (file.isDirectory()) {
                file.close();
                deleteRecursive(filePath);
                if (!SD.rmdir(filePath)) _deleteFailed = true;
            } else {
                file.close();
                if (!SD.remove(filePath)) _deleteFailed = true;
            }
            file = root.openNextFile();
        }
        root.close();
    }
};

#endif // SD_CARD_MANAGER_H
