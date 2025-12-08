#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

namespace Backup {

    struct Filter {
        bool enabled = false;

        std::vector<std::string> nameKeywords; // 文件名关键词列表
        std::string nameRegex;
        std::vector<std::string> suffixes; // 文件后缀列表

        uint64_t minSize = 0;
        uint64_t maxSize = 0; // 0 表示不限制

        time_t startTime = 0;
        time_t endTime = 0;

        std::string userName;
    };
}