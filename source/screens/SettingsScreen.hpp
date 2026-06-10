#pragma once
#include "Screen.hpp"
#include "../utils/LanguageManager.hpp"
#include "../utils/Animation.hpp"

struct ThemeSource;

class SettingsScreen : public Screen {
public:
    SettingsScreen();
    ~SettingsScreen() override;

    void Draw() override;
    bool Update(Input &input) override;

private:
    enum SettingsItem {
        SETTINGS_LANGUAGE,
        SETTINGS_DOWNLOAD_PATH,
        SETTINGS_BGM_ENABLED,
        SETTINGS_LOGGING_ENABLED,
        SETTINGS_LOGGING_VERBOSE,
        SETTINGS_CLEAR_CACHE,
        
        SETTINGS_COUNT
    };
    
    int mFrameCount = 0;
    int mSelectedItem = SETTINGS_LANGUAGE;
    bool mLanguageDialogOpen = false;
    int mSelectedLanguage = 0;
    int mSelectedColumn = 0;  // 0 = left column, 1 = right column
    int mPrevSelectedItem = SETTINGS_LANGUAGE; // 用于追踪上一个选中项
    Animation mTitleAnim;
    Animation mSelectionAnim; // 选项切换动画
    float mItemAnimProgress[SETTINGS_COUNT]; // 每个选项的动画进度
    
    // 长按连续选择
    int mHoldFrames = 0;
    int mRepeatDelay = 30;  // 初始延迟帧数 (约0.5秒)
    int mRepeatRate = 8;    // 重复间隔帧数 (约0.13秒)
    
    // 语言对话框的长按连续选择
    int mDialogHoldFrames = 0;
    
    // 语言对话框动画
    int mPrevSelectedLanguage = 0;  // 用于追踪上一个选中的语言
    int mPrevSelectedColumn = 0;    // 用于追踪上一个选中的列
    std::vector<float> mLanguageItemAnimProgress;  // 每个语言项的动画进度
    
    // 触屏支持
    struct CardBounds {
        int x, y, w, h;
    };
    std::vector<CardBounds> mLanguageCardBounds;  // 语言卡片边界
    CardBounds mSettingItemBounds[SETTINGS_COUNT];  // 设置项边界
    bool mTouchStartedOnLanguageCard = false;
    
    // 第二页：数据源选择
    int mCurrentPage = 0;          // 0=设置, 1=数据源
    int mSelectedSource = 0;       // 当前选中的源索引
    std::vector<CardBounds> mSourceItemBounds;  // 源列表项边界
    
    // 页面切换动画（逐项交错）
    bool mIsPageTransitioning = false;
    int mTransitionFromPage = 0;
    int mTransitionTargetPage = 0;
    Animation mPageTransitionAnim;
    float mCurItemOffsetX = 0.0f;
    float mCurItemOffsetY = 0.0f;
    
    // Themezer logo 纹理
    SDL_Texture* mThemezerLogo = nullptr;
    
    // 翻页按钮
    CardBounds mPageBtnLeftBounds;
    CardBounds mPageBtnRightBounds;
    static const int TOTAL_PAGES = 3;  // 0=设置, 1=数据源, 2=转储菜单
    
    void DrawPageButtons();
    void DrawSourcePage();
    void DrawDumpPage();  // 第3页：转储菜单文件
    
    // 转储菜单文件状态
    enum DumpState {
        DUMP_IDLE,
        DUMP_DUMPING,
        DUMP_COMPLETE,
        DUMP_ERROR
    };
    DumpState mDumpState = DUMP_IDLE;
    std::string mDumpStatusMessage;
    
    // 分块复制状态
    FILE* mDumpSrcFile = nullptr;
    FILE* mDumpDstFile = nullptr;
    size_t mDumpFileSize = 0;
    size_t mDumpFileCopied = 0;
    int mDumpCurrentFileIndex = 0;
    std::vector<std::string> mDumpSrcPaths;
    std::vector<std::string> mDumpDstPaths;
    bool mDumpDirCreated = false;
    
    void StartDump();
    void ContinueDump();
    void EndDump();
    void ScanMenuFiles(const std::string& basePath, const std::string& currentPath,
                       const std::string& targetDir, std::vector<std::string>& srcList,
                       std::vector<std::string>& dstList);
    
    CardBounds mDumpBtnBounds;  // 转储按钮触摸区域
    void DrawSourceItem(int x, int y, int w, int h, const ThemeSource& source,
                        bool isActive, bool isSelected, float anim,
                        bool showActiveHighlight);
    
    // 清除缓存退出计时器
    bool mWaitingForExit = false;
    uint64_t mExitStartTime = 0;
    
    void DrawSettingItem(int x, int y, int w, const std::string& title, 
                        const std::string& description, const std::string& value, 
                        bool selected, float animProgress, uint16_t icon = 0);
    void DrawLanguageDialog();
    bool UpdateLanguageDialog(Input &input);
    void ClearCache();
    bool IsTouchInRect(int touchX, int touchY, int rectX, int rectY, int rectW, int rectH);
};
