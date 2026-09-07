#include "MapFolder.h"

#define NOMINMAX
#include <Windows.h>

#include <atomic>
#include <fstream>
#include <iterator>
#include <limits>

namespace Tank::Map
{
    namespace
    {
        bool Fail(std::string& error, const std::string& message)
        {
            error = message;
            return false;
        }

        std::string WindowsError(const char* action)
        {
            return std::string(action) + " (Windows error " + std::to_string(GetLastError()) + ")";
        }

        // Write beside the destination and replace only after the full file is flushed.
        // Creating a new manifest never overwrites a file created by another process.
        bool WriteManifest(const std::filesystem::path& folder, const std::string& json,
            bool replaceExisting, std::string& error)
        {
            std::string bytes;
            for (const char c : json)
            {
                if (c == '\n') bytes += '\r';
                bytes += c;
            }
            bytes += "\r\n";
            if (bytes.size() > (std::numeric_limits<DWORD>::max)())
                return Fail(error, "Manifest is too large to save");

            static std::atomic<unsigned long long> serial = 0;
            std::filesystem::path temporary;
            HANDLE file = INVALID_HANDLE_VALUE;
            for (int attempt = 0; attempt < 100; ++attempt)
            {
                temporary = folder / (L".Manifest." + std::to_wstring(GetCurrentProcessId()) +
                    L"." + std::to_wstring(++serial) + L".tmp");
                file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file != INVALID_HANDLE_VALUE) break;
                if (GetLastError() != ERROR_FILE_EXISTS)
                    return Fail(error, WindowsError("Cannot create temporary manifest"));
            }
            if (file == INVALID_HANDLE_VALUE)
                return Fail(error, "Cannot allocate a temporary manifest file");

            DWORD written = 0;
            bool succeeded = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
            if (!succeeded) error = WindowsError("Cannot write manifest");
            else if (written != bytes.size())
            {
                succeeded = false;
                error = "Incomplete manifest write";
            }
            if (succeeded && !FlushFileBuffers(file))
            {
                succeeded = false;
                error = WindowsError("Cannot flush manifest");
            }
            if (!CloseHandle(file) && succeeded)
            {
                succeeded = false;
                error = WindowsError("Cannot close manifest");
            }
            const auto destination = folder / L"Manifest.json";
            if (succeeded && !MoveFileExW(temporary.c_str(), destination.c_str(),
                MOVEFILE_WRITE_THROUGH | (replaceExisting ? MOVEFILE_REPLACE_EXISTING : 0)))
            {
                succeeded = false;
                error = WindowsError("Cannot replace manifest");
            }
            if (!succeeded) DeleteFileW(temporary.c_str());
            return succeeded;
        }
    }

    bool MapFolder::Open(const std::filesystem::path& folder, std::string& error)
    {
        try
        {
            if (folder.empty() || !std::filesystem::is_directory(folder))
                return Fail(error, "Select an existing map folder");
            auto resolved = std::filesystem::canonical(folder);
            const auto path = resolved / L"Manifest.json";
            Manifest loaded;
            std::string serialized;
            // A broken link or a directory is not a missing manifest.
            if (std::filesystem::symlink_status(path).type() != std::filesystem::file_type::not_found)
            {
                if (!std::filesystem::is_regular_file(path))
                    return Fail(error, "Manifest.json is not a readable regular file");
                std::ifstream input(path, std::ios::binary);
                if (!input) return Fail(error, "Cannot open Manifest.json");
                const std::string bytes((std::istreambuf_iterator<char>(input)), {});
                if (input.bad()) return Fail(error, "Cannot read Manifest.json");
                if (!DeserializeManifest(bytes, loaded, error)) return false;
            }
            else
            {
                if (!SerializeManifest(loaded, serialized, error) ||
                    !WriteManifest(resolved, serialized, false, error)) return false;
            }
            if (!SerializeManifest(loaded, serialized, error)) return false;
            m_folder = std::move(resolved);
            m_manifest = std::move(loaded);
            m_savedJson = serialized;
            m_currentJson = std::move(serialized);
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            return Fail(error, exception.what());
        }
    }

    bool MapFolder::Save(std::string& error)
    {
        if (!IsOpen()) return Fail(error, "Open a map folder before saving");
        try
        {
            if (!WriteManifest(m_folder, m_currentJson, true, error)) return false;
            m_savedJson = m_currentJson;
            error.clear();
            return true;
        }
        catch (const std::exception& exception)
        {
            return Fail(error, exception.what());
        }
    }

    bool MapFolder::SetManifest(const Manifest& manifest, std::string& error)
    {
        if (!IsOpen()) return Fail(error, "Open a map folder before editing");
        std::string serialized;
        if (!SerializeManifest(manifest, serialized, error)) return false;
        m_manifest = manifest;
        m_currentJson = std::move(serialized);
        return true;
    }
}
