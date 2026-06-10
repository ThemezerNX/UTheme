#include "SettingsScreen.hpp"
#include "Gfx.hpp"
#include "../utils/LanguageManager.hpp"
#include "../utils/FileLogger.hpp"
#include "../utils/Config.hpp"
#include "../utils/ThemePatcher.hpp"
#include <SDL2/SDL_image.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <functional>
#include <cstring>
#include <sysapp/launch.h>
#include <coreinit/time.h>
#include "../themezer_png.h"

SettingsScreen::SettingsScreen()
    : mPrevSelectedItem(SETTINGS_LANGUAGE)
    , mCurrentPage(0)
    , mSelectedSource(Config::GetInstance().GetActiveSource())
{
    mTitleAnim.Start(0, 1, 500);
    mSelectionAnim.Start(0, 1, 200);
    
    // 初始化所有选项动画进度
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        mItemAnimProgress[i] = 0.0f;
    }
    mItemAnimProgress[mSelectedItem] = 1.0f; // 当前选中项从完全显示开始
    
    // 找到当前语言在列表中的位置
    const auto& languages = Lang().GetAvailableLanguages();
    const std::string& currentLang = Lang().GetCurrentLanguage();
    
    const int itemsPerColumn = (static_cast<int>(languages.size()) + 1) / 2;
    
    // 初始化语言项动画进度数组
    mLanguageItemAnimProgress.resize(languages.size(), 0.0f);
    mLanguageCardBounds.resize(languages.size());  // 初始化边界数组
    
    for (size_t i = 0; i < languages.size(); i++) {
        if (languages[i].code == currentLang) {
            // 计算在双列布局中的位置
            mSelectedColumn = static_cast<int>(i) / itemsPerColumn;
            mSelectedLanguage = static_cast<int>(i) % itemsPerColumn;
            mPrevSelectedColumn = mSelectedColumn;
            mPrevSelectedLanguage = mSelectedLanguage;
            
            // 当前语言项从完全显示开始
            mLanguageItemAnimProgress[i] = 1.0f;
            break;
        }
    }
    // 加载 Themezer logo（从内嵌数据）
    SDL_RWops* rw = SDL_RWFromConstMem(themezer_png, themezer_png_len);
    if (rw) {
        mThemezerLogo = IMG_LoadTexture_RW(Gfx::GetRenderer(), rw, 1);
    }
}

SettingsScreen::~SettingsScreen() {
    if (mThemezerLogo) SDL_DestroyTexture(mThemezerLogo);
}

void SettingsScreen::Draw() {
    mFrameCount++;
    
    // 更新选项动画
    mSelectionAnim.Update();
    
    // 更新每个选项的动画进度
    for (int i = 0; i < SETTINGS_COUNT; i++) {
        if (i == mSelectedItem) {
            // 当前选中项:逐渐增加到1.0
            mItemAnimProgress[i] += (1.0f - mItemAnimProgress[i]) * 0.2f;
        } else {
            // 非选中项:逐渐减少到0.0
            mItemAnimProgress[i] *= 0.8f;
        }
    }
    
    // Draw gradient background
    Gfx::DrawGradientV(0, 0, Gfx::SCREEN_WIDTH, Gfx::SCREEN_HEIGHT, 
                       Gfx::COLOR_BACKGROUND, Gfx::COLOR_ALT_BACKGROUND);
    
    DrawAnimatedTopBar(_("settings.title"), mTitleAnim, 0xf013);
    
    // 页面切换过渡动画（逐条交错滑出/滑入）
    if (mIsPageTransitioning) {
        mPageTransitionAnim.Update();
        float t = mPageTransitionAnim.GetValue();
        
        if (t >= 1.0f) {
            mIsPageTransitioning = false;
            mCurrentPage = mTransitionTargetPage;
            mCurItemOffsetX = 0.0f;
            mCurItemOffsetY = 0.0f;
        } else {
            // dir: -1 = 按R翻到下一页(旧页左滑), +1 = 按L翻到上一页(旧页右滑)
            int dir = (mTransitionTargetPage > mTransitionFromPage) ? -1 : 1;
            const float staggerDelay = 0.075f;    // 每条延迟 ~120ms，给每条独立呼吸感
            const float itemDuration = 0.3125f;    // 单条动画 ~500ms
            const float enterStart = 0.25f;        // 新页从 25% 开始进入
            const float screenW = (float)Gfx::SCREEN_WIDTH;
            
            auto clamp = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };
            
            // ====== 绘制旧页（逐条滑出）======
            if (mTransitionFromPage == 0) {
                // --- 设置项逐条滑出 ---
                const int topBarHeight = 120;
                const int itemHeight = 120;
                const int itemSpacing = 20;
                const int listX = 200;
                const int listY = topBarHeight + 30;
                const int listW = Gfx::SCREEN_WIDTH - 400;
                
                const auto& languages = Lang().GetAvailableLanguages();
                std::string currentLanguageName = "Unknown";
                for (const auto& lang : languages) {
                    if (lang.code == Lang().GetCurrentLanguage()) { currentLanguageName = lang.name; break; }
                }
                
                int currentY = listY;
                for (int i = 0; i < SETTINGS_COUNT; i++) {
                    float localT = clamp((t - i * staggerDelay) / itemDuration, 0.0f, 1.0f);
                    if (localT >= 1.0f) { currentY += itemHeight + itemSpacing; continue; }
                    float ease = Easing::EaseInCubic(localT);
                    mCurItemOffsetX = dir * screenW * ease;
                    mCurItemOffsetY = 0.0f;
                    mSettingItemBounds[i] = {listX, currentY, listW, itemHeight};
                    switch (i) {
                        case SETTINGS_LANGUAGE:
                            DrawSettingItem(listX, currentY, listW, _("settings.language"), _("settings.language_desc"), currentLanguageName, mSelectedItem == i, mItemAnimProgress[i], 0xf0ac); break;
                        case SETTINGS_DOWNLOAD_PATH:
                            DrawSettingItem(listX, currentY, listW, _("settings.download_path"), _("settings.download_path_desc"), "SD:/wiiu/themes/", mSelectedItem == i, mItemAnimProgress[i], 0xf07c); break;
                        case SETTINGS_BGM_ENABLED:
                            DrawSettingItem(listX, currentY, listW, _("settings.bgm_enabled"), _("settings.bgm_enabled_desc"), Config::GetInstance().IsBgmEnabled() ? _("common.yes") : _("common.no"), mSelectedItem == i, mItemAnimProgress[i], 0xf001); break;
                        case SETTINGS_LOGGING_ENABLED:
                            DrawSettingItem(listX, currentY, listW, _("settings.logging"), _("settings.logging_desc"), Config::GetInstance().IsLoggingEnabled() ? _("common.yes") : _("common.no"), mSelectedItem == i, mItemAnimProgress[i], 0xf15c); break;
                        case SETTINGS_LOGGING_VERBOSE:
                            DrawSettingItem(listX, currentY, listW, _("settings.verbose_logging"), _("settings.verbose_logging_desc"), Config::GetInstance().IsVerboseLogging() ? _("common.yes") : _("common.no"), mSelectedItem == i, mItemAnimProgress[i], 0xf0ae); break;
                        case SETTINGS_CLEAR_CACHE:
                            DrawSettingItem(listX, currentY, listW, _("settings.clear_cache"), _("settings.clear_cache_desc"), _("settings.press_to_clear"), mSelectedItem == i, mItemAnimProgress[i], 0xf1f8); break;
                    }
                    currentY += itemHeight + itemSpacing;
                }
            } else if (mTransitionFromPage == 2) {
                // --- 转储页整页滑出 ---
                float localT = clamp(t / itemDuration, 0.0f, 1.0f);
                float ease = Easing::EaseInCubic(localT);
                mCurItemOffsetX = dir * screenW * ease;
                mCurItemOffsetY = 0.0f;
                DrawDumpPage();
            } else {
                // --- 源页逐条滑出 ---
                auto& sources = Config::GetInstance().GetSources();
                int activeSrc = Config::GetInstance().GetActiveSource();
                const int srcListX = 200;
                const int srcListY = 230;
                const int srcListW = Gfx::SCREEN_WIDTH - 400;
                const int srcItemH = 130;
                const int srcSpacing = 16;
                
                mSourceItemBounds.clear();
                mSourceItemBounds.resize(sources.size());
                
                // 标题也滑出
                float titleT = clamp(t / itemDuration, 0.0f, 1.0f);
                if (titleT < 1.0f) {
                    float te = Easing::EaseInCubic(titleT);
                    Gfx::Print(Gfx::SCREEN_WIDTH / 2 + (int)(dir * screenW * te), srcListY - 50, 52, Gfx::COLOR_TEXT, _("settings.select_source"), Gfx::ALIGN_CENTER);
                }
                
                for (size_t i = 0; i < sources.size(); i++) {
                    float localT = clamp((t - i * staggerDelay) / itemDuration, 0.0f, 1.0f);
                    if (localT >= 1.0f) continue;
                    float ease = Easing::EaseInCubic(localT);
                    mCurItemOffsetX = dir * screenW * ease;
                    mCurItemOffsetY = 0.0f;
                    
                    int sy = srcListY + (int)i * (srcItemH + srcSpacing);
                    int dx = srcListX + (int)mCurItemOffsetX;
                    int dy = sy + (int)mCurItemOffsetY;
                    mSourceItemBounds[i] = {srcListX, sy, srcListW, srcItemH};
                    
                    // 滑出时：选中项保留高亮，但不显示激活绿色
                    bool isSel = ((int)i == mSelectedSource);
                    float anim = isSel ? 1.0f : 0.0f;
                    float scale = 1.0f + anim * 0.025f;
                    int sw = (int)(srcListW * scale), sh = (int)(srcItemH * scale);
                    int sdx = dx + (srcListW - sw) / 2, sdy = dy + (srcItemH - sh) / 2;
                    
                    SDL_Color bg;
                    if (isSel) {
                        auto n = Gfx::COLOR_CARD_BG;
                        auto h = Gfx::COLOR_CARD_HOVER;
                        bg = {(Uint8)(n.r+(h.r-n.r)*anim),(Uint8)(n.g+(h.g-n.g)*anim),(Uint8)(n.b+(h.b-n.b)*anim),(Uint8)(n.a+(h.a-n.a)*anim)};
                    } else {
                        bg = Gfx::COLOR_CARD_BG;
                    }
                    Gfx::DrawRectRounded(sdx, sdy, sw, sh, 14, bg);
                    
                    if (isSel) {
                        SDL_Color bd = Gfx::COLOR_ACCENT;
                        bd.a = (Uint8)(180 * anim);
                        Gfx::DrawRectRoundedOutline(sdx-2, sdy-2, sw+4, sh+4, 16, 3, bd);
                    }
                    
                    bool isActive = ((int)i == activeSrc);
                    DrawSourceItem(sdx, sdy, sw, sh, sources[i], isActive, ((int)i == mSelectedSource), 0.0f, false);
                }
            }
            
            // ====== 绘制新页（逐条滑入）======
            if (mTransitionTargetPage == 1) {
                // --- 源页逐条滑入 ---
                auto& sources = Config::GetInstance().GetSources();
                int activeSrc = Config::GetInstance().GetActiveSource();
                const int srcListX = 200;
                const int srcListY = 230;
                const int srcListW = Gfx::SCREEN_WIDTH - 400;
                const int srcItemH = 130;
                const int srcSpacing = 16;
                
                if (mSourceItemBounds.size() != sources.size())
                    mSourceItemBounds.resize(sources.size());
                
                // 标题先滑入
                float titleT = clamp((t - enterStart) / itemDuration, 0.0f, 1.0f);
                if (titleT > 0.0f) {
                    float te = Easing::EaseOutCubic(titleT);
                    Gfx::Print(Gfx::SCREEN_WIDTH / 2 + (int)(-dir * screenW * (1.0f - te)), srcListY - 50, 52, Gfx::COLOR_TEXT, _("settings.select_source"), Gfx::ALIGN_CENTER);
                }
                
                for (size_t i = 0; i < sources.size(); i++) {
                    float localT = clamp((t - enterStart - i * staggerDelay) / itemDuration, 0.0f, 1.0f);
                    if (localT <= 0.0f) continue;
                    float ease = Easing::EaseOutCubic(localT);
                    mCurItemOffsetX = -dir * screenW * (1.0f - ease);
                    mCurItemOffsetY = 0.0f;
                    
                    int sy = srcListY + (int)i * (srcItemH + srcSpacing);
                    int dx = srcListX + (int)mCurItemOffsetX;
                    int dy = sy + (int)mCurItemOffsetY;
                    mSourceItemBounds[i] = {srcListX, sy, srcListW, srcItemH};
                    bool isActive = ((int)i == activeSrc);
                    bool isSel = ((int)i == mSelectedSource);
                    
                    float anim = isSel ? 1.0f : 0.0f;
                    float scale = 1.0f + anim * 0.025f;
                    int sw = (int)(srcListW * scale), sh = (int)(srcItemH * scale);
                    int sdx = dx + (srcListW - sw) / 2, sdy = dy + (srcItemH - sh) / 2;
                    
                    SDL_Color bg;
                    if (isActive) bg = {20, 60, 40, 230};
                    else if (isSel) { auto n = Gfx::COLOR_CARD_BG; auto h = Gfx::COLOR_CARD_HOVER; bg = {(Uint8)(n.r+(h.r-n.r)*anim),(Uint8)(n.g+(h.g-n.g)*anim),(Uint8)(n.b+(h.b-n.b)*anim),(Uint8)(n.a+(h.a-n.a)*anim)}; }
                    else bg = Gfx::COLOR_CARD_BG;
                    Gfx::DrawRectRounded(sdx, sdy, sw, sh, 14, bg);
                    
                    if (isSel) { SDL_Color bd = Gfx::COLOR_ACCENT; bd.a = (Uint8)(180 * anim); Gfx::DrawRectRoundedOutline(sdx-2, sdy-2, sw+4, sh+4, 16, 3, bd); }
                    
                    DrawSourceItem(sdx, sdy, sw, sh, sources[i], isActive, isSel, anim, true);
                }
            } else if (mTransitionTargetPage == 2) {
                // --- 转储页整页滑入 ---
                float localT = clamp((t - enterStart) / itemDuration, 0.0f, 1.0f);
                float ease = Easing::EaseOutCubic(localT);
                mCurItemOffsetX = -dir * screenW * (1.0f - ease);
                mCurItemOffsetY = 0.0f;
                DrawDumpPage();
            } else {
                // --- 设置项逐条滑入 ---
                const int topBarHeight = 120;
                const int itemHeight = 120;
                const int itemSpacing = 20;
                const int listX = 200;
                const int listY = topBarHeight + 30;
                const int listW = Gfx::SCREEN_WIDTH - 400;
                
                const auto& languages = Lang().GetAvailableLanguages();
                std::string currentLanguageName = "Unknown";
                for (const auto& lang : languages) {
                    if (lang.code == Lang().GetCurrentLanguage()) { currentLanguageName = lang.name; break; }
                }
                
                int currentY = listY;
                for (int i = 0; i < SETTINGS_COUNT; i++) {
                    float localT = clamp((t - enterStart - i * staggerDelay) / itemDuration, 0.0f, 1.0f);
                    if (localT <= 0.0f) { currentY += itemHeight + itemSpacing; continue; }
                    float ease = Easing::EaseOutCubic(localT);
                    mCurItemOffsetX = -dir * screenW * (1.0f - ease);
                    mCurItemOffsetY = 0.0f;
                    mSettingItemBounds[i] = {listX, currentY, listW, itemHeight};
                    switch (i) {
                        case SETTINGS_LANGUAGE:
                            DrawSettingItem(listX, currentY, listW, _("settings.language"), _("settings.language_desc"), currentLanguageName, mSelectedItem == i, mItemAnimProgress[i], 0xf0ac); break;
                        case SETTINGS_DOWNLOAD_PATH:
                            DrawSettingItem(listX, currentY, listW, _("settings.download_path"), _("settings.download_path_desc"), "SD:/wiiu/themes/", mSelectedItem == i, mItemAnimProgress[i], 0xf07c); break;
                        case SETTINGS_BGM_ENABLED:
                            DrawSettingItem(listX, currentY, listW, _("settings.bgm_enabled"), _("settings.bgm_enabled_desc"), Config::GetInstance().IsBgmEnabled() ? _("common.yes") : _("common.no"), mSelectedItem == i, mItemAnimProgress[i], 0xf001); break;
                        case SETTINGS_LOGGING_ENABLED:
                            DrawSettingItem(listX, currentY, listW, _("settings.logging"), _("settings.logging_desc"), Config::GetInstance().IsLoggingEnabled() ? _("common.yes") : _("common.no"), mSelectedItem == i, mItemAnimProgress[i], 0xf15c); break;
                        case SETTINGS_LOGGING_VERBOSE:
                            DrawSettingItem(listX, currentY, listW, _("settings.verbose_logging"), _("settings.verbose_logging_desc"), Config::GetInstance().IsVerboseLogging() ? _("common.yes") : _("common.no"), mSelectedItem == i, mItemAnimProgress[i], 0xf0ae); break;
                        case SETTINGS_CLEAR_CACHE:
                            DrawSettingItem(listX, currentY, listW, _("settings.clear_cache"), _("settings.clear_cache_desc"), _("settings.press_to_clear"), mSelectedItem == i, mItemAnimProgress[i], 0xf1f8); break;
                    }
                    currentY += itemHeight + itemSpacing;
                }
            }
            
            // 翻页按钮（过渡期间也绘制，但不响应触摸）
            DrawPageButtons();
            
            // 底栏和返回按钮（动画期间静态）
            std::string tgPageHint = mTransitionTargetPage == 0
                ? "R " + std::string(_("common.next_page")) + " | " + std::string(_("settings.select_source"))
                : (mTransitionTargetPage == 1
                    ? "L " + std::string(_("common.prev_page")) + " | R " + std::string(_("common.next_page"))
                    : "L " + std::string(_("common.prev_page")));
            DrawBottomBar(nullptr, (std::string("\ue044 ") + _("input.exit")).c_str(), tgPageHint.c_str());
            Screen::DrawBackButton();
            return;
        }
    }
    
    if (mLanguageDialogOpen) {
        DrawLanguageDialog();
        return;
    }
    
    // 页面绘制
    if (mCurrentPage == 1) {
        DrawSourcePage();
    } else if (mCurrentPage == 2) {
        DrawDumpPage();
    } else {
        // 设置项列表
        const int topBarHeight = 120;
    const int itemHeight = 120;
    const int itemSpacing = 20;
    const int listX = 200;
    const int listY = topBarHeight + 30;
    const int listW = Gfx::SCREEN_WIDTH - 400;
    
    // 获取当前语言名称
    const auto& languages = Lang().GetAvailableLanguages();
    std::string currentLanguageName = "Unknown";
    for (const auto& lang : languages) {
        if (lang.code == Lang().GetCurrentLanguage()) {
            currentLanguageName = lang.name;
            break;
        }
    } 
    
    // 绘制设置项
    int currentY = listY;
    
    // 语言设置 - 地球图标
    mSettingItemBounds[SETTINGS_LANGUAGE] = {listX, currentY, listW, itemHeight};
    DrawSettingItem(listX, currentY, listW, 
                   _("settings.language"), 
                   _("settings.language_desc"), 
                   currentLanguageName,
                   mSelectedItem == SETTINGS_LANGUAGE,
                   mItemAnimProgress[SETTINGS_LANGUAGE],
                   0xf0ac);  // fa-globe
    currentY += itemHeight + itemSpacing;
    
    // 下载路径设置 - 文件夹图标
    mSettingItemBounds[SETTINGS_DOWNLOAD_PATH] = {listX, currentY, listW, itemHeight};
    DrawSettingItem(listX, currentY, listW, 
                   _("settings.download_path"), 
                   _("settings.download_path_desc"), 
                   "SD:/wiiu/themes/",
                   mSelectedItem == SETTINGS_DOWNLOAD_PATH,
                   mItemAnimProgress[SETTINGS_DOWNLOAD_PATH],
                   0xf07c);  // fa-folder-open
    currentY += itemHeight + itemSpacing;
    
    // 背景音乐设置 - 音乐图标
    mSettingItemBounds[SETTINGS_BGM_ENABLED] = {listX, currentY, listW, itemHeight};
    DrawSettingItem(listX, currentY, listW, 
                   _("settings.bgm_enabled"), 
                   _("settings.bgm_enabled_desc"), 
                   Config::GetInstance().IsBgmEnabled() ? _("common.yes") : _("common.no"),
                   mSelectedItem == SETTINGS_BGM_ENABLED,
                   mItemAnimProgress[SETTINGS_BGM_ENABLED],
                   0xf001);  // fa-music
    currentY += itemHeight + itemSpacing;
    
    // 日志启用设置 - 文件图标
    mSettingItemBounds[SETTINGS_LOGGING_ENABLED] = {listX, currentY, listW, itemHeight};
    DrawSettingItem(listX, currentY, listW, 
                   _("settings.logging"), 
                   _("settings.logging_desc"), 
                   Config::GetInstance().IsLoggingEnabled() ? _("common.yes") : _("common.no"),
                   mSelectedItem == SETTINGS_LOGGING_ENABLED,
                   mItemAnimProgress[SETTINGS_LOGGING_ENABLED],
                   0xf15c);  // fa-file-text
    currentY += itemHeight + itemSpacing;
    
    // 详细日志设置 - 列表图标
    mSettingItemBounds[SETTINGS_LOGGING_VERBOSE] = {listX, currentY, listW, itemHeight};
    DrawSettingItem(listX, currentY, listW, 
                   _("settings.verbose_logging"), 
                   _("settings.verbose_logging_desc"), 
                   Config::GetInstance().IsVerboseLogging() ? _("common.yes") : _("common.no"),
                   mSelectedItem == SETTINGS_LOGGING_VERBOSE,
                   mItemAnimProgress[SETTINGS_LOGGING_VERBOSE],
                   0xf0ae);  // fa-tasks
    currentY += itemHeight + itemSpacing;
    
    // 清除缓存设置 - 垃圾桶图标
    mSettingItemBounds[SETTINGS_CLEAR_CACHE] = {listX, currentY, listW, itemHeight};
    DrawSettingItem(listX, currentY, listW, 
                   _("settings.clear_cache"), 
                   _("settings.clear_cache_desc"), 
                   _("settings.press_to_clear"),
                   mSelectedItem == SETTINGS_CLEAR_CACHE,
                   mItemAnimProgress[SETTINGS_CLEAR_CACHE],
                   0xf1f8);  // fa-trash
    }
    
    // 绘制翻页按钮
    DrawPageButtons();
    
    std::string pageHint = mCurrentPage == 0 
        ? "R " + std::string(_("common.next_page")) + " | " + std::string(_("settings.select_source"))
        : (mCurrentPage == 1
            ? "L " + std::string(_("common.prev_page")) + " | R " + std::string(_("common.next_page"))
            : "L " + std::string(_("common.prev_page")));
    DrawBottomBar(nullptr, (std::string("\ue044 ") + _("input.exit")).c_str(), pageHint.c_str());
    
    // 绘制圆形返回按钮
    Screen::DrawBackButton();
}

void SettingsScreen::DrawSettingItem(int x, int y, int w, const std::string& title, 
                                     const std::string& description, const std::string& value, 
                                     bool selected, float animProgress, uint16_t icon) {
    // 应用页面切换偏移
    x += (int)mCurItemOffsetX;
    y += (int)mCurItemOffsetY;
    
    const int itemH = 120; // 匹配Draw()中的itemHeight
    
    // 使用动画进度计算缩放效果
    float scale = 1.0f + (animProgress * 0.03f); // 选中时放大3%
    int scaledW = (int)(w * scale);
    int scaledH = (int)(itemH * scale);
    int offsetX = (w - scaledW) / 2;
    int offsetY = (itemH - scaledH) / 2;
    
    int drawX = x + offsetX;
    int drawY = y + offsetY;
    
    // 绘制背景和选中效果
    SDL_Color bgColor = Gfx::COLOR_CARD_BG;
    if (selected) {
        // 根据动画进度插值背景色
        SDL_Color hoverColor = Gfx::COLOR_CARD_HOVER;
        bgColor.r = (Uint8)(bgColor.r + (hoverColor.r - bgColor.r) * animProgress);
        bgColor.g = (Uint8)(bgColor.g + (hoverColor.g - bgColor.g) * animProgress);
        bgColor.b = (Uint8)(bgColor.b + (hoverColor.b - bgColor.b) * animProgress);
        
        // 绘制阴影
        SDL_Color shadowColor = Gfx::COLOR_SHADOW;
        shadowColor.a = (Uint8)(100 * animProgress);
        Gfx::DrawRectRounded(drawX + 4, drawY + 4, scaledW, scaledH, 12, shadowColor);
        
        // 绘制边框
        SDL_Color borderColor = Gfx::COLOR_ACCENT;
        borderColor.a = (Uint8)(180 * animProgress);
        Gfx::DrawRectRoundedOutline(drawX - 2, drawY - 2, scaledW + 4, scaledH + 4, 14, 3, borderColor);
    }
    
    Gfx::DrawRectRounded(drawX, drawY, scaledW, scaledH, 12, bgColor);
    
    // 绘制图标(如果提供)
    int iconSize = 50;
    int iconX = drawX + 30;
    int textStartX = drawX + 40;  // 默认文本起始位置
    
    if (icon != 0) {
        SDL_Color iconColor = Gfx::COLOR_ICON;
        if (selected) {
            // 选中时图标颜色变为accent色
            SDL_Color accentColor = Gfx::COLOR_ACCENT;
            iconColor.r = (Uint8)(iconColor.r + (accentColor.r - iconColor.r) * animProgress);
            iconColor.g = (Uint8)(iconColor.g + (accentColor.g - iconColor.g) * animProgress);
            iconColor.b = (Uint8)(iconColor.b + (accentColor.b - iconColor.b) * animProgress);
        }
        Gfx::DrawIcon(iconX, drawY + scaledH/2, iconSize, iconColor, icon, Gfx::ALIGN_VERTICAL);
        textStartX = iconX + iconSize + 30;  // 有图标时文本往右移
    }
    
    // 绘制文本
    SDL_Color titleColor = Gfx::COLOR_TEXT;
    SDL_Color descColor = Gfx::COLOR_ALT_TEXT;
    SDL_Color valueColor = Gfx::COLOR_ICON;
    
    if (selected) {
        // 根据动画进度插值文本颜色
        SDL_Color whiteColor = Gfx::COLOR_WHITE;
        titleColor.r = (Uint8)(titleColor.r + (whiteColor.r - titleColor.r) * animProgress);
        titleColor.g = (Uint8)(titleColor.g + (whiteColor.g - titleColor.g) * animProgress);
        titleColor.b = (Uint8)(titleColor.b + (whiteColor.b - titleColor.b) * animProgress);
        
        SDL_Color accentColor = Gfx::COLOR_ACCENT;
        valueColor.r = (Uint8)(valueColor.r + (accentColor.r - valueColor.r) * animProgress);
        valueColor.g = (Uint8)(valueColor.g + (accentColor.g - valueColor.g) * animProgress);
        valueColor.b = (Uint8)(valueColor.b + (accentColor.b - valueColor.b) * animProgress);
    }
    
    int valueX = drawX + scaledW - 40;
    
    // 计算垂直居中位置 - 往下移动
    const int titleSize = 38;  // 从44减小到38
    const int descSize = 28;   // 从32减小到28
    const int titleHeight = Gfx::GetTextHeight(titleSize, title.c_str());
    const int descHeight = Gfx::GetTextHeight(descSize, description.c_str());
    const int totalTextHeight = titleHeight + descHeight + 8; // 8px间距
    const int textStartY = drawY + (scaledH - totalTextHeight) / 2 + 10;
    
    Gfx::Print(textStartX, textStartY + 15, titleSize, titleColor, title, Gfx::ALIGN_VERTICAL);
    Gfx::Print(textStartX, textStartY + titleHeight + 8, descSize, descColor, description, Gfx::ALIGN_VERTICAL);
    
    // 计算值文本宽度,为箭头留出空间
    const int valueSize = 36;  // 从40减小到36
    int valueWidth = Gfx::GetTextWidth(valueSize, value.c_str());
    int arrowWidth = 28;       // 从32减小到28
    int spacing = 50; // 箭头和值文本之间的间距
    
    if (selected && animProgress > 0.1f) {
        // 箭头位置:在值文本左侧,带有动画淡入效果
        int arrowX = valueX - valueWidth - spacing;
        SDL_Color arrowColor = Gfx::COLOR_ACCENT;
        arrowColor.a = (Uint8)(255 * animProgress);
        Gfx::DrawIcon(arrowX, drawY + scaledH/2, arrowWidth, arrowColor, 0xf054, Gfx::ALIGN_VERTICAL);
    }
    
    Gfx::Print(valueX, drawY + scaledH/2, valueSize, valueColor, value, Gfx::ALIGN_VERTICAL | Gfx::ALIGN_RIGHT);
}

void SettingsScreen::DrawLanguageDialog() {
    // 更新语言项动画进度
    const auto& languages = Lang().GetAvailableLanguages();
    const int itemsPerColumn = (static_cast<int>(languages.size()) + 1) / 2;
    
    for (size_t i = 0; i < languages.size(); i++) {
        int column = static_cast<int>(i) / itemsPerColumn;
        int rowInColumn = static_cast<int>(i) % itemsPerColumn;
        bool isSelected = (column == mSelectedColumn && rowInColumn == mSelectedLanguage);
        
        if (isSelected) {
            // 当前选中项:逐渐增加到1.0
            mLanguageItemAnimProgress[i] += (1.0f - mLanguageItemAnimProgress[i]) * 0.2f;
        } else {
            // 非选中项:逐渐减少到0.0
            mLanguageItemAnimProgress[i] *= 0.8f;
        }
    }
    
    // 半透明遮罩
    SDL_Color overlay = {0, 0, 0, 180};
    Gfx::DrawRectFilled(0, 0, Gfx::SCREEN_WIDTH, Gfx::SCREEN_HEIGHT, overlay);
    
    // 对话框 - 更大更宽,减少底部空白
    const int dialogW = 1600;  // 从1400增加到1600
    const int dialogH = 800;   // 减少从900到800,减少底部空白
    const int dialogX = (Gfx::SCREEN_WIDTH - dialogW) / 2;
    const int dialogY = (Gfx::SCREEN_HEIGHT - dialogH) / 2;
    
    // 绘制对话框背景
    SDL_Color shadowColor = Gfx::COLOR_SHADOW;
    shadowColor.a = 120;
    Gfx::DrawRectRounded(dialogX + 8, dialogY + 8, dialogW, dialogH, 20, shadowColor);
    Gfx::DrawRectRounded(dialogX, dialogY, dialogW, dialogH, 20, Gfx::COLOR_CARD_BG);
    
    // 双列布局 - 移除标题,直接显示语言列表
    const int columnW = (dialogW - 120) / 2;  // 两列宽度
    const int columnSpacing = 40;  // 列间距
    const int leftColumnX = dialogX + 40;
    const int rightColumnX = leftColumnX + columnW + columnSpacing;
    const int itemH = 80;          // 从70增加到80
    const int listStartY = dialogY + 60;  // 从130减少到60,顶部留更少空间
    
    for (size_t i = 0; i < languages.size(); i++) {
        // 确定当前项在哪一列
        int column = static_cast<int>(i) / itemsPerColumn;
        int rowInColumn = static_cast<int>(i) % itemsPerColumn;
        
        int itemX = (column == 0) ? leftColumnX : rightColumnX;
        int itemY = listStartY + rowInColumn * itemH;
        
        // 修复选中判断:需要同时匹配列和行
        bool isSelected = (column == mSelectedColumn && rowInColumn == mSelectedLanguage);
        bool isCurrent = (languages[i].code == Lang().GetCurrentLanguage());
        
        // 获取动画进度
        float animProgress = mLanguageItemAnimProgress[i];
        
        // 使用动画进度计算缩放效果
        float scale = 1.0f + (animProgress * 0.02f); // 选中时放大2%
        int scaledW = (int)(columnW * scale);
        int scaledH = (int)((itemH - 10) * scale);
        int offsetX = (columnW - scaledW) / 2;
        int offsetY = ((itemH - 10) - scaledH) / 2;
        
        int drawX = itemX + offsetX;
        int drawY = itemY + 5 + offsetY;
        
        // 绘制选中背景和动画效果
        SDL_Color bgColor = Gfx::COLOR_CARD_BG;
        if (isSelected) {
            // 根据动画进度插值背景色
            SDL_Color selectBg = Gfx::COLOR_ACCENT;
            selectBg.a = (Uint8)(80 + 40 * animProgress);  // 80-120透明度
            
            // 绘制阴影
            SDL_Color shadowColor = Gfx::COLOR_SHADOW;
            shadowColor.a = (Uint8)(60 * animProgress);
            Gfx::DrawRectRounded(drawX + 3, drawY + 3, scaledW, scaledH, 8, shadowColor);
            
            // 绘制边框
            SDL_Color borderColor = Gfx::COLOR_ACCENT;
            borderColor.a = (Uint8)(180 * animProgress);
            Gfx::DrawRectRoundedOutline(drawX - 2, drawY - 2, scaledW + 4, scaledH + 4, 10, 2, borderColor);
            
            // 绘制背景
            Gfx::DrawRectRounded(drawX, drawY, scaledW, scaledH, 8, selectBg);
            
            // 绘制左侧指示条
            SDL_Color indicatorColor = Gfx::COLOR_ACCENT;
            indicatorColor.a = (Uint8)(255 * animProgress);
            Gfx::DrawRectRounded(drawX + 5, drawY + 10, 6, scaledH - 20, 3, indicatorColor);
        } else {
            // 未选中项只绘制基础背景
            Gfx::DrawRectRounded(drawX, drawY, scaledW, scaledH, 8, bgColor);
        }
        
        // 绘制语言名称 - 临时切换字体以正确显示各语言文字
        SDL_Color textColor = Gfx::COLOR_TEXT;
        if (isSelected) {
            // 根据动画进度插值文本颜色
            SDL_Color whiteColor = Gfx::COLOR_WHITE;
            textColor.r = (Uint8)(textColor.r + (whiteColor.r - textColor.r) * animProgress);
            textColor.g = (Uint8)(textColor.g + (whiteColor.g - textColor.g) * animProgress);
            textColor.b = (Uint8)(textColor.b + (whiteColor.b - textColor.b) * animProgress);
        }
        
        // 保存当前字体设置
        bool originalFontSetting = (languages[i].code == "zh-cn" || 
                                    languages[i].code == "zh-tw" || 
                                    languages[i].code == "ja-jp" || 
                                    languages[i].code == "ko-kr");
        
        // 临时切换到该语言需要的字体
        Gfx::SetUseLatinFont(!originalFontSetting);
        
        Gfx::Print(drawX + 30, drawY + scaledH/2, 38, textColor, 
                  languages[i].name, Gfx::ALIGN_VERTICAL);
        
        // 恢复到当前语言的字体设置
        bool currentNeedsCJK = (Lang().GetCurrentLanguage() == "zh-cn" || 
                                Lang().GetCurrentLanguage() == "zh-tw" || 
                                Lang().GetCurrentLanguage() == "ja-jp" || 
                                Lang().GetCurrentLanguage() == "ko-kr");
        Gfx::SetUseLatinFont(!currentNeedsCJK);
        
        // 绘制当前语言标记
        if (isCurrent) {
            SDL_Color checkColor = Gfx::COLOR_SUCCESS;
            if (isSelected) {
                // 选中时检查标记也有动画
                checkColor.a = (Uint8)(255 * (0.6f + 0.4f * animProgress));
            }
            Gfx::DrawIcon(drawX + scaledW - 50, drawY + scaledH/2, 28, checkColor, 
                         0xf00c, Gfx::ALIGN_VERTICAL);
        }
        
        // 保存边界用于触摸检测
        mLanguageCardBounds[i] = {drawX, drawY, scaledW, scaledH};
    }
}

bool SettingsScreen::Update(Input &input) {
    // 检测返回按钮点击
    if (Screen::UpdateBackButton(input)) {
        return false;
    }
    
    // 按B键返回（在所有页面和过渡动画期间都有效）
    if (input.data.buttons_d & Input::BUTTON_B) {
        return false;
    }
    
    // R 翻下一页 / L 翻上一页（过渡动画期间也能反向切换）
    int effectivePage = mIsPageTransitioning ? mTransitionTargetPage : mCurrentPage;
    
    if (input.data.buttons_d & Input::BUTTON_R && effectivePage < TOTAL_PAGES - 1) {
        mTransitionFromPage = effectivePage;
        mTransitionTargetPage = effectivePage + 1;
        mPageTransitionAnim.Start(0.0f, 1.0f, 1600.0f);
        mIsPageTransitioning = true;
        mSelectedSource = Config::GetInstance().GetActiveSource();
        Config::GetInstance().GetSources();  // 预热源列表
    }
    if (input.data.buttons_d & Input::BUTTON_L && effectivePage > 0) {
        mTransitionFromPage = effectivePage;
        mTransitionTargetPage = effectivePage - 1;
        mPageTransitionAnim.Start(0.0f, 1.0f, 1600.0f);
        mIsPageTransitioning = true;
    }
    
    // 检测触摸翻页按钮（过渡动画期间也能反向切换）
    if (input.data.touched && input.data.validPointer && !input.lastData.touched) {
        int touchX = (int)((input.data.x * 1920.0f / 1280.0f) + 960);
        int touchY = (int)(540 - (input.data.y * 1080.0f / 720.0f));
        
        // 左箭头
        if (effectivePage > 0 && IsTouchInRect(touchX, touchY, 
            mPageBtnLeftBounds.x, mPageBtnLeftBounds.y, mPageBtnLeftBounds.w, mPageBtnLeftBounds.h)) {
            mTransitionFromPage = effectivePage;
            mTransitionTargetPage = effectivePage - 1;
            mPageTransitionAnim.Start(0.0f, 1.0f, 1600.0f);
            mIsPageTransitioning = true;
            mSelectedSource = Config::GetInstance().GetActiveSource();
            return true;
        }
        
        // 右箭头
        if (effectivePage < TOTAL_PAGES - 1 && IsTouchInRect(touchX, touchY,
            mPageBtnRightBounds.x, mPageBtnRightBounds.y, mPageBtnRightBounds.w, mPageBtnRightBounds.h)) {
            mTransitionFromPage = effectivePage;
            mTransitionTargetPage = effectivePage + 1;
            mPageTransitionAnim.Start(0.0f, 1.0f, 1600.0f);
            mIsPageTransitioning = true;
            mSelectedSource = Config::GetInstance().GetActiveSource();
            Config::GetInstance().GetSources();  // 预热源列表
            return true;
        }
    }
    
    // 过渡动画期间屏蔽其他输入
    if (mIsPageTransitioning) {
        return true;
    }
    
    // 数据源选择页
    if (mCurrentPage == 1) {
        auto& sources = Config::GetInstance().GetSources();
        if (sources.empty()) return true;
        
        if (input.data.buttons_d & Input::BUTTON_UP && mSelectedSource > 0) mSelectedSource--;
        if (input.data.buttons_d & Input::BUTTON_DOWN && mSelectedSource < (int)sources.size() - 1) mSelectedSource++;
        if (input.data.buttons_d & Input::BUTTON_A)
            Config::GetInstance().SetActiveSource(mSelectedSource);
        
        // 触屏选择
        if (input.data.touched && input.data.validPointer && !input.lastData.touched) {
            int touchX = (int)((input.data.x * 1920.0f / 1280.0f) + 960);
            int touchY = (int)(540 - (input.data.y * 1080.0f / 720.0f));
            for (size_t i = 0; i < mSourceItemBounds.size() && i < sources.size(); i++) {
                auto& b = mSourceItemBounds[i];
                if (IsTouchInRect(touchX, touchY, b.x, b.y, b.w, b.h)) {
                    mSelectedSource = (int)i;
                    Config::GetInstance().SetActiveSource(mSelectedSource);
                    break;
                }
            }
        }
        return true;
    }
    
    // 转储菜单文件页
    if (mCurrentPage == 2) {
        if (mDumpState == DUMP_DUMPING) {
            ContinueDump();
            return true;
        }
        
        // 触摸按钮（空闲或完成/错误时可重新操作）
        if (input.data.touched && input.data.validPointer && !input.lastData.touched) {
            int touchX = (int)((input.data.x * 1920.0f / 1280.0f) + 960);
            int touchY = (int)(540 - (input.data.y * 1080.0f / 720.0f));
            if (IsTouchInRect(touchX, touchY, mDumpBtnBounds.x, mDumpBtnBounds.y, 
                              mDumpBtnBounds.w, mDumpBtnBounds.h)) {
                StartDump();
                return true;
            }
        }
        
        if (input.data.buttons_d & Input::BUTTON_A) {
            StartDump();
        }
        return true;
    }
    
    // 检查是否正在等待退出
    if (mWaitingForExit) {
        uint64_t currentTime = OSGetSystemTime();
        uint64_t elapsed = OSTicksToMilliseconds(currentTime - mExitStartTime);
        
        // 等待2秒后退出
        if (elapsed >= 2000) {
            SYSLaunchMenu();
            return false;
        }
        
        // 在等待期间继续返回 true 保持渲染
        return true;
    }
    
    if (mLanguageDialogOpen) {
        return UpdateLanguageDialog(input);
    }
    
    // 检测触摸设置项
    if (input.data.touched && input.data.validPointer && !input.lastData.touched) {
        // 转换触摸坐标 (DRC: 854x480 -> 1920x1080)
        int touchX = (int)((input.data.x * 1920.0f / 1280.0f) + 960);
        int touchY = (int)(540 - (input.data.y * 1080.0f / 720.0f));
        
        // 检查是否点击了设置项
        for (int i = 0; i < SETTINGS_COUNT; i++) {
            const auto& bounds = mSettingItemBounds[i];
            if (IsTouchInRect(touchX, touchY, bounds.x, bounds.y, bounds.w, bounds.h)) {
                // 如果点击已选中的项，执行操作
                if (i == mSelectedItem) {
                    // 执行该设置项的操作
                    switch (i) {
                        case SETTINGS_LANGUAGE:
                            mLanguageDialogOpen = true;
                            break;
                        case SETTINGS_DOWNLOAD_PATH:
                            // TODO: 打开路径设置对话框
                            break;
                        case SETTINGS_BGM_ENABLED:
                            {
                                bool newState = !Config::GetInstance().IsBgmEnabled();
                                Config::GetInstance().SetBgmEnabled(newState);
                            }
                            break;
                        case SETTINGS_LOGGING_ENABLED:
                            {
                                bool newState = !Config::GetInstance().IsLoggingEnabled();
                                Config::GetInstance().SetLoggingEnabled(newState);
                                FileLogger::GetInstance().SetEnabled(newState);
                                if (newState) {
                                    FileLogger::GetInstance().StartLog();
                                }
                            }
                            break;
                        case SETTINGS_LOGGING_VERBOSE:
                            {
                                bool newState = !Config::GetInstance().IsVerboseLogging();
                                Config::GetInstance().SetVerboseLogging(newState);
                                FileLogger::GetInstance().SetVerbose(newState);
                            }
                            break;
                        case SETTINGS_CLEAR_CACHE:
                            ClearCache();
                            break;
                    }
                } else {
                    // 否则选中该项
                    mSelectedItem = i;
                }
                return true;
            }
        }
    }
    
    // 检测上下按键(D-Pad + 摇杆)
    bool upPressed = (input.data.buttons_d & Input::BUTTON_UP) || (input.data.buttons_d & Input::STICK_L_UP);
    bool downPressed = (input.data.buttons_d & Input::BUTTON_DOWN) || (input.data.buttons_d & Input::STICK_L_DOWN);
    bool upHeld = (input.data.buttons_h & Input::BUTTON_UP) || (input.data.buttons_h & Input::STICK_L_UP);
    bool downHeld = (input.data.buttons_h & Input::BUTTON_DOWN) || (input.data.buttons_h & Input::STICK_L_DOWN);
    
    // 上下选择设置项(支持循环选择)
    bool shouldMoveUp = false;
    bool shouldMoveDown = false;
    
    if (upPressed) {
        shouldMoveUp = true;
        mHoldFrames = 0;
    } else if (upHeld) {
        mHoldFrames++;
        if (mHoldFrames >= mRepeatDelay) {
            if ((mHoldFrames - mRepeatDelay) % mRepeatRate == 0) {
                shouldMoveUp = true;
            }
        }
    } else if (downPressed) {
        shouldMoveDown = true;
        mHoldFrames = 0;
    } else if (downHeld) {
        mHoldFrames++;
        if (mHoldFrames >= mRepeatDelay) {
            if ((mHoldFrames - mRepeatDelay) % mRepeatRate == 0) {
                shouldMoveDown = true;
            }
        }
    } else {
        mHoldFrames = 0;
    }
    
    if (shouldMoveUp) {
        mSelectedItem = (mSelectedItem - 1 + SETTINGS_COUNT) % SETTINGS_COUNT;
    } else if (shouldMoveDown) {
        mSelectedItem = (mSelectedItem + 1) % SETTINGS_COUNT;
    }
    
    // A键进入设置项
    if (input.data.buttons_d & Input::BUTTON_A) {
        switch (mSelectedItem) {
            case SETTINGS_LANGUAGE:
                mLanguageDialogOpen = true;
                break;
            case SETTINGS_DOWNLOAD_PATH:
                // TODO: 打开路径设置对话框
                break;
            case SETTINGS_BGM_ENABLED:
                // 切换背景音乐
                {
                    bool newState = !Config::GetInstance().IsBgmEnabled();
                    Config::GetInstance().SetBgmEnabled(newState);
                    // MusicPlayer会在下一帧检查配置并更新状态
                }
                break;
            case SETTINGS_LOGGING_ENABLED:
                // 切换日志启用状态
                {
                    bool newState = !Config::GetInstance().IsLoggingEnabled();
                    Config::GetInstance().SetLoggingEnabled(newState);
                    FileLogger::GetInstance().SetEnabled(newState);
                    if (newState) {
                        FileLogger::GetInstance().StartLog();
                    }
                }
                break;
            case SETTINGS_LOGGING_VERBOSE:
                // 切换详细日志模式
                {
                    bool newState = !Config::GetInstance().IsVerboseLogging();
                    Config::GetInstance().SetVerboseLogging(newState);
                    FileLogger::GetInstance().SetVerbose(newState);
                }
                break;
            case SETTINGS_CLEAR_CACHE:
                // 清除缓存
                {
                    ClearCache();
                }
                break;
        }
    }
    
    return true;
}

bool SettingsScreen::UpdateLanguageDialog(Input &input) {
    const auto& languages = Lang().GetAvailableLanguages();
    
    // 按B键关闭对话框
    if (input.data.buttons_d & Input::BUTTON_B) {
        mLanguageDialogOpen = false;
        mDialogHoldFrames = 0;  // 重置计数器
        return true;
    }
    
    const int itemsPerColumn = (static_cast<int>(languages.size()) + 1) / 2;
    
    // 触摸处理
    if (input.data.touched && input.data.validPointer) {
        float scaleX = 1920.0f / 1280.0f;
        float scaleY = 1080.0f / 720.0f;
        int touchX = (Gfx::SCREEN_WIDTH / 2) + (int)(input.data.x * scaleX);
        int touchY = (Gfx::SCREEN_HEIGHT / 2) - (int)(input.data.y * scaleY);
        
        // 检测点击哪个语言卡片
        for (size_t i = 0; i < languages.size(); i++) {
            const auto& bounds = mLanguageCardBounds[i];
            if (touchX >= bounds.x && touchX <= bounds.x + bounds.w &&
                touchY >= bounds.y && touchY <= bounds.y + bounds.h) {
                if (!mTouchStartedOnLanguageCard) {
                    int column = static_cast<int>(i) / itemsPerColumn;
                    int rowInColumn = static_cast<int>(i) % itemsPerColumn;
                    
                    if (column == mSelectedColumn && rowInColumn == mSelectedLanguage) {
                        // 双击效果: 点击选中项 = 确认
                        std::string newLang = languages[i].code;
                        Lang().SetCurrentLanguage(newLang);
                        mLanguageDialogOpen = false;
                    } else {
                        // 切换选中项
                        mSelectedColumn = column;
                        mSelectedLanguage = rowInColumn;
                    }
                    mTouchStartedOnLanguageCard = true;
                }
                break;
            }
        }
    } else {
        mTouchStartedOnLanguageCard = false;
    }
    
    // 检测上下左右按键(D-Pad + 摇杆)
    bool upPressed = (input.data.buttons_d & Input::BUTTON_UP) || (input.data.buttons_d & Input::STICK_L_UP);
    bool downPressed = (input.data.buttons_d & Input::BUTTON_DOWN) || (input.data.buttons_d & Input::STICK_L_DOWN);
    bool leftPressed = (input.data.buttons_d & Input::BUTTON_LEFT) || (input.data.buttons_d & Input::STICK_L_LEFT);
    bool rightPressed = (input.data.buttons_d & Input::BUTTON_RIGHT) || (input.data.buttons_d & Input::STICK_L_RIGHT);
    bool upHeld = (input.data.buttons_h & Input::BUTTON_UP) || (input.data.buttons_h & Input::STICK_L_UP);
    bool downHeld = (input.data.buttons_h & Input::BUTTON_DOWN) || (input.data.buttons_h & Input::STICK_L_DOWN);
    
    // 上下选择语言
    bool shouldMoveUp = false;
    bool shouldMoveDown = false;
    
    if (upPressed) {
        shouldMoveUp = true;
        mDialogHoldFrames = 0;
    } else if (upHeld) {
        mDialogHoldFrames++;
        if (mDialogHoldFrames >= mRepeatDelay) {
            if ((mDialogHoldFrames - mRepeatDelay) % mRepeatRate == 0) {
                shouldMoveUp = true;
            }
        }
    } else if (downPressed) {
        shouldMoveDown = true;
        mDialogHoldFrames = 0;
    } else if (downHeld) {
        mDialogHoldFrames++;
        if (mDialogHoldFrames >= mRepeatDelay) {
            if ((mDialogHoldFrames - mRepeatDelay) % mRepeatRate == 0) {
                shouldMoveDown = true;
            }
        }
    } else {
        mDialogHoldFrames = 0;
    }
    
    // 上下移动
    if (shouldMoveUp) {
        if (mSelectedLanguage > 0) {
            mSelectedLanguage--;
        } else if (mSelectedColumn == 1) {
            // 在右列顶部,移到左列底部
            mSelectedColumn = 0;
            mSelectedLanguage = itemsPerColumn - 1;
        }
    } else if (shouldMoveDown) {
        int currentColumnItems = (mSelectedColumn == 0) ? itemsPerColumn : 
                                  static_cast<int>(languages.size()) - itemsPerColumn;
        if (mSelectedLanguage < currentColumnItems - 1) {
            mSelectedLanguage++;
        } else if (mSelectedColumn == 0 && itemsPerColumn < static_cast<int>(languages.size())) {
            // 在左列底部,移到右列顶部
            mSelectedColumn = 1;
            mSelectedLanguage = 0;
        }
    }
    
    // 左右切换列
    if (leftPressed && mSelectedColumn == 1) {
        mSelectedColumn = 0;
        // 确保索引有效
        if (mSelectedLanguage >= itemsPerColumn) {
            mSelectedLanguage = itemsPerColumn - 1;
        }
        mDialogHoldFrames = 0;
    } else if (rightPressed && mSelectedColumn == 0) {
        int rightColumnItems = static_cast<int>(languages.size()) - itemsPerColumn;
        if (rightColumnItems > 0) {
            mSelectedColumn = 1;
            // 确保索引有效
            if (mSelectedLanguage >= rightColumnItems) {
                mSelectedLanguage = rightColumnItems - 1;
            }
        }
        mDialogHoldFrames = 0;
    }
    
    // A键确认选择语言
    if (input.data.buttons_d & Input::BUTTON_A) {
        int selectedIndex = mSelectedColumn * itemsPerColumn + mSelectedLanguage;
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(languages.size())) {
            std::string newLang = languages[selectedIndex].code;
            // SetCurrentLanguage内部会自动保存到Config
            Lang().SetCurrentLanguage(newLang);
            mLanguageDialogOpen = false;
        }
    }
    
    return true;
}

void SettingsScreen::ClearCache() {
    FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Starting cache clear operation");
    
    // 不区分大小写比较字符串的辅助函数
    auto strcasecmp_custom = [](const char* s1, const char* s2) -> bool {
        while (*s1 && *s2) {
            if (tolower(*s1) != tolower(*s2)) return false;
            s1++;
            s2++;
        }
        return *s1 == *s2;
    };
    
    // 递归删除目录的辅助函数
    std::function<bool(const std::string&)> removeDirectory = [&](const std::string& path) -> bool {
        DIR* dir = opendir(path.c_str());
        if (!dir) {
            FileLogger::GetInstance().LogError("[CLEAR CACHE] Failed to open directory: %s", path.c_str());
            return false;
        }
        
        FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Processing directory: %s", path.c_str());
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            std::string fullPath = path + "/" + entry->d_name;
            
            if (entry->d_type == DT_DIR) {
                // 递归删除子目录
                removeDirectory(fullPath);
            } else {
                // 删除文件
                FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Deleting file: %s", fullPath.c_str());
                if (remove(fullPath.c_str()) != 0) {
                    FileLogger::GetInstance().LogError("[CLEAR CACHE] Failed to delete file: %s", fullPath.c_str());
                }
            }
        }
        closedir(dir);
        
        // 删除目录本身
        FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Removing directory: %s", path.c_str());
        bool result = rmdir(path.c_str()) == 0;
        if (!result) {
            FileLogger::GetInstance().LogError("[CLEAR CACHE] Failed to remove directory: %s", path.c_str());
        }
        return result;
    };
    
    // 删除 SD:/utheme 文件夹(扫描并不区分大小写匹配)
    FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Scanning SD root for utheme folders");
    DIR* sdRoot = opendir("fs:/vol/external01");
    if (sdRoot) {
        struct dirent* entry;
        while ((entry = readdir(sdRoot)) != nullptr) {
            if (strcasecmp_custom(entry->d_name, "utheme")) {
                std::string fullPath = std::string("fs:/vol/external01/") + entry->d_name;
                FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Found utheme folder: %s", fullPath.c_str());
                struct stat st;
                if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    removeDirectory(fullPath);
                }
            }
        }
        closedir(sdRoot);
    } else {
        FileLogger::GetInstance().LogError("[CLEAR CACHE] Failed to open SD root directory");
    }
    
    // 删除配置文件 SD:/wiiu/utheme.cfg(扫描并不区分大小写匹配)
    FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Scanning wiiu folder for config files");
    DIR* wiiuDir = opendir("fs:/vol/external01/wiiu");
    if (wiiuDir) {
        struct dirent* entry;
        while ((entry = readdir(wiiuDir)) != nullptr) {
            if (strcasecmp_custom(entry->d_name, "utheme.cfg")) {
                std::string fullPath = std::string("fs:/vol/external01/wiiu/") + entry->d_name;
                FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Deleting config file: %s", fullPath.c_str());
                if (remove(fullPath.c_str()) != 0) {
                    FileLogger::GetInstance().LogError("[CLEAR CACHE] Failed to delete config file: %s", fullPath.c_str());
                }
            }
        }
        closedir(wiiuDir);
    } else {
        FileLogger::GetInstance().LogError("[CLEAR CACHE] Failed to open wiiu directory");
    }
    
    FileLogger::GetInstance().LogInfo("[CLEAR CACHE] Cache clear operation completed");
    
    // 显示两条通知
    Screen::GetBgmNotification().ShowInfo(_("settings.cache_cleared"));
    Screen::GetBgmNotification().ShowInfo(_("settings.restarting_app"));
    
    // 启动退出计时器
    mWaitingForExit = true;
    mExitStartTime = OSGetSystemTime();
}

// 检查触摸点是否在矩形内
bool SettingsScreen::IsTouchInRect(int touchX, int touchY, int rectX, int rectY, int rectW, int rectH) {
    return touchX >= rectX && touchX <= rectX + rectW &&
           touchY >= rectY && touchY <= rectY + rectH;
}

// ============================================================
// 统一的源项绘制（DrawSourcePage + 过渡动画共用）
// ============================================================
void SettingsScreen::DrawSourceItem(int x, int y, int w, int h, const ThemeSource& source,
                                    bool isActive, bool isSelected, float anim,
                                    bool showActiveHighlight) {
    // 背景
    SDL_Color bg;
    if (isActive && showActiveHighlight) {
        bg = {20, 60, 40, 230};
    } else if (isSelected) {
        auto n = Gfx::COLOR_CARD_BG;
        auto hv = Gfx::COLOR_CARD_HOVER;
        bg = {(Uint8)(n.r+(hv.r-n.r)*anim),(Uint8)(n.g+(hv.g-n.g)*anim),(Uint8)(n.b+(hv.b-n.b)*anim),(Uint8)(n.a+(hv.a-n.a)*anim)};
    } else {
        bg = Gfx::COLOR_CARD_BG;
    }
    Gfx::DrawRectRounded(x, y, w, h, 14, bg);
    
    // 选中边框
    if (isSelected) {
        SDL_Color bd = Gfx::COLOR_ACCENT;
        bd.a = (Uint8)(180 * anim);
        Gfx::DrawRectRoundedOutline(x-2, y-2, w+4, h+4, 16, 3, bd);
    }
    
    // 名称 / Logo
    if (mThemezerLogo && source.name == "Themezer") {
        int logoH = 90, logoW = 313;
        SDL_Rect dst = {x + 30, y + (h - logoH) / 2, logoW, logoH};
        SDL_RenderCopy(Gfx::GetRenderer(), mThemezerLogo, nullptr, &dst);
    } else {
        Gfx::Print(x + 30, y + 30, 40, (isActive && showActiveHighlight) ? Gfx::COLOR_SUCCESS : Gfx::COLOR_TEXT, source.name, Gfx::ALIGN_VERTICAL);
        SDL_Color uc = Gfx::COLOR_ALT_TEXT; uc.a = 180;
        std::string ud = source.graphqlUrl;
        if (ud.size() > 60) ud = ud.substr(0, 57) + "...";
        Gfx::Print(x + 30, y + 75, 24, uc, ud, Gfx::ALIGN_VERTICAL);
    }
    
    // 激活标签
    if (isActive) {
        Gfx::Print(x + w - 40, y + h / 2, 30, Gfx::COLOR_SUCCESS, _("settings.active"), Gfx::ALIGN_CENTER | Gfx::ALIGN_RIGHT);
    }
}

// ============================================================
// 第二页：数据源选择
// ============================================================
void SettingsScreen::DrawSourcePage() {
    auto& sources = Config::GetInstance().GetSources();
    if (sources.empty()) {
        Gfx::Print(Gfx::SCREEN_WIDTH / 2, Gfx::SCREEN_HEIGHT / 2, 48, Gfx::COLOR_ALT_TEXT,
            _("settings.no_sources"), Gfx::ALIGN_CENTER);
        return;
    }
    
    const int listX = 200;
    const int listY = 230;
    const int listW = Gfx::SCREEN_WIDTH - 400;
    const int itemH = 130;
    const int spacing = 16;
    int activeSrc = Config::GetInstance().GetActiveSource();
    
    mSourceItemBounds.clear();
    mSourceItemBounds.resize(sources.size());
    
    Gfx::Print(Gfx::SCREEN_WIDTH / 2, listY - 50, 52, Gfx::COLOR_TEXT, _("settings.select_source"), Gfx::ALIGN_CENTER);
    
    for (size_t i = 0; i < sources.size(); i++) {
        int y = listY + (int)i * (itemH + spacing);
        mSourceItemBounds[i] = {listX, y, listW, itemH};
        
        bool isActive = ((int)i == activeSrc);
        bool isSelected = ((int)i == mSelectedSource);
        
        // 动画进度
        float anim = (isSelected ? 1.0f : 0.0f);
        float scale = 1.0f + anim * 0.025f;
        int sw = (int)(listW * scale), sh = (int)(itemH * scale);
        int dx = listX + (listW - sw) / 2, dy = y + (itemH - sh) / 2;
        
        // 背景
        SDL_Color bgActive = {20, 60, 40, 230};
        SDL_Color bgHover = Gfx::COLOR_CARD_HOVER;
        SDL_Color bgNormal = Gfx::COLOR_CARD_BG;
        SDL_Color bg;
        if (isActive) {
            bg = bgActive;
        } else if (isSelected) {
            bg.r = (Uint8)(bgNormal.r + (bgHover.r - bgNormal.r) * anim);
            bg.g = (Uint8)(bgNormal.g + (bgHover.g - bgNormal.g) * anim);
            bg.b = (Uint8)(bgNormal.b + (bgHover.b - bgNormal.b) * anim);
            bg.a = (Uint8)(bgNormal.a + (bgHover.a - bgNormal.a) * anim);
        } else {
            bg = bgNormal;
        }
        Gfx::DrawRectRounded(dx, dy, sw, sh, 14, bg);
        
        // 选中边框
        if (isSelected) {
            SDL_Color border = Gfx::COLOR_ACCENT;
            border.a = (Uint8)(180 * anim);
            Gfx::DrawRectRoundedOutline(dx - 2, dy - 2, sw + 4, sh + 4, 16, 3, border);
        }
        
        DrawSourceItem(dx, dy, sw, sh, sources[i], isActive, isSelected, anim, true);
    }
}

void SettingsScreen::DrawPageButtons() {
    const int btnY = 930;
    const int btnSize = 48;
    const int btnRadius = 24;
    
    // 过渡期间使用目标页，过渡结束后使用当前页
    int effectivePage = mIsPageTransitioning ? mTransitionTargetPage : mCurrentPage;
    
    SDL_Color btnBg = {0, 0, 0, 80};
    SDL_Color arrowColor = Gfx::COLOR_ALT_TEXT;
    
    // 左箭头按钮（非第一页时显示）
    const int leftBtnX = 70;
    mPageBtnLeftBounds = {leftBtnX - btnSize/2, btnY - btnSize/2, btnSize, btnSize};
    
    if (effectivePage > 0) {
        arrowColor.a = 200;
        Gfx::DrawRectRounded(mPageBtnLeftBounds.x, mPageBtnLeftBounds.y, btnSize, btnSize, btnRadius, btnBg);
        Gfx::DrawIcon(leftBtnX, btnY, 28, arrowColor, 0xf053, Gfx::ALIGN_CENTER);  // chevron-left
    }
    
    // 右箭头按钮（非末页时显示）
    const int rightBtnX = Gfx::SCREEN_WIDTH - 70;
    mPageBtnRightBounds = {rightBtnX - btnSize/2, btnY - btnSize/2, btnSize, btnSize};
    
    if (effectivePage < TOTAL_PAGES - 1) {
        arrowColor.a = 200;
        Gfx::DrawRectRounded(mPageBtnRightBounds.x, mPageBtnRightBounds.y, btnSize, btnSize, btnRadius, btnBg);
        Gfx::DrawIcon(rightBtnX, btnY, 28, arrowColor, 0xf054, Gfx::ALIGN_CENTER);  // chevron-right
    }
    
    // 页码指示点
    const int dotSize = 10;
    const int dotSpacing = 18;
    const int totalDotsWidth = TOTAL_PAGES * dotSize + (TOTAL_PAGES - 1) * dotSpacing;
    int dotStartX = Gfx::SCREEN_WIDTH / 2 - totalDotsWidth / 2;
    
    for (int i = 0; i < TOTAL_PAGES; i++) {
        int dotX = dotStartX + i * (dotSize + dotSpacing) + dotSize / 2;
        if (i == effectivePage) {
            SDL_Color fillColor = Gfx::COLOR_ACCENT;
            fillColor.a = 200;
            Gfx::DrawRectRounded(dotX - dotSize/2, btnY - dotSize/2, dotSize, dotSize, dotSize/2, fillColor);
        } else {
            SDL_Color ringColor = Gfx::COLOR_ALT_TEXT;
            ringColor.a = 100;
            Gfx::DrawRectRoundedOutline(dotX - dotSize/2, btnY - dotSize/2, dotSize, dotSize, dotSize/2, 2, ringColor);
        }
    }
}

// ============================================================
// 第三页：转储菜单文件
// ============================================================
void SettingsScreen::DrawDumpPage() {
    const int centerX = Gfx::SCREEN_WIDTH / 2;
    int ox = (int)mCurItemOffsetX;
    int oy = (int)mCurItemOffsetY;
    
    // 标题
    Gfx::Print(centerX + ox, 200 + oy, 48, Gfx::COLOR_TEXT, _("settings.dump_title"), Gfx::ALIGN_CENTER);
    
    // 描述
    Gfx::Print(centerX + ox, 280 + oy, 28, Gfx::COLOR_ALT_TEXT, _("settings.dump_desc"), Gfx::ALIGN_CENTER);
    
    // 文件数量提示
    int totalFiles = (int)mDumpSrcPaths.size();
    if (totalFiles > 0) {
        SDL_Color c = Gfx::COLOR_ALT_TEXT;
        c.a = 120;
        Gfx::Print(centerX + ox, 350 + oy, 24, c,
                   std::to_string(totalFiles) + " files from menu content directory", Gfx::ALIGN_CENTER);
    }
    
    // 按钮/状态区域
    const int btnW = 480;
    const int btnH = 110;
    const int btnX = centerX - btnW/2 + ox;
    const int btnY = 500 + oy;
    mDumpBtnBounds = {centerX - btnW/2 + ox, 500 + oy, btnW, btnH};
    
    SDL_Color btnBg;
    std::string btnText;
    uint16_t btnIcon = 0xf0c7;  // fa-save
    
    switch (mDumpState) {
        case DUMP_IDLE:
            btnBg = Gfx::COLOR_ACCENT;
            btnBg.a = 220;
            btnText = _("settings.dump_start");
            break;
        case DUMP_DUMPING:
            btnBg = {60, 60, 80, 230};
            btnText = _("common.dumping");
            btnIcon = 0xf110;  // fa-spinner
            break;
        case DUMP_COMPLETE:
            btnBg = {20, 80, 40, 230};
            btnText = _("common.dump_complete");
            btnIcon = 0xf00c;  // fa-check
            break;
        case DUMP_ERROR:
            btnBg = Gfx::COLOR_ERROR;
            btnBg.a = 220;
            btnText = _("common.dump_error");
            btnIcon = 0xf071;  // fa-exclamation-triangle
            break;
    }
    
    Gfx::DrawRectRounded(btnX, btnY, btnW, btnH, 18, btnBg);
    Gfx::DrawIcon(btnX + 55, btnY + btnH/2, 44, Gfx::COLOR_WHITE, btnIcon, Gfx::ALIGN_LEFT | Gfx::ALIGN_VERTICAL);
    Gfx::Print(btnX + 120, btnY + btnH/2, 36, Gfx::COLOR_WHITE, btnText, Gfx::ALIGN_VERTICAL);
    
    // 进度/状态信息
    if (!mDumpStatusMessage.empty()) {
        Gfx::Print(centerX + ox, btnY + btnH + 50 + oy, 26, Gfx::COLOR_ALT_TEXT, mDumpStatusMessage, Gfx::ALIGN_CENTER);
    }
    
    // 进度条（转储中）
    if (mDumpState == DUMP_DUMPING) {
        const int barW = 700;
        const int barH = 20;
        const int barX = centerX - barW/2 + ox;
        const int barY = btnY + btnH + 90 + oy;
        
        // 背景
        SDL_Color barBg = Gfx::COLOR_CARD_BG;
        Gfx::DrawRectRounded(barX, barY, barW, barH, 10, barBg);
        
        // 填充
        int totalFiles = (int)mDumpSrcPaths.size();
        float fileProgress = mDumpFileSize > 0 ? (float)mDumpFileCopied / mDumpFileSize : 0.0f;
        float overall = (mDumpCurrentFileIndex + fileProgress) / totalFiles;
        int fillW = (int)(barW * overall);
        if (fillW > 0) {
            Gfx::DrawRectRounded(barX, barY, fillW, barH, 10, Gfx::COLOR_ACCENT);
        }
    }
}

void SettingsScreen::StartDump() {
    auto [menuPath, _] = ThemePatcher::GetMenuPaths();
    
    // 目标目录
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char folderName[64];
    strftime(folderName, sizeof(folderName), "menu_files_%Y%m%d_%H%M%S", tm_info);
    std::string dumpDir = "fs:/vol/external01/UTheme/Dump/";
    std::string targetDir = dumpDir + folderName + "/";
    
    // 确保目标目录存在
    std::string buildPath;
    for (size_t i = 0; i < dumpDir.size(); i++) {
        if (dumpDir[i] == '/' || i == dumpDir.size() - 1) {
            if (i == dumpDir.size() - 1 && dumpDir[i] != '/') buildPath += dumpDir[i];
            mkdir(buildPath.c_str(), 0777);
            if (dumpDir[i] == '/') buildPath += '/';
        } else { buildPath += dumpDir[i]; }
    }
    mkdir(targetDir.c_str(), 0777);
    
    // 递归扫描菜单内容目录所有文件
    mDumpSrcPaths.clear();
    mDumpDstPaths.clear();
    ScanMenuFiles(menuPath, menuPath, targetDir, mDumpSrcPaths, mDumpDstPaths);
    
    if (mDumpSrcPaths.empty()) {
        mDumpState = DUMP_ERROR;
        mDumpStatusMessage = "No files found in menu content";
        return;
    }
    
    mDumpCurrentFileIndex = 0;
    mDumpState = DUMP_DUMPING;
    mDumpDirCreated = true;
    
    // 开始复制第一个文件
    mDumpSrcFile = fopen(mDumpSrcPaths[0].c_str(), "rb");
    if (!mDumpSrcFile) {
        mDumpState = DUMP_ERROR;
        mDumpStatusMessage = "Failed to open: " + mDumpSrcPaths[0];
        return;
    }
    
    fseek(mDumpSrcFile, 0, SEEK_END);
    mDumpFileSize = ftell(mDumpSrcFile);
    fseek(mDumpSrcFile, 0, SEEK_SET);
    
    mDumpDstFile = fopen(mDumpDstPaths[0].c_str(), "wb");
    if (!mDumpDstFile) {
        fclose(mDumpSrcFile);
        mDumpSrcFile = nullptr;
        mDumpState = DUMP_ERROR;
        mDumpStatusMessage = "Failed to create: " + mDumpDstPaths[0];
        return;
    }
    
    mDumpFileCopied = 0;
    
    // 显示当前文件名
    std::string shortName = mDumpSrcPaths[0].substr(mDumpSrcPaths[0].rfind('/') + 1);
    mDumpStatusMessage = "Copying " + shortName + "...";
}

void SettingsScreen::ContinueDump() {
    if (mDumpState != DUMP_DUMPING || !mDumpSrcFile || !mDumpDstFile) return;
    
    const size_t CHUNK = 256 * 1024;
    char buf[CHUNK];
    
    // 每帧复制一个块（256KB）
    size_t bytesRead = fread(buf, 1, CHUNK, mDumpSrcFile);
    if (bytesRead > 0) {
        fwrite(buf, 1, bytesRead, mDumpDstFile);
        mDumpFileCopied += bytesRead;
    }
    
    // 检查当前文件是否完成
    if (mDumpFileCopied >= mDumpFileSize || feof(mDumpSrcFile)) {
        fclose(mDumpSrcFile);
        fclose(mDumpDstFile);
        mDumpSrcFile = nullptr;
        mDumpDstFile = nullptr;
        
        // 下一个文件
        mDumpCurrentFileIndex++;
        if (mDumpCurrentFileIndex < (int)mDumpSrcPaths.size()) {
            int idx = mDumpCurrentFileIndex;
            mDumpSrcFile = fopen(mDumpSrcPaths[idx].c_str(), "rb");
            if (!mDumpSrcFile) {
                mDumpState = DUMP_ERROR;
                mDumpStatusMessage = "Failed to open: " + mDumpSrcPaths[idx];
                return;
            }
            fseek(mDumpSrcFile, 0, SEEK_END);
            mDumpFileSize = ftell(mDumpSrcFile);
            fseek(mDumpSrcFile, 0, SEEK_SET);
            
            mDumpDstFile = fopen(mDumpDstPaths[idx].c_str(), "wb");
            if (!mDumpDstFile) {
                fclose(mDumpSrcFile);
                mDumpSrcFile = nullptr;
                mDumpState = DUMP_ERROR;
                mDumpStatusMessage = "Failed to create: " + mDumpDstPaths[idx];
                return;
            }
            mDumpFileCopied = 0;
            std::string sn = mDumpSrcPaths[idx].substr(mDumpSrcPaths[idx].rfind('/') + 1);
            mDumpStatusMessage = "Copying " + sn + "...";
        } else {
            // 全部完成
            mDumpState = DUMP_COMPLETE;
            mDumpStatusMessage = _("common.dump_complete");
            mDumpStatusMessage += " | SD:/UTheme/Dump/";
            
            // 记录日志
            FileLogger::GetInstance().LogInfo("[DUMP] Menu files dumped to SD:/UTheme/Dump/");
        }
    }
}

void SettingsScreen::EndDump() {
    if (mDumpSrcFile) { fclose(mDumpSrcFile); mDumpSrcFile = nullptr; }
    if (mDumpDstFile) { fclose(mDumpDstFile); mDumpDstFile = nullptr; }
}

void SettingsScreen::ScanMenuFiles(const std::string& basePath, const std::string& currentPath,
                                   const std::string& targetDir, std::vector<std::string>& srcList,
                                   std::vector<std::string>& dstList) {
    DIR* dir = opendir(currentPath.c_str());
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        
        std::string fullPath = currentPath + "/" + name;
        std::string relPath = fullPath.substr(basePath.size());
        if (!relPath.empty() && relPath[0] == '/') relPath = relPath.substr(1);
        
        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            std::string subTarget = targetDir + relPath;
            mkdir(subTarget.c_str(), 0777);
            ScanMenuFiles(basePath, fullPath, targetDir, srcList, dstList);
        } else if (S_ISREG(st.st_mode)) {
            srcList.push_back(fullPath);
            dstList.push_back(targetDir + relPath);
        }
    }
    closedir(dir);
}
