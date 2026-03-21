#ifndef MPACT_RISCV_RISCV_TEST_SUBPROCESS_RUNNER_H_
#define MPACT_RISCV_RISCV_TEST_SUBPROCESS_RUNNER_H_

#include <sys/types.h>

#include <string>
#include <vector>

namespace mpact {
namespace sim {
namespace riscv {
namespace test {

class SubprocessRunner {
 public:
  SubprocessRunner(const std::string& executable_path, const std::vector<std::string>& args);
  ~SubprocessRunner();

  // Runs the subprocess, sends `input` to stdin, and captures stdout/stderr into `output`.
  // Returns the exit status of the subprocess (the raw waitpid status).
  int RunWithInput(const std::string& input, std::string* output);

 private:
  bool SetupPipes(int pipe_in[2], int pipe_out[2]);
  pid_t SpawnChildProcess(int pipe_in[2], int pipe_out[2]);
  void PollBidirectionalIO(pid_t pid, int pipe_in_fd, int pipe_out_fd, const std::string& input, std::string* output);

  std::string executable_path_;
  std::vector<std::string> args_;
};

}  // namespace test
}  // namespace riscv
}  // namespace sim
}  // namespace mpact

#endif  // MPACT_RISCV_RISCV_TEST_SUBPROCESS_RUNNER_H_
