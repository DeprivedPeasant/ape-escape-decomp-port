/* Mesh projection and emission boundaries owned by the native renderer. */
#include "ae_effects.h"

static int32_t effects_mesh_renderer_call(AeRam *r, const AeEffectsHooks *hooks,
                                          AeEffectsCallFn callback, const char *name,
                                          uint32_t a0, uint32_t a1, uint32_t a2) {
    if (hooks == NULL || callback == NULL) {
        ae_ram_fail(r, "AeEffectsHooks.%s not provided", name);
        return 0;
    }
    return callback(hooks->user, r, a0, a1, a2, 0);
}

/* MeshTransform @800160EC is entirely the render-side RTPT loop: it loads the
 * supplied transform, projects each three-vertex group, and writes screen-space
 * FIFOs. The native renderer consumes these source values and owns projection. */
void ae_MeshTransform(AeRam *r, const AeEffectsHooks *hooks,
                      uint32_t mesh, uint32_t matrix, uint32_t output) {
    (void)effects_mesh_renderer_call(r, hooks,
                                     hooks == NULL ? NULL : hooks->GpuProjectMesh,
                                     "GpuProjectMesh", mesh, matrix, output);
}

/* MeshDraw @80017E5C performs GTE lighting and emits the model's GPU packets.
 * Those operations belong to the native renderer; gameplay selection and
 * transform construction remain in FlyingMeshesUpdateDraw. */
void ae_MeshDraw(AeRam *r, const AeEffectsHooks *hooks,
                 uint32_t ordering_table, uint32_t screen_vertices,
                 uint32_t mesh) {
    (void)effects_mesh_renderer_call(r, hooks,
                                     hooks == NULL ? NULL : hooks->GpuDrawMesh,
                                     "GpuDrawMesh", ordering_table,
                                     screen_vertices, mesh);
}
