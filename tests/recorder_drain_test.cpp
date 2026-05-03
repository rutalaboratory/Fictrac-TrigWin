#include "Recorder.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

std::size_t count_lines(const std::filesystem::path& path)
{
    std::ifstream handle(path);
    if (!handle.is_open()) {
        return 0;
    }

    std::size_t line_count = 0;
    std::string line;
    while (std::getline(handle, line)) {
        ++line_count;
    }
    return line_count;
}

} // namespace

int main()
{
    constexpr std::size_t expected_line_count = 20000;
    const std::filesystem::path output_path =
        std::filesystem::temp_directory_path() / "fictrac_recorder_drain_test.log";

    std::error_code error_code;
    std::filesystem::remove(output_path, error_code);

    {
        Recorder recorder(RecorderInterface::RecordType::FILE, output_path.string());
        if (!recorder.is_active()) {
            std::cerr << "Failed to open recorder output: " << output_path << std::endl;
            return 1;
        }

        for (std::size_t index = 0; index < expected_line_count; ++index) {
            if (!recorder.addMsg("line " + std::to_string(index) + "\n")) {
                std::cerr << "Recorder rejected queued message at index " << index << std::endl;
                return 1;
            }
        }
    }

    const std::size_t actual_line_count = count_lines(output_path);
    std::filesystem::remove(output_path, error_code);

    if (actual_line_count != expected_line_count) {
        std::cerr << "Expected " << expected_line_count << " lines but found " << actual_line_count << std::endl;
        return 1;
    }

    return 0;
}