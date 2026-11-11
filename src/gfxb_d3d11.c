// SNKHD - backend Direct3D 11 (Windows): swap chain DXGI flip model, V-SYNC con Present(1)
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <SDL_syswm.h>
#include <stdio.h>
#include <string.h>
#include "gfxb.h"

static SDL_Window *gwin;
static HWND hwnd;
static ID3D11Device *dev;
static ID3D11DeviceContext *dc;
static IDXGISwapChain1 *sc1;
static IDXGISwapChain *sc;
static ID3D11RenderTargetView *bb_rtv;
static int bb_w, bb_h;
static ID3D11VertexShader *vs;
static ID3D11PixelShader *ps_basic, *ps_crt;
static ID3D11InputLayout *layout;
static ID3D11Buffer *vbuf, *cbuf;
static ID3D11SamplerState *smp;
static ID3D11BlendState *blend_on;
static ID3D11RasterizerState *rast;
static int vsync_on = 1;
static int flip_model;
#define VBUF_VERTS (6 * 512)

// texture
#define MAXTEX 256
typedef struct { ID3D11Texture2D *t; ID3D11ShaderResourceView *srv; int w, h, fmt; } dtex_t;
static dtex_t texs[MAXTEX];
// buffer intermedio
static ID3D11Texture2D *rt_tex; static ID3D11RenderTargetView *rt_rtv; static ID3D11ShaderResourceView *rt_srv;
static int rt_w, rt_h;
static int cur_w, cur_h;    // destinazione corrente

typedef struct { float screen[2]; float pad0[2]; float color[4]; int mode; float blur; int scan; float pad1; float tsz[2]; float scl[2]; } cb_t;
static float sharp_p[4] = { 1, 1, 1, 1 };
void gfxb_set_sharp(float tw, float th, float sx, float sy) { sharp_p[0] = tw; sharp_p[1] = th; sharp_p[2] = sx; sharp_p[3] = sy; }

static const char *hlsl =
"cbuffer CB : register(b0) { float2 uScreen; float2 pad0; float4 uColor; int uMode; float uBlur; int uScan; float pad1; float2 uTsz; float2 uScl; };\n"
"struct VSI { float2 pos : POSITION; float2 uv : TEXCOORD0; };\n"
"struct PSI { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
"Texture2D tex : register(t0); SamplerState smp : register(s0);\n"
"PSI vs_main(VSI i) { PSI o; o.pos = float4(i.pos.x / uScreen.x * 2.0 - 1.0, 1.0 - i.pos.y / uScreen.y * 2.0, 0.0, 1.0); o.uv = i.uv; return o; }\n"
"float4 ps_basic(PSI i) : SV_Target {\n"
"  if (uMode == 0) return uColor;\n"
"  if (uMode == 3) {\n"
"    float2 texel = i.uv * uTsz; float2 sc = max(uScl, 1.0); float2 tf = floor(texel);\n"
"    float2 rr = 0.5 - 0.5 / sc; float2 cd = texel - tf - 0.5;\n"
"    float2 f = (cd - clamp(cd, -rr, rr)) * sc + 0.5;\n"
"    return tex.Sample(smp, (tf + f) / uTsz) * uColor;\n"
"  }\n"
"  float4 t = tex.Sample(smp, i.uv);\n"
"  if (uMode == 1) return t * uColor;\n"
"  return float4(uColor.rgb, uColor.a * t.r);\n"
"}\n"
// Port di CRT-ZERO.fx (CRT-Frutbunn semplificato, 320x240)
"static const float2 tsize = float2(320.0, 240.0);\n"
"float3 S(float2 uv) { return tex.Sample(smp, uv).rgb; }\n"
"float4 ps_crt(PSI i) : SV_Target {\n"
"  float2 uv = i.uv; float2 st = uv - 0.5;\n"
"  float2 r = tsize * 2.0; float b = uBlur / (r.x / r.y); float dx = b / r.x, dy = b / r.y;\n"
"  float3 c = S(uv + float2(-dx,-dy)) * 0.077847 + S(uv + float2(-dx,0)) * 0.123317 + S(uv + float2(-dx,dy)) * 0.077847;\n"
"  c += S(uv + float2(0,-dy)) * 0.123317 + S(uv) * 0.195346 + S(uv + float2(0,dy)) * 0.123317;\n"
"  c += S(uv + float2(dx,-dy)) * 0.077847 + S(uv + float2(dx,0)) * 0.123317 + S(uv + float2(dx,dy)) * 0.077847;\n"
"  if (uScan != 0) { float s = 2.5 + uScreen.y * (1.0 / tsize.y); float j = cos(st.y * tsize.y * s) * 0.25 * (uScan == 2 ? 0.5 : 1.0); c -= j * c; }\n"
"  return float4(c, 1.0);\n"
"}\n";

typedef HRESULT (WINAPI *PFN_SNK_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*,
    LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
static PFN_SNK_D3DCompile snk_D3DCompile;

static ID3DBlob *compile(const char *entry, const char *target) {
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr = snk_D3DCompile(hlsl, strlen(hlsl), "snkhd", NULL, NULL, entry, target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);
    if (FAILED(hr)) {
        if (err) { fprintf(stderr, "%s\n", (const char*)ID3D10Blob_GetBufferPointer(err)); ID3D10Blob_Release(err); }
        return NULL;
    }
    if (err) ID3D10Blob_Release(err);
    return code;
}

static void make_bb_rtv(void) {
    ID3D11Texture2D *bb = NULL;
    if (SUCCEEDED(IDXGISwapChain_GetBuffer(sc, 0, &IID_ID3D11Texture2D, (void**)&bb))) {
        ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource*)bb, NULL, &bb_rtv);
        D3D11_TEXTURE2D_DESC d; ID3D11Texture2D_GetDesc(bb, &d);
        bb_w = d.Width; bb_h = d.Height;
        ID3D11Texture2D_Release(bb);
    }
}

Uint32 gfxb_window_flags(void) { return 0; }

int gfxb_init(SDL_Window *w, int vsync, char *info, int infolen) {
    gwin = w;
    vsync_on = vsync;
    SDL_SysWMinfo wm; SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(w, &wm)) return 0;
    hwnd = wm.info.win.window;

    HMODULE dcm = LoadLibraryA("d3dcompiler_47.dll");
    if (!dcm) dcm = LoadLibraryA("d3dcompiler_43.dll");
    if (!dcm) { snprintf(info, infolen, "d3dcompiler not found"); return 0; }
    snk_D3DCompile = (PFN_SNK_D3DCompile)(void*)GetProcAddress(dcm, "D3DCompile");
    if (!snk_D3DCompile) return 0;

    D3D_FEATURE_LEVEL fl[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, fl, 3, D3D11_SDK_VERSION, &dev, NULL, &dc);
    if (FAILED(hr)) { snprintf(info, infolen, "D3D11CreateDevice failed %08lx", (unsigned long)hr); return 0; }

    IDXGIDevice1 *xdev = NULL; IDXGIAdapter *ad = NULL; IDXGIFactory2 *fac = NULL;
    ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice1, (void**)&xdev);
    IDXGIDevice1_SetMaximumFrameLatency(xdev, 1);
    IDXGIDevice1_GetAdapter(xdev, &ad);
    IDXGIAdapter_GetParent(ad, &IID_IDXGIFactory2, (void**)&fac);
    DXGI_ADAPTER_DESC ades; IDXGIAdapter_GetDesc(ad, &ades);
    char name[128]; WideCharToMultiByte(CP_UTF8, 0, ades.Description, -1, name, sizeof name, NULL, NULL);
    if (!fac) { snprintf(info, infolen, "DXGI 1.2 not available"); return 0; }

    int ww, wh; SDL_GetWindowSizeInPixels(w, &ww, &wh);
    DXGI_SWAP_CHAIN_DESC1 d; memset(&d, 0, sizeof d);
    d.Width = ww; d.Height = wh;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = 2;
    d.SwapEffect = 4; // DXGI_SWAP_EFFECT_FLIP_DISCARD (Windows 10)
    flip_model = 1;
    hr = IDXGIFactory2_CreateSwapChainForHwnd(fac, (IUnknown*)dev, hwnd, &d, NULL, NULL, &sc1);
    if (FAILED(hr)) {
        d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // Windows 8
        hr = IDXGIFactory2_CreateSwapChainForHwnd(fac, (IUnknown*)dev, hwnd, &d, NULL, NULL, &sc1);
    }
    if (FAILED(hr)) {
        d.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; d.BufferCount = 1; flip_model = 0;
        hr = IDXGIFactory2_CreateSwapChainForHwnd(fac, (IUnknown*)dev, hwnd, &d, NULL, NULL, &sc1);
    }
    if (FAILED(hr)) { snprintf(info, infolen, "swap chain failed %08lx", (unsigned long)hr); return 0; }
    IDXGIFactory2_MakeWindowAssociation(fac, hwnd, DXGI_MWA_NO_ALT_ENTER);
    IDXGISwapChain1_QueryInterface(sc1, &IID_IDXGISwapChain, (void**)&sc);
    IDXGIFactory2_Release(fac); IDXGIAdapter_Release(ad); IDXGIDevice1_Release(xdev);
    make_bb_rtv();
    snprintf(info, infolen, "Direct3D 11 | %s | swap %s", name, flip_model ? "flip" : "discard");

    ID3DBlob *bvs = compile("vs_main", "vs_4_0"), *bps = compile("ps_basic", "ps_4_0"), *bcrt = compile("ps_crt", "ps_4_0");
    if (!bvs || !bps || !bcrt) { snprintf(info, infolen, "shader compilation failed"); return 0; }
    ID3D11Device_CreateVertexShader(dev, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), NULL, &vs);
    ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(bps), ID3D10Blob_GetBufferSize(bps), NULL, &ps_basic);
    ID3D11Device_CreatePixelShader(dev, ID3D10Blob_GetBufferPointer(bcrt), ID3D10Blob_GetBufferSize(bcrt), NULL, &ps_crt);
    D3D11_INPUT_ELEMENT_DESC ie[2] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
    ID3D11Device_CreateInputLayout(dev, ie, 2, ID3D10Blob_GetBufferPointer(bvs), ID3D10Blob_GetBufferSize(bvs), &layout);
    ID3D10Blob_Release(bvs); ID3D10Blob_Release(bps); ID3D10Blob_Release(bcrt);

    D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof bd);
    bd.ByteWidth = VBUF_VERTS * 16; bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Device_CreateBuffer(dev, &bd, NULL, &vbuf);
    bd.ByteWidth = sizeof(cb_t); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ID3D11Device_CreateBuffer(dev, &bd, NULL, &cbuf);

    D3D11_SAMPLER_DESC sd; memset(&sd, 0, sizeof sd);
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11Device_CreateSamplerState(dev, &sd, &smp);

    D3D11_BLEND_DESC bl; memset(&bl, 0, sizeof bl);
    bl.RenderTarget[0].BlendEnable = TRUE;
    bl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11Device_CreateBlendState(dev, &bl, &blend_on);

    D3D11_RASTERIZER_DESC rd; memset(&rd, 0, sizeof rd);
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    ID3D11Device_CreateRasterizerState(dev, &rd, &rast);
    return 1;
}

void gfxb_quit(void) {
    if (sc) { IDXGISwapChain_SetFullscreenState(sc, FALSE, NULL); }
}

void gfxb_set_vsync(int on) { vsync_on = on; }

void gfxb_drawable(int *w, int *h) { SDL_GetWindowSizeInPixels(gwin, w, h); }

static void set_target(ID3D11RenderTargetView *rtv, int w, int h) {
    ID3D11DeviceContext_OMSetRenderTargets(dc, 1, &rtv, NULL);
    D3D11_VIEWPORT vp = { 0, 0, (float)w, (float)h, 0, 1 };
    ID3D11DeviceContext_RSSetViewports(dc, 1, &vp);
    cur_w = w; cur_h = h;
    float black[4] = { 0, 0, 0, 1 };
    ID3D11DeviceContext_ClearRenderTargetView(dc, rtv, black);
}

void gfxb_begin(int w, int h) {
    if (w != bb_w || h != bb_h) {
        ID3D11DeviceContext_OMSetRenderTargets(dc, 0, NULL, NULL);
        if (bb_rtv) { ID3D11RenderTargetView_Release(bb_rtv); bb_rtv = NULL; }
        IDXGISwapChain_ResizeBuffers(sc, 0, w, h, DXGI_FORMAT_UNKNOWN, 0);
        make_bb_rtv();
    }
    set_target(bb_rtv, bb_w, bb_h);
}

unsigned gfxb_tex_create(int w, int h, int fmt, const void *data, int mips) {
    int slot = 0;
    for (int i = 1; i < MAXTEX; ++i) if (!texs[i].t) { slot = i; break; }
    if (!slot) return 0;
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1;
    d.SampleDesc.Count = 1;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d.Usage = D3D11_USAGE_DEFAULT;
    if (fmt == TEXF_R8) d.Format = DXGI_FORMAT_R8_UNORM;
    else if (fmt == TEXF_BGRA8_DYN) {
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.Usage = D3D11_USAGE_DYNAMIC; d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    }
    else d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    if (mips) { d.MipLevels = 0; d.BindFlags |= D3D11_BIND_RENDER_TARGET; d.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS; }
    D3D11_SUBRESOURCE_DATA sd, *psd = NULL;
    if (data && !mips) {
        sd.pSysMem = data; sd.SysMemPitch = w * (fmt == TEXF_R8 ? 1 : 4); sd.SysMemSlicePitch = 0;
        psd = &sd;
    }
    if (FAILED(ID3D11Device_CreateTexture2D(dev, &d, psd, &texs[slot].t))) return 0;
    ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource*)texs[slot].t, NULL, &texs[slot].srv);
    if (data && mips) {
        ID3D11DeviceContext_UpdateSubresource(dc, (ID3D11Resource*)texs[slot].t, 0, NULL, data, w * 4, 0);
        ID3D11DeviceContext_GenerateMips(dc, texs[slot].srv);
    }
    texs[slot].w = w; texs[slot].h = h; texs[slot].fmt = fmt;
    return (unsigned)slot;
}

void gfxb_tex_update(unsigned t, const void *data, int w, int h, int pitch_px) {
    if (!t || t >= MAXTEX || !texs[t].t) return;
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ID3D11DeviceContext_Map(dc, (ID3D11Resource*)texs[t].t, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    const unsigned char *src = (const unsigned char*)data;
    for (int y = 0; y < h; ++y)
        memcpy((unsigned char*)m.pData + (size_t)y * m.RowPitch, src + (size_t)y * pitch_px * 4, (size_t)w * 4);
    ID3D11DeviceContext_Unmap(dc, (ID3D11Resource*)texs[t].t, 0);
}

void gfxb_tex_free(unsigned t) {
    if (!t || t >= MAXTEX || !texs[t].t) return;
    ID3D11ShaderResourceView_Release(texs[t].srv);
    ID3D11Texture2D_Release(texs[t].t);
    memset(&texs[t], 0, sizeof texs[t]);
}

static void draw_raw(ID3D11ShaderResourceView *srv, ID3D11PixelShader *ps, const cb_t *cb, const float *v, int nv, int blend) {
    D3D11_MAPPED_SUBRESOURCE m;
    if (nv > VBUF_VERTS) nv = VBUF_VERTS;
    if (FAILED(ID3D11DeviceContext_Map(dc, (ID3D11Resource*)vbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    memcpy(m.pData, v, (size_t)nv * 16);
    ID3D11DeviceContext_Unmap(dc, (ID3D11Resource*)vbuf, 0);
    if (SUCCEEDED(ID3D11DeviceContext_Map(dc, (ID3D11Resource*)cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, cb, sizeof *cb);
        ID3D11DeviceContext_Unmap(dc, (ID3D11Resource*)cbuf, 0);
    }
    UINT stride = 16, off = 0;
    ID3D11DeviceContext_IASetInputLayout(dc, layout);
    ID3D11DeviceContext_IASetVertexBuffers(dc, 0, 1, &vbuf, &stride, &off);
    ID3D11DeviceContext_IASetPrimitiveTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(dc, vs, NULL, 0);
    ID3D11DeviceContext_VSSetConstantBuffers(dc, 0, 1, &cbuf);
    ID3D11DeviceContext_PSSetShader(dc, ps, NULL, 0);
    ID3D11DeviceContext_PSSetConstantBuffers(dc, 0, 1, &cbuf);
    ID3D11DeviceContext_PSSetSamplers(dc, 0, 1, &smp);
    ID3D11DeviceContext_PSSetShaderResources(dc, 0, 1, &srv);
    ID3D11DeviceContext_RSSetState(dc, rast);
    float bf[4] = { 0, 0, 0, 0 };
    ID3D11DeviceContext_OMSetBlendState(dc, blend ? blend_on : NULL, bf, 0xffffffff);
    ID3D11DeviceContext_Draw(dc, nv, 0);
}

void gfxb_draw(unsigned tex, int mode, const float c[4], const float *v, int nv) {
    cb_t cb; memset(&cb, 0, sizeof cb);
    cb.screen[0] = (float)cur_w; cb.screen[1] = (float)cur_h;
    memcpy(cb.color, c, sizeof cb.color);
    cb.mode = mode;
    memcpy(cb.tsz, sharp_p, sizeof sharp_p);
    ID3D11ShaderResourceView *srv = (tex && tex < MAXTEX) ? texs[tex].srv : NULL;
    if (mode != 0 && !srv) return;
    draw_raw(srv, ps_basic, &cb, v, nv, 1);
}

void gfxb_rt_begin(void) {
    if (!rt_tex || rt_w != bb_w || rt_h != bb_h) {
        if (rt_tex) { ID3D11RenderTargetView_Release(rt_rtv); ID3D11ShaderResourceView_Release(rt_srv); ID3D11Texture2D_Release(rt_tex); }
        D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
        d.Width = bb_w; d.Height = bb_h; d.MipLevels = 1; d.ArraySize = 1;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ID3D11Device_CreateTexture2D(dev, &d, NULL, &rt_tex);
        ID3D11Device_CreateRenderTargetView(dev, (ID3D11Resource*)rt_tex, NULL, &rt_rtv);
        ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource*)rt_tex, NULL, &rt_srv);
        rt_w = bb_w; rt_h = bb_h;
    }
    ID3D11ShaderResourceView *nul = NULL;
    ID3D11DeviceContext_PSSetShaderResources(dc, 0, 1, &nul);
    set_target(rt_rtv, rt_w, rt_h);
}

static float blur_scale = 1.0f;
void gfxb_set_blur_scale(float s) { blur_scale = s > 0 ? s : 1.0f; }

void gfxb_rt_crt(int scan, int blur) {
    ID3D11DeviceContext_OMSetRenderTargets(dc, 1, &bb_rtv, NULL);
    D3D11_VIEWPORT vp = { 0, 0, (float)bb_w, (float)bb_h, 0, 1 };
    ID3D11DeviceContext_RSSetViewports(dc, 1, &vp);
    cur_w = bb_w; cur_h = bb_h;
    cb_t cb; memset(&cb, 0, sizeof cb);
    cb.screen[0] = (float)bb_w; cb.screen[1] = (float)bb_h;
    // BLUR: ON = kernel a 320x240, 50% = kernel a 480x360 (equivale a forza 320/480), OFF = 0
    cb.blur = (blur == 2 ? (320.0f / 480.0f) : blur ? 1.0f : 0.0f) * blur_scale;
    cb.scan = scan == 2 ? 2 : scan ? 1 : 0;   // SCANLINES: 1 = ON (1.00), 2 = 50% (0.50)
    float W = (float)bb_w, H = (float)bb_h;
    float q[6][4] = { {0,0,0,0},{W,0,1,0},{W,H,1,1},{0,0,0,0},{W,H,1,1},{0,H,0,1} };
    draw_raw(rt_srv, ps_crt, &cb, &q[0][0], 6, 0);
    ID3D11ShaderResourceView *nul = NULL;
    ID3D11DeviceContext_PSSetShaderResources(dc, 0, 1, &nul);
}

void gfxb_present(void) {
    IDXGISwapChain_Present(sc, vsync_on ? 1 : 0, 0);
}

// ------------------------------------------------------------------ cattura (giochi esterni)
void *gfxb_d3d_device(void) { return dev; }
void *gfxb_d3d_context(void) { return dc; }
unsigned gfxb_tex_create_gpu(int w, int h) {
    int slot = 0;
    for (int i = 1; i < MAXTEX; ++i) if (!texs[i].t) { slot = i; break; }
    if (!slot) return 0;
    D3D11_TEXTURE2D_DESC d; memset(&d, 0, sizeof d);
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.SampleDesc.Count = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(ID3D11Device_CreateTexture2D(dev, &d, NULL, &texs[slot].t))) return 0;
    ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource*)texs[slot].t, NULL, &texs[slot].srv);
    texs[slot].w = w; texs[slot].h = h; texs[slot].fmt = TEXF_RGBA8;
    return (unsigned)slot;
}
void *gfxb_tex_resource(unsigned t) { return (t && t < MAXTEX) ? (void*)texs[t].t : NULL; }
