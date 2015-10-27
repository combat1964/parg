#include <par.h>
#include <parwin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_TABLE(F)          \
    F(P_SIMPLE, "p_simple")     \
    F(A_POSITION, "a_position") \
    F(U_MVP, "u_mvp")           \
    F(U_DENSITY, "u_density")   \
    F(U_POINTSIZE, "u_pointsize")

TOKEN_TABLE(PAR_TOKEN_DECLARE);

#define ASSET_TABLE(F)                  \
    F(SHADER_SIMPLE, "trillium.glsl")   \
    F(TEXTURE_TRILLIUM, "trillium.png") \
    F(BUFFER_BLUENOISE, "bluenoise.bin")
ASSET_TABLE(PAR_TOKEN_DECLARE);

par_buffer* ptsvbo;
par_bluenoise_context* ctx;
float pointscale = 1;
const float gray = 0.8;
const float fovy = 16 * PAR_TWOPI / 180;
const float worldwidth = 1;
const int maxpts = 100000;

#define clamp(x, min, max) ((x < min) ? min : ((x > max) ? max : x))
#define sqr(a) (a * a)

void init(float winwidth, float winheight, float pixratio)
{
    par_buffer* buffer;
    void* buffer_data;

    buffer = par_buffer_slurp_asset(BUFFER_BLUENOISE, &buffer_data);
    ctx = par_bluenoise_create(buffer_data, par_buffer_length(buffer), maxpts);
    par_buffer_free(buffer);

    buffer = par_buffer_slurp_asset(TEXTURE_TRILLIUM, &buffer_data);
    par_bluenoise_density_from_gray(ctx, buffer_data + 12, 3500, 3500, 4);
    par_buffer_free(buffer);

    ptsvbo = par_buffer_alloc(maxpts * sizeof(float) * 3, PAR_GPU_ARRAY);
    par_state_clearcolor((Vector4){gray, gray, gray, 1});
    par_state_depthtest(0);
    par_state_cullfaces(0);
    par_shader_load_from_asset(SHADER_SIMPLE);
    float worldheight = worldwidth;
    par_zcam_init(worldwidth, worldheight, fovy);
}

void draw()
{
    float lbrt[4];
    par_zcam_get_viewport(lbrt);
    float left = lbrt[0];
    float bottom = lbrt[1];
    float right = lbrt[2];
    float top = lbrt[3];

    int npts;
    float* cpupts =
        par_bluenoise_generate(ctx, 30000, left, bottom, right, top, &npts);
    float* gpupts = par_buffer_lock(ptsvbo, PAR_WRITE);
    memcpy(gpupts, cpupts, npts * 3 * sizeof(float));
    par_buffer_unlock(ptsvbo);

    Matrix4 view;
    Matrix4 projection;
    par_zcam_matrices(&projection, &view);
    Matrix4 model = M4MakeIdentity();
    Matrix4 modelview = M4Mul(view, model);
    Matrix4 mvp = M4Mul(projection, modelview);
    par_draw_clear();
    par_shader_bind(P_SIMPLE);
    par_uniform_matrix4f(U_MVP, &mvp);
    par_uniform1f(U_POINTSIZE, 2.5f * pointscale);
    par_varray_enable(ptsvbo, A_POSITION, 3, PAR_FLOAT, 0, 0);
    par_draw_points(npts);
}

int tick(float winwidth, float winheight, float pixratio, float seconds)
{
    pointscale = pixratio;
    par_zcam_tick(winwidth / winheight, seconds);
    return par_zcam_has_moved();
}

void dispose()
{
    par_shader_free(P_SIMPLE);
    par_buffer_free(ptsvbo);
}

void input(par_event evt, float x, float y, float z)
{
    switch (evt) {
    case PAR_EVENT_DOWN:
        par_zcam_grab_begin(x, y);
        break;
    case PAR_EVENT_UP:
        par_zcam_grab_update(x, y, z);
        par_zcam_grab_end();
        break;
    case PAR_EVENT_MOVE:
        par_zcam_grab_update(x, y, z);
        break;
    }
}

int main(int argc, char* argv[])
{
    TOKEN_TABLE(PAR_TOKEN_DEFINE);
    ASSET_TABLE(PAR_ASSET_TABLE);
    par_window_setargs(argc, argv);
    par_window_oninit(init);
    par_window_ontick(tick);
    par_window_ondraw(draw);
    par_window_onexit(dispose);
    par_window_oninput(input);
    return par_window_exec(700, 350, 1);
}