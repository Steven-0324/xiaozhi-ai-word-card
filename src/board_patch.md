# 板级文件改动说明

文件：`main/boards/zhengchen/1.54tft-wifi/zhengchen-1.54tft-wifi.cc`

这是需要在上游小智源码的板级文件里做的 4 处小改动。本项目是把单词卡功能"嫁接"到小智现成的语音/AI 管道上，所以只动这一处板级文件。

## 改动 1：加头文件

在 `#include` 区加两行：

```cpp
#include "mcp_server.h"
#include "word_card.h"
```

## 改动 2：加成员变量

在 `ZHENGCHEN_1_54TFT_WIFI` 类的 private 区加：

```cpp
    WordCard word_card_;
```

（放在 `PowerManager* power_manager_;` 附近即可）

## 改动 3：注册 MCP 工具

把空的 `InitializeTools()` 替换成：

```cpp
    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.word.get_today",
            "Get the English word for read-aloud (跟读) practice. "
            "Use this tool whenever the user wants to learn, practice or read English words, "
            "e.g. 教单词、读单词、背单词、学英语、跟读、单词打卡、开始上课.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return word_card_.GetCurrentWordJson();
            });

        mcp.AddTool("self.word.next",
            "Get the next English word for read-aloud practice. "
            "Use this when the user says 下一个、换一个、next word during word practice.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                word_card_.AdvanceWord();
                return word_card_.GetCurrentWordJson();
            });

        mcp.AddTool("self.word.mark_passed",
            "Mark that the user has pronounced the word correctly in read-aloud practice. "
            "Use this ONLY after the user pronounced the word correctly.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                word_card_.MarkPassed();
                return true;
            });
    }
```

## 改动 4：启动单词卡

在构造函数末尾，`GetBacklight()->RestoreBrightness();` 之前加一行：

```cpp
        word_card_.Start(display_);
```

完整构造函数尾部应该是：

```cpp
        InitializeSt7789Display();
        InitializeTools();
        word_card_.Start(display_);
        GetBacklight()->RestoreBrightness();
```

---

完成后即可按 [BUILD_AND_FLASH.md](../docs/BUILD_AND_FLASH.md) 编译烧录。
