# 环境搭建 · 编译 · 烧录教程

面向小白的完整流程。已在 Windows + ESP-IDF 5.5.6 + 小智 v2.4.2 上实测通过。

## 0. 版本组合（不要混搭）

| 组件 | 版本 | 原因 |
|---|---|---|
| xiaozhi-esp32 | **v2.4.2** | v2.5.0+ 要求 ESP-IDF 6.x，对新手不友好 |
| ESP-IDF | **5.5.6** | Windows 离线安装器，自带编译工具链和 Python |
| 板型 | `zhengchen/1.54tft-wifi` | 征辰 1.54TFT Wi-Fi 开发板 |

## 1. 安装 ESP-IDF

1. 下载 ESP-IDF v5.5.6 Windows 离线安装器（约 1.6GB）：https://dl.espressif.cn/dl/esp-idf/
2. 安装到纯英文路径，例如 `D:\Espressif`（**路径不能有中文**）。
3. 装完桌面出现 "ESP-IDF 5.5 PowerShell" 快捷方式，双击即是可用的编译终端。

## 2. 获取源码

```bash
git clone --branch v2.4.2 --depth 1 https://github.com/78/xiaozhi-esp32.git
```

解压/克隆到纯英文路径，例如 `D:\xiaozhi-esp32`。

## 3. 放入本项目代码

把本项目 `src/` 下三个文件复制到源码的 `main/boards/zhengchen/1.54tft-wifi/`：

```
word_data.h
word_card.h
```

并按 `src/board_patch.md` 修改 `zhengchen-1.54tft-wifi.cc`（加 include、加一个成员、注册工具、启动卡片）。

## 4. 编译

```bash
python scripts/build.py zhengchen/1.54tft-wifi
```

首次编译约 20~30 分钟（2000+ 文件）；后续改这几个文件的增量编译约 1 分钟。

产物在 `build/`：`xiaozhi.bin`、`bootloader/bootloader.bin`、`partition_table/partition-table.bin`、`ota_data_initial.bin`、`generated_assets.bin`。

## 5. 烧录（关键：手动进下载模式）

这块板的自动下载电路不工作，必须手动操作。

**步骤 A：进下载模式**
1. 按 ON/OFF 关机（屏幕熄灭）
2. 按住 BOOT 键不放
3. 保持按住，按一下 ON/OFF 开机
4. 继续按住 BOOT 约 3 秒
5. **先松 BOOT，最后松 ON/OFF**

成功标志：屏幕全黑不亮。若你的板子没有电池：拔线 → 按住 BOOT → 插线 → 保持按住按一下 ON/OFF → 3 秒后松 BOOT。
可用串口是否静默 2 秒来确认进没进下载模式（静默 = 进了）。

**步骤 B：烧录**

```bash
python -m esptool --chip esp32s3 -p COM5 -b 460800 --before no-reset --after hard-reset \
  write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x20000 build/xiaozhi.bin \
  0x800000 build/generated_assets.bin
```

- `COM5` 换成你的实际串口（设备管理器里看 USB-SERIAL CH340K）。
- 若 `esptool.exe` 被安全软件拦截，改用 `python -m esptool`。
- 全部 5 个分区出现 `Hash of data verified` 即成功。

**步骤 C：开机**

烧完板子会断电，按 ON/OFF 开机。首次可能需重新配网（手机连 `Xiaozhi-XXXX` 热点 → 浏览器 192.168.4.1 填 WiFi → xiaozhi.me 绑定验证码）。

## 6. 使用

- 空闲：显示时钟 + 今日一词 + 打卡天数
- 说「你好小智，教我读单词」开始跟读课
- 说「下一个 / 换一个」换词

## 常见故障

| 现象 | 原因 / 处理 |
|---|---|
| esptool 读到 `0x4B`/`0x45` 乱流 | 没进下载模式，重做步骤 A |
| `Could not open COM5, port is busy` | 关掉占用串口的程序（Arduino IDE 串口监视器等），或拔插 USB |
| 编译报 `gpio_input_get` 未声明 | 用了不兼容的屏幕库，本项目用 LovyanGFX / 板载方案，不需要 TFT_eSPI |
| 屏幕白屏/反色 | 确认 `invert = true` |
| 时钟一直 `--:--` | 等 5~10 秒联网对时；确认 WiFi 已连 |
