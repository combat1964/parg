#include <parg.h>
#include <parwin.h>
#include <assert.h>
#include <sds.h>
#include <stdio.h>

#define PAR_MSQUARES_IMPLEMENTATION
#include <par/par_msquares.h>

#define TOKEN_TABLE(F)          \
    F(P_LANDMASS, "p_landmass") \
    F(P_OCEAN, "p_ocean")       \
    F(P_SOLID, "p_solid")       \
    F(A_POSITION, "a_position") \
    F(U_MVP, "u_mvp")           \
    F(U_COLOR, "u_color")       \
    F(U_MAGNIFICATION, "u_magnification")
TOKEN_TABLE(PARG_TOKEN_DECLARE);

#define ASSET_TABLE(F)              \
    F(SHADER_DEFAULT, "ztex.glsl")  \
    F(TEXTURE_PAPER, "paper.png")   \
    F(TEXTURE_EUROPE, "europe.png") \
    F(TEXTURE_OCEAN, "water.png")
ASSET_TABLE(PARG_TOKEN_DECLARE);

const float fovy = 16 * PARG_TWOPI / 180;
int mode_highp = 1;
parg_mesh* landmass_mesh;
parg_mesh* ocean_mesh;
parg_mesh* quad_mesh = 0;
parg_texture* ocean_texture;
parg_texture* paper_texture;
parg_aar quadrect = {0, 0, 0, 0};
int quad_dirty = 1;

void init(float winwidth, float winheight, float pixratio)
{
    printf("Press P to highlight the current slippy area.\n"
        "Spacebar to toggle texture modes.\n");
    parg_state_clearcolor((Vector4){0.43, 0.61, 0.8, 1});
    parg_state_cullfaces(1);
    parg_state_depthtest(0);
    parg_shader_load_from_asset(SHADER_DEFAULT);
    parg_zcam_init(1, 1, fovy);
    parg_zcam_grab_update(0.5, 0.5, 30);
    ocean_texture = parg_texture_from_asset(TEXTURE_OCEAN);
    paper_texture = parg_texture_from_asset(TEXTURE_PAPER);

    // Decode the europe image and pass it into msquares.
    int* rawdata;
    parg_buffer* colorbuf =
        parg_buffer_slurp_asset(TEXTURE_EUROPE, (void*) &rawdata);
    int width = *rawdata++;
    int height = *rawdata++;
    int ncomps = *rawdata++;
    parg_texture_fliprows(rawdata, width * ncomps, height);
    par_msquares_meshlist* mlist = par_msquares_color((parg_byte*) rawdata,
            width, height, 16, 0xffa0d9e1, 4,
            PAR_MSQUARES_DUAL | PAR_MSQUARES_HEIGHTS | PAR_MSQUARES_SIMPLIFY);
    printf("%d meshes\n", par_msquares_get_count(mlist));

    par_msquares_mesh const* mesh;
    mesh = par_msquares_get_mesh(mlist, 0);
    landmass_mesh = parg_mesh_create(
        mesh->points, mesh->npoints, mesh->triangles, mesh->ntriangles);

    mesh = par_msquares_get_mesh(mlist, 1);
    ocean_mesh = parg_mesh_create(
        mesh->points, mesh->npoints, mesh->triangles, mesh->ntriangles);

    parg_buffer_unlock(colorbuf);
    par_msquares_free(mlist);
}

void draw()
{
    if (quad_dirty) {
        quad_dirty = 0;
        parg_mesh_free(quad_mesh);
        quad_mesh = parg_mesh_aar(quadrect);
    }

    DMatrix4 view, projection;
    parg_zcam_dmatrices(&projection, &view);
    DMatrix4 model = DM4MakeTranslation((DVector3){-0.5, -0.5, -1});
    Matrix4 mvp = M4MakeFromDM4(DM4Mul(projection, DM4Mul(view, model)));
    float mag = parg_zcam_get_magnification();
    const Vector4 BLACK = {0, 0, 0, 1};
    const Vector4 SEMI = {1, 1, 1, 0.5};

    parg_draw_clear();
    parg_shader_bind(P_OCEAN);
    parg_uniform_matrix4f(U_MVP, &mvp);
    parg_uniform1f(U_MAGNIFICATION, mag);
    parg_texture_bind(ocean_texture, 0);
    parg_varray_bind(parg_mesh_index(ocean_mesh));
    parg_varray_enable(
        parg_mesh_coord(ocean_mesh), A_POSITION, 3, PARG_FLOAT, 0, 0);
    parg_draw_triangles_u16(0, parg_mesh_ntriangles(ocean_mesh));
    parg_shader_bind(P_SOLID);
    parg_uniform_matrix4f(U_MVP, &mvp);
    parg_uniform4f(U_COLOR, &BLACK);
    parg_varray_bind(parg_mesh_index(landmass_mesh));
    parg_varray_enable(
        parg_mesh_coord(landmass_mesh), A_POSITION, 3, PARG_FLOAT, 0, 0);
    parg_draw_wireframe_triangles_u16(0, parg_mesh_ntriangles(landmass_mesh));
    parg_shader_bind(P_LANDMASS);
    parg_uniform_matrix4f(U_MVP, &mvp);
    parg_uniform1f(U_MAGNIFICATION, mag);
    parg_texture_bind(paper_texture, 0);
    parg_draw_triangles_u16(0, parg_mesh_ntriangles(landmass_mesh));

    model = DM4MakeTranslation((DVector3){0, 0, 0});
    mvp = M4MakeFromDM4(DM4Mul(projection, DM4Mul(view, model)));
    parg_state_blending(1);
    parg_shader_bind(P_SOLID);
    parg_uniform_matrix4f(U_MVP, &mvp);
    parg_varray_enable(
        parg_mesh_coord(quad_mesh), A_POSITION, 2, PARG_FLOAT, 0, 0);
    parg_uniform4f(U_COLOR, &SEMI);
    parg_draw_one_quad();
    parg_state_blending(0);
}

int tick(float winwidth, float winheight, float pixratio, float seconds)
{
    parg_zcam_tick(winwidth / winheight, seconds);
    return parg_zcam_has_moved() || quad_dirty;
}

void dispose()
{
    parg_mesh_free(landmass_mesh);
    parg_mesh_free(ocean_mesh);
    parg_mesh_free(quad_mesh);
    parg_texture_free(ocean_texture);
    parg_texture_free(paper_texture);
}

void input(parg_event evt, float x, float y, float z)
{
    switch (evt) {
    case PARG_EVENT_KEYPRESS:
        if ((char) x == ' ') {
            mode_highp = !mode_highp;
            printf("Precision %s.\n", mode_highp ? "on" : "off");
        }
        if ((char) x == 'P') {
            Vector2 mapsize = {1, 1};
            parg_aar rect = parg_zcam_get_rectangle();
            parg_tilerange tiles;
            parg_aar_to_tilerange(rect, mapsize, &tiles);
            quadrect = parg_aar_from_tilerange(tiles, mapsize);
            quad_dirty = 1;
        }
        break;
    case PARG_EVENT_DOWN:
        parg_zcam_grab_begin(x, y);
        break;
    case PARG_EVENT_UP:
        parg_zcam_grab_update(x, y, z);
        parg_zcam_grab_end();
        break;
    case PARG_EVENT_MOVE:
        parg_zcam_grab_update(x, y, z);
        break;
    default:
        break;
    }
}

void message(const char* msg)
{
    if (!strcmp(msg, "high")) {
        mode_highp = 1;
    } else if (!strcmp(msg, "low")) {
        mode_highp = 0;
    }
}

int main(int argc, char* argv[])
{
    TOKEN_TABLE(PARG_TOKEN_DEFINE);
    ASSET_TABLE(PARG_ASSET_TABLE);
    parg_window_setargs(argc, argv);
    parg_window_oninit(init);
    parg_window_ontick(tick);
    parg_window_ondraw(draw);
    parg_window_onexit(dispose);
    parg_window_oninput(input);
    parg_window_onmessage(message);
    return parg_window_exec(500, 500, 1);
}