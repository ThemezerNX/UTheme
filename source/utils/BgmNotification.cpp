#include "BgmNotification.hpp"
#include "../Gfx.hpp"
#include "../utils/LanguageManager.hpp"
#include <coreinit/time.h>

// ============= 文本换行辅助函数 =============

// 获取 UTF-8 字符的字节长度
static int Utf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if (c < 0xE0) return 2;
    if (c < 0xF0) return 3;
    return 4;
}

// 判断是否是 UTF-8 字符的起始字节（非延续字节）
static bool IsUtf8Start(unsigned char c) {
    return (c & 0xC0) != 0x80;
}

std::vector<std::string> BgmNotification::WrapText(const std::string& text, int fontSize, int maxWidth) {
    std::vector<std::string> lines;
    std::string currentLine;
    
    for (size_t i = 0; i < text.size(); ) {
        unsigned char c = (unsigned char)text[i];
        
        // 跳过 UTF-8 延续字节（不应该出现在这里但以防万一）
        if (!IsUtf8Start(c)) {
            i++;
            continue;
        }
        
        int charLen = Utf8CharLen(c);
        if (i + charLen > text.size()) break;
        
        std::string ch = text.substr(i, charLen);
        i += charLen;
        
        // 处理换行符
        if (ch == "\n") {
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
                currentLine.clear();
            }
            continue;
        }
        
        // 测试添加此字符后是否超出最大宽度
        std::string testLine = currentLine + ch;
        if (Gfx::GetTextWidth(fontSize, testLine) > maxWidth) {
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
                currentLine = ch;
            } else {
                // 单个字符就超出宽度，强制添加
                lines.push_back(ch);
            }
        } else {
            currentLine += ch;
        }
    }
    
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }
    
    // 确保至少有一行
    if (lines.empty()) {
        lines.push_back("");
    }
    
    return lines;
}

int BgmNotification::CalculateNotificationHeight(const Notification& notif) {
    if (notif.type == TYPE_MUSIC) {
        // 音乐通知："Now Playing" 标签 + 歌名 + 艺术家
        int totalHeight = NOTIF_TOP_PAD;
        
        // "Now Playing" 标签（字体 22）
        totalHeight += Gfx::GetTextHeight(22, "Now Playing") + LINE_GAP;
        
        // 歌名行（字体 28）
        for (const auto& line : notif.wrappedMessageLines) {
            totalHeight += Gfx::GetTextHeight(28, line) + LINE_GAP;
        }
        
        // 艺术家行（字体 22）
        if (!notif.wrappedTitleLines.empty()) {
            totalHeight += 4;  // 歌名和艺术家之间间距
            for (const auto& line : notif.wrappedTitleLines) {
                totalHeight += Gfx::GetTextHeight(22, line) + LINE_GAP;
            }
        }
        
        totalHeight += NOTIF_BOT_PAD;
        
        if (totalHeight < NOTIFICATION_MIN_HEIGHT) {
            totalHeight = NOTIFICATION_MIN_HEIGHT;
        }
        
        return totalHeight;
    }
    
    int totalHeight = NOTIF_TOP_PAD;
    
    // 标题行高度
    for (const auto& line : notif.wrappedTitleLines) {
        totalHeight += Gfx::GetTextHeight(28, line) + LINE_GAP;
    }
    
    // 标题和消息之间的额外间距（如果有标题）
    if (!notif.wrappedTitleLines.empty() && !notif.wrappedMessageLines.empty()) {
        totalHeight += 4;
    }
    
    // 消息行高度
    for (const auto& line : notif.wrappedMessageLines) {
        totalHeight += Gfx::GetTextHeight(24, line) + LINE_GAP;
    }
    
    totalHeight += NOTIF_BOT_PAD;
    
    // 最小高度保证
    if (totalHeight < NOTIFICATION_MIN_HEIGHT) {
        totalHeight = NOTIFICATION_MIN_HEIGHT;
    }
    
    return totalHeight;
}

BgmNotification::BgmNotification() {
}

void BgmNotification::ShowNowPlaying(const std::string& musicName) {
    ShowNowPlaying(musicName, "");
}

void BgmNotification::ShowNowPlaying(const std::string& musicName, const std::string& artist) {
    AddNotification(TYPE_MUSIC, musicName, artist);
}

void BgmNotification::ShowError(const std::string& message, uint64_t displayMs, const std::string& title) {
    AddNotification(TYPE_ERROR, message, "", displayMs, title);
}

void BgmNotification::ShowWarning(const std::string& message, uint64_t displayMs, const std::string& title) {
    AddNotification(TYPE_WARNING, message, "", displayMs, title);
}

void BgmNotification::ShowInfo(const std::string& message, uint64_t displayMs, const std::string& title) {
    AddNotification(TYPE_INFO, message, "", displayMs, title);
}

void BgmNotification::AddNotification(NotificationType type, const std::string& text1, const std::string& text2, uint64_t displayMs, const std::string& title) {
    Notification notif;
    notif.type = type;
    notif.showTime = OSGetTime();
    notif.displayDuration = displayMs;
    notif.title = title;
    
    // 计算文字区域可用宽度
    int textMaxWidth = NOTIFICATION_WIDTH - TEXT_AREA_LEFT - TEXT_AREA_PADDING;
    
    if (type == TYPE_MUSIC) {
        notif.musicName = text1;
        notif.artist = text2;
        // 音乐通知也做换行处理
        notif.wrappedMessageLines = WrapText(text1, 28, textMaxWidth);
        if (!text2.empty()) {
            notif.wrappedTitleLines = WrapText(text2, 22, textMaxWidth);
        }
        notif.actualHeight = CalculateNotificationHeight(notif);
    } else {
        notif.message = text1;
        // 对标题和消息进行自动换行
        if (!title.empty()) {
            notif.wrappedTitleLines = WrapText(title, 28, textMaxWidth);
        }
        notif.wrappedMessageLines = WrapText(text1, 24, textMaxWidth);
        notif.actualHeight = CalculateNotificationHeight(notif);
    }
    
    // 计算Y位置 - 从上往下堆叠（使用动态高度）
    int baseY = 140; // 基础Y坐标
    if (!mNotifications.empty()) {
        // 找到最底部的通知
        int maxY = baseY;
        for (const auto& n : mNotifications) {
            int bottom = n.targetY + n.actualHeight + NOTIFICATION_SPACING;
            if (bottom > maxY) {
                maxY = bottom;
            }
        }
        notif.targetY = maxY;
    } else {
        notif.targetY = baseY;
    }
    
    // Y 位置动画 - 新通知从当前位置开始（直接设置，不滑入）
    notif.yAnim.SetImmediate((float)notif.targetY);
    notif.yPosition = notif.targetY;
    
    // 设置动画 - 从右侧滑入
    notif.slideAnim.SetImmediate(1.0f); // 从屏幕外开始
    notif.slideAnim.SetTarget(0.0f, 400); // 滑入
    
    notif.fadeAnim.SetImmediate(0.0f);
    notif.fadeAnim.SetTarget(1.0f, 300);
    
    mNotifications.push_back(notif);
}

void BgmNotification::Hide() {
    for (auto& notif : mNotifications) {
        if (notif.fadeAnim.GetTarget() > 0.0f) {
            notif.fadeAnim.SetTarget(0.0f, 300);
            notif.slideAnim.SetTarget(1.0f, 400);
        }
    }
}

void BgmNotification::Update() {
    if (mNotifications.empty()) return;
    
    uint64_t currentTime = OSGetTime();
    uint64_t ticksPerSecond = OSGetSystemInfo()->busClockSpeed / 4;
    
    bool removed = false;
    
    // 更新所有通知
    for (auto it = mNotifications.begin(); it != mNotifications.end(); ) {
        it->fadeAnim.Update();
        it->slideAnim.Update();
        
        // 检查是否已显示足够长时间
        uint64_t elapsedMs = ((currentTime - it->showTime) * 1000) / ticksPerSecond;
        if (elapsedMs > it->displayDuration && it->fadeAnim.GetTarget() > 0.0f) {
            // 开始淡出动画
            it->fadeAnim.SetTarget(0.0f, 300);
            it->slideAnim.SetTarget(1.0f, 400);
        }
        
        // 如果完全淡出,移除通知
        if (it->fadeAnim.GetValue() <= 0.0f && it->fadeAnim.GetTarget() <= 0.0f) {
            it = mNotifications.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    
    // 有通知被移除时，重新计算剩余通知的目标位置
    if (removed) {
        RecalculateTargetPositions();
    }
    
    // 更新所有通知的 Y 动画，让它们平滑移动到目标位置
    for (auto& notif : mNotifications) {
        notif.yAnim.Update();
        notif.yPosition = (int)notif.yAnim.GetValue();
    }
}

void BgmNotification::RecalculateTargetPositions() {
    int baseY = 140;
    int curY = baseY;
    
    for (auto& notif : mNotifications) {
        // 如果目标位置变了，设置动画平滑过渡
        if (notif.targetY != curY) {
            notif.yAnim.SetTarget((float)curY, 300);  // 300ms 平滑上移
        }
        notif.targetY = curY;
        curY = curY + notif.actualHeight + NOTIFICATION_SPACING;
    }
}

void BgmNotification::Draw() {
    if (mNotifications.empty()) return;
    
    for (const auto& notif : mNotifications) {
        float fadeAlpha = notif.fadeAnim.GetValue();
        float slideOffset = notif.slideAnim.GetValue();
        
        if (fadeAlpha <= 0.0f) continue;
        
        // 计算位置 - 从右侧滑入
        int x = Gfx::SCREEN_WIDTH - NOTIFICATION_WIDTH - 40 + (int)(slideOffset * (NOTIFICATION_WIDTH + 100));
        int y = notif.yPosition;
        
        DrawNotification(notif, x, y, fadeAlpha, slideOffset);
    }
}

void BgmNotification::DrawNotification(const Notification& notif, int x, int y, float fadeAlpha, float slideOffset) {
    int height = notif.actualHeight;
    
    // 绘制阴影
    SDL_Color shadowColor = Gfx::COLOR_SHADOW;
    shadowColor.a = (Uint8)(100 * fadeAlpha);
    Gfx::DrawRectRounded(x + 5, y + 5, NOTIFICATION_WIDTH, height, 16, shadowColor);
    
    // 绘制背景
    SDL_Color bgColor;
    if (notif.type == TYPE_ERROR) {
        bgColor = SDL_Color{50, 20, 20, (Uint8)(240 * fadeAlpha)};
    } else if (notif.type == TYPE_WARNING) {
        bgColor = SDL_Color{50, 40, 20, (Uint8)(240 * fadeAlpha)};
    } else if (notif.type == TYPE_INFO) {
        bgColor = SDL_Color{20, 40, 50, (Uint8)(240 * fadeAlpha)};
    } else {
        bgColor = SDL_Color{30, 35, 50, (Uint8)(240 * fadeAlpha)};
    }
    Gfx::DrawRectRounded(x, y, NOTIFICATION_WIDTH, height, 16, bgColor);
    
    // 绘制左侧装饰条
    SDL_Color accentColor;
    if (notif.type == TYPE_ERROR) {
        accentColor = Gfx::COLOR_ERROR;
    } else if (notif.type == TYPE_WARNING) {
        accentColor = Gfx::COLOR_WARNING;
    } else if (notif.type == TYPE_INFO) {
        accentColor = Gfx::COLOR_SUCCESS;
    } else {
        accentColor = Gfx::COLOR_ACCENT;
    }
    accentColor.a = (Uint8)(255 * fadeAlpha);
    Gfx::DrawRectRounded(x, y, 6, height, 12, accentColor);
    
    // 图标垂直居中
    int iconCenterY = y + height / 2;
    
    if (notif.type == TYPE_ERROR) {
        // 错误图标
        SDL_Color iconColor = Gfx::COLOR_ERROR;
        iconColor.a = (Uint8)(255 * fadeAlpha);
        Gfx::DrawIcon(x + 35, iconCenterY, 36, iconColor, 0xf06a, Gfx::ALIGN_CENTER);
        
        // 绘制标题行
        int curY = y + NOTIF_TOP_PAD;
        SDL_Color titleColor = Gfx::COLOR_TEXT;
        titleColor.a = (Uint8)(255 * fadeAlpha);
        for (const auto& line : notif.wrappedTitleLines) {
            Gfx::Print(x + TEXT_AREA_LEFT, curY, 28, titleColor, line, Gfx::ALIGN_LEFT);
            curY += Gfx::GetTextHeight(28, line) + LINE_GAP;
        }
        
        if (!notif.wrappedTitleLines.empty() && !notif.wrappedMessageLines.empty()) {
            curY += 4; // 标题和消息之间的间距
        }
        
        // 绘制消息行
        SDL_Color msgColor = notif.wrappedTitleLines.empty() ? Gfx::COLOR_TEXT : Gfx::COLOR_ALT_TEXT;
        msgColor.a = (Uint8)(notif.wrappedTitleLines.empty() ? 255 : 220 * fadeAlpha);
        int msgFontSize = notif.wrappedTitleLines.empty() ? 26 : 24;
        for (const auto& line : notif.wrappedMessageLines) {
            Gfx::Print(x + TEXT_AREA_LEFT, curY, msgFontSize, msgColor, line, Gfx::ALIGN_LEFT);
            curY += Gfx::GetTextHeight(msgFontSize, line) + LINE_GAP;
        }
    } else if (notif.type == TYPE_WARNING) {
        // 警告图标
        SDL_Color iconColor = Gfx::COLOR_WARNING;
        iconColor.a = (Uint8)(255 * fadeAlpha);
        Gfx::DrawIcon(x + 35, iconCenterY, 36, iconColor, 0xf071, Gfx::ALIGN_CENTER);
        
        // 绘制标题行
        int curY = y + NOTIF_TOP_PAD;
        SDL_Color titleColor = Gfx::COLOR_TEXT;
        titleColor.a = (Uint8)(255 * fadeAlpha);
        for (const auto& line : notif.wrappedTitleLines) {
            Gfx::Print(x + TEXT_AREA_LEFT, curY, 28, titleColor, line, Gfx::ALIGN_LEFT);
            curY += Gfx::GetTextHeight(28, line) + LINE_GAP;
        }
        
        if (!notif.wrappedTitleLines.empty() && !notif.wrappedMessageLines.empty()) {
            curY += 4;
        }
        
        // 绘制消息行
        SDL_Color msgColor = notif.wrappedTitleLines.empty() ? Gfx::COLOR_TEXT : Gfx::COLOR_ALT_TEXT;
        msgColor.a = (Uint8)(notif.wrappedTitleLines.empty() ? 255 : 220 * fadeAlpha);
        int msgFontSize = notif.wrappedTitleLines.empty() ? 26 : 24;
        for (const auto& line : notif.wrappedMessageLines) {
            Gfx::Print(x + TEXT_AREA_LEFT, curY, msgFontSize, msgColor, line, Gfx::ALIGN_LEFT);
            curY += Gfx::GetTextHeight(msgFontSize, line) + LINE_GAP;
        }
    } else if (notif.type == TYPE_INFO) {
        // 提示图标
        SDL_Color iconColor = Gfx::COLOR_SUCCESS;
        iconColor.a = (Uint8)(255 * fadeAlpha);
        Gfx::DrawIcon(x + 35, iconCenterY, 36, iconColor, 0xf05a, Gfx::ALIGN_CENTER);
        
        // 绘制标题行
        int curY = y + NOTIF_TOP_PAD;
        SDL_Color titleColor = Gfx::COLOR_TEXT;
        titleColor.a = (Uint8)(255 * fadeAlpha);
        for (const auto& line : notif.wrappedTitleLines) {
            Gfx::Print(x + TEXT_AREA_LEFT, curY, 28, titleColor, line, Gfx::ALIGN_LEFT);
            curY += Gfx::GetTextHeight(28, line) + LINE_GAP;
        }
        
        if (!notif.wrappedTitleLines.empty() && !notif.wrappedMessageLines.empty()) {
            curY += 4;
        }
        
        // 绘制消息行
        SDL_Color msgColor = notif.wrappedTitleLines.empty() ? Gfx::COLOR_TEXT : Gfx::COLOR_ALT_TEXT;
        msgColor.a = (Uint8)(notif.wrappedTitleLines.empty() ? 255 : 220 * fadeAlpha);
        int msgFontSize = notif.wrappedTitleLines.empty() ? 26 : 24;
        for (const auto& line : notif.wrappedMessageLines) {
            Gfx::Print(x + TEXT_AREA_LEFT, curY, msgFontSize, msgColor, line, Gfx::ALIGN_LEFT);
            curY += Gfx::GetTextHeight(msgFontSize, line) + LINE_GAP;
        }
    } else {
        // 音乐模式 - 动态高度布局，支持多行换行
        SDL_Color iconColor = Gfx::COLOR_ACCENT;
        iconColor.a = (Uint8)(255 * fadeAlpha);
        Gfx::DrawIcon(x + 35, iconCenterY, 40, iconColor, 0xf001, Gfx::ALIGN_CENTER);
        
        int curY = y + NOTIF_TOP_PAD;
        
        // "Now Playing" 标签
        SDL_Color labelColor = Gfx::COLOR_ALT_TEXT;
        labelColor.a = (Uint8)(200 * fadeAlpha);
        Gfx::Print(x + TEXT_AREA_LEFT, curY, 22, labelColor, "Now Playing", Gfx::ALIGN_LEFT);
        curY += Gfx::GetTextHeight(22, "Now Playing") + LINE_GAP;
        
        // 音乐名称（支持换行）
        SDL_Color nameColor = Gfx::COLOR_TEXT;
        nameColor.a = (Uint8)(255 * fadeAlpha);
        for (const auto& line : notif.wrappedMessageLines) {
            Gfx::Print(x + TEXT_AREA_LEFT, curY, 28, nameColor, line, Gfx::ALIGN_LEFT);
            curY += Gfx::GetTextHeight(28, line) + LINE_GAP;
        }
        
        // 艺术家（支持换行）
        if (!notif.wrappedTitleLines.empty()) {
            curY += 4;
            SDL_Color artistColor = Gfx::COLOR_ALT_TEXT;
            artistColor.a = (Uint8)(200 * fadeAlpha);
            for (const auto& line : notif.wrappedTitleLines) {
                Gfx::Print(x + TEXT_AREA_LEFT, curY, 22, artistColor, line, Gfx::ALIGN_LEFT);
                curY += Gfx::GetTextHeight(22, line) + LINE_GAP;
            }
        }
    }
}
