// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>

#include <boost/process/error.hpp>
#include <boost/process/execute.hpp>
#include <boost/process/async.hpp>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#if defined(BOOST_WINDOWS_API)
#   include <Windows.h>
typedef boost::asio::windows::stream_handle pipe_end;
#elif defined(BOOST_POSIX_API)
#   include <sys/wait.h>
#   include <signal.h>
typedef boost::asio::posix::stream_descriptor pipe_end;
#endif

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(sync_wait)
{
    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::child c = bp::execute(
        master_test_suite().argv[1],
        "test", "--exit-code", "123",
        ec
    );
    BOOST_REQUIRE(!ec);
    c.wait();
    int exit_code = c.exit_code();
#if defined(BOOST_WINDOWS_API)
    BOOST_CHECK_EQUAL(123, exit_code);
#elif defined(BOOST_POSIX_API)
    BOOST_CHECK_EQUAL(123, WEXITSTATUS(exit_code));
#endif
    c.wait();
}

#if defined(BOOST_WINDOWS_API)
struct wait_handler
{
    HANDLE handle_;

    wait_handler(HANDLE handle) : handle_(handle) {}

    void operator()(const boost::system::error_code &ec)
    {
        BOOST_REQUIRE(!ec);
        DWORD exit_code;
        BOOST_REQUIRE(GetExitCodeProcess(handle_, &exit_code));
        BOOST_CHECK_EQUAL(123, exit_code);
        BOOST_REQUIRE(GetExitCodeProcess(handle_, &exit_code));
        BOOST_CHECK_EQUAL(123, exit_code);
    }
};
#elif defined(BOOST_POSIX_API)
struct wait_handler
{
    void operator()(const boost::system::error_code &ec, int signal)
    {
        BOOST_REQUIRE(!ec);
        BOOST_REQUIRE_EQUAL(SIGCHLD, signal);
        int status;
        wait(&status);
        BOOST_CHECK_EQUAL(123, WEXITSTATUS(status));
    }
};
#endif

BOOST_AUTO_TEST_CASE(async_wait)
{
    using boost::unit_test::framework::master_test_suite;
    using namespace boost::asio;

    boost::asio::io_service io_service;

#if defined(BOOST_POSIX_API)
    signal_set set(io_service, SIGCHLD);
    set.async_wait(wait_handler());
#endif

    std::error_code ec;
    bp::child c = bp::execute(
        master_test_suite().argv[1],
        "test", "--exit-code", "123",
        ec
#if defined(BOOST_POSIX_API)
        , io_service
#endif
    );
    BOOST_REQUIRE(!ec);

#if defined(BOOST_WINDOWS_API)
    windows::object_handle handle(io_service, c.native_handle());
    handle.async_wait(wait_handler(handle.native()));
#endif

    io_service.run();
    c.wait();
}
