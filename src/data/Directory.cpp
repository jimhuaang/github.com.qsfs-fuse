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

#include "data/Directory.h"

#include <cassert>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/LogMacros.h"
#include "base/HashUtils.h"
#include "data/FileMetaDataManager.h"

namespace QS {

namespace Data {

using QS::Data::FileMetaDataManager;
using QS::HashUtils::EnumHash;
using std::lock_guard;
using std::make_shared;
using std::recursive_mutex;
using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using std::weak_ptr;

static const char *const ROOT_PATH = "/";

// --------------------------------------------------------------------------
const string &GetFileTypeName(FileType fileType) {
  static unordered_map<FileType, string, EnumHash> fileTypeNames = {
      {FileType::File, "File"},
      {FileType::Directory, "Directory"},
      {FileType::SymLink, "Symbolic Link"},
      {FileType::Block, "Block"},
      {FileType::Character, "Character"},
      {FileType::FIFO, "FIFO"},
      {FileType::Socket, "Socket"}};
  return fileTypeNames[fileType];
}

// --------------------------------------------------------------------------
Entry::Entry(const std::string &filePath, uint64_t fileSize, time_t atime,
             time_t mtime, uid_t uid, gid_t gid, mode_t fileMode,
             FileType fileType, const std::string &mimeType,
             const std::string &eTag, bool encrypted, dev_t dev) {
  auto meta = make_shared<FileMetaData>(filePath, fileSize, atime, mtime, uid,
                                        gid, fileMode, fileType, mimeType, eTag,
                                        encrypted, dev);
  m_metaData = meta;
  FileMetaDataManager::Instance().Add(std::move(meta));
}

// --------------------------------------------------------------------------
Entry::Entry(std::shared_ptr<FileMetaData> &&fileMetaData)
    : m_metaData(fileMetaData) {
  FileMetaDataManager::Instance().Add(std::move(fileMetaData));
}

// --------------------------------------------------------------------------
Node::~Node() {
  if (!m_entry) return;

  m_entry.DecreaseNumLink();
  if (m_entry.GetNumLink() == 0 ||
      (m_entry.GetNumLink() <= 1 && m_entry.IsDirectory())) {
    FileMetaDataManager::Instance().Erase(GetFilePath());
  }
}

// --------------------------------------------------------------------------
shared_ptr<Node> Node::Find(const string &childFileName) const {
  auto child = m_children.find(childFileName);
  if (child != m_children.end()) {
    return child->second;
  }
  return shared_ptr<Node>(nullptr);
}

// --------------------------------------------------------------------------
const FileNameToNodeUnorderedMap &Node::GetChildren() const {
  return m_children;
}

// --------------------------------------------------------------------------
shared_ptr<Node> Node::Insert(const shared_ptr<Node> &child) {
  assert(IsDirectory());
  if (child) {
    auto res = m_children.emplace(child->GetFilePath(), child);
    if (res.second) {
      if(child->IsDirectory()){
        m_entry.IncreaseNumLink();
      }
    } else {
      DebugInfo(child->GetFilePath() +
                " is already existed, no insertion happens");
    }
  } else {
    DebugWarning("Try to insert null Node. Go on");
  }
  return child;
}

// --------------------------------------------------------------------------
void Node::Remove(const shared_ptr<Node> &child) {
  if (child) {
    bool reset = m_children.size() == 1 ? true : false;

    auto it = m_children.find(child->GetFilePath());
    if (it != m_children.end()) {
      m_children.erase(it);
      if (reset) m_children.clear();
    } else {
      DebugWarning("Try to remove Node " + child->GetFilePath() +
                   " which is not found. Go on");
    }
  } else {
    DebugWarning("Try to remove null Node. Go on")
  }
}

// --------------------------------------------------------------------------
void Node::RenameChild(const string &oldFilePath, const string &newFilePath) {
  if (oldFilePath == newFilePath) {
    DebugInfo("New file name is the same as the old one. Go on");
    return;
  }

  if (m_children.find(newFilePath) != m_children.end()) {
    DebugWarning("Cannot rename " + oldFilePath + " to " + newFilePath +
                 " which is already existed. But continue...");
    return;
  }

  auto it = m_children.find(oldFilePath);
  if (it != m_children.end()) {
    auto tmp = it->second;
    tmp->SetFilePath(newFilePath);
    auto hint = m_children.erase(it);
    m_children.emplace_hint(hint, newFilePath, tmp);
  } else {
    DebugWarning("Try to rename Node " + oldFilePath +
                 " which is not found. Go on");
  }
}

// --------------------------------------------------------------------------
weak_ptr<Node> DirectoryTree::Find(const string &filePath) const{
  lock_guard<recursive_mutex> lock(m_mutex);
  auto it = m_map.find(filePath);
  if(it != m_map.end()){
    return it->second;
  } else {
    DebugInfo("Node (" + filePath + ") is not existed in directory tree");
    return weak_ptr<Node>();
  }
}

// --------------------------------------------------------------------------
std::pair<ChildrenMultiMapConstIterator, ChildrenMultiMapConstIterator>
DirectoryTree::FindChildren(const string &dirName) const {
  return m_parentToChildrenMap.equal_range(dirName);
}

// --------------------------------------------------------------------------
ChildrenMultiMapConstIterator DirectoryTree::CBeginParentToChildrenMap() const {
  return m_parentToChildrenMap.cbegin();
}

// --------------------------------------------------------------------------
ChildrenMultiMapConstIterator DirectoryTree::CEndParentToChildrenMap() const {
  return m_parentToChildrenMap.cend();
}

// --------------------------------------------------------------------------
void DirectoryTree::Grow(shared_ptr<FileMetaData> &&fileMeta) {
  lock_guard<recursive_mutex> lock(m_mutex);
  string filePath = fileMeta->GetFilePath();

  auto node = Find(filePath).lock();
  if(node){
    node->SetEntry(Entry(std::move(fileMeta)));  // update entry
  } else {
    bool isDir = fileMeta->IsDirectory();
    auto dirName = fileMeta->MyDirName();
    node = make_shared<Node>(Entry(std::move(fileMeta)));
    m_map.emplace(filePath, node);

    // hook up with parent
    assert(!dirName.empty());
    auto it = m_map.find(dirName);
    if (it != m_map.end()) {
      if (auto parent = it->second.lock()) {
        parent->Insert(node);
        node->SetParent(parent);
      } else {
        DebugInfo("Parent Node of " + filePath +
                  " is not available at the time in directory tree");
      }
    }

    // hook up with children
    if (isDir) {
      auto range = m_parentToChildrenMap.equal_range(filePath);
      for (auto it = range.first; it != range.second; ++it) {
        if (auto child = it->second.lock()) {
          child->SetParent(node);
          node->Insert(child);
        }
      }
    }

    // record parent to children map
    m_parentToChildrenMap.emplace(dirName, node);
  }

  m_currentNode = node;
}

// --------------------------------------------------------------------------
void DirectoryTree::Grow(vector<shared_ptr<FileMetaData>> &&fileMetas) {
  lock_guard<recursive_mutex> lock(m_mutex);
  for (auto &meta : fileMetas) {
    Grow(std::move(meta));
  }
}

// --------------------------------------------------------------------------
DirectoryTree::DirectoryTree(time_t mtime, uid_t uid, gid_t gid, mode_t mode) {
  lock_guard<recursive_mutex> lock(m_mutex);
  m_root = make_shared<Node>( Entry(
      ROOT_PATH, 0, mtime, mtime, uid, gid, mode, FileType::Directory));
  m_currentNode = m_root;
  m_map.emplace(ROOT_PATH, m_root);
}

}  // namespace Data
}  // namespace QS
