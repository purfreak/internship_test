/*MIT License

Copyright (c) 2021 Nataliya Sakovets

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

void FatalErrno(const char* description) {
  std::perror(description);
  std::exit(1);
}

uid_t GetUserID(const char* name);
gid_t GetGroupID(const char* name);

bool IsDirectoryResolvable(fs::path dir, uid_t uid, gid_t gid);
void FindWritableFiles(const fs::path& path, uid_t uid, gid_t gid);

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: my_checker USER GROUP PATH\n";
    return 1;
  }

  auto uid = GetUserID(argv[1]);
  auto gid = GetGroupID(argv[2]);

  auto dir = fs::canonical(argv[3]);
  if (!fs::is_directory(dir)) {
    std::cerr << dir << " is not a directory\n";
    return 1;
  }

  if (IsDirectoryResolvable(dir, uid, gid)) {
    FindWritableFiles(dir, uid, gid);
  }
}

mode_t GetPermBits(const fs::path& path, uid_t uid, gid_t gid) {
  struct stat st;
  if (lstat(path.c_str(), &st)) FatalErrno("lstat");

  if (st.st_uid == uid) {
    return (st.st_mode & S_IRWXU) >> 6;
  } else if (st.st_gid == gid) {
    return (st.st_mode & S_IRWXG) >> 3;
  } else {
    return st.st_mode & S_IRWXO;
  }
}

bool IsDirectoryResolvable(fs::path dir, uid_t uid, gid_t gid) {
  while (dir.has_relative_path()) {
    std::cout << dir << std::endl;
    if (!(GetPermBits(dir, uid, gid) & 01)) {
      return false;
    }
    dir = dir.parent_path();
  }
  return GetPermBits(dir, uid, gid) & 01;
}

void FindWritableFiles(const fs::path& path, uid_t uid, gid_t gid) {
  for (auto& entry : fs::directory_iterator(path)) {
    if (entry.is_symlink()) {
      continue;
    }

    if (!path.has_relative_path() && (entry.path().filename() == "sys" || entry.path().filename() == "proc")) {
      continue;
    }

    auto perms = GetPermBits(entry, uid, gid);
    if (perms & 02) {
      std::cout << entry.path().string() << '\n';
    }

    if (entry.is_directory() && perms & 01) {
      FindWritableFiles(entry.path(), uid, gid);
    }
  }
}

uid_t GetUserID(const char* name) {
  errno = 0;
  auto record = getpwnam(name);
  if (record != nullptr) {
    return record->pw_uid;
  } else if (errno == 0) {
    std::cerr << "user " << name << " does not exist\n";
    std::exit(1);
  } else {
    FatalErrno("can not get user record");
  }
  return 0;
}

gid_t GetGroupID(const char* name) {
  errno = 0;
  auto record = getgrnam(name);
  if (record != nullptr) {
    return record->gr_gid;
  } else if (errno == 0) {
    std::cerr << "group " << name << " does not exist\n";
    std::exit(1);
  } else {
    FatalErrno("can not get group record");
  }
  return 0;
}
