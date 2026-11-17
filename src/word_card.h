#ifndef WORD_CARD_H
#define WORD_CARD_H

// ============================================================
// 单词卡摆件（idle 界面）
//   - 时钟（NTP 对时，CST-8 北京时间）
//   - 今日一词（按天自动轮换）
//   - 连续打卡天数（断一天归零）
// 只在设备空闲（kDeviceStateIdle）时显示，一说话就自动隐藏
// ============================================================

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_sntp.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/time.h>
#include <memory>

#include "board.h"
#include "application.h"
#include "settings.h"
#include "device_state.h"
#include "display/display.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_font.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "word_data.h"

// 30px 内置字库（ASCII 齐全，用来显示数字和英文单词，比 20px 清晰得多）
LV_FONT_DECLARE(font_noto_sans_basic_30_4);

#define WORD_CARD_TAG "WordCard"

class WordCard {
public:
    WordCard() = default;

    ~WordCard() {
        if (timer_ != nullptr) {
            esp_timer_stop(timer_);
            esp_timer_delete(timer_);
            timer_ = nullptr;
        }
    }

    // 在板子构造函数里调用：此时 LVGL 还没初始化完，所以这里只建定时器
    void Start(LcdDisplay* display) {
        if (display == nullptr || timer_ != nullptr) {
            return;
        }
        display_ = display;

        esp_timer_create_args_t args = {
            .callback = [](void* arg) { static_cast<WordCard*>(arg)->OnTick(); },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "word_card",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&args, &timer_) != ESP_OK) {
            ESP_LOGE(WORD_CARD_TAG, "创建定时器失败");
            return;
        }
        esp_timer_start_periodic(timer_, 1000 * 1000);
        ESP_LOGI(WORD_CARD_TAG, "单词卡已启动");
    }

    // ============================================================
    // 供 MCP 工具调用（运行在 MCP 任务里：只做数据读写，不碰 LVGL，
    // 界面由 1Hz 的 OnTick 自动刷新）
    // ============================================================

    // 当前练习词条的 JSON（内含给大模型的教学指令）
    std::string GetCurrentWordJson() {
        const WordEntry& w = kWordBook[CurrentIndex()];
        char buf[2048];
        snprintf(buf, sizeof(buf),
            "{\"word\":\"%s\",\"phonetic\":\"%s\",\"meaning\":\"%s\","
            "\"instruction\":\"这是一节英语跟读课。请严格按以下步骤进行："
            "1.用英语慢速清晰地读出单词%s，只读单词本身；"
            "2.用中文告诉用户音标%s和释义；"
            "3.对用户说『跟我读一遍%s』，然后等用户开口；"
            "4.用户跟读后，把听到的内容和目标单词%s比较："
            "如果用户正确说出了这个单词(允许轻微口音)，表扬用户并调用self.word.mark_passed工具；"
            "如果用户说错、说成别的词或没听清，简短指出问题(比如重音在第几音节)，让用户再读一遍，最多重试3次；"
            "5.用户说『下一个』『换一个』时，调用self.word.next工具；"
            "6.全程用简短的中文交流，不要闲聊其他话题。\"}",
            w.word, w.phonetic, w.meaning,
            w.word, w.phonetic, w.word, w.word);
        return std::string(buf);
    }

    // 用户说「下一个」：当天内偏移 +1
    void AdvanceWord() {
        session_offset_++;
        ESP_LOGI(WORD_CARD_TAG, "切换到下一个单词");
    }

    // 用户念对了：打卡 +1（同一天重复调用不会重复计数）
    void MarkPassed() {
        time_t now = time(nullptr);
        struct tm tm_now {};
        localtime_r(&now, &tm_now);
        int today_key =
            (tm_now.tm_year + 1900) * 10000 + (tm_now.tm_mon + 1) * 100 + tm_now.tm_mday;
        UpdateStreak(today_key, YesterdayKey(now));
        ESP_LOGI(WORD_CARD_TAG, "跟读通过，打卡 %d 天", streak_);
    }

private:
    LcdDisplay* display_ = nullptr;
    esp_timer_handle_t timer_ = nullptr;

    lv_obj_t* card_ = nullptr;
    lv_obj_t* clock_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* word_label_ = nullptr;
    lv_obj_t* phonetic_label_ = nullptr;
    lv_obj_t* meaning_label_ = nullptr;
    lv_obj_t* streak_label_ = nullptr;

    std::shared_ptr<LvglCBinFont> cn_font_ = nullptr;
    bool font_loaded_ = false;
    bool created_ = false;
    bool visible_ = false;

    bool sntp_started_ = false;
    static inline bool time_synced_ = false;

    int shown_minute_ = -1;
    int shown_day_ = -1;   // yyyymmdd
    int streak_ = 0;
    int shown_streak_ = -1;
    int word_index_ = -1;
    int session_offset_ = 0;   // 当天内「下一个」的偏移量

    // 今日词条 = 按天轮换 + 会话内偏移
    int TodayIndex() const {
        return (int)((time(nullptr) / 86400) % kWordBookSize);
    }
    int CurrentIndex() const {
        int idx = (TodayIndex() + session_offset_) % kWordBookSize;
        return idx < 0 ? idx + kWordBookSize : idx;
    }

    // ---------------- 字体 ----------------
    void LoadFontOnce() {
        if (font_loaded_) {
            return;
        }
        font_loaded_ = true;
        Assets* assets = &Assets::GetInstance();
        void* ptr = nullptr;
        size_t size = 0;
        if (assets != nullptr &&
            assets->GetAssetData("font_noto_sans_common_20_4.bin", ptr, size)) {
            auto font = std::make_shared<LvglCBinFont>(ptr);
            if (font->font() != nullptr) {
                cn_font_ = font;
                ESP_LOGI(WORD_CARD_TAG, "已加载中文字库 %u 字节", (unsigned)size);
                return;
            }
        }
        ESP_LOGW(WORD_CARD_TAG, "中文字库加载失败，回退主题字体（可能缺字）");
    }

    const lv_font_t* CnFont() const {
        if (cn_font_ != nullptr && cn_font_->font() != nullptr) {
            return cn_font_->font();
        }
        if (display_ != nullptr) {
            auto theme = static_cast<LvglTheme*>(display_->GetTheme());
            if (theme != nullptr && theme->text_font() != nullptr) {
                return theme->text_font()->font();
            }
        }
        return LV_FONT_DEFAULT;
    }

    // ---------------- NTP 对时 ----------------
    void EnsureSntp() {
        if (sntp_started_) {
            return;
        }
        sntp_started_ = true;
        setenv("TZ", "CST-8", 1);
        tzset();
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "ntp.aliyun.com");
        esp_sntp_set_time_sync_notification_cb([](struct timeval* tv) { time_synced_ = true; });
        esp_sntp_init();
        ESP_LOGI(WORD_CARD_TAG, "NTP 对时已启动");
    }

    // ---------------- 界面创建 ----------------
    void EnsureCreated() {
        if (created_ || display_ == nullptr) {
            return;
        }
        LoadFontOnce();

        DisplayLockGuard lock(display_);
        auto screen = lv_screen_active();
        if (screen == nullptr) {
            return;
        }
        created_ = true;

        auto theme = static_cast<LvglTheme*>(display_->GetTheme());
        lv_color_t bg = theme ? theme->background_color() : lv_color_black();
        lv_color_t fg = theme ? theme->text_color() : lv_color_white();

        // 卡片避开顶部状态栏（WiFi/电量/状态文字）
        const int kTopReserved = 34;
        card_ = lv_obj_create(screen);
        lv_obj_set_size(card_, LV_HOR_RES, LV_VER_RES - kTopReserved);
        lv_obj_align(card_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(card_, 0, 0);
        lv_obj_set_style_border_width(card_, 0, 0);
        lv_obj_set_style_bg_opa(card_, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(card_, bg, 0);
        lv_obj_set_style_pad_all(card_, 4, 0);
        lv_obj_set_style_pad_row(card_, 0, 0);
        lv_obj_set_flex_flow(card_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card_, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(card_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(card_, LV_OBJ_FLAG_SCROLLABLE);

        clock_label_ = lv_label_create(card_);
        lv_obj_set_style_text_font(clock_label_, &font_noto_sans_basic_30_4, 0);
        lv_obj_set_style_text_color(clock_label_, fg, 0);
        lv_label_set_text(clock_label_, "--:--");

        date_label_ = lv_label_create(card_);
        lv_obj_set_style_text_font(date_label_, CnFont(), 0);
        lv_obj_set_style_text_color(date_label_, fg, 0);
        lv_obj_set_style_text_opa(date_label_, LV_OPA_70, 0);
        lv_label_set_text(date_label_, "等待对时");

        word_label_ = lv_label_create(card_);
        lv_obj_set_style_text_font(word_label_, &font_noto_sans_basic_30_4, 0);
        lv_obj_set_style_text_color(word_label_, fg, 0);
        lv_label_set_long_mode(word_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(word_label_, LV_HOR_RES - 20);
        lv_obj_set_style_text_align(word_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(word_label_, "");

        phonetic_label_ = lv_label_create(card_);
        lv_obj_set_style_text_font(phonetic_label_, CnFont(), 0);
        lv_obj_set_style_text_color(phonetic_label_, fg, 0);
        lv_obj_set_style_text_opa(phonetic_label_, LV_OPA_70, 0);
        lv_label_set_text(phonetic_label_, "");

        meaning_label_ = lv_label_create(card_);
        lv_obj_set_style_text_font(meaning_label_, CnFont(), 0);
        lv_obj_set_style_text_color(meaning_label_, fg, 0);
        lv_obj_set_width(meaning_label_, LV_HOR_RES - 20);
        lv_label_set_long_mode(meaning_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(meaning_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(meaning_label_, "");

        streak_label_ = lv_label_create(card_);
        lv_obj_set_style_text_font(streak_label_, CnFont(), 0);
        lv_obj_set_style_text_color(streak_label_, fg, 0);
        lv_obj_set_style_text_opa(streak_label_, LV_OPA_70, 0);
        lv_label_set_text(streak_label_, "");

        lv_obj_add_flag(card_, LV_OBJ_FLAG_HIDDEN);
        visible_ = false;
    }

    void Show() {
        EnsureCreated();
        if (card_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(display_);
        lv_obj_remove_flag(card_, LV_OBJ_FLAG_HIDDEN);
        visible_ = true;
    }

    void Hide() {
        if (card_ == nullptr) {
            return;
        }
        DisplayLockGuard lock(display_);
        lv_obj_add_flag(card_, LV_OBJ_FLAG_HIDDEN);
        visible_ = false;
    }

    // ---------------- 打卡 / 选词 ----------------
    static int YesterdayKey(time_t now) {
        time_t t = now - 86400;
        struct tm tm_y {};
        localtime_r(&t, &tm_y);
        return (tm_y.tm_year + 1900) * 10000 + (tm_y.tm_mon + 1) * 100 + tm_y.tm_mday;
    }

    void UpdateStreak(int today_key, int yesterday_key) {
        Settings settings("word_card", true);
        int last_day = settings.GetInt("last_day", 0);
        int streak = settings.GetInt("streak", 0);

        if (last_day == today_key) {
            streak_ = streak > 0 ? streak : 1;
            return;
        }
        streak = (last_day == yesterday_key) ? (streak + 1) : 1;
        settings.SetInt("streak", streak);
        settings.SetInt("last_day", today_key);
        streak_ = streak;
        ESP_LOGI(WORD_CARD_TAG, "打卡更新：连续 %d 天", streak);
    }

    // ---------------- 文本刷新（调用方必须已持锁） ----------------
    void SetClockText(int hour, int minute, bool valid) {
        if (clock_label_ == nullptr) {
            return;
        }
        char buf[16];
        if (valid) {
            snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
        } else {
            snprintf(buf, sizeof(buf), "--:--");
        }
        if (strcmp(lv_label_get_text(clock_label_), buf) != 0) {
            lv_label_set_text(clock_label_, buf);
        }
    }

    void SetDateText(int month, int day, int weekday, bool valid) {
        if (date_label_ == nullptr) {
            return;
        }
        static const char* kWeek[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        char buf[48];
        if (valid) {
            snprintf(buf, sizeof(buf), "%d月%d日 %s", month, day, kWeek[weekday % 7]);
        } else {
            snprintf(buf, sizeof(buf), "等待对时");
        }
        if (strcmp(lv_label_get_text(date_label_), buf) != 0) {
            lv_label_set_text(date_label_, buf);
        }
    }

    void SetWordText() {
        if (word_label_ == nullptr) {
            return;
        }
        if (word_index_ < 0 || word_index_ >= kWordBookSize) {
            word_index_ = 0;
        }
        const WordEntry& w = kWordBook[word_index_];
        lv_label_set_text(word_label_, w.word);
        lv_label_set_text(phonetic_label_, w.phonetic);
        lv_label_set_text(meaning_label_, w.meaning);
    }

    void SetStreakText() {
        if (streak_label_ == nullptr) {
            return;
        }
        char buf[40];
        snprintf(buf, sizeof(buf), "连续打卡 %d 天", streak_);
        if (strcmp(lv_label_get_text(streak_label_), buf) != 0) {
            lv_label_set_text(streak_label_, buf);
        }
    }

    // ---------------- 主循环（1Hz） ----------------
    void OnTick() {
        if (display_ == nullptr) {
            return;
        }

        DeviceState state = Application::GetInstance().GetDeviceState();

        // 开机 / 配网 / 激活阶段不打扰
        if (state == kDeviceStateStarting || state == kDeviceStateWifiConfiguring ||
            state == kDeviceStateActivating) {
            if (visible_) {
                Hide();
            }
            return;
        }

        EnsureSntp();

        time_t now = time(nullptr);
        struct tm tm_now {};
        localtime_r(&now, &tm_now);
        bool time_valid = time_synced_ && (tm_now.tm_year + 1900) >= 2024;

        if (state != kDeviceStateIdle) {
            if (visible_) {
                Hide();
            }
            return;
        }

        if (!visible_) {
            Show();
            shown_minute_ = -1;
            shown_day_ = -1;
        }

        int day_key =
            (tm_now.tm_year + 1900) * 10000 + (tm_now.tm_mon + 1) * 100 + tm_now.tm_mday;
        bool minute_changed = (tm_now.tm_min != shown_minute_);
        bool day_changed = (day_key != shown_day_);

        if (minute_changed || day_changed) {
            shown_minute_ = tm_now.tm_min;
            DisplayLockGuard lock(display_);
            SetClockText(tm_now.tm_hour, tm_now.tm_min, time_valid);
            if (day_changed) {
                SetDateText(tm_now.tm_mon + 1, tm_now.tm_mday, (tm_now.tm_wday + 6) % 7,
                            time_valid);
            }
        }

        if (day_changed) {
            shown_day_ = day_key;
            if (time_valid) {
                UpdateStreak(day_key, YesterdayKey(now));
            } else {
                Settings settings("word_card", false);
                streak_ = settings.GetInt("streak", 0);
            }
        }

        // 词条或打卡数变化就刷新（跨天 / 用户说「下一个」/ 念对打卡）
        int idx = CurrentIndex();
        if (idx != word_index_ || streak_ != shown_streak_) {
            word_index_ = idx;
            shown_streak_ = streak_;
            DisplayLockGuard lock(display_);
            SetWordText();
            SetStreakText();
        }
    }
};

#endif  // WORD_CARD_H
