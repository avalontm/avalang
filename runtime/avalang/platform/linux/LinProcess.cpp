#include "LinProcess.h"

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>

namespace ava {
namespace platform {
namespace linux_ {

namespace {

bool PipeReadAll(int fd, std::string& out) {
    char buf[4096];
    ssize_t n;
    while ((n = ::read(fd, buf, sizeof(buf))) > 0) {
        out.append(buf, n);
    }
    ::close(fd);
    return n >= 0;
}

} // namespace

uint64_t LinProcess::CurrentProcessId() const {
    return static_cast<uint64_t>(::getpid());
}

bool LinProcess::Execute(const std::string& command,
                           const std::vector<std::string>& args,
                           ProcessResult& out_result) {
    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    if (::pipe(out_pipe) != 0) return false;
    if (::pipe(err_pipe) != 0) {
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        return false;
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(out_pipe[0]); ::close(out_pipe[1]);
        ::close(err_pipe[0]); ::close(err_pipe[1]);
        return false;
    }

    if (pid == 0) {
        // child
        ::close(out_pipe[0]);
        ::close(err_pipe[0]);
        ::dup2(out_pipe[1], STDOUT_FILENO);
        ::dup2(err_pipe[1], STDERR_FILENO);
        ::close(out_pipe[1]);
        ::close(err_pipe[1]);

        std::vector<char*> argv_c;
        argv_c.push_back(const_cast<char*>(command.c_str()));
        for (const auto& a : args) {
            argv_c.push_back(const_cast<char*>(a.c_str()));
        }
        argv_c.push_back(nullptr);
        ::execvp(command.c_str(), argv_c.data());
        // execvp only returns on failure
        std::fprintf(stderr, "execvp failed: %s\n", std::strerror(errno));
        std::_Exit(127);
    }

    // parent
    ::close(out_pipe[1]);
    ::close(err_pipe[1]);

    PipeReadAll(out_pipe[0], out_result.stdout_output);
    PipeReadAll(err_pipe[0], out_result.stderr_output);

    int status = 0;
    ::waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        out_result.exit_code = WEXITSTATUS(status);
    } else {
        out_result.exit_code = -1;
    }
    return true;
}

} // namespace linux_
} // namespace platform
} // namespace ava
