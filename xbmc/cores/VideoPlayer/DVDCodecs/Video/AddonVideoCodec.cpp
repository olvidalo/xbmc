/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
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

#include "AddonVideoCodec.h"
#include "cores/VideoPlayer/DVDStreamInfo.h"
#include "cores/VideoPlayer/DVDDemuxers/DemuxCrypto.h"
#include "cores/VideoPlayer/DVDCodecs/DVDCodecs.h"
#include "cores/VideoPlayer/TimingConstants.h"
#include "cores/VideoPlayer/MemoryBroker/MemoryBroker.h"
#include "ServiceBroker.h"
#include "utils/log.h"
#include "settings/AdvancedSettings.h"

using namespace kodi::addon;

#define ALIGN(value, alignment) (((value)+(alignment-1))&~(alignment-1))

CAddonVideoCodec::CAddonVideoCodec(CProcessInfo &processInfo, ADDON::AddonInfoPtr& addonInfo, kodi::addon::IAddonInstance* parentInstance)
  : CDVDVideoCodec(processInfo),
    IAddonInstanceHandler(ADDON::ADDON_VIDEOCODEC, addonInfo, parentInstance)
  , m_displayAspect(0.0f)
  , m_codecFlags(0)
  , m_lastPictureBuffer(nullptr)
{
  memset(&m_struct, 0, sizeof(m_struct));
  m_struct.toKodi.kodiInstance = this;
  m_struct.toKodi.GetFrameBuffer = get_frame_buffer;
  if (!CreateInstance(ADDON_INSTANCE_VIDEOCODEC, &m_struct, reinterpret_cast<KODI_HANDLE*>(&m_addonInstance)) || !m_struct.toAddon.Open)
  {
    CLog::Log(LOGERROR, "CAddonVideoCodec: Failed to create add-on instance for '%s'", addonInfo->ID().c_str());
    return;
  }
  if (m_struct.toAddon.GetName)
    m_processInfo.SetVideoDecoderName(m_struct.toAddon.GetName(m_addonInstance), false);
}

CAddonVideoCodec::~CAddonVideoCodec()
{
  DestroyInstance();

  if (m_bufferPool)
    m_bufferPool->ReleaseMemoryPointer(m_lastPictureBuffer);
}

bool CAddonVideoCodec::CopyToInitData(VIDEOCODEC_INITDATA &initData, CDVDStreamInfo &hints)
{
  initData.codecProfile = CODEC_PROFILE::CodecProfileNotNeeded;
  switch (hints.codec)
  {
  case AV_CODEC_ID_H264:
    initData.codec = VIDEOCODEC_INITDATA::CodecH264;
    switch (hints.profile)
    {
    case 0:
    case FF_PROFILE_UNKNOWN:
      initData.codecProfile = CODEC_PROFILE::CodecProfileUnknown;
      break;
    case FF_PROFILE_H264_BASELINE:
      initData.codecProfile = CODEC_PROFILE::H264CodecProfileBaseline;
      break;
    case FF_PROFILE_H264_MAIN:
      initData.codecProfile = CODEC_PROFILE::H264CodecProfileMain;
      break;
    case FF_PROFILE_H264_EXTENDED:
      initData.codecProfile = CODEC_PROFILE::H264CodecProfileExtended;
      break;
    case FF_PROFILE_H264_HIGH:
      initData.codecProfile = CODEC_PROFILE::H264CodecProfileHigh;
      break;
    case FF_PROFILE_H264_HIGH_10:
      initData.codecProfile = CODEC_PROFILE::H264CodecProfileHigh10;
      break;
    case FF_PROFILE_H264_HIGH_422:
      initData.codecProfile = CODEC_PROFILE::H264CodecProfileHigh422;
      break;
    case FF_PROFILE_H264_HIGH_444_PREDICTIVE:
      initData.codecProfile = CODEC_PROFILE::H264CodecProfileHigh444Predictive;
      break;
    default:
      return false;
    }
    break;
  case AV_CODEC_ID_VP8:
    initData.codec = VIDEOCODEC_INITDATA::CodecVp8;
    break;
  case AV_CODEC_ID_VP9:
    initData.codec = VIDEOCODEC_INITDATA::CodecVp9;
    break;
  default:
    return false;
  }
  if (hints.cryptoSession)
  {
    switch (hints.cryptoSession->keySystem)
    {
    case CRYPTO_SESSION_SYSTEM_NONE:
      initData.cryptoInfo.m_CryptoKeySystem = CRYPTO_INFO::CRYPTO_KEY_SYSTEM_NONE;
      break;
    case CRYPTO_SESSION_SYSTEM_WIDEVINE:
      initData.cryptoInfo.m_CryptoKeySystem = CRYPTO_INFO::CRYPTO_KEY_SYSTEM_WIDEVINE;
      break;
    case CRYPTO_SESSION_SYSTEM_PLAYREADY:
      initData.cryptoInfo.m_CryptoKeySystem = CRYPTO_INFO::CRYPTO_KEY_SYSTEM_PLAYREADY;
      break;
    default:
      return false;
    }
    initData.cryptoInfo.m_CryptoSessionIdSize = hints.cryptoSession->sessionIdSize;
    //We assume that we need this sessionid only for the directly following call
    initData.cryptoInfo.m_CryptoSessionId = hints.cryptoSession->sessionId;
  }

  initData.extraData = reinterpret_cast<const uint8_t*>(hints.extradata);
  initData.extraDataSize = hints.extrasize;
  initData.width = hints.width;
  initData.height = hints.height;
  initData.videoFormats = m_formats;

  m_displayAspect = (hints.aspect > 0.0 && !hints.forced_aspect) ? hints.aspect : 0.0f;
  m_width = hints.width;
  m_height = hints.height;

  m_processInfo.SetVideoDimensions(hints.width, hints.height);

  return true;
}

bool CAddonVideoCodec::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
  if (!m_struct.toAddon.Open)
    return false;

  unsigned int nformats(0);
  for (auto fmt : options.m_formats)
    if (fmt == RENDER_FMT_YUV420P)
    {
      m_formats[nformats++] = VideoFormatYV12;
      break;
    }
  m_formats[nformats] = UnknownVideoFormat;

  if (nformats == 0)
    return false;

  m_bufferPool = CServiceBroker::GetMemoryBroker().AquireMemoryProvider(options);

  if (!m_bufferPool)
    return false;

  VIDEOCODEC_INITDATA initData;
  if (!CopyToInitData(initData, hints))
    return false;

  m_lastPictureBuffer = nullptr;

  return m_struct.toAddon.Open(m_addonInstance, initData);
}

bool CAddonVideoCodec::Reconfigure(CDVDStreamInfo &hints)
{
  if (!m_struct.toAddon.Reconfigure)
    return false;

  VIDEOCODEC_INITDATA initData;
  if (!CopyToInitData(initData, hints))
    return false;

  return m_struct.toAddon.Reconfigure(m_addonInstance, initData);
}

bool CAddonVideoCodec::AddData(const DemuxPacket &packet)
{
  if (!m_struct.toAddon.AddData)
    return false;

  return m_struct.toAddon.AddData(m_addonInstance, packet);
}

CDVDVideoCodec::VCReturn CAddonVideoCodec::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  if (!m_struct.toAddon.GetPicture)
    return CDVDVideoCodec::VC_ERROR;

  VIDEOCODEC_PICTURE picture;
  picture.flags = (m_codecFlags & DVD_CODEC_CTRL_DRAIN) ? VIDEOCODEC_PICTURE::FLAG_DRAIN : 0;

  switch (m_struct.toAddon.GetPicture(m_addonInstance, picture))
  {
  case VIDEOCODEC_RETVAL::VC_NONE:
    return CDVDVideoCodec::VC_NONE;
  case VIDEOCODEC_RETVAL::VC_ERROR:
    return CDVDVideoCodec::VC_ERROR;
  case VIDEOCODEC_RETVAL::VC_BUFFER:
    return CDVDVideoCodec::VC_BUFFER;
  case VIDEOCODEC_RETVAL::VC_PICTURE:
    pDvdVideoPicture->data[0] = picture.decodedData + picture.planeOffsets[0];
    pDvdVideoPicture->data[1] = picture.decodedData + picture.planeOffsets[1];
    pDvdVideoPicture->data[2] = picture.decodedData + picture.planeOffsets[2];
    pDvdVideoPicture->iLineSize[0] = picture.stride[0];
    pDvdVideoPicture->iLineSize[1] = picture.stride[1];
    pDvdVideoPicture->iLineSize[2] = picture.stride[2];
    pDvdVideoPicture->iWidth = picture.width;
    pDvdVideoPicture->iHeight = picture.height;
    pDvdVideoPicture->pts = picture.pts;
    pDvdVideoPicture->dts = DVD_NOPTS_VALUE;
    pDvdVideoPicture->color_range = 0;
    pDvdVideoPicture->color_matrix = 4;
    pDvdVideoPicture->iFlags = DVP_FLAG_ALLOCATED;
    if (m_codecFlags & DVD_CODEC_CTRL_DROP)
      pDvdVideoPicture->iFlags |= DVP_FLAG_DROPPED;

    pDvdVideoPicture->format = RENDER_FMT_YUV420P;

    pDvdVideoPicture->iDisplayWidth = pDvdVideoPicture->iWidth;
    pDvdVideoPicture->iDisplayHeight = pDvdVideoPicture->iHeight;
    if (m_displayAspect > 0.0)
    {
      pDvdVideoPicture->iDisplayWidth = ((int)lrint(pDvdVideoPicture->iHeight * m_displayAspect)) & ~3;
      if (pDvdVideoPicture->iDisplayWidth > pDvdVideoPicture->iWidth)
      {
        pDvdVideoPicture->iDisplayWidth = pDvdVideoPicture->iWidth;
        pDvdVideoPicture->iDisplayHeight = ((int)lrint(pDvdVideoPicture->iWidth / m_displayAspect)) & ~3;
      }
    }

    if (g_advancedSettings.CanLogComponent(LOGVIDEO))
      CLog::Log(LOGDEBUG, "CAddonVideoCodec: GetPicture::VC_PICTURE with pts %llu", picture.pts);

    m_bufferPool->ReleaseMemoryPointer(m_lastPictureBuffer);
    m_lastPictureBuffer = picture.decodedData;

    if (picture.width != m_width || picture.height != m_height)
    {
      m_width = picture.width;
      m_height = picture.height;
      m_processInfo.SetVideoDimensions(m_width, m_height);
    }

    return CDVDVideoCodec::VC_PICTURE;
  case VIDEOCODEC_RETVAL::VC_EOF:
    return CDVDVideoCodec::VC_EOF;
  default:
    return CDVDVideoCodec::VC_ERROR;
  }
}

const char* CAddonVideoCodec::GetName()
{
  if (m_struct.toAddon.GetName)
    return m_struct.toAddon.GetName(m_addonInstance);
  return "";
}

void CAddonVideoCodec::Reset()
{
  if (!m_struct.toAddon.Reset)
    return;

  CLog::Log(LOGDEBUG, "CAddonVideoCodec: Reset");

  // Get the remaining pictures out of the external decoder
  VIDEOCODEC_PICTURE picture;
  picture.flags = VIDEOCODEC_PICTURE::FLAG_DRAIN;

  VIDEOCODEC_RETVAL ret;
  while ((ret = m_struct.toAddon.GetPicture(m_addonInstance, picture)) != VIDEOCODEC_RETVAL::VC_EOF)
  {
    if (ret == VIDEOCODEC_RETVAL::VC_PICTURE)
    {
      m_bufferPool->ReleaseMemoryPointer(m_lastPictureBuffer);
      m_lastPictureBuffer = picture.decodedData;
    }
  }
  m_bufferPool->ReleaseMemoryPointer(m_lastPictureBuffer);
  m_lastPictureBuffer = nullptr;

  m_struct.toAddon.Reset(m_addonInstance);
}

bool CAddonVideoCodec::GetFrameBuffer(VIDEOCODEC_PICTURE &picture)
{
  picture.decodedData = static_cast<uint8_t*>(m_bufferPool->GetMemoryPointer(picture.decodedDataSize));
  return picture.decodedData != nullptr;
}

/*********************     ADDON-TO-KODI    **********************/

bool CAddonVideoCodec::get_frame_buffer(void* kodiInstance, VIDEOCODEC_PICTURE &picture)
{
  if (!kodiInstance)
    return false;

  return static_cast<CAddonVideoCodec*>(kodiInstance)->GetFrameBuffer(picture);
}
