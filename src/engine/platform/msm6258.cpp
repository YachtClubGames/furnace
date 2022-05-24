/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2022 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "msm6258.h"
#include "../engine.h"
#include "../../ta-log.h"
#include "sound/oki/okim6258.h"
#include <string.h>
#include <math.h>

#define rWrite(v) if (!skipRegisterWrites) {writes.emplace(0,v); if (dumpWrites) {addWrite(0,v);} }

const char** DivPlatformMSM6258::getRegisterSheet() {
  return NULL;
}

const char* DivPlatformMSM6258::getEffectName(unsigned char effect) {
  return NULL;
}

void DivPlatformMSM6258::acquire(short* bufL, short* bufR, size_t start, size_t len) {
  for (size_t h=start; h<start+len; h++) {
    short* outs[2]={
      &bufL[h],
      NULL
    };
    if (delay<=0) {
      if (!writes.empty()) {
        //QueuedWrite& w=writes.front();
        //msm->command_w(w.val);
        writes.pop();
        delay=32;
      }
    } else {
      delay--;
    }
    
    msm->sound_stream_update(outs,1);
    bufL[h]=msm->data_w(0)?32767:0;

    /*if (++updateOsc>=22) {
      updateOsc=0;
      // TODO: per-channel osc
      for (int i=0; i<1; i++) {
        oscBuf[i]->data[oscBuf[i]->needle++]=msm->m_voice[i].m_muted?0:(msm->m_voice[i].m_out<<6);
      }
    }*/
  }
}

void DivPlatformMSM6258::tick(bool sysTick) {
  // nothing
}

int DivPlatformMSM6258::dispatch(DivCommand c) {
  switch (c.cmd) {
    case DIV_CMD_NOTE_ON: {
      DivInstrument* ins=parent->getIns(chan[c.chan].ins,DIV_INS_FM);
      if (ins->type==DIV_INS_AMIGA) {
        chan[c.chan].furnacePCM=true;
      } else {
        chan[c.chan].furnacePCM=false;
      }
      if (skipRegisterWrites) break;
      if (chan[c.chan].furnacePCM) {
        chan[c.chan].macroInit(ins);
        if (!chan[c.chan].std.vol.will) {
          chan[c.chan].outVol=chan[c.chan].vol;
        }
        chan[c.chan].sample=ins->amiga.getSample(c.value);
        if (chan[c.chan].sample>=0 && chan[c.chan].sample<parent->song.sampleLen) {
          //DivSample* s=parent->getSample(chan[c.chan].sample);
          if (c.value!=DIV_NOTE_NULL) {
            chan[c.chan].note=c.value;
            chan[c.chan].freqChanged=true;
          }
          chan[c.chan].active=true;
          chan[c.chan].keyOn=true;
          msm->ctrl_w(1);
          msm->ctrl_w(2);
          //rWrite((8<<c.chan)); // turn off
          //rWrite(0x80|chan[c.chan].sample); // set phrase
          //rWrite((16<<c.chan)|(8-chan[c.chan].outVol)); // turn on
        } else {
          break;
        }
      } else {
        chan[c.chan].sample=-1;
        chan[c.chan].macroInit(NULL);
        chan[c.chan].outVol=chan[c.chan].vol;
        if ((12*sampleBank+c.value%12)>=parent->song.sampleLen) {
          break;
        }
        //DivSample* s=parent->getSample(12*sampleBank+c.value%12);
        chan[c.chan].sample=12*sampleBank+c.value%12;
        //rWrite((8<<c.chan)); // turn off
        //rWrite(0x80|chan[c.chan].sample); // set phrase
        //rWrite((16<<c.chan)|(8-chan[c.chan].outVol)); // turn on
        msm->ctrl_w(1);
        msm->ctrl_w(2);
      }
      break;
    }
    case DIV_CMD_NOTE_OFF:
      chan[c.chan].keyOff=true;
      chan[c.chan].keyOn=false;
      chan[c.chan].active=false;
      //rWrite((8<<c.chan)); // turn off
      chan[c.chan].macroInit(NULL);
      break;
    case DIV_CMD_NOTE_OFF_ENV:
      chan[c.chan].keyOff=true;
      chan[c.chan].keyOn=false;
      chan[c.chan].active=false;
      //rWrite((8<<c.chan)); // turn off
      chan[c.chan].std.release();
      break;
    case DIV_CMD_ENV_RELEASE:
      chan[c.chan].std.release();
      break;
    case DIV_CMD_VOLUME: {
      chan[c.chan].vol=c.value;
      if (!chan[c.chan].std.vol.has) {
        chan[c.chan].outVol=c.value;
      }
      break;
    }
    case DIV_CMD_GET_VOLUME: {
      return chan[c.chan].vol;
      break;
    }
    case DIV_CMD_INSTRUMENT:
      if (chan[c.chan].ins!=c.value || c.value2==1) {
        chan[c.chan].insChanged=true;
      }
      chan[c.chan].ins=c.value;
      break;
    case DIV_CMD_PITCH: {
      break;
    }
    case DIV_CMD_NOTE_PORTA: {
      return 2;
    }
    case DIV_CMD_SAMPLE_BANK:
      sampleBank=c.value;
      if (sampleBank>(parent->song.sample.size()/12)) {
        sampleBank=parent->song.sample.size()/12;
      }
      break;
    case DIV_CMD_LEGATO: {
      break;
    }
    case DIV_ALWAYS_SET_VOLUME:
      return 0;
      break;
    case DIV_CMD_GET_VOLMAX:
      return 8;
      break;
    case DIV_CMD_PRE_PORTA:
      break;
    case DIV_CMD_PRE_NOTE:
      break;
    default:
      //printf("WARNING: unimplemented command %d\n",c.cmd);
      break;
  }
  return 1;
}

void DivPlatformMSM6258::muteChannel(int ch, bool mute) {
  isMuted[ch]=mute;
}

void DivPlatformMSM6258::forceIns() {
  while (!writes.empty()) writes.pop();
  for (int i=0; i<1; i++) {
    chan[i].insChanged=true;
  }
}

void* DivPlatformMSM6258::getChanState(int ch) {
  return &chan[ch];
}

DivDispatchOscBuffer* DivPlatformMSM6258::getOscBuffer(int ch) {
  return oscBuf[ch];
}

unsigned char* DivPlatformMSM6258::getRegisterPool() {
  return NULL;
}

int DivPlatformMSM6258::getRegisterPoolSize() {
  return 0;
}

void DivPlatformMSM6258::poke(unsigned int addr, unsigned short val) {
  //immWrite(addr,val);
}

void DivPlatformMSM6258::poke(std::vector<DivRegWrite>& wlist) {
  //for (DivRegWrite& i: wlist) immWrite(i.addr,i.val);
}

void DivPlatformMSM6258::reset() {
  while (!writes.empty()) writes.pop();
  msm->device_reset();
  if (dumpWrites) {
    addWrite(0xffffffff,0);
  }
  for (int i=0; i<1; i++) {
    chan[i]=DivPlatformMSM6258::Channel();
    chan[i].std.setEngine(parent);
  }
  for (int i=0; i<1; i++) {
    chan[i].vol=8;
    chan[i].outVol=8;
  }

  sampleBank=0;

  delay=0;
}

bool DivPlatformMSM6258::keyOffAffectsArp(int ch) {
  return false;
}

void DivPlatformMSM6258::notifyInsChange(int ins) {
  for (int i=0; i<1; i++) {
    if (chan[i].ins==ins) {
      chan[i].insChanged=true;
    }
  }
}

void DivPlatformMSM6258::notifyInsDeletion(void* ins) {
}

const void* DivPlatformMSM6258::getSampleMem(int index) {
  return index == 0 ? adpcmMem : NULL;
}

size_t DivPlatformMSM6258::getSampleMemCapacity(int index) {
  return index == 0 ? 262144 : 0;
}

size_t DivPlatformMSM6258::getSampleMemUsage(int index) {
  return index == 0 ? adpcmMemLen : 0;
}

void DivPlatformMSM6258::renderSamples() {
  memset(adpcmMem,0,getSampleMemCapacity(0));

  // sample data
  size_t memPos=0;
  int sampleCount=parent->song.sampleLen;
  if (sampleCount>128) sampleCount=128;
  for (int i=0; i<sampleCount; i++) {
    DivSample* s=parent->song.sample[i];
    int paddedLen=s->lengthVOX;
    if (memPos>=getSampleMemCapacity(0)) {
      logW("out of ADPCM memory for sample %d!",i);
      break;
    }
    if (memPos+paddedLen>=getSampleMemCapacity(0)) {
      memcpy(adpcmMem+memPos,s->dataVOX,getSampleMemCapacity(0)-memPos);
      logW("out of ADPCM memory for sample %d!",i);
    } else {
      memcpy(adpcmMem+memPos,s->dataVOX,paddedLen);
    }
    s->offVOX=memPos;
    memPos+=paddedLen;
  }
  adpcmMemLen=memPos+256;
}

void DivPlatformMSM6258::setFlags(unsigned int flags) {
  if (flags&1) {
    chipClock=4096000;
  } else {
    chipClock=4000000;
  }
  rate=chipClock/256;
  for (int i=0; i<1; i++) {
    isMuted[i]=false;
    oscBuf[i]->rate=rate/256;
  }
}

int DivPlatformMSM6258::init(DivEngine* p, int channels, int sugRate, unsigned int flags) {
  parent=p;
  adpcmMem=new unsigned char[getSampleMemCapacity(0)];
  adpcmMemLen=0;
  dumpWrites=false;
  skipRegisterWrites=false;
  updateOsc=0;
  for (int i=0; i<1; i++) {
    isMuted[i]=false;
    oscBuf[i]=new DivDispatchOscBuffer;
  }
  msm=new okim6258_device(4000000);
  msm->device_start();
  setFlags(flags);
  reset();
  return 4;
}

void DivPlatformMSM6258::quit() {
  for (int i=0; i<1; i++) {
    delete oscBuf[i];
  }
  delete msm;
  delete[] adpcmMem;
}

DivPlatformMSM6258::~DivPlatformMSM6258() {
}