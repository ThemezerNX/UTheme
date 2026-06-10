#pragma once

#include <string>
#include <vector>
#include <mutex>

// 远程通知抓取器 - 从 GitHub 获取通知消息
class RemoteNotification {
public:
    static RemoteNotification& GetInstance();
    
    // 启动时检查通知（异步，不阻塞）
    void CheckOnStartup();
    
    // 手动刷新
    void Refresh();
    
    // 主线程调用 - 处理后台线程缓存的通知数据
    void ProcessPending();
    
private:
    RemoteNotification();
    ~RemoteNotification() = default;
    
    RemoteNotification(const RemoteNotification&) = delete;
    RemoteNotification& operator=(const RemoteNotification&) = delete;
    
    void FetchAndShow();
    void ProcessJson(const std::string& jsonContent);  // 解析并显示 JSON 通知内容
    
    struct NotificationCache {
        std::string id;
        std::string version;  // 版本号（仅 update 类型）
        int displayCount;     // 已显示次数
    };
    
    bool ShouldShowMessage(const std::string& id, const std::string& type, 
                           const std::string& version, int maxDisplayCount);
    void MarkAsShown(const std::string& id, const std::string& version);
    std::vector<NotificationCache> LoadCache();
    void SaveCache(const std::vector<NotificationCache>& cache);
    
    static const char* NOTIFICATION_URL;
    static const char* LOCAL_NOTIFICATION_PATH;  // dev 模式下从 SD 卡读取
    static const char* CACHE_FILE;
    
    // 线程安全：后台线程写入，主线程读取并处理
    std::mutex mPendingMutex;
    std::string mPendingJson;
};
