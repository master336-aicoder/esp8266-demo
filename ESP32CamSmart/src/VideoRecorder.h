/*
 * VideoRecorder - 视频录制
 * 将连续 JPEG 帧保存为 AVI 格式视频到 SD 卡
 */

#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <Arduino.h>
#include <SD.h>
#include <esp_camera.h>
#include "config.h"
#include "CameraManager.h"
#include "SdCardManager.h"

// AVI 文件头结构（简化版，MJPEG 格式）
struct AviHeader {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // 文件大小 - 8
    char avi[4];            // "AVI "
    char hdrl[4];           // "hdrl"
    uint32_t hdrlSize;      // hdrl 块大小
    char avih[4];           // "avih"
    uint32_t avihSize;      // avih 块大小 (56)
    uint32_t microSecPerFrame;  // 每帧微秒
    uint32_t maxBytesPerSec;    // 最大字节率
    uint32_t paddingGranularity; // 0
    uint32_t flags;             // 0
    uint32_t totalFrames;       // 总帧数
    uint32_t initialFrames;     // 0
    uint32_t streams;           // 流数量 (1)
    uint32_t suggestedBufferSize; // 建议缓冲区大小
    uint32_t width;             // 宽度
    uint32_t height;            // 高度
    uint32_t scale;             // 时间刻度
    uint32_t rate;              // 速率 = scale * fps
    uint32_t start;             // 0
    uint32_t length;            // 总帧数
    // strl 块
    char strl[4];           // "strl"
    uint32_t strlSize;      // strl 块大小
    char strh[4];           // "strh"
    uint32_t strhSize;      // strh 块大小 (56)
    char fccType[4];        // "vids"
    char fccHandler[4];     // "MJPG"
    uint32_t flags2;        // 0
    uint16_t priority;      // 0
    uint16_t language;      // 0
    uint32_t initialFrames2; // 0
    uint32_t scale2;         // 时间刻度
    uint32_t rate2;          // 速率
    uint32_t start2;         // 0
    uint32_t length2;        // 总帧数
    uint32_t suggestedBufferSize2; // 建议缓冲区大小
    uint32_t quality2;       // 0
    uint32_t sampleSize;     // 0
    // strf 块
    char strf[4];           // "strf"
    uint32_t strfSize;      // strf 块大小 (40)
    uint32_t biSize;        // 40
    int32_t biWidth;        // 宽度
    int32_t biHeight;       // 高度
    uint16_t biPlanes;      // 1
    uint16_t biBitCount;    // 24
    uint32_t biCompression;  // "MJPG"
    uint32_t biSizeImage;    // 0
    int32_t biXPelsPerMeter; // 0
    int32_t biYPelsPerMeter; // 0
    uint32_t biClrUsed;      // 0
    uint32_t biClrImportant; // 0
    // movi 块
    char movi[4];           // "movi"
    uint32_t moviSize;      // movi 块大小
};

class VideoRecorder {
public:
    VideoRecorder(CameraManager* cam, SdCardManager* sd) :
        _camera(cam), _sd(sd),
        _recording(false), _frameCount(0),
        _fps(15), _maxDurationSec(300),
        _startTime(0), _totalSize(0) {}

    bool startRecording(String filename = "") {
        if (_recording) return false;
        if (!_sd->isInitialized()) return false;

        // 生成文件名
        if (filename.isEmpty()) {
            char buf[64];
            uint32_t now = millis() / 1000;
            snprintf(buf, sizeof(buf), "/video_%05lu.avi", now);
            filename = String(buf);
        }

        _file = _sd->openFile(filename, FILE_WRITE);
        if (!_file) {
            DEBUG_PRINTF("[REC] 创建文件失败: %s\r\n", filename.c_str());
            return false;
        }

        // 写入占位的 AVI 头（后面更新）
        AviHeader hdr;
        memset(&hdr, 0, sizeof(hdr));
        memcpy(hdr.riff, "RIFF", 4);
        hdr.fileSize = sizeof(hdr) - 8;
        memcpy(hdr.avi, "AVI ", 4);
        memcpy(hdr.hdrl, "hdrl", 4);
        hdr.hdrlSize = sizeof(hdr) - 12;
        memcpy(hdr.avih, "avih", 4);
        hdr.avihSize = 56;
        hdr.microSecPerFrame = 1000000 / _fps;
        hdr.totalFrames = 0;
        hdr.streams = 1;
        hdr.width = 320;  // 临时值
        hdr.height = 240;
        hdr.scale = 1;
        hdr.rate = _fps;
        hdr.length = 0;

        memcpy(hdr.strl, "strl", 4);
        hdr.strlSize = 100;
        memcpy(hdr.strh, "strh", 4);
        hdr.strhSize = 56;
        memcpy(hdr.fccType, "vids", 4);
        memcpy(hdr.fccHandler, "MJPG", 4);
        hdr.scale2 = 1;
        hdr.rate2 = _fps;
        hdr.length2 = 0;

        memcpy(hdr.strf, "strf", 4);
        hdr.strfSize = 40;
        hdr.biSize = 40;
        hdr.biWidth = 320;
        hdr.biHeight = 240;
        hdr.biPlanes = 1;
        hdr.biBitCount = 24;
        hdr.biCompression = 0x47504A4D; // "MJPG"

        memcpy(hdr.movi, "movi", 4);
        hdr.moviSize = 0;

        _file.write((uint8_t*)&hdr, sizeof(hdr));
        _headerOffset = 0;
        _frameCount = 0;
        _totalSize = sizeof(hdr);
        _filename = filename;
        _recording = true;
        _startTime = millis();

        DEBUG_PRINTF("[REC] 开始录制: %s\r\n", filename.c_str());
        return true;
    }

    bool addFrame(camera_fb_t* fb) {
        if (!_recording || !fb) return false;

        // 写入帧标记 "00dc"
        char tag[4] = {'0', '0', 'd', 'c'};
        _file.write((uint8_t*)tag, 4);

        // 写入帧大小
        uint32_t size = fb->len;
        _file.write((uint8_t*)&size, 4);

        // 写入帧数据
        _file.write(fb->buf, fb->len);

        // 对齐到 4 字节
        int padding = (4 - (size % 4)) % 4;
        if (padding > 0) {
            uint8_t pad[4] = {0};
            _file.write(pad, padding);
        }

        _frameCount++;
        _totalSize += 8 + size + padding;

        // 检查是否达到最大时长
        if (millis() - _startTime >= _maxDurationSec * 1000UL) {
            DEBUG_PRINTLN("[REC] 达到最大时长，自动停止");
            stopRecording();
            return false;
        }

        return true;
    }

    bool captureAndAddFrame() {
        if (!_recording || !_camera) return false;

        camera_fb_t* fb = _camera->capture();
        if (!fb) return false;

        bool ok = addFrame(fb);
        _camera->returnFrame(fb);
        return ok;
    }

    void stopRecording() {
        if (!_recording) return;

        // 更新 AVI 头
        uint32_t fileSize = _totalSize;
        uint32_t moviSize = _totalSize - sizeof(AviHeader);

        _file.seek(4);
        uint32_t fs = fileSize - 8;
        _file.write((uint8_t*)&fs, 4);

        // 更新总帧数
        _file.seek(48);
        _file.write((uint8_t*)&_frameCount, 4);

        // 更新 movi 大小
        _file.seek(sizeof(AviHeader) - 4);
        _file.write((uint8_t*)&moviSize, 4);

        _file.close();
        _recording = false;

        _sd->updateSpace();

        DEBUG_PRINTF("[REC] 录制完成: %s\r\n", _filename.c_str());
        DEBUG_PRINTF("[REC] 总帧数: %lu, 总大小: %s\r\n",
            _frameCount, _sd->formatBytes(_totalSize).c_str());
    }

    bool isRecording() { return _recording; }
    uint32_t getFrameCount() { return _frameCount; }
    uint32_t getDurationMs() { return millis() - _startTime; }
    String getFilename() { return _filename; }

    void setFps(int fps) { if (fps > 0 && fps <= 30) _fps = fps; }
    int getFps() { return _fps; }

    void setMaxDuration(int sec) { if (sec > 0) _maxDurationSec = sec; }
    int getMaxDuration() { return _maxDurationSec; }

private:
    CameraManager* _camera;
    SdCardManager* _sd;
    File _file;

    bool _recording;
    uint32_t _frameCount;
    int _fps;
    int _maxDurationSec;
    uint32_t _startTime;
    size_t _totalSize;
    size_t _headerOffset;
    String _filename;
};

#endif // VIDEO_RECORDER_H
