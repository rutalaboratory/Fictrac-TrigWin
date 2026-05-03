#include "Trackball.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
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
    std::ofstream handle(path, std::ios::binary | std::ios::trunc);
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

    const std::string replacement = key + std::string(std::max(0, 16 - static_cast<int>(key.size())), ' ') + " : " + value;
    bool replaced = false;
    for (std::string& line : lines) {
        const std::size_t delim = line.find(':');
        if (delim == std::string::npos) {
            continue;
        }
        const std::size_t key_end = line.find_last_not_of(" \t", delim == 0 ? 0 : delim - 1);
        const std::string trimmed_key = (key_end == std::string::npos) ? std::string() : line.substr(0, key_end + 1);
        if (trimmed_key == key) {
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

std::filesystem::path write_runtime_config(const std::filesystem::path& repo_root, const std::filesystem::path& temp_dir)
{
    const std::filesystem::path sample_config_path = repo_root / "sample" / "config.txt";
    const std::filesystem::path sample_video_path = repo_root / "sample" / "sample.mp4";
    const std::filesystem::path config_copy_path = temp_dir / "config_runtime.txt";
    const std::filesystem::path output_base = temp_dir / "output" / "sample_runtime";

    std::string runtime_config = read_text_file(sample_config_path);
    runtime_config = upsert_config_line(runtime_config, "src_fn", sample_video_path.generic_string());
    runtime_config = upsert_config_line(runtime_config, "output_fn", output_base.generic_string());
    runtime_config = upsert_config_line(runtime_config, "do_display", "n");
    runtime_config = upsert_config_line(runtime_config, "save_debug", "n");
    runtime_config = upsert_config_line(runtime_config, "save_raw", "n");

    std::error_code error_code;
    std::filesystem::create_directories(output_base.parent_path(), error_code);
    if (!write_text_file(config_copy_path, runtime_config)) {
        return {};
    }
    return config_copy_path;
}

} // namespace

int main()
{
    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "fictrac_preflight_cli_test";
    std::error_code error_code;
    std::filesystem::remove_all(temp_dir, error_code);
    std::filesystem::create_directories(temp_dir, error_code);

    const std::filesystem::path runtime_config_path = write_runtime_config(repo_root, temp_dir);
    if (runtime_config_path.empty()) {
        std::cerr << "Failed to prepare runtime config for preflight test." << std::endl;
        return 1;
    }

    if (!Trackball::validateConfigFile(runtime_config_path.string())) {
        std::cerr << "Sample runtime config should pass --validate checks." << std::endl;
        return 1;
    }

    Trackball preflight_tracker(runtime_config_path.string(), "", Trackball::StartupMode::Preflight);
    if (preflight_tracker.hasFailed() || preflight_tracker.isActive()) {
        std::cerr << "Sample runtime config should pass --preflight without starting tracking." << std::endl;
        return 1;
    }

    const std::filesystem::path output_dir = temp_dir / "output";
    if (!std::filesystem::exists(output_dir) || !std::filesystem::is_empty(output_dir)) {
        std::cerr << "Successful preflight should not create output artifacts." << std::endl;
        return 1;
    }

    std::string invalid_mode_config = read_text_file(runtime_config_path);
    invalid_mode_config = upsert_config_line(invalid_mode_config, "src_fps_mode", "mother");
    const std::filesystem::path invalid_mode_path = temp_dir / "invalid_mode.txt";
    if (!write_text_file(invalid_mode_path, invalid_mode_config) || Trackball::validateConfigFile(invalid_mode_path.string())) {
        std::cerr << "Invalid config should fail --validate without launching tracking." << std::endl;
        return 1;
    }

    std::string missing_output_config = read_text_file(runtime_config_path);
    missing_output_config = upsert_config_line(missing_output_config, "output_fn", (temp_dir / "missing" / "dir" / "sample_runtime").generic_string());
    const std::filesystem::path missing_output_path = temp_dir / "missing_output.txt";
    if (!write_text_file(missing_output_path, missing_output_config)) {
        std::cerr << "Failed to write missing-output preflight config." << std::endl;
        return 1;
    }

    Trackball missing_output_tracker(missing_output_path.string(), "", Trackball::StartupMode::Preflight);
    if (!missing_output_tracker.hasFailed()) {
        std::cerr << "Preflight should fail when output targets are not writable." << std::endl;
        return 1;
    }

    std::filesystem::remove_all(temp_dir, error_code);
    return 0;
}