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
#include "MemoryBroker.h"

#include <algorithm>

CMemoryBroker::CMemoryBroker()
{
}

CMemoryBroker::~CMemoryBroker()
{
}

void CMemoryBroker::Init()
{
  // Add the built in memory provider
  memoryProvider.insert(std::shared_ptr<CMemoryProviderMalloc>(new CMemoryProviderMalloc()));
}

void CMemoryBroker::RegisterProvider(std::shared_ptr<CMemoryProvider> provider)
{
  memoryProvider.insert(provider);
}

void CMemoryBroker::UnRegisterProvider(std::shared_ptr<CMemoryProvider> provider)
{
  memoryProvider.erase(provider);
}

std::shared_ptr<CMemoryProvider> CMemoryBroker::AquireMemoryProvider(const CDVDCodecOptions &options)
{
  for (std::set<std::shared_ptr<CMemoryProvider> >::reverse_iterator b(memoryProvider.rbegin()), e(memoryProvider.rend()); b != e; ++b)
    if ((*b)->Supports(options))
      return *b;
  return nullptr;
}
