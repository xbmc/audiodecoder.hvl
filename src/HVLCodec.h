/*
 *  Copyright (C) 2014-2020 Arne Morten Kvarving
 *  Copyright (C) 2016-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <kodi/addon-instance/AudioDecoder.h>

extern "C"
{
#include "libhvl/hvl_replay.h"

#include <memory.h>
#include <stdint.h>
#include <stdio.h>
}

struct ATTRIBUTE_HIDDEN HVLContext
{
  int32_t sample_buffer[48000/50*2];
  size_t left = 0; // in terms of frames
  int64_t timePos = 0; // in terms of frames
};


class ATTRIBUTE_HIDDEN CHVLCodec : public kodi::addon::CInstanceAudioDecoder
{
public:
  CHVLCodec(KODI_HANDLE instance, const std::string& version);
  virtual ~CHVLCodec();

  bool Init(const std::string& filename,
            unsigned int filecache,
            int& channels,
            int& samplerate,
            int& bitspersample,
            int64_t& totaltime,
            int& bitrate,
            AudioEngineDataFormat& format,
            std::vector<AudioEngineChannel>& channellist) override;
  int ReadPCM(uint8_t* buffer, int size, int& actualsize) override;
  int64_t Seek(int64_t time) override;
  int TrackCount(const std::string& fileName) override;
  bool ReadTag(const std::string& file, kodi::addon::AudioDecoderInfoTag& tag) override;

private:
  hvl_tune* LoadHVL(const std::string& file);

  HVLContext ctx;
  hvl_tune* m_tune = nullptr;
  int m_track;
};
