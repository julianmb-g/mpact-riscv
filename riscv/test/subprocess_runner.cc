#include "riscv/test/subprocess_runner.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace mpact {
namespace sim {
namespace riscv {
namespace test {

namespace {
constexpr size_t kBufferSize = 4096;
constexpr int kPollTimeoutMs = 5000;

std::mutex g_pid_mutex;
std::set<pid_t> g_active_pids;

void CleanupActiveSubprocesses() {
  std::lock_guard<std::mutex> lock(g_pid_mutex);
  for (pid_t pid : g_active_pids) {
    killpg(pid, SIGKILL);
  }
}

class AtexitRegistrar {
 public:
  AtexitRegistrar() {
    std::atexit(CleanupActiveSubprocesses);
  }
};

AtexitRegistrar g_registrar;
}  // namespace

SubprocessRunner::SubprocessRunner(const std::string& executable_path, const std::vector<std::string>& args)
    : executable_path_(executable_path), args_(args) {}

SubprocessRunner::~SubprocessRunner() {}

bool SubprocessRunner::SetupPipes(int pipe_in[2], int pipe_out[2]) {
  if (pipe(pipe_in) != 0) {
    return false;
  }
  if (pipe(pipe_out) != 0) {
    close(pipe_in[0]);
    close(pipe_in[1]);
    return false;
  }
  return true;
}

pid_t SubprocessRunner::SpawnChildProcess(int pipe_in[2], int pipe_out[2]) {
  pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "SubprocessRunner: Catastrophic fork failure: " << std::strerror(errno) << std::endl;
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);
    return -1;
  }

  if (pid == 0) {
    // Child process
    // Put child in its own process group to allow killing entire process tree
    setpgid(0, 0);
    // Ensure the child dies if the parent dies to prevent IPC zombie deadlocks
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    
    dup2(pipe_in[0], STDIN_FILENO);
    dup2(pipe_out[1], STDOUT_FILENO);
    dup2(pipe_out[1], STDERR_FILENO);
    close(pipe_in[0]);
    close(pipe_in[1]);
    close(pipe_out[0]);
    close(pipe_out[1]);

    std::vector<const char*> argv;
    argv.push_back(executable_path_.c_str());
    for (const auto& arg : args_) {
      argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    execv(executable_path_.c_str(), const_cast<char* const*>(argv.data()));
    _exit(1);
  }

  // Parent process
  setpgid(pid, pid);
  {
    std::lock_guard<std::mutex> lock(g_pid_mutex);
    g_active_pids.insert(pid);
  }

  close(pipe_in[0]);
  close(pipe_out[1]);

  int flags = fcntl(pipe_in[1], F_GETFL, 0);
  fcntl(pipe_in[1], F_SETFL, flags | O_NONBLOCK);

  return pid;
}

void SubprocessRunner::PollBidirectionalIO(pid_t pid, int pipe_in_fd, int pipe_out_fd, const std::string& input, std::string* output) {
  if (output != nullptr) {
    output->clear();
  }
  
  struct pollfd pfds[2];
  pfds[0].fd = pipe_out_fd;
  pfds[0].events = POLLIN;
  pfds[1].fd = pipe_in_fd;
  pfds[1].events = POLLOUT;

  size_t written = 0;
  bool write_closed = false;

  if (input.empty()) {
    close(pipe_in_fd);
    write_closed = true;
    pfds[1].fd = -1;
  }

  char buffer[kBufferSize];

  while (true) {
    int ret = poll(pfds, 2, kPollTimeoutMs);
    if (ret <= 0) {
      killpg(pid, SIGKILL);
      break;
    }

    if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
      ssize_t bytes_read = read(pipe_out_fd, buffer, sizeof(buffer) - 1);
      if (bytes_read > 0) {
        if (output != nullptr) {
          buffer[bytes_read] = '\0';
          *output += buffer;
        }
      } else if (bytes_read == 0) {
        break; // EOF
      } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        break; // Error
      }
    }

    if (!write_closed && (pfds[1].revents & POLLOUT)) {
      if (written < input.length()) {
        ssize_t w = write(pipe_in_fd, input.c_str() + written, input.length() - written);
        if (w > 0) {
          written += w;
        } else if (w < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
          close(pipe_in_fd);
          write_closed = true;
          pfds[1].fd = -1;
        }
      }
      
      if (written >= input.length()) {
        close(pipe_in_fd);
        write_closed = true;
        pfds[1].fd = -1;
      }
    }
  }

  if (!write_closed) {
    close(pipe_in_fd);
  }
  close(pipe_out_fd);
}

int SubprocessRunner::RunWithInput(const std::string& input, std::string* output) {
  int pipe_in[2];
  int pipe_out[2];
  
  if (!SetupPipes(pipe_in, pipe_out)) {
    std::cerr << "SubprocessRunner: Catastrophic pipe creation failure: " << std::strerror(errno) << std::endl;
    return -1;
  }

  pid_t pid = SpawnChildProcess(pipe_in, pipe_out);
  if (pid < 0) {
    std::cerr << "SubprocessRunner: Catastrophic fork failure: " << std::strerror(errno) << std::endl;
    return -1;
  }

  PollBidirectionalIO(pid, pipe_in[1], pipe_out[0], input, output);

  int status;
  waitpid(pid, &status, 0);

  {
    std::lock_guard<std::mutex> lock(g_pid_mutex);
    g_active_pids.erase(pid);
  }

  return status;
}

}  // namespace test
}  // namespace riscv
}  // namespace sim
}  // namespace mpact