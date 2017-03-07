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

#include <set>
#include "MemoryProvider.h"

class CDVDCodecOptions;

class CMemoryBroker
{
public:
  static CMemoryBroker &getInstance();
  virtual ~CMemoryBroker();

  /**
  * Registeres a new provider
  */
  void RegisterProvider(std::shared_ptr<CMemoryProvider> provider);

  /*
  * Unregisteres a previous registered provider
  */
  void UnRegisterProvider(std::shared_ptr<CMemoryProvider> provider);

  /*
  * Aquires a new memory provider
  */
  std::shared_ptr<CMemoryProvider> AquireMemoryProvider(const CDVDCodecOptions &options);
private:
  CMemoryBroker();
  std::set<std::shared_ptr<CMemoryProvider> > memoryProvider;
};
