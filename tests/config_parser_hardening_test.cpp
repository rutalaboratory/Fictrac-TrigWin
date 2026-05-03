#include "ConfigParser.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace {

bool write_text_file(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream handle(path);
    if (!handle.is_open()) {
        return false;
    }
    handle << content;
    return handle.good();
}

} // namespace

int main()
{
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "fictrac_config_parser_hardening_test";
    std::error_code error_code;
    std::filesystem::create_directories(temp_dir, error_code);

    const std::filesystem::path valid_path = temp_dir / "valid.txt";
    const std::filesystem::path malformed_path = temp_dir / "malformed.txt";
    const std::filesystem::path duplicate_path = temp_dir / "duplicate.txt";

    if (!write_text_file(valid_path, "src_fn           : sample.mp4\nvfov             : 45\n")
        || !write_text_file(malformed_path, "src_fn           : sample.mp4\nthis line is broken\nvfov             : 45\n")
        || !write_text_file(duplicate_path, "src_fn           : sample.mp4\nsrc_fn           : sample2.mp4\nvfov             : 45\n")) {
        std::cerr << "Failed to prepare config parser test files." << std::endl;
        return 1;
    }

    ConfigParser valid_config;
    if (valid_config.read(valid_path.string()) != 2) {
        std::cerr << "Expected valid config to parse successfully." << std::endl;
        return 1;
    }

    ConfigParser malformed_config;
    if (malformed_config.read(malformed_path.string()) >= 0) {
        std::cerr << "Malformed config should fail to parse." << std::endl;
        return 1;
    }

    ConfigParser duplicate_config;
    if (duplicate_config.read(duplicate_path.string()) >= 0) {
        std::cerr << "Duplicate config keys should fail to parse." << std::endl;
        return 1;
    }

    std::filesystem::remove_all(temp_dir, error_code);
    return 0;
}