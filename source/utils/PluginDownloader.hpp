#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <curl/curl.h>

// 插件下载状态
enum PluginDownloadState {
    PLUGIN_IDLE,
    PLUGIN_DOWNLOADING,
    PLUGIN_COMPLETE,
    PLUGIN_ERROR,
    PLUGIN_CANCELLED
};

// 插件下载器 - 异步下载，和BgmDownloader一样后台线程避免阻塞UI
class PluginDownloader {
public:
    static PluginDownloader& GetInstance();
    
    // 开始异步下载 StyleMiiU 插件
    void StartDownload();
    
    // 取消下载
    void Cancel();
    
    // 每帧更新 - 在主循环中调用
    void Update();
    
    // 状态查询
    PluginDownloadState GetState() const { return mState; }
    float GetProgress() const { return mProgress; }
    const std::string& GetError() const { return mErrorMessage; }
    bool IsDownloading() const { return mState == PLUGIN_DOWNLOADING; }
    bool IsDone() const { return mState == PLUGIN_COMPLETE || mState == PLUGIN_ERROR || mState == PLUGIN_CANCELLED; }
    
    // 获取下载信息
    long GetDownloadedBytes() const { return mDownloadedBytes; }
    long GetTotalBytes() const { return mTotalBytes; }
    
    // 回调设置 - 下载完成时触发
    void SetCompletionCallback(std::function<void(bool success, const std::string& filepath)> callback);
    
private:
    PluginDownloader();
    ~PluginDownloader();
    PluginDownloader(const PluginDownloader&) = delete;
    PluginDownloader& operator=(const PluginDownloader&) = delete;
    
    std::atomic<PluginDownloadState> mState;
    std::atomic<float> mProgress;
    std::atomic<long> mDownloadedBytes;
    std::atomic<long> mTotalBytes;
    std::atomic<bool> mCancelRequested;
    
    std::string mErrorMessage;
    std::string mDestPath;
    std::mutex mMutex;
    std::function<void(bool success, const std::string& filepath)> mCompletionCallback;
    
    // 后台下载线程
    std::thread mDownloadThread;
    bool mThreadRunning;
    
    // 内部方法 - 在后台线程中运行
    void PerformDownload();
    
    // CURL进度回调
    static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                curl_off_t ultotal, curl_off_t ulnow);
};
