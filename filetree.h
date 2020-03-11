#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>

#define MAX_PATH 260

struct FileEntry;
struct FileProvider {
    std::function<void* (FileEntry*)> OpenFile;
    std::function<void(void*)> CloseFile;
    // seems like there is no current "position," the offset is from the start of the file :)
    std::function<void(void*, void*, uint64_t, uint32_t, uint64_t*)> ReadFile;

    uint64_t fileSize;
};

struct FileTree;
struct FileEntry {
    std::wstring name;
    FileTree* parent;
    uint32_t fileAttributes;
    FileProvider* fileProvider;
    FileTree* directory; // ONLY SET IF THE ENTRY IS FOR A DIRECTORY
};

struct FileTree
{
    std::unique_ptr<FileEntry> value;
    std::vector<std::unique_ptr<FileEntry>> files;
    std::vector<std::unique_ptr<FileTree>> folders;
};

static void FileTree_GetFullName(FileEntry* entry, wchar_t* buffer) {
    wchar_t filePath[MAX_PATH + 1];
    filePath[MAX_PATH] = '\0';
    int ptr = MAX_PATH;
    do {
        if (entry->name.size() && entry->parent) {
            ptr -= entry->name.size();
            memcpy(filePath + ptr, entry->name.c_str(), sizeof(wchar_t) * entry->name.size());
        }
        else {
            wcscpy(buffer, filePath + ptr);
            return;
        }
        filePath[--ptr] = L'\\';
        entry = entry->parent->value.get();
    } while (true);
}

static void FileTree_InitializeFolder(FileTree* tree) {
    tree->value = std::make_unique<FileEntry>();
    tree->value->fileProvider = new FileProvider
    {
        .OpenFile = [&](FileEntry* entry) {
            return entry->directory;
        },
        .CloseFile = [](void* handle) {

        },
        .ReadFile = [](void* handle, void* buffer, uint64_t offset, uint32_t length, uint64_t* outBytesRead) {
            *outBytesRead = 0;
        }
    };
    tree->value->fileAttributes = FILE_ATTRIBUTE_DIRECTORY;// | FILE_ATTRIBUTE_READONLY;
    tree->value->parent = nullptr;
    tree->value->directory = tree;
}

static FileTree* FileTree_AddFolder(FileTree* tree, std::wstring name) {
    auto folder = new FileTree;
    FileTree_InitializeFolder(folder);
    folder->value->parent = tree;
    folder->value->name = name;
    tree->folders.emplace_back(folder);

    return folder;
}

// work on this later on to add custom reads and set the file size :)
static void FileTree_AddFile(FileTree* tree, std::wstring name) {
    auto file = new FileEntry;
    file->fileAttributes = FILE_ATTRIBUTE_READONLY;
    file->parent = tree;
    file->fileProvider = new FileProvider
    {
        .OpenFile = [&](FileEntry* entry) {
            return entry;
        },
        .CloseFile = [](void* handle) {

        },
        .ReadFile = [](void* handle, void* buffer, uint64_t offset, uint32_t length, uint64_t* outBytesRead) {
            *outBytesRead = 0;
        }
    };
    tree->files.emplace_back(file);
}