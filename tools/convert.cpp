#include <lunasvg.h>
#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace lunasvg;

void convert_file(const fs::path& src_path, const fs::path& dst_path) {
    auto document = Document::loadFromFile(src_path.string());
    if (!document) {
        std::cerr << "Failed to load: " << src_path << std::endl;
        return;
    }

    auto bitmap = document->renderToBitmap();
    if (!bitmap.isNull()) {
        if (bitmap.writeToPng(dst_path.string())) {
            std::cout << "Converted: " << src_path.filename() << " -> " << dst_path.filename() << std::endl;
        } else {
            std::cerr << "Failed to write PNG: " << dst_path << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: convert <input_dir> <output_dir>" << std::endl;
        return 1;
    }

    fs::path src_dir(argv[1]);
    fs::path dst_dir(argv[2]);

    if (!fs::exists(src_dir) || !fs::is_directory(src_dir)) {
        std::cerr << "Error: Input directory does not exist or is not a directory." << std::endl;
        return 1;
    }

    if (!fs::exists(dst_dir)) {
        fs::create_directories(dst_dir);
    }

    for (const auto& entry : fs::directory_iterator(src_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".svg") {
            fs::path src_file = entry.path();
            fs::path dst_file = dst_dir / src_file.stem();
            dst_file.replace_extension(".png");

            convert_file(src_file, dst_file);
        }
    }

    std::cout << "Batch conversion complete." << std::endl;
    return 0;
}
