// SPDX-FileCopyrightText: (c) 2026 Julian Duwe
// SPDX-License-Identifier: Apache-2.0

#ifndef JLD_MCC_DIAGNOSTIC_SOURCE_MANAGER_H
#define JLD_MCC_DIAGNOSTIC_SOURCE_MANAGER_H

#include <cstddef>
#include <fstream>
#include <ios>
#include <iosfwd>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "source.h"

struct SourceFile {
  std::string     path;
  std::span<char> source;
};

class SourceManager {
 public:
  SourceManager() {
    m_sourceFiles_.resize(10);
  }
  ~SourceManager() {
    for (const SourceFile& file : m_sourceFiles_) delete[] file.source.data();
  }

  SourceManager(const SourceManager&) = delete;
  SourceManager(SourceManager&&) = delete;
  auto operator=(const SourceManager&) -> SourceManager& = delete;
  auto operator=(SourceManager&&) -> SourceManager& = delete;

  auto open(std::string path) -> FileId {
    std::fstream f{ path, std::ios::in };
    if (!f.is_open()) {
      // ERROR
      return 0;
    }

    f.seekg(0, std::ios::end);
    size_t size{ static_cast<size_t>(f.tellg()) + 1 };

    char* data{ new char[size] };
    data[size - 1] = '\0';

    f.seekg(0, std::ios::beg);
    f.read(data, static_cast<std::streamsize>(size - 1));

    FileId id{ m_nextId_++ };
    m_sourceFiles_[id] = SourceFile{
      .path = std::move(path),
      .source = std::span<char>{ data, size },
    };
    return id;
  }

  [[nodiscard]] auto query(FileId id) const -> const SourceFile& {
    if (id >= m_nextId_) {
      // ERROR
    }
    return m_sourceFiles_[id];
  }

 private:
  FileId                  m_nextId_{ 1 };
  std::vector<SourceFile> m_sourceFiles_;
};

#endif  // JLD_MCC_DIAGNOSTIC_SOURCE_MANAGER_H