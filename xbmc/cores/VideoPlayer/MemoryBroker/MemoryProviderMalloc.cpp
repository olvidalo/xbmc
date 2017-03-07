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

#include "MemoryProviderMalloc.h"

CMemoryProviderMalloc::~CMemoryProviderMalloc()
{
  for (BUFFER &buf : freeBuffer)
    free(buf.mem);
  for (BUFFER &buf : usedBuffer)
    free(buf.mem);
}

bool CMemoryProviderMalloc::Supports(const CDVDCodecOptions &options)
{
  for (auto fmt : options.m_formats)
    if (fmt == RENDER_FMT_YUV420P)
      return true;
}

void *CMemoryProviderMalloc::GetMemoryPointer(size_t sz)
{
  if (freeBuffer.empty())
    freeBuffer.resize(1);

  BUFFER &buf(freeBuffer.back());
  if (buf.memSize < sz)
  {
    buf.memSize = sz;
    buf.mem = malloc(buf.memSize);
    if (buf.mem == nullptr)
    {
      buf.memSize = 0;
      return nullptr;
    }
  }
  usedBuffer.push_back(buf);
  freeBuffer.pop_back();
  return buf.mem;
}

void CMemoryProviderMalloc::ReleaseMemoryPointer(void *mem)
{
  std::vector<BUFFER>::iterator res(std::find(usedBuffer.begin(), usedBuffer.end(), mem));
  if (res == usedBuffer.end())
    return;
  freeBuffer.push_back(*res);
  usedBuffer.erase(res);
}
