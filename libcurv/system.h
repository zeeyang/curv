// Copyright 2016-2018 Doug Moen
// Licensed under the Apache License, version 2.0
// See accompanying file LICENSE or https://www.apache.org/licenses/LICENSE-2.0

#ifndef LIBCURV_SYSTEM_H
#define LIBCURV_SYSTEM_H

#include <ostream>
#include <unordered_set>
#include <libcurv/filesystem.h>
#include <libcurv/builtin.h>

namespace curv {

/// An abstract interface to the client and operating system.
///
/// The System object is owned by the client, who is responsible for ensuring
/// that it exists for as long as references to it might exist in Curv
/// data structures.
struct System
{
    /// This is the set of standard or builtin bindings
    /// used by the `file` primitive to interpret Curv source files.
    virtual const Namespace& std_namespace() = 0;
    virtual std::ostream& console() = 0;
    // This is non-empty while a `file` operation is being evaluated.
    // It is used to detect recursive file references.
    // Later, this may change to a file cache.
    std::unordered_set<Filesystem::path,Path_Hash> active_files_{};
};

/// Default implementation of the System interface.
struct System_Impl : public System
{
    Namespace std_namespace_;
    std::ostream& console_;
    System_Impl(std::ostream&);
    void load_library(Shared<const String> path);
    virtual const Namespace& std_namespace() override;
    virtual std::ostream& console() override;
};

} // namespace curv
#endif // header guard
