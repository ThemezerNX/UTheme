#pragma once
#include <string>
#include <vector>

struct ThemeSource {
    std::string name;           // 显示名
    std::string graphqlUrl;     // API地址
    std::string themeListQuery; // 主题列表查询
    std::string updateCheckQuery; // 更新检测查询
};

class Config {
public:
    static Config& GetInstance();
    
    bool IsLoggingEnabled() const { return mLoggingEnabled; }
    void SetLoggingEnabled(bool enabled);
    bool IsVerboseLogging() const { return mVerboseLogging; }
    void SetVerboseLogging(bool verbose);
    
    std::string GetLanguage() const { return mLanguage; }
    void SetLanguage(const std::string& lang);
    std::string GetDownloadPath() const { return mDownloadPath; }
    void SetDownloadPath(const std::string& path);
    bool IsAutoInstallEnabled() const { return mAutoInstall; }
    void SetAutoInstallEnabled(bool enabled);
    bool IsBgmEnabled() const { return mBgmEnabled; }
    void SetBgmEnabled(bool enabled);
    std::string GetBgmUrl() const { return mBgmUrl; }
    void SetBgmUrl(const std::string& url);
    
    // 多数据源
    const std::vector<ThemeSource>& GetSources() const { return mSources; }
    int GetActiveSource() const { return mActiveSource; }
    void SetActiveSource(int index);
    const ThemeSource& GetCurrentSource() const;
    
    // 获取当前生效的API信息（自定义覆盖优先）
    std::string GetGraphQLUrl() const;
    std::string GetGraphQLQuery() const;
    std::string GetUpdateCheckQuery() const;
    
    // 用户自定义覆盖（为空则不覆盖，用源的数据）
    std::string GetCustomGraphQLUrl() const { return mCustomGraphQLUrl; }
    void SetCustomGraphQLUrl(const std::string& url);
    std::string GetCustomGraphQLQuery() const { return mCustomGraphQLQuery; }
    void SetCustomGraphQLQuery(const std::string& query);
    std::string GetCustomUpdateCheckQuery() const { return mCustomUpdateCheckQuery; }
    void SetCustomUpdateCheckQuery(const std::string& query);
    
    // 远程同步数据源
    void SyncApiSources();
    
    bool HasShownTouchHint() const { return mHasShownTouchHint; }
    void SetTouchHintShown(bool shown);
    bool HasShownLanguageSwitchHint() const { return mHasShownLanguageSwitchHint; }
    void SetLanguageSwitchHintShown(bool shown);
    bool IsThemeChanged() const { return mThemeChanged; }
    void SetThemeChanged(bool changed) { mThemeChanged = changed; }
    
    bool Load();
    bool Save();
    
private:
    Config();
    ~Config();
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    bool mLoggingEnabled;
    bool mVerboseLogging;
    std::string mLanguage;
    std::string mDownloadPath;
    bool mAutoInstall;
    bool mBgmEnabled;
    std::string mBgmUrl;
    
    std::vector<ThemeSource> mSources;
    int mActiveSource;
    std::string mCustomGraphQLUrl;
    std::string mCustomGraphQLQuery;
    std::string mCustomUpdateCheckQuery;
    
    bool mHasShownTouchHint;
    bool mHasShownLanguageSwitchHint;
    bool mThemeChanged;
    std::string mConfigPath;
};
