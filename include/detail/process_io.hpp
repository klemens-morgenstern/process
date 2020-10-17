#ifndef PROCESS_IO_HPP
#define PROCESS_IO_HPP

#if defined(__unix__)
#include <detail/process/posix/process_io.hpp>
#endif
#if defined(_WIN32) || defined(WIN32)
#include <detail/process/windows/process_io.hpp>
#endif

namespace PROCESS_NAMESPACE
{

class process_io {
public:
// OS dependent handle type
    using native_handle = implementation-defined;
    using in_default = implementation-defined;
    using out_default = implementation-defined;
    using err_default = implementation-defined;


    template<ProcessReadableStream In = in_default,
             ProcessWritableStream Out = out_default,
             ProcessWritableStream Err = err_default>
    process_io(In&& in, Out&& out, Err&& err);
// Rest is implementation-defined
};


}

#endif //PROCESS_HPP
