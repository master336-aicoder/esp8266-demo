/*
 * CameraManager - 摄像头管理
 * 支持：拍照、分辨率调节、特效、翻转、镜像
 */

#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include <Arduino.h>
#include <esp_camera.h>
#include <EEPROM.h>
#include "config.h"

class CameraManager {
public:
    CameraManager() : _initialized(false),
        _frameSize(FRAMESIZE_QVGA),
        _quality(12), _effect(0),
        _vflip(false), _hmirror(false) {}

    bool begin() {
        DEBUG_PRINTLN_TAG("CAM", "初始化摄像头...");

        camera_config_t config;
        config.ledc_channel = LEDC_CHANNEL_0;
        config.ledc_timer = LEDC_TIMER_0;
        config.pin_d0 = Y2_GPIO_NUM;
        config.pin_d1 = Y3_GPIO_NUM;
        config.pin_d2 = Y4_GPIO_NUM;
        config.pin_d3 = Y5_GPIO_NUM;
        config.pin_d4 = Y6_GPIO_NUM;
        config.pin_d5 = Y7_GPIO_NUM;
        config.pin_d6 = Y8_GPIO_NUM;
        config.pin_d7 = Y9_GPIO_NUM;
        config.pin_xclk = XCLK_GPIO_NUM;
        config.pin_pclk = PCLK_GPIO_NUM;
        config.pin_vsync = VSYNC_GPIO_NUM;
        config.pin_href = HREF_GPIO_NUM;
        config.pin_sccb_sda = SIOD_GPIO_NUM;
        config.pin_sccb_scl = SIOC_GPIO_NUM;
        config.pin_pwdn = PWDN_GPIO_NUM;
        config.pin_reset = RESET_GPIO_NUM;
        config.xclk_freq_hz = 20000000;
        config.pixel_format = PIXFORMAT_JPEG;

        if (psramFound()) {
            config.frame_size = _frameSize;
            config.jpeg_quality = _quality;
            config.fb_count = 2;
        } else {
            config.frame_size = FRAMESIZE_QQVGA;
            config.jpeg_quality = 12;
            config.fb_count = 1;
            DEBUG_PRINTLN_TAG("CAM", "未检测到PSRAM，降低分辨率");
        }

        esp_err_t err = esp_camera_init(&config);
        if (err != ESP_OK) {
            DEBUG_PRINTF("[CAM] 初始化失败: 0x%x\r\n", err);
            return false;
        }

        _initialized = true;
        sensor_t* s = esp_camera_sensor_get();
        DEBUG_PRINTF("[CAM] 初始化成功，传感器ID: 0x%x\r\n", s->id);

        // 加载保存的配置
        loadConfig();
        applySettings();

        return true;
    }

    // 拍照，返回帧缓冲区（用完后必须调用 returnFrame）
    camera_fb_t* capture() {
        if (!_initialized) return NULL;
        return esp_camera_fb_get();
    }

    void returnFrame(camera_fb_t* fb) {
        if (fb) esp_camera_fb_return(fb);
    }

    // 设置参数
    void setFrameSize(int size) {
        if (!_initialized || size < 0 || size > 12) return;
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_framesize(s, (framesize_t)size);
            _frameSize = (framesize_t)size;
            saveConfig();
        }
    }

    void setQuality(int q) {
        if (!_initialized || q < 4 || q > 63) return;
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_quality(s, q);
            _quality = q;
            saveConfig();
        }
    }

    void setEffect(int e) {
        if (!_initialized || e < 0 || e > 14) return;
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_special_effect(s, e);
            _effect = e;
            saveConfig();
        }
    }

    void setVFlip(bool flip) {
        if (!_initialized) return;
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_vflip(s, flip ? 1 : 0);
            _vflip = flip;
            saveConfig();
        }
    }

    void setHMirror(bool mirror) {
        if (!_initialized) return;
        sensor_t* s = esp_camera_sensor_get();
        if (s) {
            s->set_hmirror(s, mirror ? 1 : 0);
            _hmirror = mirror;
            saveConfig();
        }
    }

    // 获取当前参数
    int getFrameSize() { return _frameSize; }
    int getQuality() { return _quality; }
    int getEffect() { return _effect; }
    bool getVFlip() { return _vflip; }
    bool getHMirror() { return _hmirror; }
    bool isInitialized() { return _initialized; }

    // 获取分辨率名称
    static const char* getResolutionName(int idx) {
        static const char* names[] = {
            "96x96", "QQVGA (160x120)", "QCIF (176x144)",
            "HQVGA (240x176)", "240x240", "QVGA (320x240)",
            "CIF (400x296)", "VGA (640x480)", "SVGA (800x600)",
            "XGA (1024x768)", "HD (1280x720)", "SXGA (1280x1024)",
            "UXGA (1600x1200)"
        };
        if (idx < 0 || idx > 12) return "未知";
        return names[idx];
    }

    static const char* getEffectName(int idx) {
        static const char* names[] = {
            "无", "负片", "灰度", "红色调",
            "绿色调", "蓝色调", "复古", "黑白",
            "正片", "对比度+", "对比度-",
            "亮度+", "亮度-", "饱和度+", "饱和度-"
        };
        if (idx < 0 || idx > 14) return "未知";
        return names[idx];
    }

private:
    bool _initialized;
    framesize_t _frameSize;
    int _quality;
    int _effect;
    bool _vflip;
    bool _hmirror;

    void loadConfig() {
        _frameSize = (framesize_t)EEPROM.read(EEP_ADDR_CAM_RES);
        if (_frameSize > 12) _frameSize = FRAMESIZE_QVGA;

        _quality = EEPROM.read(EEP_ADDR_CAM_QUAL);
        if (_quality < 4 || _quality > 63) _quality = 12;

        _effect = EEPROM.read(EEP_ADDR_CAM_EFF);
        if (_effect > 14) _effect = 0;

        _vflip = EEPROM.read(EEP_ADDR_CAM_FLIP) == 1;
        _hmirror = EEPROM.read(EEP_ADDR_CAM_MIRROR) == 1;
    }

    void saveConfig() {
        EEPROM.write(EEP_ADDR_CAM_RES, _frameSize);
        EEPROM.write(EEP_ADDR_CAM_QUAL, _quality);
        EEPROM.write(EEP_ADDR_CAM_EFF, _effect);
        EEPROM.write(EEP_ADDR_CAM_FLIP, _vflip ? 1 : 0);
        EEPROM.write(EEP_ADDR_CAM_MIRROR, _hmirror ? 1 : 0);
        EEPROM.commit();
    }

    void applySettings() {
        sensor_t* s = esp_camera_sensor_get();
        if (!s) return;

        s->set_framesize(s, _frameSize);
        s->set_quality(s, _quality);
        s->set_special_effect(s, _effect);
        s->set_vflip(s, _vflip ? 1 : 0);
        s->set_hmirror(s, _hmirror ? 1 : 0);
    }
};

#endif // CAMERA_MANAGER_H
