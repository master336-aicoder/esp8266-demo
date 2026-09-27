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

        if (!SD.begin(SD_CS_PIN, SPI, 40000000)) {
            DEBUG_PRINTLN_TAG("SD", "SD卡初始化失败");
            return false;
        }

        _initialized = true;
        updateSpace();

        DEBUG_PRINTF("[SD] 初始化成功\r\n");
        DEBUG_PRINTF("[SD] 总空间: %s\r\n", formatBytes(_totalBytes).c_str());
        DEBUG_PRINTF("[SD] 已用: %s\r\n", formatBytes(_usedBytes).c_str());
        DEBUG_PRINTF("[SD] 可用: %s\r\n", formatBytes(_totalBytes - _usedBytes).c_str());

        return true;
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

    // 列出目录
    String listDir(String path, int levels = 1) {
        if (!_initialized) return "[]";

        String json = "[";
        bool first = true;

        File root = SD.open(path);
        if (!root || !root.isDirectory()) return "[]";

        File file = root.openNextFile();
        while (file) {
            if (!first) json += ",";
            first = false;

            json += "{";
            json += "\"name\":\"" + String(file.name()) + "\",";
            json += "\"isDir\":" + String(file.isDirectory() ? "true" : "false") + ",";
            if (!file.isDirectory()) {
                json += "\"size\":" + String(file.size()) + ",";
            }
            // 获取修改时间（如果有 RTC 的话）
            json += "\"path\":\"" + path + "/" + file.name() + "\"";
            json += "}";

            file.close();
            file = root.openNextFile();
        }

        root.close();
        json += "]";
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

    // 格式化SD卡（删除所有文件）
    bool format() {
        if (!_initialized) return false;
        DEBUG_PRINTLN_TAG("SD", "正在清空SD卡...");
        deleteRecursive("/");
        updateSpace();
        DEBUG_PRINTLN_TAG("SD", "清空完成");
        return true;
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
    size_t _totalBytes;
    size_t _usedBytes;

    void deleteRecursive(String path) {
        File root = SD.open(path);
        if (!root || !root.isDirectory()) return;
        File file = root.openNextFile();
        while (file) {
            String filePath = path + (path.endsWith("/") ? "" : "/") + file.name();
            if (file.isDirectory()) {
                file.close();
                deleteRecursive(filePath);
                SD.rmdir(filePath);
            } else {
                file.close();
                SD.remove(filePath);
            }
            file = root.openNextFile();
        }
        root.close();
    }
};

#endif // SD_CARD_MANAGER_H
