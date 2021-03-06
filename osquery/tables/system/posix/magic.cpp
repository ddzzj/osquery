/**
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed as defined on the LICENSE file found in the
 *  root directory of this source tree.
 */

#include <magic.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>
#include <numeric>

#include <osquery/logger.h>
#include <osquery/tables.h>

namespace osquery {
namespace tables {

namespace {

constexpr std::array<const char*, 3> kMagicFiles = {
    "/usr/share/misc/magic",
    "/usr/share/misc/magic.mgc",
    "/usr/share/file/magic/magic"};

constexpr char* kMagicFileDBSep = ":";
} // namespace

QueryData genMagicData(QueryContext& context) {
  QueryData results;
  magic_t magic_cookie = nullptr;

  // No default flags
  magic_cookie = magic_open(MAGIC_NONE);

  if (magic_cookie == nullptr) {
    VLOG(1) << "Unable to initialize magic library";
    return results;
  }

  std::string magic_db_files;

  if (context.hasConstraint("magic_db_files")) {
    auto magic_files = context.constraints["magic_db_files"].getAll(EQUALS);
    magic_db_files = boost::algorithm::join(magic_files, kMagicFileDBSep);
  } else {
    for (auto& file : kMagicFiles) {
      if (boost::filesystem::exists(file)) {
        magic_db_files = file;
        break;
      }
    }
  }

  if (magic_db_files.empty()) {
    VLOG(1) << "Will be loading magic with the default database ";
    // because of how we compile this, unless you are a developer the default
    // one will never work
    if (magic_load(magic_cookie, nullptr) != 0) {
      LOG(WARNING) << "Unable to load magic default database"
                   << " because: " << magic_error(magic_cookie);
      magic_close(magic_cookie);
      return results;
    }
  } else {
    // because of how we compile this, unless you are a developer the default
    // one will never work
    if (magic_load(magic_cookie, magic_db_files.c_str()) != 0) {
      LOG(WARNING) << "Unable to load magic list of database: "
                   << magic_db_files
                   << " because: " << magic_error(magic_cookie);
      magic_close(magic_cookie);
      return results;
    }
  }

  // Iterate through all the provided paths
  auto paths = context.constraints["path"].getAll(EQUALS);
  for (const auto& path_string : paths) {
    Row r;
    r["path"] = path_string;
    r["magic_db_files"] = magic_db_files;
    r["data"] = magic_file(magic_cookie, path_string.c_str());

    // Retrieve MIME type
    magic_setflags(magic_cookie, MAGIC_MIME_TYPE);
    r["mime_type"] = magic_file(magic_cookie, path_string.c_str());

    // Retrieve MIME encoding
    magic_setflags(magic_cookie, MAGIC_MIME_ENCODING);
    r["mime_encoding"] = magic_file(magic_cookie, path_string.c_str());

    results.push_back(r);
  }

  magic_close(magic_cookie);
  return results;
}
}
}
