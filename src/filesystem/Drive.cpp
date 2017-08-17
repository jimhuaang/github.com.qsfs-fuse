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

#include "filesystem/Drive.h"

#include <time.h>

#include <sys/types.h>

#include <future>  // NOLINT
#include <memory>
#include <mutex>  // NOLINT
#include <utility>

#include "base/Exception.h"
#include "base/LogMacros.h"
#include "base/Utils.h"
#include "client/Client.h"
#include "client/ClientError.h"
#include "client/ClientFactory.h"
#include "client/QSError.h"
#include "client/TransferManager.h"
#include "client/TransferManagerFactory.h"
#include "data/Cache.h"
#include "data/Directory.h"
#include "filesystem/Configure.h"

namespace QS {

namespace FileSystem {

using QS::Client::Client;
using QS::Client::ClientError;
using QS::Client::ClientFactory;
using QS::Client::GetMessageForQSError;
using QS::Client::IsGoodQSError;
using QS::Client::QSError;
using QS::Client::TransferManager;
using QS::Client::TransferManagerConfigure;
using QS::Client::TransferManagerFactory;
using QS::Data::Cache;
using QS::Data::ChildrenMultiMapConstIterator;
using QS::Data::DirectoryTree;
using QS::Data::FileMetaData;
using QS::Data::FilePathToNodeUnorderedMap;
using QS::Data::Node;
using QS::Exception::QSException;
using QS::Utils::AppendPathDelim;
using QS::Utils::GetDirName;
using QS::Utils::GetProcessEffectiveUserID;
using QS::Utils::GetProcessEffectiveGroupID;
using QS::Utils::IsRootDirectory;
using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::weak_ptr;

static std::unique_ptr<Drive> instance(nullptr);
static std::once_flag flag;

// --------------------------------------------------------------------------
Drive &Drive::Instance() {
  std::call_once(flag, [] { instance.reset(new Drive); });
  return *instance.get();
}

// --------------------------------------------------------------------------
Drive::Drive()
    : m_mountable(true),
      m_client(ClientFactory::Instance().MakeClient()),
      m_transferManager(std::move(
          TransferManagerFactory::Create(TransferManagerConfigure()))),
      m_cache(std::move(unique_ptr<Cache>(new Cache))) {
  uid_t uid = GetProcessEffectiveUserID();
  gid_t gid = GetProcessEffectiveGroupID();

  m_directoryTree = unique_ptr<DirectoryTree>(
      new DirectoryTree(time(NULL), uid, gid, Configure::GetRootMode()));
}

// --------------------------------------------------------------------------
void Drive::SetClient(shared_ptr<Client> client) { m_client = client; }

// --------------------------------------------------------------------------
void Drive::SetTransferManager(unique_ptr<TransferManager> transferManager) {
  m_transferManager = std::move(transferManager);
}

// --------------------------------------------------------------------------
void Drive::SetCache(unique_ptr<Cache> cache) { m_cache = std::move(cache); }

// --------------------------------------------------------------------------
void Drive::SetDirectoryTree(unique_ptr<DirectoryTree> dirTree) {
  m_directoryTree = std::move(dirTree);
}

// --------------------------------------------------------------------------
bool Drive::IsMountable() const {
  m_mountable.store(GetClient()->Connect());
  return m_mountable.load();
}

// --------------------------------------------------------------------------
shared_ptr<Node> Drive::GetRoot() {
  bool connectSuccess = GetClient()->Connect();
  if (!connectSuccess) {
    throw QSException("Unable to connect to object storage bucket");
  }
  return m_directoryTree->GetRoot();
}

// --------------------------------------------------------------------------
struct statvfs Drive::GetFilesystemStatistics() {
  struct statvfs statv;
  auto err = GetClient()->Statvfs(&statv);
  DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
  return statv;
}

// --------------------------------------------------------------------------
pair<weak_ptr<Node>, bool> Drive::GetNode(const string &path,
                                          bool updateIfDirectory) {
  if (path.empty()) {
    Error("Null file path");
    return {weak_ptr<Node>(), false};
  }
  auto node = m_directoryTree->Find(path).lock();
  bool modified = false;

  auto UpdateNode = [this, &modified](const string &path,
                                      const shared_ptr<Node> &node) {
    time_t modifiedSince = 0;
    modifiedSince = const_cast<const Node &>(*node).GetEntry().GetMTime();
    auto err = GetClient()->Stat(path, modifiedSince, &modified);
    DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
  };

  if (node) {
    UpdateNode(path, node);
  } else {
    auto err = GetClient()->Stat(path, 0, &modified);  // head it
    if (IsGoodQSError(err)) {
      node = m_directoryTree->Find(path).lock();
    } else {
      DebugError(GetMessageForQSError(err));
    }
  }

  // Update directory tree asynchornizely.
  if (node->IsDirectory() && updateIfDirectory && modified) {
    auto receivedHandler = [](const ClientError<QSError> &err) {
      DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
    };
    GetClient()->GetExecutor()->SubmitAsync(receivedHandler, [this, path] {
      return GetClient()->ListDirectory(AppendPathDelim(path));
    });
  }

  return {node, modified};
}

// --------------------------------------------------------------------------
pair<ChildrenMultiMapConstIterator, ChildrenMultiMapConstIterator>
Drive::GetChildren(const string &dirPath) {
  auto emptyRes = std::make_pair(m_directoryTree->CEndParentToChildrenMap(),
                                 m_directoryTree->CEndParentToChildrenMap());
  if (dirPath.empty()) {
    Error("Null dir path");
    return emptyRes;
  }

  auto path = dirPath;
  if (dirPath.back() != '/') {
    path = AppendPathDelim(dirPath);
    DebugInfo("Input dir path not ending with '/', append it");
  }

  auto res = GetNode(path, false);  // Do not invoke updating dirctory
                                    // as we will do it synchronizely
  auto node = res.first.lock();
  bool modified = res.second;
  if (node) {
    if (modified || node->IsEmpty()) {
      // Update directory tree synchornizely
      auto f = GetClient()->GetExecutor()->SubmitCallablePrioritized(
          [this, path] { return GetClient()->ListDirectory(path); });
      auto err = f.get();
      DebugErrorIf(!IsGoodQSError(err), GetMessageForQSError(err));
    }
    return m_directoryTree->FindChildren(path);
  } else {
    DebugInfo("Directory is not existing for " + dirPath);
    return emptyRes;
  }
}

// --------------------------------------------------------------------------
void Drive::RenameFile(const string &filePath, const string &newFilePath) {
  if (filePath.empty() || newFilePath.empty()) {
    DebugWarning("Invalid empty parameter");
    return;
  }
  if (IsRootDirectory(filePath)) {
    DebugError("Unable to rename root");
    return;
  }
  auto oldDir = GetDirName(filePath);
  auto newDir = GetDirName(newFilePath);
  if (oldDir.empty() || newDir.empty()) {
    DebugError("Input file paths have invalid dir path");
    return;
  }
  if (oldDir != newDir) {
    DebugError("Input file paths have different dir path [old file: " +
               filePath + ", new file: " + newFilePath + "]");
    return;
  }

  auto res = GetNode(filePath, false);  // Do not invoke updating dirctory
                                        // as we are changing it
  auto node = res.first.lock();
  if (node) {
    // Check parameter
    auto newPath = newFilePath;
    bool isDir = node->IsDirectory();
    if (isDir) {
      if (newFilePath.back() != '/') {
        DebugWarning(
            "New file path is not ending with '/' for a directory, appending "
            "it");
        newPath = AppendPathDelim(newFilePath);
      }
    } else {
      if (newFilePath.back() == '/') {
        DebugWarning(
            "New file path ending with '/' for a non directory, cut it");
        newPath.pop_back();
      }
    }

    // Do Renaming
    auto err = GetClient()->RenameFile(filePath, newPath);
    if (IsGoodQSError(err)) {
      // Update meta and invoking updating directory tree asynchronizely
      res = GetNode(newPath, true);
      node = res.first.lock();
      DebugErrorIf(!node, "Fail to rename file for " + filePath);
      return;
    } else {
      DebugError(GetMessageForQSError(err));
      return;
    }
  } else {
    DebugInfo("File is not existing for " + filePath);
    return;
  }
}

}  // namespace FileSystem
}  // namespace QS
