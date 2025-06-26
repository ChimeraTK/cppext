// SPDX-FileCopyrightText: Deutsches Elektronen-Synchrotron DESY, MSK, ChimeraTK Project <chimeratk-support@desy.de>
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <pthread.h>

#include <string>

namespace cppext {

  /**
   * Set name of the current thread.
   *
   * @note: This function contains platform-dependent code and may need adjustment for new platforms. On unsupported
   * platforms, this function does nothing.
   */
  inline void setThreadName(const std::string& name) {
#if defined(__linux__)
    pthread_setname_np(pthread_self(), name.substr(0, std::min<std::string::size_type>(name.length(), 15)).c_str());
#elif defined(__APPLE__)
    pthread_setname_np(name.substr(0, std::min<std::string::size_type>(name.length(), 15)).c_str());
#endif
  }

} // namespace cppext
