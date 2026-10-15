#ifndef AE_GAME_FRAME_HOST_H
#define AE_GAME_FRAME_HOST_H
/* Original game entry points that a native host can supply until ported.
 * Meaningful arguments retain their original a0-a3 words. */
typedef int32_t (*AeGameFrameFn)(void *, AeRam *, uint32_t, uint32_t, uint32_t, uint32_t);
typedef struct AeGameFrameHost {
    AeGameFrameFn BeginFrame;
    AeGameFrameFn DrawTerrain, DrawAreaProps;
    AeGameFrameFn QueueFrameFlip;
    AeGameFrameFn WaitFrameFlip;
} AeGameFrameHost;
#endif
