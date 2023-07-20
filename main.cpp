#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm> // Add this header for std::transform
#include <fstream>
#include <cstring>

#ifdef _WIN32

#else
#include <dirent.h>
#endif


struct ThreadParams
{
    std::wstring directory;
    std::unordered_map<std::wstring, ULONGLONG> &pathSpaceUsage;
    std::unordered_map<std::wstring, ULONGLONG> &fileTypeSpaceUsage;
};


bool hasFileExtension(const std::wstring &filePath, const std::wstring &extension)
{
    size_t pos = filePath.find_last_of(L".");
    if (pos != std::wstring::npos)
    {
        std::wstring ext = filePath.substr(pos + 1);
        std::wstring extLower(ext);
        std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
        std::wstring extensionLower(extension);
        std::transform(extensionLower.begin(), extensionLower.end(), extensionLower.begin(), ::tolower);
        return extLower == extensionLower;
    }
    return false;
}
// Helper function to check if a file is a video file
bool isVideoFile(const std::wstring &filePath)
{
    return hasFileExtension(filePath, L"mp4") || hasFileExtension(filePath, L"avi") || hasFileExtension(filePath, L"mkv");
}

// Helper function to check if a file is an image file
bool isImageFile(const std::wstring &filePath)
{
    return hasFileExtension(filePath, L"jpg") || hasFileExtension(filePath, L"png") || hasFileExtension(filePath, L"gif");
}
// Function to calculate space utilization for a given directory
DWORD WINAPI calculateSpaceUtilization(LPVOID lpParam)
{
    ThreadParams *params = static_cast<ThreadParams *>(lpParam);
    std::wstring directory = params->directory;

    WIN32_FIND_DATAW findFileData;
    HANDLE hFind = FindFirstFileW((directory + L"\\*").c_str(), &findFileData);

    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                ULONGLONG fileSize = (static_cast<ULONGLONG>(findFileData.nFileSizeHigh) << (sizeof(findFileData.nFileSizeLow) * 8)) + findFileData.nFileSizeLow;

                // Calculate space utilization for paths
                std::wstring path = directory + L"\\" + findFileData.cFileName;
                params->pathSpaceUsage[path] += fileSize;

                // Calculate space utilization for file types
                if (isVideoFile(findFileData.cFileName))
                {
                    params->fileTypeSpaceUsage[L"video"] += fileSize;
                }
                else if (isImageFile(findFileData.cFileName))
                {
                    params->fileTypeSpaceUsage[L"image"] += fileSize;
                }
                else
                {
                    params->fileTypeSpaceUsage[L"other"] += fileSize;
                }
            }
        }
        while (FindNextFileW(hFind, &findFileData) != 0);

        FindClose(hFind);
    }

    return 0;
}

class MenuDrivenProgram
{
private:
    // Structure to store file information
    struct FileInfo
    {
        std::string path;
        std::string hash;
        uintmax_t size;
    };

    std::wstring formatBytes(ULONGLONG bytes)
    {
        const wchar_t* units[] = { L"Bytes", L"KB", L"MB", L"GB", L"TB", L"PB", L"EB", L"ZB", L"YB" };
        int unitIndex = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024 && unitIndex < sizeof(units) / sizeof(units[0]) - 1)
        {
            size /= 1024;
            unitIndex++;
        }

        wchar_t buffer[100];
        swprintf(buffer, sizeof(buffer), L"%.2f %s", size, units[unitIndex]);
        return buffer;
    }


    void IdentifyLargeFiles(const std::wstring &dirPath, uintmax_t thresholdSize)
    {
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW((dirPath + L"\\*").c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE)
        {
            std::wcout << L"Error opening directory." << std::endl;
            return;
        }

        do
        {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
                continue;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                std::wstring subDirPath = dirPath + L"\\" + findData.cFileName;
                IdentifyLargeFiles(subDirPath, thresholdSize);
            }
            else
            {
                uintmax_t fileSize = static_cast<uintmax_t>(findData.nFileSizeHigh) << (sizeof(DWORD) * 8) | findData.nFileSizeLow;
                if (fileSize > thresholdSize)
                {
                    std::wcout << L"Large File: " << dirPath << L"\\" << findData.cFileName << L" (" << fileSize << L" bytes)" << std::endl;
                }
            }

        }
        while (FindNextFileW(hFind, &findData) != 0);

        FindClose(hFind);
    }

    // Function to calculate the hash of a file
    std::string calculateFileHash(const std::string &filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        {
            return "";
        }

        constexpr size_t bufferSize = 8192;
        char buffer[bufferSize];
        std::hash<std::string> hasher;

        std::string hashResult;
        while (file.read(buffer, bufferSize))
        {
            hashResult += hasher(std::string(buffer, buffer + file.gcount()));
        }

        if (file.gcount() > 0)
        {
            hashResult += hasher(std::string(buffer, buffer + file.gcount()));
        }

        return hashResult;
    }


    void processDirectory(const std::wstring& currentPath, const std::vector<std::wstring>& userExtensions, std::unordered_map<std::wstring, ULONGLONG>& fileTypeSpaceUsage)
    {
        std::wstring path = currentPath + L"\\*";

        WIN32_FIND_DATAW findFileData;
        HANDLE hFind = FindFirstFileW(path.c_str(), &findFileData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (wcscmp(findFileData.cFileName, L".") != 0 && wcscmp(findFileData.cFileName, L"..") != 0)
                    {
                        std::wstring subPath = currentPath + L"\\" + findFileData.cFileName;
                        processDirectory(subPath, userExtensions, fileTypeSpaceUsage);
                    }
                }
                else
                {
                    ULONGLONG fileSize = (static_cast<ULONGLONG>(findFileData.nFileSizeHigh) << (sizeof(findFileData.nFileSizeLow) * 8)) + findFileData.nFileSizeLow;

                    // Calculate space utilization for file types
                    bool isFileTypeIncluded = false;
                    for (const auto& ext : userExtensions)
                    {
                        if (hasFileExtension(findFileData.cFileName, ext))
                        {
                            fileTypeSpaceUsage[currentPath] += fileSize;
                            isFileTypeIncluded = true;
                            break;
                        }
                    }

                    // If the file doesn't match any specified extensions, categorize it as "other"
                    if (!isFileTypeIncluded)
                    {
                        fileTypeSpaceUsage[currentPath] += fileSize;
                    }
                }
            }
            while (FindNextFileW(hFind, &findFileData) != 0);

            FindClose(hFind);
        }
    }

    // Function to find duplicate files on the disk
    void findDuplicateFiles(const std::string &directoryPath, std::unordered_map<std::string, std::vector<FileInfo>> &filesByHash)
    {
#ifdef _WIN32
        WIN32_FIND_DATA findFileData;
        HANDLE hFind = FindFirstFile((directoryPath + "/*").c_str(), &findFileData);
        if (hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }

        do
        {
            if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                const auto filePath = directoryPath + "/" + findFileData.cFileName;
                const auto fileSize = static_cast<uintmax_t>(findFileData.nFileSizeLow) + (static_cast<uintmax_t>(findFileData.nFileSizeHigh) << 32);
                const auto fileHash = calculateFileHash(filePath);

                if (!fileHash.empty())
                {
                    filesByHash[fileHash].push_back({filePath, fileHash, fileSize});
                }
            }
        }
        while (FindNextFile(hFind, &findFileData) != 0);

        FindClose(hFind);
#else
        DIR *dir = opendir(directoryPath.c_str());
        if (!dir)
        {
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_type == DT_REG)
            {
                const auto filePath = directoryPath + "/" + entry->d_name;
                const auto fileHash = calculateFileHash(filePath);

                if (!fileHash.empty())
                {
                    struct stat st;
                    if (stat(filePath.c_str(), &st) == 0)
                    {
                        const auto fileSize = static_cast<uintmax_t>(st.st_size);
                        filesByHash[fileHash].push_back({filePath, fileHash, fileSize});
                    }
                }
            }
        }

        closedir(dir);
#endif
    }


public:
    void displayMenu()
    {
        std::cout << "================ Disk Manager Menu =====================\n";
        std::cout<<"1. Display the amount of free space available on the disk\n";
        std::cout<<"2. Present the amount of space utilized on the disk\n";
        std::cout<<"3. Provide a breakdown of space utilization\n";
        std::cout<<"4. Detect duplicate files \n";
        std::cout<<"5. Identify large files\n";
        std::cout<<"6. Provide the capability to scan specific file types\n";
        std::cout<<"7. Allow users to delete files of specific types\n";

    }
    void processChoice(int choice)
    {
        switch (choice)
        {
        case 1:
            option1();
            break;
        case 2:
            option2();
            break;
        case 3:
            option3();
            break;
        case 4:
            option4();
            break;
        case 5:
            option5();
            break;
        case 6:
            option6();
            break;
        case 7:
            option7();
            break;
        case 0:
            std::cout << "Exiting the program. Goodbye!\n";
            break;
        default:
            std::cout << "Invalid choice. Please try again.\n";
            break;
        }
    }



    void option1()
    {

        LPCSTR lpDirectoryName = "C:\\";

        ULARGE_INTEGER freeBytesAvailableToCaller;
        ULARGE_INTEGER totalNumberOfBytes;
        ULARGE_INTEGER totalNumberOfFreeBytes;

        BOOL result = GetDiskFreeSpaceExA(
                          lpDirectoryName,
                          &freeBytesAvailableToCaller,
                          &totalNumberOfBytes,
                          &totalNumberOfFreeBytes);

        if (result)
        {
            std::cout << "Disk space information for directory: " << lpDirectoryName << std::endl;
            std::wcout << "Free space available to caller: "<<formatBytes(freeBytesAvailableToCaller.QuadPart) << std::endl;
            std::wcout << "Total number of bytes on disk: " <<formatBytes(totalNumberOfBytes.QuadPart) << std::endl;
            std::wcout << "Total number of free bytes on disk: "<<formatBytes(totalNumberOfFreeBytes.QuadPart) << std::endl;
        }
        else
        {
            std::cerr << "Failed to get disk space information. Error code: " << GetLastError() << std::endl;
        }


    }






    void option2()
    {
        LPCWSTR rootPath = L"C:\\";

        ULARGE_INTEGER freeBytesAvailable;
        ULARGE_INTEGER totalNumberOfBytes;
        ULARGE_INTEGER totalNumberOfFreeBytes;

        if (!GetDiskFreeSpaceExW(rootPath, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes))
        {
            std::cerr << "Failed to get disk space information. Error code: " << GetLastError() << std::endl;
            return;
        }

        ULONGLONG totalSpace = totalNumberOfBytes.QuadPart;
        ULONGLONG freeSpace = totalNumberOfFreeBytes.QuadPart;
        ULONGLONG utilizedSpace = totalSpace - freeSpace;

        std::wcout << L"Utilized space: " << formatBytes(utilizedSpace) << std::endl;
    }






    void option3()
    {
        const std::wstring rootPath = L"D:\\"; // Change this to your desired root directory

        std::unordered_map<std::wstring, ULONGLONG> pathSpaceUsage;
        std::unordered_map<std::wstring, ULONGLONG> fileTypeSpaceUsage;

        // Get the list of subdirectories in the root path
        WIN32_FIND_DATAW findFileData;
        HANDLE hFind = FindFirstFileW((rootPath + L"\\*").c_str(), &findFileData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (wcscmp(findFileData.cFileName, L".") != 0) && (wcscmp(findFileData.cFileName, L"..") != 0))
                {
                    std::wstring subdirectory = rootPath + L"\\" + findFileData.cFileName;

                    // Create a new thread to process each subdirectory
                    ThreadParams params = {subdirectory, pathSpaceUsage, fileTypeSpaceUsage};
                    HANDLE hThread = CreateThread(NULL, 0, calculateSpaceUtilization, &params, 0, NULL);
                    if (hThread)
                    {
                        // Wait for the thread to finish before proceeding to the next iteration
                        WaitForSingleObject(hThread, INFINITE);
                        CloseHandle(hThread);
                    }
                }
            }
            while (FindNextFileW(hFind, &findFileData) != 0);

            FindClose(hFind);
        }

        // Display space utilization breakdown for paths
        std::wcout << "Space Utilization Breakdown (Based on Paths):" << std::endl;
        for (const auto &entry : pathSpaceUsage)
        {
            std::wcout << "Path " << entry.first << ": " << entry.second << " bytes" << std::endl;
        }

        // Display space utilization breakdown for file types
        std::wcout << "\nSpace Utilization Breakdown (Based on File Types):" << std::endl;
        for (const auto &entry : fileTypeSpaceUsage)
        {
            std::wcout << "File Type " << entry.first << ": " << entry.second << " bytes" << std::endl;
        }



    }


    void option4()
    {
        const std::string directoryPath = "D://";

        std::unordered_map<std::string, std::vector<FileInfo>> filesByHash;

        findDuplicateFiles(directoryPath, filesByHash);

        std::vector<std::vector<FileInfo>> duplicateFiles;
        duplicateFiles.reserve(filesByHash.size());

        // Remove non-duplicates from the map
        for (auto it = filesByHash.begin(); it != filesByHash.end();)
        {
            if (it->second.size() < 2)
            {
                it = filesByHash.erase(it);
            }
            else
            {
                duplicateFiles.push_back(std::move(it->second));
                ++it;
            }
        }

        if (duplicateFiles.empty())
        {
            std::cout << "No duplicate files found." << std::endl;
        }
        else
        {
            std::cout << "Duplicate files found:" << std::endl;
            for (const auto &group : duplicateFiles)
            {
                std::cout << "Group:" << std::endl;
                for (const auto &fileInfo : group)
                {
                    std::cout << "File Path: " << fileInfo.path << std::endl;
                    std::wcout << "File Size: " << formatBytes(fileInfo.size) << " bytes" << std::endl;
                    std::cout << "File Hash: " << fileInfo.hash << std::endl;
                }
                std::cout << std::endl;
            }
        }

    }

    void option5()
    {
        std::wstring path = L"D://";
        uintmax_t thresholdSize = 1024*1024*100; // 100 MB
        IdentifyLargeFiles(path, thresholdSize);
    }

    void option6()
    {
        const std::wstring rootPath = L"D://"; // Change this to your desired root directory

        std::unordered_map<std::wstring, ULONGLONG> fileTypeSpaceUsage;

        std::vector<std::wstring> videoExtensions = { L"mp4", L"avi", L"mkv" };
        std::vector<std::wstring> imageExtensions = { L"jpg", L"png", L"gif" };

        // Ask the user for file extensions to scan
        std::wcout << "Enter file extensions to scan (separated by spaces, e.g., 'mp4 jpg avi'): ";
        std::wstring userInput;
        std::wcin.ignore(); // Clear the input stream
        std::getline(std::wcin, userInput);

        // Split the user input into individual file extensions
        std::vector<std::wstring> userExtensions;
        size_t pos = 0;
        while ((pos = userInput.find(L' ')) != std::wstring::npos)
        {
            std::wstring ext = userInput.substr(0, pos);
            userExtensions.push_back(ext);
            userInput.erase(0, pos + 1);
        }
        userExtensions.push_back(userInput); // Add the last extension

        // Recursively process the root directory and its subdirectories
        processDirectory(rootPath, userExtensions, fileTypeSpaceUsage);

        // Display space utilization breakdown for specified file types
        std::wcout << "\nSpace Utilization Breakdown:" << std::endl;
        for (const auto& entry : fileTypeSpaceUsage)
        {
            std::wcout << entry.first << L": " << formatBytes(entry.second) << std::endl;
        }
    }

    void option7()
    {
        const std::wstring rootPath = L"D:\\Sample"; // Change this to your desired root directory

        std::vector<std::wstring> fileExtensionsToDelete;
        std::wstring userInput;
        std::wcin.ignore(); // Clear the input stream

        std::cout << "Enter file extensions to delete (separated by spaces, e.g., 'mp4 jpg avi'): ";
        std::getline(std::wcin, userInput);

        // Parse the user input and store the file extensions in the vector
        size_t startPos = 0;
        size_t endPos = 0;
        while ((endPos = userInput.find(L' ', startPos)) != std::wstring::npos)
        {
            fileExtensionsToDelete.push_back(userInput.substr(startPos, endPos - startPos));
            startPos = endPos + 1;
        }
        fileExtensionsToDelete.push_back(userInput.substr(startPos)); // Add the last extension

        WIN32_FIND_DATAW findFileData;
        HANDLE hFind = FindFirstFileW((rootPath + L"\\*").c_str(), &findFileData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    std::wstring filePath = rootPath + L"\\" + findFileData.cFileName;

                    // Check if the file has any of the specified extensions and delete if matched
                    for (const auto& extension : fileExtensionsToDelete)
                    {
                        if (hasFileExtension(findFileData.cFileName, extension))
                        {
                            if (DeleteFileW(filePath.c_str()))
                            {
                                std::wcout << "Deleted: " << filePath << std::endl;
                            }
                            else
                            {
                                std::wcout << "Failed to delete: " << filePath << std::endl;
                            }
                            break; // Move to the next file after deleting one matching extension
                        }
                    }
                }
            }
            while (FindNextFileW(hFind, &findFileData) != 0);

            FindClose(hFind);
        }
    }



};

int main()
{

    MenuDrivenProgram program;
    int choice;

    do
    {
        program.displayMenu();
        std::cout << "Enter your choice (1-7, 0 to exit): ";
        std::cin >> choice;
        program.processChoice(choice);
    }
    while (choice != 0);

    return 0;

}
