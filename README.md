# 小智 AI 单词卡 v1.0

一块征辰 1.54TFT Wi-Fi 开发板（ESP32-S3）改造成的 **AI 单词学习桌面摆件**：平时显示时钟、今日一词和连续打卡天数；开口说一句话，它就变成会念单词、纠发音的英语家教。

> 基于开源项目 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) v2.4.2 二次开发，保留小智全部语音/AI 能力。

---

## ✨ 主要功能

| 功能 | 说明 |
|---|---|
| 🕐 桌面摆件 | NTP 自动对时的时钟 + 日期星期 + 今日一词 + 连续打卡天数，设备空闲时自动显示 |
| 🔤 今日一词 | 词库按天轮换，英文 + 音标 + 中文释义，中文用板载 6845 字点阵字库 |
| 🎤 语音跟读打分 | 说「教我读单词」→ 它念词、报音标释义 → 你跟读 → AI 判断对错、纠正重音 → 念对自动打卡 |
| ⏭️ 语音翻卡 | 说「下一个 / 换一个」切换单词，屏幕卡片同步更新 |
| 🔥 连续打卡 | 断一天自动归零，数据存在 NVS，掉电不丢 |
| 🗣️ 小智原有能力 | 语音唤醒、打断、自由对话全部保留 |

## 🛠️ 硬件

- **主控**：征辰科技 1.54TFT Wi-Fi 开发板，ESP32-S3-N16R8（16MB Flash + 8MB PSRAM）
- **屏幕**：ST7789，240×240 SPI 彩屏（板载 FPC 排线，零飞线）
- **音频**：板载数字麦克风 + I2S 喇叭功放
- **按键**：BOOT(GPIO0) / VOL+(GPIO10) / VOL-(GPIO39)
- **电源**：GPIO2 电平锁存，ON/OFF 键切换
- **串口**：CH340K（USB 转串口）

详细引脚和架构见 [docs/HARDWARE.md](docs/HARDWARE.md) 与 [assets/hardware-architecture.svg](assets/hardware-architecture.svg)。

## 🧠 技术实现要点

1. **界面**：LVGL 9，`WordCard` 类用 1Hz 的 esp_timer 驱动，只在 `kDeviceStateIdle` 显示，说话时自动隐藏。
2. **中文**：复用小智资源分区里的 `font_noto_sans_common_20_4.bin`（6845 汉字 + 音标符号），无需额外字库。
3. **对时**：ESP-SNTP，`CST-8` 北京时间，阿里云 / pool.ntp.org 双服务器。
4. **AI 教学**：通过 3 个 MCP 工具（`self.word.get_today` / `next` / `mark_passed`）把词条和"教学流程指令"交给服务端大模型主持，设备端只做数据与显示，不碰 ASR/TTS 底层。
5. **线程安全**：MCP 回调只读写数据，界面刷新统一由定时器任务持 `DisplayLockGuard` 完成。

详见 [docs/TECHNICAL.md](docs/TECHNICAL.md)。

## 📁 目录结构

```
.
├── src/
│   ├── word_data.h                    # 词库（改这里加词）
│   ├── word_card.h                    # 摆件界面 + 打卡逻辑 + MCP 数据接口
│   └── board_patch.md                 # 需要在板级文件里加的几行代码
├── docs/
│   ├── HARDWARE.md                    # 硬件资料、引脚表、架构
│   ├── TECHNICAL.md                   # 关键技术实现
│   └── BUILD_AND_FLASH.md             # 环境搭建、编译、烧录教程
├── assets/
│   └── hardware-architecture.svg      # 硬件架构图
└── README.md
```

## 🚀 快速开始

想自己编译？需要 ESP-IDF 5.5 + 小智 v2.4.2 源码。完整步骤见 [docs/BUILD_AND_FLASH.md](docs/BUILD_AND_FLASH.md)。

核心三步：

```bash
# 1. 把 src/ 下三个文件放进小智源码的 main/boards/zhengchen/1.54tft-wifi/
# 2. 按 board_patch.md 修改板级初始化文件
python scripts/build.py zhengchen/1.54tft-wifi

# 3. 手动进下载模式后烧录（关键：先按住 BOOT 再上电，详见 docs/BUILD_AND_FLASH.md）
python -m esptool --chip esp32s3 -p COM5 -b 460800 --before no-reset --after hard-reset \
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x20000 build/xiaozhi.bin \
  0x800000 build/generated_assets.bin
```

## 📄 许可

本项目修改部分采用 MIT 许可；上游 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 及其依赖遵循各自许可。
