#pragma once

/*
 *      Copyright (C) 2017 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "DVDCodecs/DVDCodecs.h"

class CMemoryProvider
{
public:

  CMemoryProvider() {}
  virtual ~CMemoryProvider() {}

  /**
   * Return if the requested memorytype is supported
   */
  virtual bool Supports(const CDVDCodecOptions &options) = 0;

  /**
   * Get a block of Memory
   */
  virtual void *GetMemoryPointer(size_t sz) = 0;

  /**
   * Release a block of memory
   */
  virtual void ReleaseMemoryPointer(void *mem) = 0;

  /**
  * Get a native handle to the buffer
  */
  virtual int64_t GetNativeMemoryHandle(size_t sz) { return -1; };

  /**
  * Release a block of memory
  */
  virtual void ReleaseNativeMemoryHandle(int64_t mem) {};

  /**
  * Finalise a block of memory
  */
  virtual void Finalise(void *mem) {}
};
