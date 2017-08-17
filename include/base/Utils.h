// +-------------------------------------------------------------------------
// | Copyright (C) 2017 Yunify, Inc.
// +-------------------------------------------------------------------------
// | Licensed under the Apache License, Version 2.0 (the "License");
// | You may not use this work except in compliance with the License.
// | You may obtain a copy of the License in the LICENSE file, or at:
// |
// | http://www.apache.org/licenses/LICENSE-2.0
// |
// | Unless required by applicable law or agreed to in writing, software
// | distributed under the License is distributed on an "AS IS" BASIS,
// | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// | See the License for the specific language governing permissions and
// | limitations under the License.
// +-------------------------------------------------------------------------

#ifndef _QSFS_FUSE_INCLUDE_BASE_UTILS_H_  // NOLINT
#define _QSFS_FUSE_INCLUDE_BASE_UTILS_H_  // NOLINT


#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <utility>


namespace QS {

namespace Utils {

bool CreateDirectoryIfNotExistsNoLog(const std::string &path);
bool CreateDirectoryIfNotExists(const std::string &path);

bool RemoveDirectoryIfExistsNoLog(const std::string &path);
bool RemoveDirectoryIfExists(const std::string &path);

bool RemoveFileIfExistsNoLog(const std::string &path);
bool RemoveFileIfExists(const std::string &path);

std::pair<bool, std::string> DeleteFilesInDirectoryNoLog(
    const std::string &path, bool deleteDirectorySelf);
bool DeleteFilesInDirectory(const std::string &path, bool deleteDirectorySelf);

bool FileExists(const std::string &path);
bool IsDirectory(const std::string &path);
bool IsRootDirectory(const std::string &path);

std::string AppendPathDelim(const std::string &path);
std::string GetPathDelimiter();
std::string GetDirName(const std::string &path);
std::string GetBaseName(const std::string &path);

// Return true and parent directory if success,
// return false and message if fail.
std::pair<bool, std::string> GetParentDirectory(const std::string &path);

bool IsDirectoryEmpty(const std::string &dir);

std::string GetUserName(uid_t uid, bool logOn);
// Is given uid included in group of gid.
bool IsIncludedInGroup(uid_t uid, gid_t gid, bool logOn);

uid_t GetProcessEffectiveUserID();
gid_t GetProcessEffectiveGroupID();

bool HavePermission(struct stat *st, bool logOn);
bool HavePermission(const std::string &path, bool logOn);

std::string AccessModeToString(int amode, bool logOn = true);

}  // namespace Utils
}  // namespace QS

// NOLINTNEXTLINE
#endif  // _QSFS_FUSE_INCLUDE_BASE_UTILS_H_
