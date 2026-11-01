/*
 * VideoRecorder - 视频录制
 * 将连续 JPEG 帧封装为标准 AVI(MJPEG) 视频保存到 SD 卡
 * 结构：RIFF/AVI + LIST(hdrl) + LIST(movi) + idx1 索引
 */

#ifndef VIDEO_RECORDER_H
#define VIDEO_RECORDER_H

#include <Arduino.h>
#include <SD.h>
#include <esp_camera.h>
#include "config.h"
#include "CameraManager.h"
#include "SdCardManager.h"

// AVI 头部关键偏移（字节）
#define AVI_OFF_RIFF_SIZE     4
#define AVI_OFF_TOTAL_FRAMES 48
#define AVI_OFF_STRH_LENGTH  140
#define AVI_OFF_MOVI_SIZE    216
#define AVI_OFF_MOVI_DATA    224
#define AVI_MAX_INDEX        3000

class VideoRecorder {
public:
    VideoRecorder(CameraManager* cam, SdCardManager* sd) :
        _camera(cam), _sd(sd),
        _recording(false), _frameCount(0), _fps(10), _maxDurationSec(300),
        _startTime(0), _totalSize(0), _moviBytes(0), _maxFrameSize(0) {}

    bool startRecording(String filename = "", int fps = -1) {
        if (_recording) return false;
        if (!_sd || !_sd->isInitialized()) return false;
        if (!_camera || !_camera->isInitialized()) return false;
        if (fps > 0 && fps <= 30) _fps = fps;

        if (filename.isEmpty()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "/video_%08lu.avi", (unsigned long)(millis() / 1000));
            filename = String(buf);
        }

        _file = _sd->openFile(filename, FILE_WRITE);
        if (!_file) {
            DEBUG_PRINTF("[REC] 创建文件失败: %s\r\n", filename.c_str());
            return false;
        }

        int w = _camera->getWidth();
        int h = _camera->getHeight();
        writeHeader((uint32_t)w, (uint32_t)h);

        _filename = filename;
        _frameCount = 0;
        _moviBytes = 0;
        _maxFrameSize = 0;
        _totalSize = AVI_OFF_MOVI_DATA;
        _recording = true;
        _startTime = millis();

        DEBUG_PRINTF("[REC] 开始录制: %s (%d x %d @ %d fps)\r\n",
            filename.c_str(), w, h, _fps);
        return true;
    }

    bool addFrame(camera_fb_t* fb) {
        if (!_recording || !fb) return false;

        // SD 剩余空间保护（留 64KB）
        if (_sd->getFreeBytes() < 65536) {
            DEBUG_PRINTLN_TAG("REC", "SD空间不足，停止录制");
            stopRecording();
            return false;
        }

        uint32_t chunkOffset = _moviBytes + 4; // 相对于 movi fourcc

        // 帧块 "00dc" + 长度 + 数据 + 4字节对齐
        char tag[4] = {'0', '0', 'd', 'c'};
        _file.write((uint8_t*)tag, 4);
        uint32_t size = fb->len;
        _file.write((uint8_t*)&size, 4);
        size_t written = _file.write(fb->buf, fb->len);
        if (written != fb->len) {
            DEBUG_PRINTLN_TAG("REC", "写入失败，停止录制");
            stopRecording();
            return false;
        }

        uint8_t padding = (uint8_t)((4 - (size % 4)) % 4);
        if (padding > 0) {
            uint8_t pad[4] = {0};
            _file.write(pad, padding);
        }

        if (_frameCount < AVI_MAX_INDEX) {
            _idxOffset[_frameCount] = chunkOffset;
            _idxSize[_frameCount] = size;
        }
        _frameCount++;
        _moviBytes += 8 + size + padding;
        _totalSize = AVI_OFF_MOVI_DATA + _moviBytes;
        if (size > _maxFrameSize) _maxFrameSize = size;

        if (millis() - _startTime >= (uint32_t)_maxDurationSec * 1000UL ||
            _frameCount >= AVI_MAX_INDEX) {
            DEBUG_PRINTLN_TAG("REC", "达到录制上限，自动停止");
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

        uint32_t frames = _frameCount > AVI_MAX_INDEX ? AVI_MAX_INDEX : _frameCount;

        // 写入 idx1 索引块
        _file.seek(_totalSize);
        uint32_t idxSize = frames * 16;
        if (frames > 0) {
            char idx1[4] = {'i', 'd', 'x', '1'};
            _file.write((uint8_t*)idx1, 4);
            _file.write((uint8_t*)&idxSize, 4);
            for (uint32_t i = 0; i < frames; i++) {
                char id[4] = {'0', '0', 'd', 'c'};
                uint32_t flags = 0x10; // AVIIF_KEYFRAME
                _file.write((uint8_t*)id, 4);
                _file.write((uint8_t*)&flags, 4);
                _file.write((uint8_t*)&_idxOffset[i], 4);
                _file.write((uint8_t*)&_idxSize[i], 4);
            }
        }

        uint32_t fileSize = _totalSize + (frames > 0 ? 8 + idxSize : 0);

        // 回填头部关键字段
        patchU32(AVI_OFF_RIFF_SIZE, fileSize - 8);
        patchU32(AVI_OFF_TOTAL_FRAMES, _frameCount);
        patchU32(AVI_OFF_STRH_LENGTH, _frameCount);
        // movi LIST 的大小必须包含 "movi" 四个字节的类型码，否则播放器按大小跳过时会错位
        patchU32(AVI_OFF_MOVI_SIZE, _moviBytes + 4);
        // 建议缓冲区大小
        patchU32(60, _maxFrameSize);

        _file.close();
        _recording = false;
        _sd->updateSpace();

        DEBUG_PRINTF("[REC] 录制完成: %s\r\n", _filename.c_str());
        DEBUG_PRINTF("[REC] 帧数: %lu, 大小: %s\r\n",
            (unsigned long)_frameCount, _sd->formatBytes(fileSize).c_str());
    }

    bool isRecording() { return _recording; }
    uint32_t getFrameCount() { return _frameCount; }
    uint32_t getDurationMs() { return _recording ? (millis() - _startTime) : 0; }
    String getFilename() { return _filename; }
    size_t getFileSize() { return _totalSize; }
    int getFps() { return _fps; }
    void setFps(int fps) { if (fps > 0 && fps <= 30) _fps = fps; }
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
    uint32_t _totalSize;
    uint32_t _moviBytes;
    uint32_t _maxFrameSize;
    String _filename;

    uint32_t _idxOffset[AVI_MAX_INDEX];
    uint32_t _idxSize[AVI_MAX_INDEX];

    void w4(const char* s) { _file.write((uint8_t*)s, 4); }
    void wU32(uint32_t v) { _file.write((uint8_t*)&v, 4); }
    void wU16(uint16_t v) { _file.write((uint8_t*)&v, 2); }

    void writeHeader(uint32_t width, uint32_t height) {
        w4("RIFF");
        wU32(0);              // RIFF size，稍后回填
        w4("AVI ");

        w4("LIST");
        wU32(192);            // hdrl 列表长度（偏移20~212）
        w4("hdrl");

        w4("avih");
        wU32(56);
        wU32(1000000UL / (uint32_t)_fps); // 每帧微秒
        wU32(0);              // 最大字节率
        wU32(0);              // 对齐粒度
        wU32(0x10);           // AVIF_HASINDEX
        wU32(0);              // 总帧数，稍后回填
        wU32(0);              // 初始帧
        wU32(1);              // 流数量
        wU32(0);              // 建议缓冲区，稍后回填
        wU32(width);
        wU32(height);
        for (int i = 0; i < 4; i++) wU32(0); // 保留

        w4("LIST");
        wU32(116);            // strl 列表长度 = 4("strl") + 64(strh) + 48(strf)，漏算 strf 会让严格的播放器解析错位
        w4("strl");

        w4("strh");
        wU32(56);
        w4("vids");
        w4("MJPG");
        wU32(0);              // flags
        wU16(0);              // priority
        wU16(0);              // language
        wU32(0);              // initialFrames
        wU32(1);              // scale
        wU32((uint32_t)_fps); // rate
        wU32(0);              // start
        wU32(0);              // length，稍后回填
        wU32(0);              // suggestedBufferSize
        wU32(0xFFFFFFFF);     // quality = -1
        wU32(0);              // sampleSize
        wU16(0); wU16(0);     // rcFrame.left/top
        wU16((uint16_t)width); wU16((uint16_t)height);

        w4("strf");
        wU32(40);
        wU32(40);             // biSize
        wU32(width);
        wU32(height);
        wU16(1);              // biPlanes
        wU16(24);             // biBitCount
        w4("MJPG");           // biCompression
        wU32(0);              // biSizeImage
        wU32(0); wU32(0);     // 分辨率单位
        wU32(0); wU32(0);     // 调色板

        w4("LIST");
        wU32(0);              // movi 大小，稍后回填
        w4("movi");
    }

    void patchU32(uint32_t offset, uint32_t value) {
        _file.seek(offset);
        _file.write((uint8_t*)&value, 4);
    }
};

#endif // VIDEO_RECORDER_H
