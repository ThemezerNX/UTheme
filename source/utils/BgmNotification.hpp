#pragma once

#include "../utils/Animation.hpp"
#include <string>
#include <vector>
#include <SDL2/SDL.h>

// BGM播放通知组件 - 支持多个通知同时显示
class BgmNotification {
public:
    BgmNotification();
    
    // 显示当前播放的音乐
    void ShowNowPlaying(const std::string& musicName);
    
    // 显示当前播放的音乐(带艺术家信息)
    void ShowNowPlaying(const std::string& musicName, const std::string& artist);
    
    // 显示错误消息
    void ShowError(const std::string& message, uint64_t displayMs = 5000, const std::string& title = "");
    
    // 显示警告消息
    void ShowWarning(const std::string& message, uint64_t displayMs = 5000, const std::string& title = "");
    
    // 显示提示消息
    void ShowInfo(const std::string& message, uint64_t displayMs = 5000, const std::string& title = "");
    
    // 更新和绘制
    void Update();
    void Draw();
    
    // 状态查询
    bool IsVisible() const { return !mNotifications.empty(); }
    
    // 立即隐藏所有通知
    void Hide();
    
private:
    enum NotificationType {
        TYPE_MUSIC,
        TYPE_ERROR,
        TYPE_WARNING,
        TYPE_INFO
    };
    
    struct Notification {
        NotificationType type;
        std::string title;       // 主标题（可选）
        std::string musicName;
        std::string artist;
        std::string message;
        Animation fadeAnim;
        Animation slideAnim;
        Animation yAnim;         // Y 位置动画（通知消失后下方通知平滑上移）
        uint64_t showTime;
        uint64_t displayDuration;
        int yPosition;  // 当前 Y 位置（由 yAnim 驱动）
        int targetY;    // 目标 Y 位置
        
        // 自动换行支持
        std::vector<std::string> wrappedTitleLines;
        std::vector<std::string> wrappedMessageLines;
        int actualHeight;  // 动态计算的实际高度
    };
    
    std::vector<Notification> mNotifications;
    
    void AddNotification(NotificationType type, const std::string& text1, const std::string& text2 = "", uint64_t displayMs = 4000, const std::string& title = "");
    void DrawNotification(const Notification& notif, int x, int y, float fadeAlpha, float slideOffset);
    void RecalculateTargetPositions();  // 重新计算所有通知的目标 Y 位置
    
    // 文本换行辅助
    static std::vector<std::string> WrapText(const std::string& text, int fontSize, int maxWidth);
    static int CalculateNotificationHeight(const Notification& notif);
    
    static const int NOTIFICATION_WIDTH = 500;
    static const int NOTIFICATION_MIN_HEIGHT = 90;
    static const int NOTIFICATION_SPACING = 10;  // 通知之间的间距
    static const int TEXT_AREA_LEFT = 70;   // 文字区域左边界（相对于通知框）
    static const int TEXT_AREA_PADDING = 20; // 文字区域右内边距
    static const int NOTIF_TOP_PAD = 25;    // 顶部内边距
    static const int NOTIF_BOT_PAD = 20;    // 底部内边距
    static const int LINE_GAP = 6;          // 行间距
};
