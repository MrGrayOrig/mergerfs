/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_base_utime.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

#include <fcntl.h>

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::Config;
using namespace mergerfs;

namespace local
{
  static
  int
  utimens_loop_core(const string   *basepath_,
                    const char     *fusepath_,
                    const timespec  ts_[2],
                    const int       error_)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    rv = fs::lutimens(fullpath,ts_);

    return error::calc(rv,error_,errno);
  }

  static
  int
  utimens_loop(const vector<const string*> &basepaths_,
               const char                  *fusepath_,
               const timespec               ts_[2])
  {
    int error;

    error = -1;
    for(size_t i = 0, ei = basepaths_.size(); i != ei; i++)
      {
        error = local::utimens_loop_core(basepaths_[i],fusepath_,ts_,error);
      }

    return -error;
  }

  static
  int
  utimens(Policy::Func::Action  actionFunc_,
          const Branches       &branches_,
          const uint64_t        minfreespace_,
          const char           *fusepath_,
          const timespec        ts_[2])
  {
    int rv;
    vector<const string*> basepaths;

    rv = actionFunc_(branches_,fusepath_,minfreespace_,basepaths);
    if(rv == -1)
      return -errno;

    return local::utimens_loop(basepaths,fusepath_,ts_);
  }

  static
  inline
  int
  utimens(const char     *fusepath_,
          const timespec  ts_[2])
  {
    const fuse_context      *fc     = fuse_get_context();
    const Config            &config = Config::get(fc);
    const ugid::Set          ugid(fc->uid,fc->gid);
    const rwlock::ReadGuard  readlock(&config.branches_lock);

    return local::utimens(config.utimens,
                          config.branches,
                          config.minfreespace,
                          fusepath_,
                          ts_);
  }

  static
  int
  futimens(const fuse_file_info *ffi_,
           const timespec        ts_[2])
  {
    int rv;
    FileInfo *fi;

    fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    rv = fs::futimens(fi->fd,ts_);

    return ((rv == -1) ? -errno : 0);
  }
}

namespace mergerfs
{
  namespace fuse
  {
    int
    utimens(const char     *fusepath_,
            const timespec  ts_[2],
            fuse_file_info *ffi_)
    {
      if(ffi_ != NULL)
        return local::futimens(ffi_,ts_);

      return local::utimens(fusepath_,ts_);
    }
  }
}
