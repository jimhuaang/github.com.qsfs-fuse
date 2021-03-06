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

#ifndef INCLUDE_DATA_FILEMETADATAMANAGER_H_
#define INCLUDE_DATA_FILEMETADATAMANAGER_H_

#include <assert.h>

#include <list>
#include <memory>
#include <mutex>  // NOLINT
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


#include "base/HashUtils.h"
#include "data/FileMetaData.h"

namespace QS {

namespace Data {

class Entry;
class Node;

using FileIdToMetaDataPair =
    std::pair<std::string, std::shared_ptr<FileMetaData>>;
using MetaDataList = std::list<FileIdToMetaDataPair>;
using MetaDataListIterator = MetaDataList::iterator;
using MetaDataListConstIterator = MetaDataList::const_iterator;
using FileIdToMetaDataListIteratorMap =
    std::unordered_map<std::string, MetaDataListIterator,
                       HashUtils::StringHash>;

class FileMetaDataManager {
 public:
  FileMetaDataManager(FileMetaDataManager &&) = delete;
  FileMetaDataManager(const FileMetaDataManager &) = delete;
  FileMetaDataManager &operator=(FileMetaDataManager &&) = delete;
  FileMetaDataManager &operator=(const FileMetaDataManager &) = delete;
  ~FileMetaDataManager() = default;

 public:
  static FileMetaDataManager &Instance();
  size_t GetMaxCount() const { return m_maxCount; }

 public:
  // Get file meta data
  MetaDataListConstIterator Get(const std::string &filePath) const;

  // Return begin of meta data list
  MetaDataListConstIterator Begin() const;

  // Return end of meta data list
  MetaDataListConstIterator End() const;

  // Has file meta data
  bool Has(const std::string &filePath) const;

  // If the meta data list size plus needCount surpass
  // MaxFileMetaDataCount then there is no available space
  bool HasFreeSpace(size_t needCount) const;

 private:
  // Get file meta data
  MetaDataListIterator Get(const std::string &filePath);

  // Return begin of meta data list
  MetaDataListIterator Begin();

  // Return end of meta data list
  MetaDataListIterator End();

  // Add file meta data
  // Return the iterator to the new added meta (should be the begin)
  // if add sucessfully, otherwise return the end iterator.
  // Notes: if file meta already existed, this will update its meta data.
  MetaDataListIterator Add(std::shared_ptr<FileMetaData> &&fileMetaData);

  // Add file meta data array
  // Return the iterator to the new added meta (should be the begin)
  // if sucessfully, otherwise return the end iterator
  // Notes: to obey 'the most recently used meta is always put at front',
  // the sequence of the input array will be reversed.
  MetaDataListIterator Add(
      std::vector<std::shared_ptr<FileMetaData>> &&fileMetaDatas);

  // Remove file meta data
  MetaDataListIterator Erase(const std::string &filePath);

  // Remvoe all file meta datas
  void Clear();

  // Rename
  void Rename(const std::string &oldFilePath, const std::string &newFilePath);

 private:
  // internal use only
  MetaDataListIterator UnguardedMakeMetaDataMostRecentlyUsed(
      MetaDataListConstIterator pos);
  bool HasFreeSpaceNoLock(size_t needCount) const;
  bool FreeNoLock(size_t needCount, const std::string fileUnfreeable);
  MetaDataListIterator AddNoLock(std::shared_ptr<FileMetaData> &&fileMetaData);

 private:
  explicit FileMetaDataManager(size_t maxCount = 0);

  // Most recently used meta data is put at front,
  // Least recently used meta data in put at back.
  MetaDataList m_metaDatas;
  FileIdToMetaDataListIteratorMap m_map;
  size_t m_maxCount;  // max count of meta datas

  mutable std::recursive_mutex m_mutex;

  friend class QS::Data::Entry;
  friend class QS::Data::Node;
  friend class FileMetaDataManagerTest;
};

}  // namespace Data
}  // namespace QS


#endif  // INCLUDE_DATA_FILEMETADATAMANAGER_H_
