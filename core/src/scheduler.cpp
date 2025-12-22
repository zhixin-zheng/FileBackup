#include "scheduler.h"
#include "traverser.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace Backup {

BackupScheduler::BackupScheduler() : m_running(false) {}

BackupScheduler::~BackupScheduler() {
    stop();
}

void BackupScheduler::start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&BackupScheduler::loop, this);
    std::cout << "[Scheduler] Started background service." << std::endl;
}

void BackupScheduler::stop() {
    if (!m_running) return;
    m_running = false;
    m_cv.notify_all();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    std::cout << "[Scheduler] Stopped background service." << std::endl;
}

int BackupScheduler::addScheduledTask(const std::string& srcDir, const std::string& dstDir, 
                                      const std::string& prefix, int intervalSec, int maxKeep) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto task = std::make_shared<BackupTask>();
    task->id = m_nextId++;
    task->type = TaskType::SCHEDULED;
    task->srcDir = srcDir;
    task->dstDir = dstDir;
    task->filePrefix = prefix;
    task->intervalSeconds = intervalSec;
    task->maxBackups = maxKeep;
    task->lastRunTime = 0;
    
    fs::create_directories(dstDir);
    m_tasks.push_back(task);
    return task->id;
}

int BackupScheduler::addRealtimeTask(const std::string& srcDir, const std::string& dstDir, 
                                     const std::string& prefix, int maxKeep) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto task = std::make_shared<BackupTask>();
    task->id = m_nextId++;
    task->type = TaskType::REALTIME;
    task->srcDir = srcDir;
    task->dstDir = dstDir;
    task->filePrefix = prefix;
    task->maxBackups = maxKeep;
    task->lastRunTime = std::time(nullptr); 
    
    fs::create_directories(dstDir);

    Traverser t;
    try {
        auto files = t.traverse(srcDir);
        for (const auto& f : files) {
            task->fileSnapshot[f.relativePath] = f.lastModified;
        }
    } catch (...) {}

    m_tasks.push_back(task);
    return task->id;
}

void BackupScheduler::setTaskFilter(int taskId, const Filter& opts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& task : m_tasks) {
        if (task->id == taskId) {
            task->systemInstance.setFilter(opts);
            task->filter = opts;
            break;
        }
    }
}

void BackupScheduler::setTaskPassword(int taskId, const std::string& pwd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& task : m_tasks) {
        if (task->id == taskId) {
            task->systemInstance.setPassword(pwd); // 设置该任务独立实例的密码
            break;
        }
    }
}

// 设置任务压缩算法
void BackupScheduler::setTaskCompressionAlgorithm(int taskId, int algo) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& task : m_tasks) {
        if (task->id == taskId) {
            task->systemInstance.setCompressionAlgorithm(algo);
            break;
        }
    }
}

void BackupScheduler::loop() {
    while (m_running) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            time_t now = std::time(nullptr);

            for (auto& task : m_tasks) {
                bool shouldRun = false;

                if (task->type == TaskType::SCHEDULED) {
                    if (task->lastRunTime == 0 || (now - task->lastRunTime) >= task->intervalSeconds) {
                        shouldRun = true;
                    }
                } 
                else if (task->type == TaskType::REALTIME) {
                    if (checkChanges(*task)) {
                        shouldRun = true;
                        std::cout << "[Scheduler] Detected changes in: " << task->srcDir << std::endl;
                    }
                }

                if (shouldRun) {
                    performBackup(*task);
                    task->lastRunTime = std::time(nullptr); 
                }
            }
        }
        std::unique_lock<std::mutex> waitLock(m_mutex);
        m_cv.wait_for(waitLock, std::chrono::seconds(2), [this] { return !m_running; });
    }
}

bool BackupScheduler::checkChanges(BackupTask& task) {
    Traverser t;
    std::vector<FileInfo> currentFiles;
    try {
        currentFiles = t.traverse(task.srcDir);
    } catch (...) { return false; }

    bool changed = false;
    std::map<std::string, time_t> newSnapshot;

    for (const auto& f : currentFiles) {
        if (f.type == FileType::DIRECTORY) continue;
        newSnapshot[f.relativePath] = f.lastModified;

        auto it = task.fileSnapshot.find(f.relativePath);
        if (it == task.fileSnapshot.end() || it->second != f.lastModified) {
            changed = true; 
        }
    }

    if (!changed && task.fileSnapshot.size() != newSnapshot.size()) {
        changed = true;
    }

    if (changed) task.fileSnapshot = std::move(newSnapshot);
    return changed;
}

std::string BackupScheduler::generateFileName(const std::string& dir, const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << dir << "/" << prefix << "_";
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    ss << ".bin";
    return ss.str();
}

void BackupScheduler::performBackup(BackupTask& task) {
    std::string dstFile = generateFileName(task.dstDir, task.filePrefix);
    std::cout << "[Scheduler] Running task " << task.id << ": " << dstFile << std::endl;
    
    bool success = task.systemInstance.backup(task.srcDir, dstFile);
    if (success) pruneOldBackups(task);
}

void BackupScheduler::pruneOldBackups(const BackupTask& task) {
    if (task.maxBackups <= 0) return;
    std::vector<fs::path> backups;
    try {
        for (const auto& entry : fs::directory_iterator(task.dstDir)) {
            if (entry.is_regular_file()) {
                std::string fname = entry.path().filename().string();
                if (fname.find(task.filePrefix) == 0 && fname.rfind(".bin") == fname.length() - 4) {
                    backups.push_back(entry.path());
                }
            }
        }
    } catch (...) { return; }

    std::sort(backups.begin(), backups.end(), [](const fs::path& a, const fs::path& b) {
        return fs::last_write_time(a) < fs::last_write_time(b);
    });

    if (backups.size() > (size_t)task.maxBackups) {
        size_t removeCount = backups.size() - task.maxBackups;
        for (size_t i = 0; i < removeCount; ++i) {
            std::cout << "[Scheduler] Pruning old backup: " << backups[i] << std::endl;
            fs::remove(backups[i]);
        }
    }
}

}