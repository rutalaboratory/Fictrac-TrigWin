#include "Trackball.h"

#include <filesystem>
#include <fstream>
#include <iostream>
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

bool trackball_rejects_config(const std::filesystem::path& repo_root, const std::string& key, const std::string& value)
{
    const std::filesystem::path sample_config_path = repo_root / "sample" / "config.txt";
    const std::filesystem::path sample_video_path = repo_root / "sample" / "sample.mp4";
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / ("fictrac_" + key + "_validation_test");
    const std::filesystem::path config_copy_path = temp_dir / "config_runtime.txt";
    const std::filesystem::path output_base = temp_dir / "output" / "sample_runtime";

    std::error_code error_code;
    std::filesystem::remove_all(temp_dir, error_code);
    std::filesystem::create_directories(output_base.parent_path(), error_code);

    std::string runtime_config = read_text_file(sample_config_path);
    if (runtime_config.empty()) {
        return false;
    }

    runtime_config = upsert_config_line(runtime_config, "src_fn", sample_video_path.generic_string());
    runtime_config = upsert_config_line(runtime_config, "output_fn", output_base.generic_string());
    runtime_config = upsert_config_line(runtime_config, "do_display", "n");
    runtime_config = upsert_config_line(runtime_config, "save_debug", "n");
    runtime_config = upsert_config_line(runtime_config, "save_raw", "n");
    runtime_config = upsert_config_line(runtime_config, key, value);

    if (!write_text_file(config_copy_path, runtime_config)) {
        std::filesystem::remove_all(temp_dir, error_code);
        return false;
    }

    Trackball tracker(config_copy_path.string());
    const bool rejected = !tracker.isActive();
    std::filesystem::remove_all(temp_dir, error_code);
    return rejected;
}

} // namespace

int main()
{
    const std::filesystem::path repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();

    if (!trackball_rejects_config(repo_root, "q_factor", "not_an_int")) {
        std::cerr << "Trackball should reject q_factor with a non-integer value." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "thr_win_pc", "1.5")) {
        std::cerr << "Trackball should reject thr_win_pc outside [0, 1]." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "thr_rgb_tfrm", "magenta")) {
        std::cerr << "Trackball should reject unsupported thr_rgb_tfrm values." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "src_fps_mode", "mother")) {
        std::cerr << "Trackball should reject unsupported src_fps_mode values even for file-backed sources." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "vid_codec", "vp9")) {
        std::cerr << "Trackball should reject unsupported vid_codec values even when video saving is disabled." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "roi_c", "{ 1.0, 2.0 }")) {
        std::cerr << "Trackball should reject roi_c vectors that do not contain exactly three values." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "c2a_src", "bogus_plane")) {
        std::cerr << "Trackball should reject unsupported c2a_src values." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "opt_max_err", "-0.5")) {
        std::cerr << "Trackball should reject opt_max_err values between -1 and 0." << std::endl;
        return 1;
    }

    if (!trackball_rejects_config(repo_root, "src_fps", "-2")) {
        std::cerr << "Trackball should reject src_fps values below -1." << std::endl;
        return 1;
    }

    return 0;
}