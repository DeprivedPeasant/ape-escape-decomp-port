#include "ae_sound.h"
#include "../core/ae_sdk.h"

static int32_t s16(uint32_t value) { return (int16_t)(uint16_t)value; }

/* SfxIsQueued @800885A8 */
int32_t ae_SfxIsQueued(AeRam *r, int32_t id) {
    id = s16(id);
    for (uint32_t i=0; i<12; ++i) {
        uint32_t entry=AE_SOUND_QUEUE+AE_SQ_SIZE*i;
        if (ae_rds16(r,entry+AE_SQ_TYPE)>=0 && ae_rds16(r,entry+AE_SQ_ID)==id) return 1;
    }
    return 0;
}

static uint32_t queue_reserve(AeRam *r, int32_t type, int32_t id) {
    for (uint32_t i=0; i<12; ++i) {
        uint32_t entry=AE_SOUND_QUEUE+AE_SQ_SIZE*i;
        if (ae_rds16(r,entry+AE_SQ_TYPE)<0) {
            ae_wr16(r,entry+AE_SQ_TYPE,type);
            ae_wr16(r,entry+AE_SQ_ID,id);
            return entry;
        }
    }
    return 0;
}

/* SoundDelayedQueueAdd @800885FC. Reads pan only after reserving (aliasing allowed). */
int32_t ae_SoundDelayedQueueAdd(AeRam *r, int32_t type, int32_t id, uint32_t pan) {
    uint32_t entry=queue_reserve(r,type,id);
    if (!entry) return 0;
    if (s16(type)==1) {
        ae_wr16(r,entry+AE_SQ_LEFT,ae_rd16(r,pan));
        ae_wr16(r,entry+AE_SQ_RIGHT,ae_rd16(r,pan+2));
    }
    return 1;
}

/* SoundLoopAllocSlot @800881D0 */
uint32_t ae_SoundLoopAllocSlot(AeRam *r) {
    for (uint32_t i=0; i<24; ++i) {
        uint32_t entry=AE_SOUND_LOOPS+AE_SL_SIZE*i;
        if (ae_rds16(r,entry+AE_SL_ID)<0) {
            ae_wr16(r,entry+AE_SL_INDEX,i);
            return entry;
        }
    }
    return 0;
}

/* SoundLoopFree @8008820C */
int32_t ae_SoundLoopFree(AeRam *r, int32_t id, int32_t voice) {
    id=s16(id); voice=s16(voice);
    for (uint32_t i=0; i<24; ++i) {
        uint32_t entry=AE_SOUND_LOOPS+AE_SL_SIZE*i;
        if (ae_rds16(r,entry+AE_SL_VOICE)==voice && ae_rds16(r,entry+AE_SL_ID)==id) {
            ae_wr16(r,entry+AE_SL_ID,0xFFFF);
            ae_wr16(r,entry+AE_SL_VOICE,0xFFFF);
            ae_wr16(r,entry+AE_SL_ACTIVE,0);
            ae_wr16(r,entry+AE_SL_INDEX,0xFFFF);
            ae_wr16(r,entry+AE_SL_FLAGS,0);
            return 0;
        }
    }
    return -1;
}

/* SeqSetTickFlag @800A1378: signed flags below 2 write; 2 and above query. */
int32_t ae_SeqSetTickFlag(AeRam *r, int32_t flag) {
    if (flag<2) ae_wr32(r,AE_SOUND_TICK_FLAG,flag);
    return ae_rds32(r,AE_SOUND_TICK_FLAG);
}

/* SpuVolumeClamp @800A119C: negative input is magnitude with phase bit restored. */
int32_t ae_SpuVolumeClamp(AeRam *r, int32_t maximum, int32_t volume) {
    (void)r;
    uint32_t result=(uint32_t)volume;
    if (volume & 0x8000) {
        result=0u-(uint32_t)volume;
        if (s16(maximum)<s16(result)) result=(uint32_t)maximum;
        result |= 0x8000;
    } else if (s16(maximum)<s16(volume)) result=(uint32_t)maximum;
    return s16(result);
}

/* SfxLookupTone @8009E82C. The function updates table globals even on rejection. */
uint32_t ae_SfxLookupTone(AeRam *r, uint32_t bank, uint32_t program, uint32_t note) {
    uint32_t record=AE_SOUND_BANKS+AE_SB_SIZE*(bank&0xFF);
    uint32_t header=ae_rd32(r,record+AE_SB_HEADER);
    ae_wr32(r,AE_SOUND_STATE+AE_SS_HEADER,header);
    ae_wr32(r,AE_SOUND_BANK_HEADER,header);
    uint32_t table=header+ae_rd32(r,header+AE_SH_SEQUENCES);
    uint32_t tones=ae_rd32(r,header+AE_SH_TONES);
    ae_wr32(r,AE_SOUND_SFX_TABLE,table);
    if (tones==0xFFFFFFFFu || (bank&0x80) || !ae_rd8(r,AE_SOUND_STATE+AE_SS_ENABLED)
        || ae_rd32(r,record+AE_SB_LOADED)!=1) return 0;
    program &= 0xFF;
    if (ae_rd16(r,table)<program) return 0;
    uint32_t program_entry=table+2*program;
    uint32_t offset=ae_rd16(r,program_entry+AE_ST_VALUES);
    if (offset==0xFFFF) return 0;
    if (ae_rd16(r,table+(offset&0xFFFE))<(note&0xFF)) return 0;
    ae_wr32(r,AE_SOUND_TONE_TABLE,header+tones);
    ae_wr32(r,AE_SOUND_VOLUME_CURVE,header+ae_rd32(r,header+AE_SH_CURVE));
    /* The low offset bit participates in the final lookup via offset >> 1. */
    uint32_t note_entry=table+2*((note&0xFF)+(ae_rd16(r,program_entry+AE_ST_VALUES)>>1));
    uint32_t sequence_base=header+ae_rd32(r,header+AE_SH_SEQUENCES);
    return sequence_base+ae_rd16(r,note_entry+AE_ST_VALUES);
}

/* SfxVoiceAlloc @8009E940. Allocation prepares event state; it does not key on hardware. */
int32_t ae_SfxVoiceAlloc(AeRam *r, uint32_t bank, uint32_t program, uint32_t note) {
    uint32_t events=ae_SfxLookupTone(r,bank&0xFF,program&0xFF,note&0xFF);
    if (!events || ae_ram_failed(r)) return -1;
    for (uint32_t i=0; i<24; ++i) {
        uint32_t voice=AE_SOUND_VOICES+AE_SV_SIZE*i;
        if (ae_rd32(r,voice+AE_SV_ALLOC_FLAGS)&0x00FF00FFu) continue;
        ae_wr8(r,voice+AE_SV_ACTIVE,1);
        ae_wr8(r,voice+AE_SV_EFFECT_MODE,1);
        ae_wr8(r,voice+AE_SV_PAUSED,0);
        ae_wr32(r,voice+AE_SV_DELTA_FRAMES,0);
        ae_wr8(r,voice+AE_SV_FLAG_E3,0);
        ae_wr32(r,voice+AE_SV_EVENTS,events);
        ae_wr32(r,voice+AE_SV_READ_POS,0);
        ae_wr8(r,voice+AE_SV_BANK,bank);
        ae_wr8(r,voice+AE_SV_PROGRAM,program);
        ae_wr8(r,voice+AE_SV_NOTE,note);
        if (ae_rd8(r,AE_SOUND_STATE+AE_SS_PENDING_FLAG)) {
            ae_wr8(r,voice+AE_SV_BANK_FLAG,1);
            ae_wr8(r,AE_SOUND_STATE+AE_SS_PENDING_FLAG,0);
        }
        ae_wr8(r,voice+AE_SV_FLAG_E2,0);
        ae_wr8(r,voice+AE_SV_MOD_A,0);
        ae_wr8(r,voice+AE_SV_MOD_B,0);
        ae_wr16(r,voice+AE_SV_RIGHT,0);
        ae_wr16(r,voice+AE_SV_LEFT,0);
        ae_wr8(r,voice+AE_SV_POSITIONAL,0);
        if (ae_rd8(r,AE_SOUND_STATE+AE_SS_PENDING_PAN)) {
            ae_wr8(r,voice+AE_SV_POSITIONAL,1);
            ae_wr16(r,voice+AE_SV_LEFT,ae_rd16(r,AE_SOUND_STATE+AE_SS_LEFT));
            ae_wr16(r,voice+AE_SV_RIGHT,ae_rd16(r,AE_SOUND_STATE+AE_SS_RIGHT));
            ae_wr16(r,voice+AE_SV_SCALE,ae_rd16(r,AE_SOUND_STATE+AE_SS_SCALE));
            ae_wr16(r,AE_SOUND_STATE+AE_SS_SCALE,0);
            ae_wr16(r,AE_SOUND_STATE+AE_SS_RIGHT,0);
            ae_wr16(r,AE_SOUND_STATE+AE_SS_LEFT,0);
            ae_wr8(r,AE_SOUND_STATE+AE_SS_PENDING_PAN,0);
        }
        return (int32_t)i;
    }
    return -1;
}

static int32_t wait_idle(AeRam *r, const AeSoundHw *hw) {
    if (!hw || !hw->wait_idle) {
        ae_ram_fail(r,"AeSoundHw.wait_idle not provided");
        return 1;
    }
    int32_t result=hw->wait_idle(hw->user,r);
    return ae_ram_failed(r) ? 1 : result;
}

/* SfxVoiceStart @800A3158 */
int32_t ae_SfxVoiceStart(AeRam *r, const AeSoundHw *hw, uint32_t bank, uint32_t program, uint32_t note) {
    if (wait_idle(r,hw)) return -1;
    return ae_SfxVoiceAlloc(r,bank&0xFF,program&0xFF,note&0xFF);
}

/* SfxVoiceStartEx @800A31B8: left/right (not pitch/pan) and a volume scale. */
int32_t ae_SfxVoiceStartEx(AeRam *r, const AeSoundHw *hw, uint32_t bank, uint32_t program,
                         uint32_t note, int32_t left, int32_t right, int32_t scale) {
    if (wait_idle(r,hw)) return -1;
    if (bank&0x80) {
        bank &= 0x7F;
        ae_wr8(r,AE_SOUND_STATE+AE_SS_PENDING_FLAG,1);
    }
    ae_wr16(r,AE_SOUND_STATE+AE_SS_LEFT,ae_SpuVolumeClamp(r,4096,s16(left)));
    ae_wr16(r,AE_SOUND_STATE+AE_SS_RIGHT,ae_SpuVolumeClamp(r,4096,s16(right)));
    ae_wr16(r,AE_SOUND_STATE+AE_SS_SCALE,scale);
    ae_wr8(r,AE_SOUND_STATE+AE_SS_PENDING_PAN,1);
    return ae_SfxVoiceAlloc(r,bank&0xFF,program&0xFF,note&0xFF);
}

/* SfxVoiceStop @800A329C. A signed high-bit handle also clears channel ADSR.
 * SPU key-off writes are per matching channel, not per SFX slot. */
void ae_SfxVoiceStop(AeRam *r, const AeSoundHw *hw, int32_t handle) {
    if (wait_idle(r,hw)) return;
    uint32_t voice=(uint32_t)handle&0x7FFF;
    int32_t signed_handle=s16(handle);
    if (voice>=24 || signed_handle>=24) return;
    uint32_t slot=AE_SOUND_VOICES+AE_SV_SIZE*voice;
    if (ae_rd8(r,slot+AE_SV_ACTIVE)) {
        ae_wr32(r,slot+AE_SV_DELTA_FRAMES,0);
        ae_wr8(r,slot+AE_SV_EFFECT_MODE,0);
        ae_wr8(r,slot+AE_SV_ACTIVE,0);
        ae_wr8(r,slot+AE_SV_FLAG_E3,0);
        ae_wr8(r,slot+AE_SV_FLAG_101,0);
        ae_wr8(r,slot+AE_SV_FLAG_100,0);
        ae_wr8(r,slot+AE_SV_FLAG_E2,0);
        ae_wr8(r,slot+AE_SV_MOD_A,0);
        ae_wr8(r,slot+AE_SV_MOD_B,0);
    }
    for (uint32_t i=0; i<24; ++i) {
        uint32_t channel=AE_SOUND_CHANNELS+AE_SC_SIZE*i;
        if (!ae_rd16(r,channel+AE_SC_ACTIVE)) continue;
        uint32_t owner=ae_rd16(r,channel+AE_SC_VOICE);
        if (owner!=(uint32_t)signed_handle && owner!=voice) continue;
        if ((uint32_t)signed_handle&0x8000) {
            if (!hw || !hw->set_voice_adsr) {
                ae_ram_fail(r,"AeSoundHw.set_voice_adsr not provided"); return;
            }
            hw->set_voice_adsr(hw->user,r,i,0,0);
            if (ae_ram_failed(r)) return;
            ae_wr16(r,channel+AE_SC_STATE,0);
        }
        ae_wr16(r,channel+AE_SC_RELEASE,1);
        if (!hw || !hw->voice_key_off) {
            ae_ram_fail(r,"AeSoundHw.voice_key_off not provided"); return;
        }
        hw->voice_key_off(hw->user,r,i<16?0:1,(uint16_t)(1u<<(i&15)));
        if (ae_ram_failed(r)) return;
        if (!hw->wait_loop) {
            ae_ram_fail(r,"AeSoundHw.wait_loop not provided"); return;
        }
        hw->wait_loop(hw->user,r,2);
        if (ae_ram_failed(r)) return;
    }
}

static int32_t product(int32_t a, int32_t b) { return (int32_t)((uint32_t)a*(uint32_t)b); }

/* Shared implementation so callers can keep the original temporary pair as native locals.
 * SDK normalization uses its already verified integer core, with halfword output truncation. */
static int32_t emitter_pan(AeRam *r, uint32_t pos, int32_t range, int16_t pan[2]) {
    pan[0]=pan[1]=0;
    uint32_t squared=0;
    for (uint32_t i=0; i<3; ++i) {
        int32_t delta=ae_rds16(r,AE_SOUND_LISTENER+2*i)-ae_rds16(r,pos+2*i);
        squared += (uint32_t)product(delta,delta);
    }
    int32_t distance=ae_sdk_SquareRoot0(r,(int32_t)squared);
    range=s16(range);
    if (range<distance || ae_ram_failed(r)) return 0;
    int32_t left=4096;
    if (distance>2048) {
        int32_t numerator=(int32_t)((uint32_t)(range-distance)<<12);
        int32_t denominator=range-2048;
        if (!denominator || (numerator==INT32_MIN && denominator==-1)) {
            ae_ram_fail(r,"SoundEmitterCalcPan: original divide trap"); return 0;
        }
        left=numerator/denominator;
    }
    int32_t right=left;
    if (ae_rd16(r,AE_SOUND_CAMERA+AE_CAM_PAN_ENABLED)) {
        int32_t camera_x=ae_rd16(r,AE_SOUND_CAMERA+AE_CAM_EYE_X);
        int32_t camera_z=ae_rd16(r,AE_SOUND_CAMERA+AE_CAM_EYE_Z);
        int32_t direction[3], facing[3];
        ae_sdk_normalize_core(r,s16(ae_rd16(r,pos)-camera_x),0,
                              s16(ae_rd16(r,pos+4)-camera_z),direction);
        if (ae_ram_failed(r)) return 0;
        ae_sdk_normalize_core(r,s16(ae_rd16(r,AE_SOUND_CAMERA+AE_CAM_TARGET_X)-camera_x),0,
                              s16(ae_rd16(r,AE_SOUND_CAMERA+AE_CAM_TARGET_Z)-camera_z),facing);
        if (ae_ram_failed(r)) return 0;
        int32_t cross=(int32_t)((uint32_t)product(-s16(facing[2]),s16(direction[0]))
                               +(uint32_t)product(s16(facing[0]),s16(direction[2])));
        if (cross>0) {
            right=product(right,4096-(cross>>12))/4096;
        } else {
            int32_t magnitude=(int32_t)(0u-(uint32_t)cross);
            left=product(left,4096-magnitude/4096)/4096;
        }
    }
    pan[0]=(int16_t)left; pan[1]=(int16_t)right;
    return right>=8 || left>=8;
}

/* SoundEmitterCalcPan @80087C8C. Output zeroing precedes reading pos (may alias). */
int32_t ae_SoundEmitterCalcPan(AeRam *r, uint32_t pos, int32_t range, uint32_t out) {
    ae_wr16(r,out+2,0);
    ae_wr16(r,out,0);
    int16_t pan[2];
    int32_t audible=emitter_pan(r,pos,range,pan);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r,out+2,pan[1]);
    ae_wr16(r,out,pan[0]);
    return audible;
}

static void print_error(AeRam *r, const AeSoundHw *hw, uint32_t fmt, uint32_t argc,
                        int32_t a, int32_t b, int32_t c) {
    if (!hw || !hw->printf_error) {
        ae_ram_fail(r,"AeSoundHw.printf_error not provided"); return;
    }
    hw->printf_error(hw->user,r,fmt,argc,a,b,c);
}

/* PlaySfxAt @80087A20: queue once this frame, start immediately, remember handle. */
int32_t ae_PlaySfxAt(AeRam *r, const AeSoundHw *hw, int32_t id, uint32_t pos) {
    uint32_t effect=AE_SOUND_EFFECTS+AE_SE_SIZE*(uint32_t)id;
    uint32_t bank_address=AE_SOUND_BANK_IDS+2*(uint32_t)ae_rds16(r,effect+AE_SE_BANK);
    if (ae_rds16(r,bank_address)<0) return -1;
    int16_t pan[2];
    if (!emitter_pan(r,pos,8192,pan) || ae_ram_failed(r)) return 0;
    if (ae_SfxIsQueued(r,s16(id))) return 0;
    uint32_t entry=queue_reserve(r,1,s16(id));
    if (!entry) return 0;
    ae_wr16(r,entry+AE_SQ_LEFT,pan[0]);
    ae_wr16(r,entry+AE_SQ_RIGHT,pan[1]);
    /* Re-read the bank index after queue writes (the original does so too). */
    bank_address=AE_SOUND_BANK_IDS+2*(uint32_t)ae_rds16(r,effect+AE_SE_BANK);
    int32_t voice=ae_SfxVoiceStartEx(r,hw,ae_rds16(r,bank_address),
                                    ae_rds16(r,effect+AE_SE_PROGRAM),ae_rds16(r,effect+AE_SE_NOTE),pan[0],pan[1],4096);
    if (ae_ram_failed(r)) return 0;
    ae_wr16(r,effect+AE_SE_LAST_VOICE,voice);
    if (s16(voice)<0) print_error(r,hw,0x800A622Cu,3,id,ae_rds16(r,effect+AE_SE_PROGRAM),ae_rds16(r,effect+AE_SE_NOTE));
    return 0;
}

/* SoundLoopStartEx @800882C4: failed starts retain the allocation's index write. */
int32_t ae_SoundLoopStartEx(AeRam *r, const AeSoundHw *hw, int32_t mode, int32_t id, int32_t extra) {
    uint32_t effect=AE_SOUND_EFFECTS+AE_SE_SIZE*((uint32_t)id&0xFFF);
    uint32_t bank_address=AE_SOUND_BANK_IDS+2*(uint32_t)ae_rds16(r,effect+AE_SE_BANK);
    if (ae_rds16(r,bank_address)<0) return -1;
    uint32_t loop=ae_SoundLoopAllocSlot(r);
    if (!loop) {
        print_error(r,hw,0x800A627Cu,1,(uint32_t)id&0xFFF,0,0);
        return -1;
    }
    int32_t voice;
    if (mode==0) {
        voice=ae_SfxVoiceStart(r,hw,ae_rds16(r,bank_address),
                              ae_rds16(r,effect+AE_SE_PROGRAM),ae_rds16(r,effect+AE_SE_NOTE));
    } else {
        voice=ae_SfxVoiceStartEx(r,hw,ae_rds16(r,bank_address),
                                ae_rds16(r,effect+AE_SE_PROGRAM),ae_rds16(r,effect+AE_SE_NOTE),1,1,4096);
    }
    if (ae_ram_failed(r)) return -1;
    voice=s16(voice);
    if (voice>=0) {
        ae_wr16(r,loop+AE_SL_ID,id);
        ae_wr16(r,loop+AE_SL_EXTRA,extra);
        ae_wr16(r,loop+AE_SL_VOICE,voice);
        ae_wr16(r,loop+AE_SL_ACTIVE,1);
        return voice;
    }
    print_error(r,hw,0x800A6240u,3,(uint32_t)id&0xFFF,
                ae_rds16(r,effect+AE_SE_PROGRAM),ae_rds16(r,effect+AE_SE_NOTE));
    return -1;
}

/* SoundLoopStopAndFree @800880D0: full-width sign test before halfword truncation. */
int32_t ae_SoundLoopStopAndFree(AeRam *r, const AeSoundHw *hw, int32_t id, int32_t voice) {
    if (voice>=0) {
        ae_SfxVoiceStop(r,hw,s16(voice));
        if (ae_ram_failed(r)) return 0;
        ae_SoundLoopFree(r,s16(id),s16(voice));
    }
    return 0;
}
