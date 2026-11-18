#pragma once

#ifdef PHEROMONE_MEMORY_TRACE
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <unistd.h>

namespace memory_trace {

// Owner addresses identify lifetimes within one process. A shared object's
// device and inode identify the same backing across all processes on the worker.
inline void event(const char* kind, const std::string& session, const std::string& name,
                  const void* owner, std::size_t bytes, uintmax_t device = 0, uintmax_t inode = 0) {
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  static std::ofstream out = [] {
    std::ofstream file("memory-events-" + std::to_string(getpid()) + ".csv");
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << "time_us,event,session,name,owner,bytes,device,inode\n";
    return file;
  }();
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  out << std::chrono::duration_cast<std::chrono::microseconds>(now).count() << ',' << kind
      << ',' << std::quoted(session, '"', '"') << ',' << std::quoted(name, '"', '"')
      << ',' << reinterpret_cast<uintptr_t>(owner) << ',' << bytes
      << ',' << device << ',' << inode << std::endl;
}

inline std::size_t argument_bytes(const std::vector<std::string>& args, std::size_t first = 0) {
  std::size_t bytes = 0;
  for (auto i = first; i < args.size(); ++i) bytes += args[i].size();
  return bytes;
}

// Records the payload portion of a buffer held across a handler or send.
struct Copy {
  Copy(const char* kind, const std::string& session, const std::string& name,
       const void* owner, std::size_t bytes) : session(session), name(name), owner(bytes ? owner : nullptr) {
    if (this->owner) event(kind, session, name, owner, bytes);
  }
  ~Copy() {
    if (owner) event("release", session, name, owner, 0);
  }
  Copy(const Copy&) = delete;
  Copy& operator=(const Copy&) = delete;
private:
  const std::string& session;
  const std::string& name;
  const void* owner;
};

} // namespace memory_trace
#define TRACE_MEMORY(...) ::memory_trace::event(__VA_ARGS__)
#define TRACE_COPY(variable, ...) ::memory_trace::Copy variable(__VA_ARGS__)
#else
#define TRACE_COPY(variable, ...) ((void)0)
#define TRACE_MEMORY(...) ((void)0)
#endif
