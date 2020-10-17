#ifndef PROCESS_ASIO_GROUP_HANDLE_HPP
#define PROCESS_ASIO_GROUP_HANDLE_HPP

#include "asio/async_result.hpp"
#include "asio/detail/io_object_impl.hpp"
#include "asio/detail/throw_error.hpp"
#include "asio/detail/win_object_handle_service.hpp"
#include "asio/error.hpp"
#include "asio/execution_context.hpp"
#include "asio/executor.hpp"

namespace PROCESS_NAMESPACE::detail::process::windows
{

template <typename Executor = asio::executor>
class basic_asio_group_handle
{
    typedef asio::detail::win_object_handle_service::native_handle_type native_handle_type;
};


using asio_group_handle =  basic_asio_group_handle<>;

}

#endif //PROCESS_ASIO_GROUP_HANDLE_HPP
