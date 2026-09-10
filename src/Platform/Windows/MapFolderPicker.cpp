#include "MapFolderPicker.h"

#include <Windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace Tank::Platform::Windows
{
    FolderPickerResult PickMapFolder(HWND__* owner, const std::filesystem::path& initialFolder,
        std::filesystem::path& selectedFolder, std::string& error)
    {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        {
            error = "Cannot initialize folder picker";
            return FolderPickerResult::Failed;
        }
        HRESULT result;
        {
            Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
            result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
            if (SUCCEEDED(result))
            {
                DWORD options = 0;
                result = dialog->GetOptions(&options);
                if (SUCCEEDED(result)) result = dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
                if (SUCCEEDED(result)) result = dialog->SetTitle(L"Open Map Folder");
                if (!initialFolder.empty())
                {
                    Microsoft::WRL::ComPtr<IShellItem> initial;
                    if (SUCCEEDED(SHCreateItemFromParsingName(initialFolder.c_str(), nullptr, IID_PPV_ARGS(&initial))))
                        dialog->SetFolder(initial.Get());
                }
                if (SUCCEEDED(result)) result = dialog->Show(owner);
                if (SUCCEEDED(result))
                {
                    Microsoft::WRL::ComPtr<IShellItem> selected;
                    result = dialog->GetResult(&selected);
                    if (SUCCEEDED(result))
                    {
                        PWSTR path = nullptr;
                        result = selected->GetDisplayName(SIGDN_FILESYSPATH, &path);
                        if (SUCCEEDED(result)) selectedFolder = path;
                        CoTaskMemFree(path);
                    }
                }
            }
        }
        if (SUCCEEDED(initialized)) CoUninitialize();
        if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            error.clear();
            return FolderPickerResult::Cancelled;
        }
        if (FAILED(result))
        {
            error = "Cannot open folder picker (HRESULT " + std::to_string(result) + ")";
            return FolderPickerResult::Failed;
        }
        error.clear();
        return FolderPickerResult::Selected;
    }
}
