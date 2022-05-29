/*
 *  Copyright (C) 2014-2022 Arne Morten Kvarving
 *  Copyright (C) 2016-2022 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include "HVLCodec.h"

#include <algorithm>
#include <iostream>
#include <kodi/Filesystem.h>
#include <kodi/General.h>


namespace
{

std::pair<int, std::string> extractInfo(const std::string& name)
{
  int track = 0;
  std::string toLoad(name);
  if (toLoad.find(".hvlstream") != std::string::npos)
  {
    size_t iStart = toLoad.rfind('-') + 1;
    track = atoi(toLoad.substr(iStart, toLoad.size() - iStart - 10).c_str()) - 1;
    //  The directory we are in, is the file
    //  that contains the bitstream to play,
    //  so extract it
    size_t slash = toLoad.rfind('\\');
    if (slash == std::string::npos)
      slash = toLoad.rfind('/');
    toLoad = toLoad.substr(0, slash);
  }

  return std::make_pair(track, toLoad);
}

}

CHVLCodec::CHVLCodec(KODI_HANDLE instance, const std::string& version)
  : CInstanceAudioDecoder(instance, version)
{
  hvl_InitReplayer();
}

CHVLCodec::~CHVLCodec()
{
  if (m_tune)
    hvl_FreeTune(m_tune);
}

bool CHVLCodec::Init(const std::string& filename,
                     unsigned int filecache,
                     int& channels,
                     int& samplerate,
                     int& bitspersample,
                     int64_t& totaltime,
                     int& bitrate,
                     AudioEngineDataFormat& format,
                     std::vector<AudioEngineChannel>& channellist)
{
  auto info = extractInfo(filename);
  m_track = info.first;

  m_tune = LoadHVL(info.second);
  if (!m_tune)
    return false;

  hvl_InitSubsong(m_tune, m_track);

  int duration = CalculateLength(m_tune, m_track);
  ctx.totaltime = duration * 48000;

  // Reset after length scan before
  hvl_InitSubsong(m_tune, m_track);

  format = AUDIOENGINE_FMT_S32NE;
  channellist = {AUDIOENGINE_CH_FL, AUDIOENGINE_CH_FR};
  channels = 2;
  bitspersample = 32;
  samplerate = 48000;
  totaltime = 1000 * duration;
  bitrate = 0;

  return true;
}

int CHVLCodec::ReadPCM(uint8_t* buffer, int size, int& actualsize)
{
  if (m_tune->ht_SongEndReached || ctx.timePos > ctx.totaltime)
    return 1;

  if (ctx.left == 0)
  {
    hvl_DecodeFrame(m_tune, reinterpret_cast<int8_t*>(ctx.sample_buffer), reinterpret_cast<int8_t*>(ctx.sample_buffer+1), 8);
    ctx.left = 48000/50;
  }

  size_t frames_to_copy = std::min(static_cast<size_t>(size) / 8, ctx.left);

  int32_t* out_buf = reinterpret_cast<int32_t*>(buffer);
  int32_t* in_buf = ctx.sample_buffer + 2*(48000/50-ctx.left);
  for (size_t i = 0; i < frames_to_copy; ++i)
  {
    out_buf[2*i] = in_buf[2*i]*(1 << 8);
    out_buf[2*i+1] = in_buf[2*i+1]*(1 << 8);
  }
  ctx.left -= frames_to_copy;
  actualsize = frames_to_copy*8;
  ctx.timePos += frames_to_copy;

  return 0;
}

int64_t CHVLCodec::Seek(int64_t time)
{
  int64_t wantedTimePos = time / 1000 * 48000;
  if (wantedTimePos < ctx.timePos)
  {
    ctx.timePos = 0;
    ctx.left = 0;
    hvl_InitSubsong(m_tune, m_track);
  }

  if (wantedTimePos - ctx.timePos > ctx.left)
  {
    ctx.timePos += ctx.left;
    while (ctx.timePos < wantedTimePos-48000/50)
    {
      hvl_DecodeFrame(m_tune, reinterpret_cast<int8_t*>(ctx.sample_buffer), reinterpret_cast<int8_t*>(ctx.sample_buffer+1), 8);
      ctx.timePos += 48000/50;
    }
  }
  // finally position ourself in the buffered data
  ctx.left = 48000/50 - (wantedTimePos - ctx.timePos);
  return time;
}

int CHVLCodec::TrackCount(const std::string& fileName)
{
  if (fileName.find(".hvlstream") != std::string::npos)
    return 0;

  hvl_tune* tune = LoadHVL(fileName);
  if (!tune)
    return 0;

  int res = tune->ht_SubsongNr+1;
  hvl_FreeTune(tune);

  return res;
}

bool CHVLCodec::ReadTag(const std::string& file, kodi::addon::AudioDecoderInfoTag& tag)
{
  auto info = extractInfo(file);

  hvl_tune* tune = LoadHVL(info.second);
  if (!tune)
    return false;
  hvl_InitSubsong(tune, info.first);

  std::string title = tune->ht_Name;
  if (title.empty())
    return false;

  tag.SetTitle(title);
  if (tune->ht_SubsongNr+1 > 1)
    tag.SetTrack(info.first+1);
  tag.SetDuration(CalculateLength(tune, info.first));
  tag.SetSamplerate(48000);
  tag.SetChannels(2);

  hvl_FreeTune(tune);

  return true;
}

hvl_tune* CHVLCodec::LoadHVL(const std::string& fileName)
{
  kodi::vfs::CFile file;
  if (!file.OpenFile(fileName, 0))
    return nullptr;

  size_t len = file.GetLength();
  std::vector<uint8_t> buf(len);
  file.Read(buf.data(), len);
  file.Close();

  hvl_tune* tune = hvl_LoadTune(buf.data(), len, 48000, 1);
  return tune;
}

int CHVLCodec::CalculateLength(hvl_tune* tune, int track)
{
  unsigned int safety = 2 * 60 * 60 * 50 * tune->ht_SpeedMultiplier; // 2 hours
  while (!tune->ht_SongEndReached && safety)
  {
    hvl_play_irq(tune);
    --safety;
  }
  return safety > 0 ? (tune->ht_PlayingTime / tune->ht_SpeedMultiplier / 50) : (5 * 60 * 1000); // fallback 5 minutes;
}

//------------------------------------------------------------------------------

class ATTRIBUTE_HIDDEN CMyAddon : public kodi::addon::CAddonBase
{
public:
  CMyAddon() = default;
  ADDON_STATUS CreateInstance(int instanceType,
                              const std::string& instanceID,
                              KODI_HANDLE instance,
                              const std::string& version,
                              KODI_HANDLE& addonInstance) override
  {
    addonInstance = new CHVLCodec(instance, version);
    return ADDON_STATUS_OK;
  }
  ~CMyAddon() override = default;
};

ADDONCREATOR(CMyAddon)
