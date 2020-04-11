#pragma once
#include <metahook.h>

#include "exportfuncs.h"
#include "enginedef.h"
#include "aud_int_internal.h"

//internal structures

typedef struct
{
  sfx_t *sfx;
  float volume;
  float pitch;
  float attenuation;
  int entnum;
  int entchannel;
  vec3_t origin;
  uint64_t start;
  uint64_t end;
  //for sentence
  int isentence;
  int iword;
  //for voice sound
  sfxcache_t *voicecache;

  // For OpenAL
  alure::SharedPtr<alure::Decoder> decoder;
  alure::Buffer buffer;
  alure::Source source;

  //X-Fi workaround
  std::chrono::time_point<std::chrono::steady_clock> playback_end_time;

  // For OpenAL EFX
  bool firstpass{ true };
  float gain_fading_current{ 0 };
  float gain_fading_elapsed_time{ 0 };
  float gain_fading_initial_value{ 0 };
  float gain_fading_last_target{ 0 };
  float gain_fading_target{ 0 };
}aud_channel_t;

typedef struct
{
  //wave info
  uint64_t length{};
  uint64_t loopstart{};
  uint64_t loopend{};
  ALuint samplerate{};
  bool looping{};
  alure::SampleType stype{};
  alure::ChannelConfig channels{};

  //OpenAL buffer
  alure::SharedPtr<alure::Decoder> decoder;
  alure::Buffer buffer;

  alure::Vector<ALubyte> data;
}aud_sfxcache_t;

typedef struct
{
  int *cl_servercount;
  int *cl_parsecount;
  int *cl_viewentity;
  int *cl_num_entities;
  int *cl_waterlevel;

  double *cl_time;
  double *cl_oldtime;

  float *g_SND_VoiceOverdrive;

  char *(*rgpszrawsentence)[CVOXFILESENTENCEMAX];
  int *cszrawsentences;

  //s_dma.c
  void(*S_Startup)(void);//hooked
  void(*S_Init)(void);//hooked
  void(*S_Shutdown)(void);//hooked
  sfx_t *(*S_FindName)(char *name, int *pfInCache);//hooked
  sfx_t *(*S_PrecacheSound)(char *name);//hooked
  void(*SND_Spatialize)(aud_channel_t *ch);
  void(*S_Update)(float *origin, float *forward, float *right, float *up);//hooked
  void(*S_StartDynamicSound)(int entnum, int entchannel, sfx_t *sfx, float *origin, float fvol, float attenuation, int flags, int pitch);//hooked
  void(*S_StartStaticSound)(int entnum, int entchannel, sfx_t *sfx, float *origin, float fvol, float attenuation, int flags, int pitch);//hooked
  void(*S_StopSound)(int entnum, int entchannel);//hooked
  void(*S_StopAllSounds)(qboolean clear);//hooked

  //s_mem.c
  aud_sfxcache_t *(*S_LoadSound)(sfx_t *s, aud_channel_t *ch);//hooked

  //s_mix.c
  void(*VOX_Shutdown)(void);
  void(*S_FreeChannel)(channel_t *ch);

  //voice_sound_engine_interface.cpp
  void(*VoiceSE_NotifyFreeChannel)(int iChannel);
  void(*VoiceSE_Idle)(float frametime);

  //sequence.c
  sentenceEntry_s*(*SequenceGetSentenceByIndex)(unsigned int index);

  //sys_dll.c
#ifdef _DEBUG
  void(*Sys_Error)(char *fmt, ...);
#endif
}aud_engine_t;

//snd_hook.cpp
void S_FillAddress(void);

namespace MetaAudio
{
  class AudioEngine;
  class SoundLoader;
}

void S_InstallHook(MetaAudio::AudioEngine* engine, MetaAudio::SoundLoader* loader);

//common
extern aud_engine_t gAudEngine;