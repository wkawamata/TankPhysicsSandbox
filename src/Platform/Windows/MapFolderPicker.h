#pragma once

#include <filesystem>
#include <string>

struct HWND__;

namespace Tank::Platform::Windows
{
    enum class FolderPickerResult { Selected, Cancelled, Failed };
    FolderPickerResult PickMapFolder(HWND__* owner, const std::filesystem::path& initialFolder,
        std::filesystem::path& selectedFolder, std::string& error);
}
