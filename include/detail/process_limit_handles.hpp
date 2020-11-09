#ifndef PROCESS_PROCESS_LIMIT_HANDLES_HPP
#define PROCESS_PROCESS_LIMIT_HANDLES_HPP

#include <detail/process/config.hpp>
#include <algorithm>
#include <array>

#if defined(__unix__)

#include <dirent.h>

#else

#include <windows.h>
#include <winternl.h>

#endif

namespace PROCESS_NAMESPACE
{

#if defined(__unix__)

inline std::vector<int> get_handles()
{
    std::vector<int> res;

    std::unique_ptr<DIR, void(*)(DIR*)> dir{::opendir("/dev/fd"), +[](DIR* p){::closedir(p);}};
    if (!dir)
        throw std::system_error(errno, std::system_category(), "Can't open file directory");

    auto my_fd = ::dirfd(dir.get());

    struct ::dirent * ent_p;
    std::array<int, 3> stdio = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};

    while ((ent_p = readdir(dir.get())) != nullptr)
    {
        if (ent_p->d_name[0] == '.')
            continue;

        const auto conv = std::atoi(ent_p->d_name);
        if (conv == 0 && (ent_p->d_name[0] != '0' && ent_p->d_name[1] != '\0'))
            continue;

        if (conv == my_fd)
            continue;

        if (std::find(stdio.begin(), stdio.end(), conv) != stdio.end())
            continue;

        res.push_back(conv);
    }
    return res;
}

#else

inline std::vector<HANDLE> get_handles()
{
    typedef struct _SYSTEM_HANDLE_ENTRY
    {
        ULONG OwnerPid;
        BYTE ObjectType;
        BYTE HandleFlags;
        USHORT HandleValue;
        PVOID ObjectPointer;
        ULONG AccessMask;
    } SYSTEM_HANDLE_ENTRY, *PSYSTEM_HANDLE_ENTRY;

    typedef struct _SYSTEM_HANDLE_INFORMATION
    {
        ULONG Count;
        SYSTEM_HANDLE_ENTRY Handle[1];
    } SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

    auto pid = GetCurrentProcessId();

    std::vector<char> buffer(2048);

    constexpr static SYSTEM_INFORMATION_CLASS SystemHandleInformation = static_cast<SYSTEM_INFORMATION_CLASS>(16);
    constexpr static auto STATUS_INFO_LENGTH_MISMATCH = static_cast<NTSTATUS>(0xC0000004l);
    auto info_pointer = reinterpret_cast<SYSTEM_HANDLE_INFORMATION*>(buffer.data());


    NTSTATUS nt_status = STATUS_INFO_LENGTH_MISMATCH;
    for (;
         nt_status == STATUS_INFO_LENGTH_MISMATCH;
         nt_status = NtQuerySystemInformation(
                            SystemHandleInformation,
                            info_pointer, static_cast<ULONG>(buffer.size()),
                            nullptr))
    {
        buffer.resize(buffer.size() * 2);
        info_pointer = reinterpret_cast<SYSTEM_HANDLE_INFORMATION*>(buffer.data());
    }


    if (nt_status < 0 || nt_status > 0x7FFFFFFF)
        throw std::system_error(GetLastError(), std::system_category(), "Can't query NT Status");


    std::array<HANDLE, 3> stdio = {
            GetStdHandle(STD_ERROR_HANDLE),
            GetStdHandle(STD_OUTPUT_HANDLE),
            GetStdHandle(STD_INPUT_HANDLE)
    };

    std::vector<HANDLE> res;
    for (auto itr = info_pointer->Handle; itr != (info_pointer->Handle + info_pointer->Count); itr++)
    {
        const auto conv = reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(itr->HandleValue));
        if (std::find(stdio.begin(), stdio.end(), conv) != stdio.end())
            continue;

        if (itr->OwnerPid == pid)
            res.push_back(conv);
    }

    return res;
}

#endif

// Satisfies process_initializer
class process_limit_handles {
public:
    // Select all the handles that should be inherited even though they are not
    // used by any initializer.
    template<class... Handles>
    process_limit_handles(Handles&&... handles);


};

}

#endif //PROCESS_PROCESS_LIMIT_HANDLES_HPP
