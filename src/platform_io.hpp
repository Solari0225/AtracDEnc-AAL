#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#endif

namespace independent_aal {
inline std::string utf8(const std::wstring& text) {
    std::string out;
    for(size_t i=0;i<text.size();++i) {
        uint32_t c=uint32_t(text[i]);
        if(sizeof(wchar_t)==2&&c>=0xd800&&c<=0xdbff) {
            if(i+1>=text.size()||uint32_t(text[i+1])<0xdc00||uint32_t(text[i+1])>0xdfff)throw std::runtime_error("Invalid Unicode string.");
            c=0x10000+((c-0xd800)<<10)+(uint32_t(text[++i])-0xdc00);
        }
        if(c>0x10ffff||(c>=0xd800&&c<=0xdfff))throw std::runtime_error("Invalid Unicode string.");
        if(c<0x80)out+=char(c);
        else if(c<0x800){out+=char(0xc0|(c>>6));out+=char(0x80|(c&63));}
        else if(c<0x10000){out+=char(0xe0|(c>>12));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
        else{out+=char(0xf0|(c>>18));out+=char(0x80|((c>>12)&63));out+=char(0x80|((c>>6)&63));out+=char(0x80|(c&63));}
    }
    return out;
}
inline void remove_owned_file(const std::filesystem::path& path) noexcept {std::error_code ignored;std::filesystem::remove(path,ignored);}
inline std::string temporary_token() {
    static std::atomic_uint64_t sequence{0};
    const auto clock=std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
    const auto process=GetCurrentProcessId();
#else
    const auto process=getpid();
#endif
    return ".tmp-"+std::to_string(process)+"-"+std::to_string(clock)+"-"+std::to_string(sequence.fetch_add(1));
}
inline void write_new(const std::filesystem::path& path,const void* bytes,size_t count) {
#ifdef _WIN32
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create temporary output (Windows error "+std::to_string(GetLastError())+").");
    try {
        const auto* p=static_cast<const uint8_t*>(bytes);
        for(size_t offset=0;offset<count;) {
            DWORD written=0,n=DWORD(std::min<size_t>(count-offset,1024*1024));
            if(!WriteFile(file,p+offset,n,&written,nullptr)||written!=n)throw std::runtime_error("Incomplete output write.");offset+=written;
        }
        if(!FlushFileBuffers(file))throw std::runtime_error("Cannot flush output file.");CloseHandle(file);
    }catch(...){CloseHandle(file);remove_owned_file(path);throw;}
#else
    int file=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL,0666);
    if(file<0)throw std::runtime_error("Cannot create temporary output (errno "+std::to_string(errno)+").");
    try {
        const auto* p=static_cast<const uint8_t*>(bytes);
        for(size_t offset=0;offset<count;) {
            const auto n=write(file,p+offset,std::min<size_t>(count-offset,1024*1024));
            if(n<0&&errno==EINTR)continue;
            if(n<=0)throw std::runtime_error("Incomplete output write.");offset+=size_t(n);
        }
        if(fsync(file))throw std::runtime_error("Cannot flush output file.");
        if(close(file)){file=-1;throw std::runtime_error("Cannot close output file.");}
    }catch(...){if(file>=0)close(file);remove_owned_file(path);throw;}
#endif
}
inline void publish_new(const std::filesystem::path& temporary,const std::filesystem::path& destination) {
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot publish file; destination may already exist.");
#elif defined(__APPLE__)
    if(renamex_np(temporary.c_str(),destination.c_str(),RENAME_EXCL))throw std::runtime_error("Cannot publish file; destination may already exist.");
#else
    // A same-directory hard link publishes a complete inode without replacing
    // any existing destination. Removing the temporary name is then safe.
    if(link(temporary.c_str(),destination.c_str()))throw std::runtime_error("Cannot publish file; destination may already exist.");
    if(unlink(temporary.c_str()))throw std::runtime_error("Cannot remove temporary output link.");
#endif
}
}
