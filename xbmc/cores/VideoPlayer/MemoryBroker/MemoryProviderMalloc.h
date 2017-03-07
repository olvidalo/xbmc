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

#include "MemoryProvider.h"

class CMemoryProviderMalloc : public CMemoryProvider
{
public:

  virtual ~CMemoryProviderMalloc();

  virtual bool Supports(const CDVDCodecOptions &options) override;
  virtual void *GetMemoryPointer(size_t sz) override;
  virtual void ReleaseMemoryPointer(void *mem) override;

private:

  struct BUFFER
  {
    BUFFER() :mem(nullptr), memSize(0) {};
    BUFFER(void* m, size_t s) :mem(m), memSize(s) {};
    bool operator == (const void* data) const { return mem == data; };

    void *mem;
    size_t memSize;
  };

  std::vector<BUFFER> freeBuffer, usedBuffer;
};
