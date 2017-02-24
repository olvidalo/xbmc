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

#include "MemoryProviderPi.h"
#include "TimingConstants.h"
#include "cores/RetroPlayer/PixelConverterRBP.h"
#include "cores/VideoPlayer/VideoRenderers/HwDecRender/MMALRenderer.h"

CMemoryProviderPi::CMemoryProviderPi()
{
  CLog::Log(LOGDEBUG, "%s: %p free:%d used:%d", __FUNCTION__, this, freeBuffer.size(), usedBuffer.size());
  m_pixelConverter.reset(new CPixelConverterRBP);
  if (!m_pixelConverter->Open(AV_PIX_FMT_YUV420P, AV_PIX_FMT_YUV420P, 128, 128))
  {
    CLog::Log(LOGERROR, "AddonVideoCodec::BufferPool failed to open pixelConverter");
    m_pixelConverter.reset();
  }
}

CMemoryProviderPi::~CMemoryProviderPi()
{
  CLog::Log(LOGDEBUG, "%s: %p free:%d used:%d", __FUNCTION__, this, freeBuffer.size(), usedBuffer.size());
  for (BUFFER &buf : usedBuffer)
    if (buf.omvb)
      buf.omvb->Release();
}

bool CMemoryProviderPi::Supports(const CDVDCodecOptions &options)
{
  for (auto fmt : options.m_formats)
    if (fmt == RENDER_FMT_MMAL)
      return true;
  return false;
}

void *CMemoryProviderPi::GetMemoryPointer(size_t sz)
{
  if (freeBuffer.empty())
    freeBuffer.resize(1);

  BUFFER &buf(freeBuffer.back());

  DVDVideoPicture* m_buf = nullptr;
  if (m_pixelConverter)
    m_buf = m_pixelConverter->AllocatePicture(0, 0, 0, 0, sz);
  MMAL::CMMALYUVBuffer *omvb = m_buf ? static_cast<MMAL::CMMALYUVBuffer*>(m_buf->MMALBuffer) : nullptr;
  if (!omvb || !omvb->gmem || !omvb->gmem->m_arm)
  {
    CLog::Log(LOGERROR, "%s: Failed to allocate picture of size %d", __FUNCTION__, sz);
    return nullptr;
  }
  buf.mem = omvb->gmem->m_arm;
  buf.memSize = sz;
  buf.omvb = omvb;
  CLog::Log(LOGDEBUG, "%s: %p data:%p size:%d (omvb:%p) free:%d, used:%d", __FUNCTION__, &buf, buf.mem, buf.memSize, omvb, freeBuffer.size(), usedBuffer.size());
  usedBuffer.push_back(buf);
  freeBuffer.pop_back();
  return buf.mem;
}

void CMemoryProviderPi::ReleaseMemoryPointer(void *bufferPtr)
{
  if (!bufferPtr)
    return;
  //for(std::vector<BUFFER>::iterator it=usedBuffer.begin() ; it < usedBuffer.end(); it++ )
  //  CLog::Log(LOGDEBUG, "%s: %p", __FUNCTION__, *it);

  std::vector<BUFFER>::iterator res(std::find(usedBuffer.begin(), usedBuffer.end(), bufferPtr));
  if (res == usedBuffer.end())
  {
    CLog::Log(LOGERROR, "Unable to release buffer:%p free:%d used:%d", bufferPtr, freeBuffer.size(), usedBuffer.size());
    return;
  }
  CLog::Log(LOGDEBUG, "%s: %p (omvb:%p) free:%d used:%d", __FUNCTION__, bufferPtr, res->omvb, freeBuffer.size(), usedBuffer.size());
  if (res->omvb)
    res->omvb->Release();
  freeBuffer.push_back(*res);
  usedBuffer.erase(res);
}

void CMemoryProviderPi::Finalise(void *picture)
{
  DVDVideoPicture* pDvdVideoPicture = static_cast<DVDVideoPicture*>(picture);
  void *bufferPtr = pDvdVideoPicture->data[0];
  std::vector<BUFFER>::iterator res(std::find(usedBuffer.begin(), usedBuffer.end(), bufferPtr));
  if (res == usedBuffer.end())
  {
    CLog::Log(LOGERROR, "Unable to finalise buffer:%p", bufferPtr);
    return;
  }
  MMAL::CMMALYUVBuffer *omvb = res->omvb;
  if (omvb)
  {
    pDvdVideoPicture->format = RENDER_FMT_MMAL;
    pDvdVideoPicture->MMALBuffer = omvb;
    omvb->m_width = pDvdVideoPicture->iWidth;
    omvb->m_height = pDvdVideoPicture->iHeight;
    omvb->m_aligned_width = pDvdVideoPicture->iLineSize[0];
    omvb->m_aligned_height = pDvdVideoPicture->iHeight;
    omvb->mmal_buffer->pts = pDvdVideoPicture->pts == DVD_NOPTS_VALUE ? MMAL_TIME_UNKNOWN : pDvdVideoPicture->pts;
    // need to flush ARM cache so GPU can see it
    omvb->gmem->Flush();
  }
}
