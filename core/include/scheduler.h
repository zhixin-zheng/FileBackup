#pragma once

#include "backup_system.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <map>
#include <functional>
#include <condition_variable>

namespace Backup {

enum class TaskType {
    SCHEDULED,  // 定时任务
    REALTIME    // 实时监听
};

struct BackupTask {
    int id;
    TaskType type;
    std::string srcDir;
    std::string dstDir;         // 目标目录
    std::string filePrefix;     // 文件名前缀
    int intervalSeconds;        // 定时备份间隔
    int maxBackups;             // 数据淘汰
    time_t lastRunTime;         // 上次运行时间
    
    std::map<std::string, time_t> fileSnapshot; // 实时备份用

    BackupSystem systemInstance; // 每个任务独立的备份系统实例
    Filter filter;
};

class BackupScheduler {
public:
    BackupScheduler();
    ~BackupScheduler();

    void start();
    void stop();

    int addScheduledTask(const std::string& srcDir, const std::string& dstDir, 
                         const std::string& prefix, int intervalSec, int maxKeep);

    int addRealtimeTask(const std::string& srcDir, const std::string& dstDir, 
                        const std::string& prefix, int maxKeep);

    // 设置任务的过滤器
    void setTaskFilter(int taskId, const Filter& opts);
    
    // 设置任务的加密密码
    void setTaskPassword(int taskId, const std::string& pwd);

    // 设置任务的压缩算法
    void setTaskCompressionAlgorithm(int taskId, int algo);

private:
    void loop();
    void performBackup(BackupTask& task);
    bool checkChanges(BackupTask& task);
    void pruneOldBackups(const BackupTask& task);
    std::string generateFileName(const std::string& dir, const std::string& prefix);

    std::vector<std::shared_ptr<BackupTask>> m_tasks;
    std::atomic<bool> m_running;
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_nextId = 1;
};

}