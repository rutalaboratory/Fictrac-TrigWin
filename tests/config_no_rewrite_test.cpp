#include "Trackball.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream handle(path, std::ios::binary);
    if (!handle.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(handle)), std::istreambuf_iterator<char>());
}

bool write_text_file(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream handle(path, std::ios::binary);
    if (!handle.is_open()) {
        return false;
    }
    handle << content;
    return handle.good();
}

std::string upsert_config_line(const std::string& content, const std::string& key, const std::string& value)
{
    std::vector<std::string> lines;
    std::string current;
    for (char ch : content) {
        if (ch == '\n') {
            lines.push_back(current);
            current.clear();
        }
        else if (ch != '\r') {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }

    const std::string prefix = key;
    const std::string replacement = key + std::string(std::max(0, 16 - static_cast<int>(key.size())), ' ') + " : " + value;
    bool replaced = false;
    for (std::string& line : lines) {
        const std::size_t delim = line.find(':');
        if (delim == std::string::npos) {
            continue;
        }
        const std::string trimmed_key = line.substr(0, line.find_last_not_of(" \t", delim == 0 ? 0 : delim - 1) + 1);
        if (trimmed_key == prefix) {
            line = replacement;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        lines.push_back(replacement);
    }

    std::string result;
    for (const std::string& line : lines) {
        result += line + "\n";
    }
    return result;
}

void remove_with_prefix(const std::filesystem::path& directory, const std::string& prefix)
{
    std::error_code error_code;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory, error_code)) {
        if (error_code) {
            return;
        }
        const std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) == 0) {
            std::filesystem::remove(entry.path(), error_code);
        }
    }
}

} // namespace

int main()
{
    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const std::filesystem::path sample_config_path = repo_root / "sample" / "config.txt";
    const std::filesystem::path sample_video_path = repo_root / "sample" / "sample.mp4";
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "fictrac_config_no_rewrite_test";
    const std::filesystem::path config_copy_path = temp_dir / "config_runtime.txt";
    const std::filesystem::path output_base = temp_dir / "output" / "sample_runtime";

    std::error_code error_code;
    std::filesystem::remove_all(temp_dir, error_code);
    std::filesystem::create_directories(output_base.parent_path(), error_code);

    std::string runtime_config = read_text_file(sample_config_path);
    if (runtime_config.empty()) {
        std::cerr << "Failed to read sample config." << std::endl;
        return 1;
    }

    runtime_config = upsert_config_line(runtime_config, "src_fn", sample_video_path.generic_string());
    runtime_config = upsert_config_line(runtime_config, "output_fn", output_base.generic_string());
    runtime_config = upsert_config_line(runtime_config, "do_display", "n");
    runtime_config = upsert_config_line(runtime_config, "save_debug", "n");
    runtime_config = upsert_config_line(runtime_config, "save_raw", "n");

    if (!write_text_file(config_copy_path, runtime_config)) {
        std::cerr << "Failed to write runtime config copy." << std::endl;
        return 1;
    }

    const std::string original_runtime_config = read_text_file(config_copy_path);

    {
        Trackball tracker(config_copy_path.string());
        if (!tracker.isActive()) {
            std::cerr << "Trackball failed to start for runtime config writeback test." << std::endl;
            return 1;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (tracker.isActive() && (std::chrono::steady_clock::now() < deadline)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (tracker.isActive()) {
            std::cerr << "Trackball did not finish sample playback within the timeout." << std::endl;
            tracker.terminate();
            return 1;
        }
    }

    const std::string final_runtime_config = read_text_file(config_copy_path);
    if (final_runtime_config != original_runtime_config) {
        std::cerr << "Runtime tracking mutated the config file contents." << std::endl;
        return 1;
    }

    remove_with_prefix(output_base.parent_path(), output_base.filename().string());
    std::filesystem::remove_all(temp_dir, error_code);
    return 0;
}