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

#ifndef INCLUDE_CLIENT_QSCLIENTCONVERTER_H_
#define INCLUDE_CLIENT_QSCLIENTCONVERTER_H_

#include <sys/statvfs.h>

#include <memory>
#include <string>
#include <vector>

#include "qingstor/Bucket.h"
#include "qingstor/types/KeyType.h"

#include "data/Directory.h"

namespace QS {

namespace Client {

namespace QSClientConverter {

void GetBucketStatisticsOutputToStatvfs(
    const QingStor::GetBucketStatisticsOutput &bucketStatsOutput,
    struct statvfs *statv);

std::shared_ptr<QS::Data::FileMetaData> HeadObjectOutputToFileMetaData(
    const std::string &objKey, const QingStor::HeadObjectOutput &headObjOutput);

std::shared_ptr<QS::Data::FileMetaData> ObjectKeyToFileMetaData(
    const KeyType &key, time_t atime);

// As sdk also provide dir itself in keys when list object of the dir path,
// ObjectKeyToDirMetaData serve to such special case only.
std::shared_ptr<QS::Data::FileMetaData> ObjectKeyToDirMetaData(
    const KeyType &key, time_t atime);

std::shared_ptr<QS::Data::FileMetaData> CommonPrefixToFileMetaData(
    const std::string &commonPrefix, time_t atime);

std::vector<std::shared_ptr<QS::Data::FileMetaData>>
ListObjectsOutputToFileMetaDatas(
    const QingStor::ListObjectsOutput &listObjsOutput, bool addSelf);

}  // namespace QSClientConverter
}  // namespace Client
}  // namespace QS


#endif  // INCLUDE_CLIENT_QSCLIENTCONVERTER_H_
