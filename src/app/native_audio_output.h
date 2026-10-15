#ifndef AE_NATIVE_AUDIO_OUTPUT_H
#define AE_NATIVE_AUDIO_OUTPUT_H
int ae_audio_start(const char *bank_path,const char *exe_path);
void ae_audio_stop(void);
int ae_audio_sync(void); /* 0 ready, 1 synchronization failure; AeSoundHw.wait_idle */
const char *ae_audio_error(void);
int ae_audio_frames(void);
int ae_audio_play_effect(int id);
int ae_audio_stage_effect(void);
#endif
