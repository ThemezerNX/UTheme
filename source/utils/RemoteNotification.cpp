#include "RemoteNotification.hpp"
#include "FileLogger.hpp"
#include "DownloadQueue.hpp"
#include "../Screen.hpp"
#include "../common.h"
#include "rapidjson/document.h"
#include <sys/stat.h>
#include <fstream>
#include <thread>
#include <algorithm>

const char* RemoteNotification::NOTIFICATION_URL = "https://raw.githubusercontent.com/Xziip/UTheme/main/notifications.json";
const char* RemoteNotification::LOCAL_NOTIFICATION_PATH = "fs:/vol/external01/UTheme/notifications.json";
const char* RemoteNotification::CACHE_FILE = "fs:/vol/external01/UTheme/temp/shown_notifications.txt";

RemoteNotification& RemoteNotification::GetInstance() {
    static RemoteNotification instance;
    return instance;
}

RemoteNotification::RemoteNotification() {
}

void RemoteNotification::CheckOnStartup() {
    // 启动后延迟 2 秒再检查（避免启动卡顿）
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        FetchAndShow();
    }).detach();
}

void RemoteNotification::Refresh() {
    FetchAndShow();
}

void RemoteNotification::FetchAndShow() {
    // dev 模式下从 SD 卡读取本地通知文件（后台线程只读文件，主线程处理）
    if (std::string(APP_VERSION_EXTRA) == "dev") {
        FileLogger::GetInstance().LogInfo("[RemoteNotification] DEV mode - reading from SD card: %s", LOCAL_NOTIFICATION_PATH);
        
        std::ifstream file(LOCAL_NOTIFICATION_PATH);
        if (!file.is_open()) {
            FileLogger::GetInstance().LogError("[RemoteNotification] Failed to open local notification file: %s", LOCAL_NOTIFICATION_PATH);
            return;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        FileLogger::GetInstance().LogInfo("[RemoteNotification] Local file read: %zu bytes, queued for main thread", content.size());
        
        // 缓存到 pending，等主线程调用 ProcessPending 处理
        std::lock_guard<std::mutex> lock(mPendingMutex);
        mPendingJson = std::move(content);
        return;
    }
    
    // 正常模式：从网络获取
    FileLogger::GetInstance().LogInfo("[RemoteNotification] Fetching notifications from GitHub...");
    FileLogger::GetInstance().LogInfo("[RemoteNotification] URL: %s", NOTIFICATION_URL);
    
    // 使用 DownloadQueue 异步下载
    DownloadQueue* queue = DownloadQueue::GetInstance();
    if (!queue) {
        FileLogger::GetInstance().LogError("[RemoteNotification] DownloadQueue not available");
        return;
    }
    
    // 创建下载操作
    DownloadOperation* op = new DownloadOperation();
    op->url = NOTIFICATION_URL;
    op->postData = "";  // GET 请求
    
    FileLogger::GetInstance().LogInfo("[RemoteNotification] Download operation created, adding to queue...");
    
    // 设置完成回调
    op->cb = [](DownloadOperation* op) {
        FileLogger::GetInstance().LogInfo("[RemoteNotification] Download callback triggered!");
        FileLogger::GetInstance().LogInfo("[RemoteNotification] Status: %d, HTTP code: %ld", 
            (int)op->status, op->response_code);
        FileLogger::GetInstance().LogInfo("[RemoteNotification] Downloaded %zu bytes", op->buffer.size());
        if (op->status != DownloadStatus::COMPLETE || op->response_code != 200) {
            FileLogger::GetInstance().LogWarning("[RemoteNotification] Download failed (HTTP %ld)", op->response_code);
            delete op;
            return;
        }
        
        FileLogger::GetInstance().LogInfo("[RemoteNotification] Download complete, parsing JSON...");
        RemoteNotification::GetInstance().ProcessJson(op->buffer);
        delete op;
    };
    
    // 添加到下载队列
    FileLogger::GetInstance().LogInfo("[RemoteNotification] Adding download to queue...");
    queue->DownloadAdd(op);
    FileLogger::GetInstance().LogInfo("[RemoteNotification] Download added successfully, waiting for callback...");
}

void RemoteNotification::ProcessPending() {
    std::string json;
    {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        if (mPendingJson.empty()) return;
        json = std::move(mPendingJson);
        mPendingJson.clear();
    }
    
    FileLogger::GetInstance().LogInfo("[RemoteNotification] Processing pending notification (%zu bytes) on main thread", json.size());
    ProcessJson(json);
}

void RemoteNotification::ProcessJson(const std::string& jsonContent) {
    // 解析 JSON
    rapidjson::Document doc;
    doc.Parse(jsonContent.c_str());
    
    if (doc.HasParseError()) {
        FileLogger::GetInstance().LogError("[RemoteNotification] JSON parse error");
        return;
    }
    
    if (!doc.HasMember("messages") || !doc["messages"].IsArray()) {
        FileLogger::GetInstance().LogError("[RemoteNotification] Invalid JSON structure");
        return;
    }
    
    // 处理消息
    const auto& messages = doc["messages"];
    bool hasNew = false;
    
    for (rapidjson::SizeType i = 0; i < messages.Size(); i++) {
        const auto& msg = messages[i];
        
        if (!msg.HasMember("id") || !msg.HasMember("text")) continue;
        
        std::string id = msg["id"].GetString();
        std::string text = msg["text"].GetString();
        std::string type = msg.HasMember("type") ? msg["type"].GetString() : "info";
        std::string title = msg.HasMember("title") ? msg["title"].GetString() : "";
        std::string version = msg.HasMember("version") ? msg["version"].GetString() : "";
        uint64_t duration = msg.HasMember("duration") ? msg["duration"].GetUint64() : 5000;
        int maxDisplayCount = msg.HasMember("maxDisplayCount") ? msg["maxDisplayCount"].GetInt() : 1;
        
        // 检查是否应该显示
        if (!ShouldShowMessage(id, type, version, maxDisplayCount)) {
            FileLogger::GetInstance().LogInfo("[RemoteNotification] Message %s skipped (version: %s, count limit reached)", 
                id.c_str(), version.c_str());
            continue;
        }
        
        // 显示通知
        FileLogger::GetInstance().LogInfo("[RemoteNotification] Showing message: %s (type: %s, title: %s, version: %s, duration: %llums, maxCount: %d)", 
            text.c_str(), type.c_str(), title.c_str(), version.c_str(), duration, maxDisplayCount);
        
        if (type == "warning") {
            Screen::GetBgmNotification().ShowWarning(text, duration, title);
        } else if (type == "error") {
            Screen::GetBgmNotification().ShowError(text, duration, title);
        } else {
            Screen::GetBgmNotification().ShowInfo(text, duration, title);
        }
        
        // 标记为已显示
        MarkAsShown(id, version);
        hasNew = true;
    }
    
    if (!hasNew) {
        FileLogger::GetInstance().LogInfo("[RemoteNotification] No new messages");
    }
}

bool RemoteNotification::ShouldShowMessage(const std::string& id, const std::string& type, 
                                           const std::string& version, int maxDisplayCount) {
    auto cache = LoadCache();
    
    // 查找缓存记录
    for (const auto& entry : cache) {
        if (entry.id == id) {
            // 找到记录
            
            // update 类型：只检查版本号是否与客户端匹配
            if (type == "update") {
                std::string clientVersion = APP_VERSION_FULL;  // 客户端当前版本
                
                if (!version.empty() && version == clientVersion) {
                    // 版本号与客户端一致，不显示（已经是最新版本）
                    FileLogger::GetInstance().LogInfo("[RemoteNotification] Update notification %s skipped - client already at version %s", 
                        id.c_str(), clientVersion.c_str());
                    return false;
                } else {
                    // 版本号不同，总是显示（即使之前显示过）
                    FileLogger::GetInstance().LogInfo("[RemoteNotification] Update notification %s will show - version mismatch (client: %s, notification: %s)", 
                        id.c_str(), clientVersion.c_str(), version.c_str());
                    return true;
                }
            }
            
            // 检查显示次数限制
            if (maxDisplayCount == 0) {
                // 0 表示无限次，总是显示
                return true;
            }
            
            // 检查是否已达到最大显示次数
            if (entry.displayCount >= maxDisplayCount) {
                return false;  // 已达到上限
            }
            
            return true;  // 还没达到上限，可以显示
        }
    }
    
    // 未找到记录，是新消息
    
    // update 类型：检查版本号是否与客户端匹配
    if (type == "update") {
        std::string clientVersion = APP_VERSION_FULL;
        if (!version.empty() && version == clientVersion) {
            // 版本号与客户端一致，不显示
            return false;
        }
        // 版本号不同，应该显示
        return true;
    }
    
    // 其他类型：新消息应该显示
    return true;
}

void RemoteNotification::MarkAsShown(const std::string& id, const std::string& version) {
    auto cache = LoadCache();
    
    // 查找是否已存在
    bool found = false;
    for (auto& entry : cache) {
        if (entry.id == id) {
            found = true;
            // 更新版本号和计数
            entry.version = version;
            entry.displayCount++;
            break;
        }
    }
    
    // 如果不存在，添加新记录
    if (!found) {
        NotificationCache newEntry;
        newEntry.id = id;
        newEntry.version = version;
        newEntry.displayCount = 1;
        cache.push_back(newEntry);
    }
    
    SaveCache(cache);
}

std::vector<RemoteNotification::NotificationCache> RemoteNotification::LoadCache() {
    std::vector<NotificationCache> cache;
    
    std::ifstream file(CACHE_FILE);
    if (!file.is_open()) {
        return cache;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        // 解析格式：id:version:count
        NotificationCache entry;
        size_t pos1 = line.find(':');
        if (pos1 == std::string::npos) {
            // 旧格式兼容：只有 id
            entry.id = line;
            entry.version = "";
            entry.displayCount = 1;
        } else {
            entry.id = line.substr(0, pos1);
            size_t pos2 = line.find(':', pos1 + 1);
            if (pos2 == std::string::npos) {
                // 格式：id:version
                entry.version = line.substr(pos1 + 1);
                entry.displayCount = 1;
            } else {
                // 完整格式：id:version:count
                entry.version = line.substr(pos1 + 1, pos2 - pos1 - 1);
                entry.displayCount = std::stoi(line.substr(pos2 + 1));
            }
        }
        
        cache.push_back(entry);
    }
    
    file.close();
    return cache;
}

void RemoteNotification::SaveCache(const std::vector<NotificationCache>& cache) {
    // 确保目录存在
    const char* dir = "fs:/vol/external01/UTheme/temp";
    struct stat st;
    if (stat(dir, &st) != 0) {
        mkdir(dir, 0777);
    }
    
    std::ofstream file(CACHE_FILE);
    if (!file.is_open()) {
        FileLogger::GetInstance().LogError("[RemoteNotification] Failed to save cache");
        return;
    }
    
    for (const auto& entry : cache) {
        file << entry.id << ":" << entry.version << ":" << entry.displayCount << "\n";
    }
    
    file.close();
}
