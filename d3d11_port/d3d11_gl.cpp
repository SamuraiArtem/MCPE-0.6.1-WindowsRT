// ============================================================
//  d3d11_gl.cpp -- GL 1.x -> Direct3D 11 shim for MCPE 0.6.1
//  Implements the (small) desktop-GL surface the game uses on
//  top of a D3D11 device. Requires nothing from opengl32/glew.
//
//  Interception points (defined here, matching wingdi decls):
//    wglCreateContext / wglMakeCurrent / wglDeleteContext
//    SwapBuffers
//  All other gl* symbols are declared in include/gl/GL.h and
//  defined below.
// ============================================================

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <map>
#include <d3dcompiler.h>

#include "gl/GL.h"
#include "gl/glew.h"

// ------------------------------------------------------------------
// small helpers
// ------------------------------------------------------------------
#ifdef MCPE_D3D11_NO_LOG
#define Log(...) ((void)0)
#define LogF(...) ((void)0)
#define MCPE_D3D11_LOG_BUDGET 0
#else
static void Log(const char* s) {
    FILE* f = fopen("d3d11_log.txt", "a");
    if (f) { fprintf(f, "%s\n", s); fclose(f); }
}

static void LogF(const char* fmt, ...) {
    FILE* f = fopen("d3d11_log.txt", "a");
    if (f) {
        va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
        fclose(f);
    }
}
#define MCPE_D3D11_LOG_BUDGET 3000000
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x) { (x)->Release(); (x) = 0; }
#endif

// ------------------------------------------------------------------
// D3D11 device / device-context / swapchain
// ------------------------------------------------------------------
static ID3D11Device*           g_dev = 0;
static ID3D11DeviceContext*    g_ctx = 0;
static IDXGISwapChain*         g_sc  = 0;
static ID3D11RenderTargetView* g_rtv = 0;
static ID3D11DepthStencilView* g_dsv = 0;
static ID3D11Texture2D*        g_depthTex = 0;
static HWND                    g_hwnd = 0;
static UINT                    g_backW = 0, g_backH = 0;

static ID3D11VertexShader*   g_vs = 0;
static ID3D11PixelShader*    g_ps = 0;
static ID3D11InputLayout*    g_layout = 0;
static ID3D11Buffer*         g_cb = 0;
static ID3D11Buffer*         g_vb = 0;      // dynamic scratch
static ID3D11SamplerState*   g_samp[4] = {0,0,0,0}; // [filterNearest|2 + wrap|1]
static ID3D11Texture2D*      g_whiteTex = 0;
static ID3D11ShaderResourceView* g_whiteSRV = 0;

// ------------------------------------------------------------------
// matrix state (GL column-major 16 floats)
// ------------------------------------------------------------------
static const int STACK_DEPTH = 32;
static float g_stackProj[STACK_DEPTH][16];
static float g_stackMv[STACK_DEPTH][16];
static int   g_spProj = 0, g_spMv = 0;
static GLenum g_matrixMode = GL_MODELVIEW;

static const float IDENTITY[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

static float* curMatrix() {
    return (g_matrixMode == GL_PROJECTION) ? g_stackProj[g_spProj] : g_stackMv[g_spMv];
}
static void matMul(float* out, const float* a, const float* b) {
    // out = a*b (all column-major)
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[k*4+r] * b[c*4+k];
            out[c*4+r] = s;
        }
}
static void matMultCur(const float* t) {
    float tmp[16]; matMul(tmp, curMatrix(), t);
    memcpy(curMatrix(), tmp, 64);
}

// range of NDC z = z_c/w_c across a vertex range (for clip debugging)
static void ComputeZRange(const float mvp[16], const unsigned char* base, size_t fsize,
                          GLint off, GLint stride, int first, int n,
                          float* ndcMin, float* ndcMax) {
    *ndcMin = 1e30f; *ndcMax = -1e30f;
    int m = n; if (m > 512) m = 512;
    for (int i = 0; i < m; i++) {
        int vi = first + i;
        float x = 0, y = 0, z = 0;
        size_t o = (size_t)off + (size_t)vi * (size_t)stride;
        if (o + 12 <= fsize) { const float* p = (const float*)(base + o); x = p[0]; y = p[1]; z = p[2]; }
        float cz = mvp[2]*x + mvp[6]*y + mvp[10]*z + mvp[14];
        float cw = mvp[3]*x + mvp[7]*y + mvp[11]*z + mvp[15];
        if (cw == 0.0f) continue;
        float ndc = cz / cw;
        if (ndc < *ndcMin) *ndcMin = ndc;
        if (ndc > *ndcMax) *ndcMax = ndc;
    }
}

static void buildTranslate(float* m, float x, float y, float z) {
    memcpy(m, IDENTITY, 64); m[12] = x; m[13] = y; m[14] = z;
}
static void buildScale(float* m, float x, float y, float z) {
    memcpy(m, IDENTITY, 64); m[0] = x; m[5] = y; m[10] = z;
}
static void buildRotate(float* m, float ang, float x, float y, float z) {
    float c = cosf(ang * 3.14159265358979f / 180.0f);
    float s = sinf(ang * 3.14159265358979f / 180.0f);
    float len = sqrtf(x*x + y*y + z*z);
    if (len == 0) { memcpy(m, IDENTITY, 64); return; }
    x /= len; y /= len; z /= len;
    float u = x, v = y, w = z;
    float cu = 1.0f - c;
    m[0]  = c + u*u*cu;   m[4]  = u*v*cu - w*s; m[8]  = u*w*cu + v*s; m[12] = 0;
    m[1]  = v*u*cu + w*s; m[5]  = c + v*v*cu;   m[9]  = v*w*cu - u*s; m[13] = 0;
    m[2]  = w*u*cu - v*s; m[6]  = w*v*cu + u*s; m[10] = c + w*w*cu;   m[14] = 0;
    m[3]  = 0;             m[7]  = 0;            m[11] = 0;            m[15] = 1;
}
static void buildOrtho(float* m, float l, float r, float b, float t, float n, float f) {
    memcpy(m, IDENTITY, 64);
    if (r == l || t == b || f == n) return;
    m[0]  = 2.0f / (r - l);
    m[5]  = 2.0f / (t - b);
    m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[14] = -(f + n) / (f - n);
}

// ------------------------------------------------------------------
// GL state
// ------------------------------------------------------------------
struct BufferObj {
    std::vector<unsigned char> data;
    BufferObj() : data() {}
};

struct TexObj {
    int w, h;
    std::vector<unsigned char> rgba;
    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* srv;
    bool nearest;
    bool clampS, clampT;
    TexObj() : w(0), h(0), tex(0), srv(0), nearest(true), clampS(false), clampT(false) {}
    ~TexObj() {
        SAFE_RELEASE(srv);
        SAFE_RELEASE(tex);
    }
    TexObj(const TexObj&) = delete;
    TexObj& operator=(const TexObj&) = delete;
};

struct ListObj {
    std::vector<unsigned int> ops;
    std::vector<unsigned char> vdata;
    ListObj() : ops(), vdata() {}
};

static std::map<GLuint, BufferObj*> s_buffers;
static std::map<GLuint, TexObj*>    s_textures;
static std::map<GLuint, ListObj*>   s_lists;

static GLuint  s_curBuffer = 0;
static GLuint  s_curTex    = 0;

static GLboolean s_capBlend=0, s_capDepth=1, s_capCull=0, s_capAlpha=0, s_capScissor=0, s_capFog=0;
static GLboolean s_capColorMaterial=0, s_capLighting=0, s_capTex2D=1;

static GLenum s_blendSrc = GL_ONE, s_blendDst = GL_ZERO;
static GLenum s_alphaFunc = GL_ALWAYS; static GLfloat s_alphaRef = 0.0f;
static GLenum s_depthFunc = GL_LESS;
static GLboolean s_depthMask = GL_TRUE;
static GLboolean s_colorMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
static GLenum s_cullFace = GL_BACK;
static GLenum s_shadeModel = GL_SMOOTH;
static GLenum s_polyMode = GL_FILL;
static GLfloat s_polyFactor = 0, s_polyUnits = 0;

static GLenum s_fogMode = GL_LINEAR;
static GLfloat s_fogColor[4] = {0,0,0,1};
static GLfloat s_fogStart = 0, s_fogEnd = 1, s_fogDensity = 1;

static GLfloat s_clearColor[4] = {0,0,0,1};
static GLfloat s_clearDepth = 1.0f;

static GLint s_viewport[4] = {0,0,0,0};
static GLint s_scissor[4] = {0,0,0,0};

static GLfloat s_curColor[4] = {1,1,1,1};

// client array state
static GLboolean s_vaEnabled=0, s_caEnabled=0, s_taEnabled=0, s_naEnabled=0;
static GLint  s_vaSize=3,  s_vaStride=24; static GLuint s_vaBuf=0; static GLint s_vaOff=0;
static GLint  s_caSize=4,  s_caStride=24; static GLuint s_caBuf=0; static GLint s_caOff=0;
static GLint  s_taSize=2,  s_taStride=24; static GLuint s_taBuf=0; static GLint s_taOff=0;
static GLint  s_naSize=3,  s_naStride=24; static GLuint s_naBuf=0; static GLint s_naOff=0;

static GLint s_unpackAlign = 4;
static GLenum s_glError = GL_NO_ERROR;

// TEMP instrumentation (in-progress debugging)
static int g_frameNo = 0;
static int g_logBudget = MCPE_D3D11_LOG_BUDGET;

// per-frame camera/geometry diagnostics (flicker hunting)
static float g_frameMv[16];
static int   g_frameDraws = 0;
static int   g_frameVerts = 0;
static int   g_frameMaxVerts = 0;
static void FrameStat(int n) {
    if (g_frameDraws == 0) memcpy(g_frameMv, g_stackMv[g_spMv], 64);
    g_frameDraws++;
    g_frameVerts += n;
    if (n > g_frameMaxVerts) g_frameMaxVerts = n;
}

// ------------------------------------------------------------------
// display list recording
// ------------------------------------------------------------------
static GLuint s_recordList = 0;   // 0 = not recording

enum {
    OP_MATRIXMODE, OP_LOADIDENTITY, OP_LOADMATRIX, OP_MULTMATRIX, OP_PUSH, OP_POP,
    OP_TRANSLATE, OP_ROTATE, OP_SCALE, OP_ORTHO,
    OP_COLOR4,
    OP_BINDTEX, OP_TEXPARAM, OP_TEXIMAGE, OP_TEXSUB, OP_PIXELSTORE,
    OP_BINDBUF, OP_BUFFERDATA,
    OP_VPOINTER, OP_TPOINTER, OP_CPOINTER, OP_NPOINTER,
    OP_ECLIENT, OP_DCLIENT,
    OP_DRAW,       // + DRAW_SNAP words
    OP_ENABLE, OP_DISABLE,
    OP_BLENDFUNC, OP_ALPHAFUNC, OP_DEPTHFUNC, OP_SHADEMODEL, OP_CULLFACE,
    OP_FOG, OP_CLEAR, OP_CLEARCOLOR, OP_CLEARDEPTH, OP_COLORMASK, OP_DEPTHMASK,
    OP_VIEWPORT, OP_SCISSOR, OP_DEPTHRANGE, OP_POLYOFF, OP_POLYMODE, OP_CALLLIST,
    OP_NORMAL3, OP_LINEWIDTH, OP_DRAW_LIST_VERTICES
};

static void pushU32(unsigned int v) {
    if (s_recordList) {
        ListObj* L = s_lists[s_recordList];
        L->ops.push_back(v);
    }
}
static void beginRecord(GLuint id) { s_recordList = id; }
static void endRecord() { s_recordList = 0; }
static bool recording() { return s_recordList != 0; }

// ------------------------------------------------------------------
// ring trace of recent client-state ops (diagnostic)
// ------------------------------------------------------------------
#define RG_RING 16
struct OpRec {
    char op[4];
    unsigned int a0, a1, a2, a3;
    int frame;
};
static OpRec g_ring[RG_RING] = {{{0}}};
static int g_ringIdx = 0;
static void RingAdd(const char* tag, unsigned int a0, unsigned int a1, unsigned int a2, unsigned int a3) {
    g_ring[g_ringIdx].op[0] = tag[0]; g_ring[g_ringIdx].op[1] = tag[1];
    g_ring[g_ringIdx].op[2] = tag[2]; g_ring[g_ringIdx].op[3] = 0;
    g_ring[g_ringIdx].a0 = a0; g_ring[g_ringIdx].a1 = a1;
    g_ring[g_ringIdx].a2 = a2; g_ring[g_ringIdx].a3 = a3;
    g_ring[g_ringIdx].frame = g_frameNo;
    g_ringIdx = (g_ringIdx + 1) % RG_RING;
}
static void RingDump() {
    for (int k = 0; k < RG_RING; k++) {
        int i = (g_ringIdx + k) % RG_RING;
        OpRec& r = g_ring[i];
        if (r.op[0] == 0) continue;
        LogF("TR %d %s %u %u %u %u\n", r.frame, r.op, r.a0, r.a1, r.a2, r.a3);
    }
}

// ------------------------------------------------------------------
// D3D11 helpers
// ------------------------------------------------------------------

typedef HRESULT (WINAPI *PFN_D3DCOMPILE)(const void*, SIZE_T, const char*,
    const D3D_SHADER_MACRO*, ID3DInclude*, const char*, const char*,
    UINT, UINT, ID3DBlob**, ID3DBlob**);

static PFN_D3DCOMPILE LoadD3DCompile() {
    HMODULE h = LoadLibraryA("d3dcompiler_47.dll");
    if (!h) return 0;
    return (PFN_D3DCOMPILE)GetProcAddress(h, "D3DCompile");
}

static const char* VS_SRC =
"cbuffer C0 : register(b0) {\n"
"  float4x4 mvp;\n"
"  float4x4 cam;\n"
"  float4 color;\n"
"  float4 fogColor;\n"
"  float4 fogP;\n"
"  float4 misc;\n"
"};\n"
"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float4 col : COLOR0; float dist : TEXCOORD1; };\n"
"VSOut VS(float4 p : POSITION, float2 uv : TEXCOORD0, float4 c : COLOR0) {\n"
"  VSOut o;\n"
"  float4 w = mul(p, cam);\n"
"  o.pos = mul(p, mvp);\n"
"  o.pos.z = o.pos.z * 0.5 + o.pos.w * 0.5;\n"
"  o.uv = uv; o.col = c;\n"
"  o.dist = length(w.xyz);\n"
"  return o;\n"
"}\n";

static const char* PS_SRC =
"Texture2D tex : register(t0);\n"
"SamplerState samp : register(s0);\n"
"cbuffer C0 : register(b0) {\n"
"  float4x4 mvp;\n"
"  float4x4 cam;\n"
"  float4 color;\n"
"  float4 fogColor;\n"
"  float4 fogP;\n"
"  float4 misc;\n"
"};\n"
"struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; float4 col : COLOR0; float dist : TEXCOORD1; };\n"
"float4 PS(VSOut i) : SV_Target {\n"
"  float4 base = i.col * color;\n"
"  float4 c;\n"
"  if (misc.z > 0.5) c = base * tex.Sample(samp, i.uv);\n"
"  else c = base;\n"
"  if (misc.x > 0.5 && c.a < misc.y) discard;\n"
"  if (fogP.x > 0.5) {\n"
"    float f;\n"
"    if (fogP.x < 1.5) f = saturate((fogP.z - i.dist) / (fogP.z - fogP.y));\n"
"    else if (fogP.x < 2.5) f = 1.0 - exp(-fogP.w * fogP.w * i.dist * i.dist);\n"
"    else f = 1.0 - exp(-fogP.w * i.dist);\n"
"    c.rgb = lerp(fogColor.rgb, c.rgb, saturate(f));\n"
"  }\n"
"  return c;\n"
"}\n";

static bool CreateShaders(ID3D11Device* dev) {
    PFN_D3DCOMPILE dc = LoadD3DCompile();
    if (!dc) { Log("d3d11: no D3DCompile"); return false; }

    D3D_FEATURE_LEVEL fl = dev->GetFeatureLevel();
    const char* vsTargets[] = { "vs_4_0_level_9_1", "vs_4_0", "vs_4_0_level_9_3", 0 };
    const char* psTargets[] = { "ps_4_0_level_9_1", "ps_4_0", "ps_4_0_level_9_3", 0 };
    if (fl >= D3D_FEATURE_LEVEL_10_0) {
        vsTargets[0] = "vs_4_0"; vsTargets[1] = "vs_4_0_level_9_1";
        psTargets[0] = "ps_4_0"; psTargets[1] = "ps_4_0_level_9_1";
    }

    ID3DBlob *vsB = 0, *psB = 0, *err = 0;
    HRESULT hr = E_FAIL;
    for (int i = 0; vsTargets[i]; i++) {
        hr = dc(VS_SRC, strlen(VS_SRC), "vs", 0, 0, "VS", vsTargets[i],
                D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vsB, &err);
        if (SUCCEEDED(hr)) break;
        if (err) { err->Release(); err = 0; }
    }
    if (FAILED(hr) || !vsB) {
        Log("d3d11: VS compile fail");
        return false;
    }
    for (int i = 0; psTargets[i]; i++) {
        hr = dc(PS_SRC, strlen(PS_SRC), "ps", 0, 0, "PS", psTargets[i],
                D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &psB, &err);
        if (SUCCEEDED(hr)) break;
        if (err) { err->Release(); err = 0; }
    }
    if (FAILED(hr) || !psB) {
        if (vsB) vsB->Release();
        Log("d3d11: PS compile fail");
        return false;
    }

    dev->CreateVertexShader(vsB->GetBufferPointer(), vsB->GetBufferSize(), 0, &g_vs);
    dev->CreatePixelShader(psB->GetBufferPointer(), psB->GetBufferSize(), 0, &g_ps);

    D3D11_INPUT_ELEMENT_DESC elems[3] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = dev->CreateInputLayout(elems, 3, vsB->GetBufferPointer(), vsB->GetBufferSize(), &g_layout);
    SAFE_RELEASE(vsB); SAFE_RELEASE(psB);
    if (FAILED(hr) || !g_layout) { Log("d3d11: layout fail"); return false; }

    D3D11_BUFFER_DESC cbd = {0};
    cbd.ByteWidth = 192; cbd.Usage = D3D11_USAGE_DEFAULT; cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&cbd, 0, &g_cb);

    D3D11_BUFFER_DESC vbd = {0};
    vbd.ByteWidth = 2u*1024u*1024u; vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER; vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    dev->CreateBuffer(&vbd, 0, &g_vb);

    // white 1x1
    unsigned int white = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA sd; sd.pSysMem = &white; sd.SysMemPitch = 4; sd.SysMemSlicePitch = 0;
    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    dev->CreateTexture2D(&td, &sd, &g_whiteTex);
    dev->CreateShaderResourceView(g_whiteTex, 0, &g_whiteSRV);

    // samplers [filter*2 + wrap]
    D3D11_SAMPLER_DESC sd2;
    for (int f = 0; f < 2; f++)
        for (int w = 0; w < 2; w++) {
            memset(&sd2, 0, sizeof(sd2));
            if (f) { sd2.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT; }
            else   { sd2.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; }
            sd2.AddressU = w ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
            sd2.AddressV = w ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
            sd2.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sd2.MaxLOD = 3.402823466e+38f;
            dev->CreateSamplerState(&sd2, &g_samp[f*2 + w]);
        }
    return true;
}

static void EnsureTargets(UINT w, UINT h);
static void InitD3D(HWND hwnd) {
    g_hwnd = hwnd;

    RECT rc; GetClientRect(hwnd, &rc);
    UINT cw = rc.right, ch = rc.bottom;
    if (cw < 1) cw = 1; if (ch < 1) ch = 1;

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,  D3D_FEATURE_LEVEL_9_1
    };
    HRESULT hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 6, D3D11_SDK_VERSION,
        &g_dev, 0, &g_ctx);
    if (FAILED(hr)) {
        LogF("d3d11: HARDWARE fail hr=0x%08X, trying WARP\n", (unsigned)hr);
        hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_WARP, 0,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 6, D3D11_SDK_VERSION,
            &g_dev, 0, &g_ctx);
    }
    if (FAILED(hr) || !g_dev || !g_ctx) {
        LogF("d3d11: device create fail hr=0x%08X\n", (unsigned)hr);
        g_dev = 0; g_ctx = 0; return;
    }

    DXGI_SWAP_CHAIN_DESC sd = {0};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = cw;
    sd.BufferDesc.Height = ch;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGIFactory* fac = 0;
    HRESULT hrFactory = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&fac);
    if (SUCCEEDED(hrFactory) && fac) {
        hr = fac->CreateSwapChain(g_dev, &sd, &g_sc);
        fac->Release();
    }
    if (FAILED(hr) || !g_sc) {
        LogF("d3d11: swapchain fail hr=0x%08X\n", (unsigned)hr);
        SAFE_RELEASE(g_dev); SAFE_RELEASE(g_ctx);
        return;
    }
    LogF("d3d11: swapchain %ux%u ok\n", cw, ch);

    if (!CreateShaders(g_dev)) {
        SAFE_RELEASE(g_sc); SAFE_RELEASE(g_dev); SAFE_RELEASE(g_ctx);
        return;
    }
    EnsureTargets(cw, ch);
}

static void EnsureTargets(UINT w, UINT h) {
    if (!g_sc || !g_dev) return;
    if (w == g_backW && h == g_backH && g_rtv) return;

    if (g_rtv) {
        g_ctx->OMSetRenderTargets(0, 0, 0);
        SAFE_RELEASE(g_rtv); SAFE_RELEASE(g_dsv); SAFE_RELEASE(g_depthTex);
    }
    HRESULT hr = g_sc->ResizeBuffers(2, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) { LogF("d3d11: ResizeBuffers fail hr=0x%08X\n", (unsigned)hr); }

    ID3D11Texture2D* back = 0;
    g_sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back);
    if (back) {
        g_dev->CreateRenderTargetView(back, 0, &g_rtv);
        back->Release();
    }
    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    hr = g_dev->CreateTexture2D(&td, 0, &g_depthTex);
    D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (SUCCEEDED(hr))
        g_dev->CreateDepthStencilView(g_depthTex, &dsv, &g_dsv);

    g_backW = w; g_backH = h;
    if (g_rtv && g_dsv)
        g_ctx->OMSetRenderTargets(1, &g_rtv, g_dsv);
}

static void MaybeResize() {
    if (!g_hwnd || !g_sc) return;
    RECT rc; GetClientRect(g_hwnd, &rc);
    UINT cw = rc.right, ch = rc.bottom;
    if (cw < 1) cw = 1; if (ch < 1) ch = 1;
    if (cw != g_backW || ch != g_backH)
        EnsureTargets(cw, ch);
}

// ------------------------------------------------------------------
// enum translation
// ------------------------------------------------------------------
static D3D11_BLEND mapBlend(GLenum f) {
    switch (f) {
        case GL_ZERO: return D3D11_BLEND_ZERO;
        case GL_ONE:  return D3D11_BLEND_ONE;
        case GL_SRC_COLOR: return D3D11_BLEND_SRC_COLOR;
        case GL_ONE_MINUS_SRC_COLOR: return D3D11_BLEND_INV_SRC_COLOR;
        case GL_SRC_ALPHA: return D3D11_BLEND_SRC_ALPHA;
        case GL_ONE_MINUS_SRC_ALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
        case GL_DST_ALPHA: return D3D11_BLEND_DEST_ALPHA;
        case GL_ONE_MINUS_DST_ALPHA: return D3D11_BLEND_INV_DEST_ALPHA;
        case GL_DST_COLOR: return D3D11_BLEND_DEST_COLOR;
        case GL_ONE_MINUS_DST_COLOR: return D3D11_BLEND_INV_DEST_COLOR;
        case GL_SRC_ALPHA_SATURATE: return D3D11_BLEND_SRC_ALPHA_SAT;
    }
    return D3D11_BLEND_ONE;
}
static D3D11_COMPARISON_FUNC mapDepth(GLenum f) {
    switch (f) {
        case GL_NEVER:    return D3D11_COMPARISON_NEVER;
        case GL_LESS:     return D3D11_COMPARISON_LESS;
        case GL_EQUAL:    return D3D11_COMPARISON_EQUAL;
        case GL_LEQUAL:   return D3D11_COMPARISON_LESS_EQUAL;
        case GL_GREATER:  return D3D11_COMPARISON_GREATER;
        case GL_NOTEQUAL: return D3D11_COMPARISON_NOT_EQUAL;
        case GL_GEQUAL:   return D3D11_COMPARISON_GREATER_EQUAL;
        case GL_ALWAYS:   return D3D11_COMPARISON_ALWAYS;
    }
    return D3D11_COMPARISON_LESS;
}

// simple state caches
static std::map<unsigned long long, ID3D11BlendState*>   s_blendCache;
static std::map<unsigned long long, ID3D11DepthStencilState*> s_dsCache;
static std::map<unsigned long long, ID3D11RasterizerState*>   s_rsCache;

static void ApplyState() {
    if (!g_dev) return;

    // ---- blend + color mask ----
    UINT wr = s_colorMask[0] ? D3D11_COLOR_WRITE_ENABLE_RED   : 0;
    UINT wg = s_colorMask[1] ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0;
    UINT wb = s_colorMask[2] ? D3D11_COLOR_WRITE_ENABLE_BLUE  : 0;
    UINT wa = s_colorMask[3] ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0;
    UINT wmask = wr | wg | wb | wa;
    if (s_capBlend && (s_blendSrc != GL_ONE || s_blendDst != GL_ZERO || wmask != 0xFFFFFFFFu) == 0 && s_blendSrc == GL_ONE && s_blendDst == GL_ZERO)
        wmask = 0xFFFFFFFF; // harmless fallback (kept simple)

    unsigned long long bk = 0;
    bk |= (unsigned long long)(s_capBlend ? 1 : 0);
    bk |= (unsigned long long)mapBlend(s_blendSrc) << 8;
    bk |= (unsigned long long)mapBlend(s_blendDst) << 16;
    bk |= (unsigned long long)wmask << 24;
    ID3D11BlendState* bl = 0;
    std::map<unsigned long long, ID3D11BlendState*>::iterator it = s_blendCache.find(bk);
    if (it == s_blendCache.end()) {
        D3D11_BLEND_DESC bd = {0};
        bd.RenderTarget[0].BlendEnable = s_capBlend ? TRUE : FALSE;
        bd.RenderTarget[0].SrcBlend = mapBlend(s_blendSrc);
        bd.RenderTarget[0].DestBlend = mapBlend(s_blendDst);
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = wmask;
        if (g_dev->CreateBlendState(&bd, &bl) == S_OK)
            s_blendCache[bk] = bl;
    } else bl = it->second;
    if (bl) g_ctx->OMSetBlendState(bl, 0, 0xFFFFFFFF);

    // ---- depth stencil ----
    unsigned long long dk = 0;
    dk |= (unsigned long long)(s_capDepth ? 1 : 0);
    dk |= (unsigned long long)mapDepth(s_depthFunc) << 8;
    dk |= (unsigned long long)(s_depthMask ? 1 : 0) << 16;
    ID3D11DepthStencilState* ds = 0;
    std::map<unsigned long long, ID3D11DepthStencilState*>::iterator dit = s_dsCache.find(dk);
    if (dit == s_dsCache.end()) {
        D3D11_DEPTH_STENCIL_DESC dd = {0};
        dd.DepthEnable = s_capDepth ? TRUE : FALSE;
        dd.DepthWriteMask = s_depthMask ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dd.DepthFunc = mapDepth(s_depthFunc);
        dd.StencilEnable = FALSE;
        if (g_dev->CreateDepthStencilState(&dd, &ds) == S_OK)
            s_dsCache[dk] = ds;
    } else ds = dit->second;
    if (ds) g_ctx->OMSetDepthStencilState(ds, 0);

    // ---- rasterizer ----
    unsigned long long rk = 0;
    rk |= (unsigned long long)(s_capCull ? (s_cullFace == GL_FRONT ? 1 : 2) : 0);
    rk |= (unsigned long long)(s_polyMode == GL_LINE ? 1 : 0) << 4;
    int bias = (int)(s_polyUnits * 16.0f + (s_polyFactor > 0 ? 1.0f : 0.0f));
    rk |= ((unsigned long long)(unsigned int)(s_polyFactor * 100.0f)) << 8;
    rk |= ((unsigned long long)(unsigned int)(s_polyUnits * 4.0f)) << 24;
    ID3D11RasterizerState* rs = 0;
    std::map<unsigned long long, ID3D11RasterizerState*>::iterator rit = s_rsCache.find(rk);
    if (rit == s_rsCache.end()) {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = (s_polyMode == GL_LINE) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
        rd.CullMode = s_capCull ? (s_cullFace == GL_FRONT ? D3D11_CULL_FRONT : D3D11_CULL_BACK) : D3D11_CULL_NONE;
        rd.FrontCounterClockwise = TRUE;
        rd.DepthClipEnable = TRUE;
        if (g_dev->CreateRasterizerState(&rd, &rs) == S_OK)
            s_rsCache[rk] = rs;
    } else rs = rit->second;
    if (rs) g_ctx->RSSetState(rs);
}

// ------------------------------------------------------------------
// texture helpers
// ------------------------------------------------------------------
static void TexUpload(TexObj* t) {
    if (!g_dev) return;
    SAFE_RELEASE(t->srv);
    SAFE_RELEASE(t->tex);
    if (t->w <= 0 || t->h <= 0 || t->rgba.empty()) return;

    D3D11_SUBRESOURCE_DATA sd = {0};
    sd.pSysMem = &t->rgba[0];
    sd.SysMemPitch = t->w * 4;
    sd.SysMemSlicePitch = 0;
    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = t->w; td.Height = t->h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (g_dev->CreateTexture2D(&td, &sd, &t->tex) != S_OK) return;
    g_dev->CreateShaderResourceView(t->tex, 0, &t->srv);
}

// ------------------------------------------------------------------
// GL entry points
// ------------------------------------------------------------------

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (recording()) { pushU32(OP_VIEWPORT); pushU32((unsigned int)x); pushU32((unsigned int)y); pushU32((unsigned int)width); pushU32((unsigned int)height); return; }
    static bool s_vpLogged = false;
    if (!s_vpLogged) { LogF("VP %d,%d %dx%d rec=%d ctx=%p rtv=%p\n", x, y, width, height, (int)recording(), g_ctx, g_rtv); s_vpLogged = true; }
    s_viewport[0]=x; s_viewport[1]=y; s_viewport[2]=width; s_viewport[3]=height;
    if (g_ctx && g_rtv) {
        D3D11_VIEWPORT vp;
        vp.TopLeftX = (float)x;
        vp.TopLeftY = (float)((int)g_backH - (int)y - (int)height);
        vp.Width = (float)width;
        vp.Height = (float)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        g_ctx->RSSetViewports(1, &vp);
    }
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    if (recording()) { pushU32(OP_SCISSOR); pushU32((unsigned int)x); pushU32((unsigned int)y); pushU32((unsigned int)width); pushU32((unsigned int)height); return; }
    s_scissor[0]=x; s_scissor[1]=y; s_scissor[2]=width; s_scissor[3]=height;
    if (g_ctx) {
        if (s_capScissor && width > 0 && height > 0) {
            D3D11_RECT rr;
            rr.left = x;
            rr.top = (int)g_backH - (y + height);
            rr.right = x + width;
            rr.bottom = (int)g_backH - y;
            g_ctx->RSSetScissorRects(1, &rr);
        }
    }
}

void glEnable(GLenum cap) {
    if (recording()) { pushU32(OP_ENABLE); pushU32((unsigned int)cap); return; }
    switch (cap) {
        case GL_BLEND: s_capBlend = GL_TRUE; break;
        case GL_DEPTH_TEST: s_capDepth = GL_TRUE; break;
        case GL_CULL_FACE: s_capCull = GL_TRUE; break;
        case GL_ALPHA_TEST: s_capAlpha = GL_TRUE; break;
        case GL_SCISSOR_TEST: s_capScissor = GL_TRUE; break;
        case GL_FOG: s_capFog = GL_TRUE; break;
        case GL_COLOR_MATERIAL: s_capColorMaterial = GL_TRUE; break;
        case GL_LIGHTING: s_capLighting = GL_TRUE; break;
        case GL_TEXTURE_2D: s_capTex2D = GL_TRUE; break;
        default: break;
    }
}
void glDisable(GLenum cap) {
    if (recording()) { pushU32(OP_DISABLE); pushU32((unsigned int)cap); return; }
    switch (cap) {
        case GL_BLEND: s_capBlend = GL_FALSE; break;
        case GL_DEPTH_TEST: s_capDepth = GL_FALSE; break;
        case GL_CULL_FACE: s_capCull = GL_FALSE; break;
        case GL_ALPHA_TEST: s_capAlpha = GL_FALSE; break;
        case GL_SCISSOR_TEST: s_capScissor = GL_FALSE; break;
        case GL_FOG: s_capFog = GL_FALSE; break;
        case GL_COLOR_MATERIAL: s_capColorMaterial = GL_FALSE; break;
        case GL_LIGHTING: s_capLighting = GL_FALSE; break;
        case GL_TEXTURE_2D: s_capTex2D = GL_FALSE; break;
        default: break;
    }
}
GLboolean glIsEnabled(GLenum cap) { return GL_FALSE; }

void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
    if (recording()) { pushU32(OP_COLORMASK); pushU32(r); pushU32(g); pushU32(b); pushU32(a); return; }
    s_colorMask[0]=r; s_colorMask[1]=g; s_colorMask[2]=b; s_colorMask[3]=a;
}
void glDepthMask(GLboolean flag) {
    if (recording()) { pushU32(OP_DEPTHMASK); pushU32(flag); return; }
    s_depthMask = flag;
}
void glDepthFunc(GLenum func) {
    if (recording()) { pushU32(OP_DEPTHFUNC); pushU32((unsigned int)func); return; }
    s_depthFunc = func;
}
void glDepthRange(GLclampd zNear, GLclampd zFar) {
    if (recording()) { pushU32(OP_DEPTHRANGE); pushU32(0); pushU32(0); return; }
    (void)zNear; (void)zFar;
}
void glDepthRangef(GLclampf zNear, GLclampf zFar) { glDepthRange(zNear, zFar); }

void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    if (recording()) { pushU32(OP_BLENDFUNC); pushU32((unsigned int)sfactor); pushU32((unsigned int)dfactor); return; }
    s_blendSrc = sfactor; s_blendDst = dfactor;
}
void glAlphaFunc(GLenum func, GLclampf ref) {
    if (recording()) { pushU32(OP_ALPHAFUNC); pushU32((unsigned int)func); pushU32(*(unsigned int*)&ref); return; }
    s_alphaFunc = func; s_alphaRef = ref;
}
void glShadeModel(GLenum mode) {
    if (recording()) { pushU32(OP_SHADEMODEL); pushU32((unsigned int)mode); return; }
    s_shadeModel = mode;
}
void glCullFace(GLenum mode) {
    if (recording()) { pushU32(OP_CULLFACE); pushU32((unsigned int)mode); return; }
    s_cullFace = mode;
}
void glLineWidth(GLfloat width) {
    if (recording()) { pushU32(OP_LINEWIDTH); pushU32(0); pushU32(0); pushU32(0); pushU32(0); (void)width; return; }
    (void)width;
}
void glPolygonOffset(GLfloat factor, GLfloat units) {
    if (recording()) {
        float fa = factor, fb = units;
        pushU32(OP_POLYOFF);
        pushU32(*(unsigned int*)&fa);
        pushU32(*(unsigned int*)&fb);
        return;
    }
    s_polyFactor = factor; s_polyUnits = units;
}
void glPolygonMode(GLenum face, GLenum mode) {
    if (recording()) { pushU32(OP_POLYMODE); pushU32((unsigned int)face); pushU32((unsigned int)mode); return; }
    (void)face; s_polyMode = mode;
}
void glHint(GLenum target, GLenum mode) { (void)target; (void)mode; }
void glColorMaterial(GLenum face, GLenum mode) { (void)face; (void)mode; }

void glFogf(GLenum pname, GLfloat param) {
    if (recording()) { pushU32(OP_FOG); pushU32((unsigned int)pname); pushU32(*(unsigned int*)&param); pushU32(0); pushU32(0); return; }
    switch (pname) {
        case GL_FOG_START: s_fogStart = param; break;
        case GL_FOG_END: s_fogEnd = param; break;
        case GL_FOG_DENSITY: s_fogDensity = param; break;
        default: break;
    }
}
void glFogi(GLenum pname, GLint param) {
    if (recording()) { pushU32(OP_FOG); pushU32((unsigned int)pname); float fp = (float)param; pushU32(*(unsigned int*)&fp); pushU32(0); pushU32(0); return; }
    if (pname == GL_FOG_MODE) s_fogMode = (GLenum)param;
}
void glFogfv(GLenum pname, const GLfloat* params) {
    if (pname == GL_FOG_COLOR) {
        if (recording()) { pushU32(OP_FOG); pushU32((unsigned int)pname); pushU32(((const unsigned int*)params)[0]); pushU32(((const unsigned int*)params)[1]); pushU32(((const unsigned int*)params)[2]); pushU32(((const unsigned int*)params)[3]); return; }
        memcpy(s_fogColor, params, 16);
    } else {
        if (recording()) { pushU32(OP_FOG); pushU32((unsigned int)pname); pushU32(((const unsigned int*)params)[0]); pushU32(0); pushU32(0); return; }
    }
}

void glMatrixMode(GLenum mode) {
    if (recording()) { pushU32(OP_MATRIXMODE); pushU32((unsigned int)mode); return; }
    g_matrixMode = mode;
}
void glLoadIdentity() {
    if (recording()) { pushU32(OP_LOADIDENTITY); return; }
    memcpy(curMatrix(), IDENTITY, 64);
}
void glLoadMatrixf(const GLfloat* m) {
    if (recording()) { pushU32(OP_LOADMATRIX); for (int i=0;i<16;i++) pushU32(((const unsigned int*)m)[i]); return; }
    memcpy(curMatrix(), m, 64);
}
void glMultMatrixf(const GLfloat* m) {
    if (recording()) { pushU32(OP_MULTMATRIX); for (int i=0;i<16;i++) pushU32(((const unsigned int*)m)[i]); return; }
    matMultCur(m);
}
void glPushMatrix() {
    if (recording()) { pushU32(OP_PUSH); return; }
    if (g_matrixMode == GL_PROJECTION) {
        if (g_spProj < STACK_DEPTH - 1) {
            memcpy(g_stackProj[g_spProj + 1], g_stackProj[g_spProj], 64);
            g_spProj++;
        }
    } else {
        if (g_spMv < STACK_DEPTH - 1) {
            memcpy(g_stackMv[g_spMv + 1], g_stackMv[g_spMv], 64);
            g_spMv++;
        }
    }
}
void glPopMatrix() {
    if (recording()) { pushU32(OP_POP); return; }
    if (g_matrixMode == GL_PROJECTION) { if (g_spProj > 0) --g_spProj; }
    else if (g_spMv > 0) --g_spMv;
}
void glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
    float m[16]; buildOrtho(m, (float)l,(float)r,(float)b,(float)t,(float)n,(float)f);
    glMultMatrixf(m);
}
void glOrthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
    float m[16]; buildOrtho(m, l,r,b,t,n,f);
    glMultMatrixf(m);
}
void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    if (recording()) { pushU32(OP_TRANSLATE); pushU32(*(unsigned int*)&x); pushU32(*(unsigned int*)&y); pushU32(*(unsigned int*)&z); return; }
    float m[16]; buildTranslate(m, x,y,z); matMultCur(m);
}
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    if (recording()) { pushU32(OP_ROTATE); pushU32(*(unsigned int*)&angle); pushU32(*(unsigned int*)&x); pushU32(*(unsigned int*)&y); pushU32(*(unsigned int*)&z); return; }
    float m[16]; buildRotate(m, angle, x,y,z); matMultCur(m);
}
void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    if (recording()) { pushU32(OP_SCALE); pushU32(*(unsigned int*)&x); pushU32(*(unsigned int*)&y); pushU32(*(unsigned int*)&z); return; }
    float m[16]; buildScale(m, x,y,z); matMultCur(m);
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    if (recording()) { pushU32(OP_COLOR4); pushU32(*(unsigned int*)&r); pushU32(*(unsigned int*)&g); pushU32(*(unsigned int*)&b); float one=1.0f; pushU32(*(unsigned int*)&one); return; }
    s_curColor[0]=r; s_curColor[1]=g; s_curColor[2]=b; s_curColor[3]=1;
}
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    if (recording()) { pushU32(OP_COLOR4); pushU32(*(unsigned int*)&r); pushU32(*(unsigned int*)&g); pushU32(*(unsigned int*)&b); pushU32(*(unsigned int*)&a); return; }
    s_curColor[0]=r; s_curColor[1]=g; s_curColor[2]=b; s_curColor[3]=a;
}
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) { (void)nx; (void)ny; (void)nz; }

void glGenTextures(GLsizei n, GLuint* textures) {
    static GLuint next = 1;
    for (int i = 0; i < n; i++) {
        while (s_textures.count(next)) next++;
        textures[i] = next;
        s_textures[next] = new TexObj();
        next++;
    }
}
void glDeleteTextures(GLsizei n, const GLuint* textures) {
    for (int i = 0; i < n; i++) {
        std::map<GLuint, TexObj*>::iterator it = s_textures.find(textures[i]);
        if (it != s_textures.end()) { delete it->second; s_textures.erase(it); }
    }
}
void glBindTexture(GLenum target, GLuint texture) {
    if (recording()) { pushU32(OP_BINDTEX); pushU32((unsigned int)texture); return; }
    (void)target; s_curTex = texture;
    if (texture && !s_textures.count(texture)) s_textures[texture] = new TexObj();
}
void glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (recording()) { pushU32(OP_TEXPARAM); pushU32((unsigned int)target); pushU32((unsigned int)pname); pushU32((unsigned int)param); return; }
    (void)target;
    if (!s_curTex) { if (!s_textures.count(0)) s_textures[0] = new TexObj(); return; }
    TexObj* t = s_textures[s_curTex];
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
            t->nearest = (param == GL_NEAREST || param == GL_NEAREST_MIPMAP_NEAREST);
            break;
        case GL_TEXTURE_WRAP_S: t->clampS = (param == GL_CLAMP_TO_EDGE || param == GL_CLAMP); break;
        case GL_TEXTURE_WRAP_T: t->clampT = (param == GL_CLAMP_TO_EDGE || param == GL_CLAMP); break;
    }
}

static bool DecodePixels(std::vector<unsigned char>& dstRgba, GLenum format, GLenum type, GLsizei w, GLsizei h, const GLvoid* pixels) {
    if (!pixels || w <= 0 || h <= 0) return false;
    dstRgba.assign((size_t)w * h * 4, 0);
    unsigned char* dst = &dstRgba[0];
    int rowAlign = (s_unpackAlign > 0) ? s_unpackAlign : 4;
    if (type == GL_UNSIGNED_BYTE && (format == GL_RGBA || format == GL_RGB)) {
        const unsigned char* src = (const unsigned char*)pixels;
        int comp = (format == GL_RGBA) ? 4 : 3;
        int rowBytes = w * comp;
        int stride = ((rowBytes + rowAlign - 1) / rowAlign) * rowAlign;
        for (int y = 0; y < h; y++) {
            const unsigned char* rsrc = src + (size_t)y * stride;
            unsigned char* rdst = dst + (size_t)y * w * 4;
            for (int x = 0; x < w; x++) {
                rdst[x*4+0] = rsrc[x*comp+0];
                rdst[x*4+1] = rsrc[x*comp+1];
                rdst[x*4+2] = rsrc[x*comp+2];
                rdst[x*4+3] = (comp == 4) ? rsrc[x*comp+3] : 255;
            }
        }
    } else if (type == GL_UNSIGNED_SHORT_5_6_5) {
        int rowBytes = w * 2;
        int stride = ((rowBytes + rowAlign - 1) / rowAlign) * rowAlign;
        for (int y = 0; y < h; y++) {
            const unsigned char* row = (const unsigned char*)pixels + (size_t)y * stride;
            const unsigned short* rsrc = (const unsigned short*)row;
            unsigned char* rdst = dst + (size_t)y * w * 4;
            for (int x = 0; x < w; x++) {
                unsigned short v = rsrc[x];
                rdst[x*4+0] = (unsigned char)(((v >> 11) & 31) * 255 / 31);
                rdst[x*4+1] = (unsigned char)(((v >> 5) & 63) * 255 / 63);
                rdst[x*4+2] = (unsigned char)((v & 31) * 255 / 31);
                rdst[x*4+3] = 255;
            }
        }
    } else if (type == GL_UNSIGNED_SHORT_4_4_4_4) {
        const unsigned char* pixelsB = (const unsigned char*)pixels;
        int rowBytes = w * 2;
        int stride = ((rowBytes + rowAlign - 1) / rowAlign) * rowAlign;
        for (int y = 0; y < h; y++) {
            const unsigned char* rsrc = pixelsB + (size_t)y * stride;
            unsigned char* rdst = dst + (size_t)y * w * 4;
            for (int x = 0; x < w; x++) {
                unsigned char lo = rsrc[x*2+0], hi = rsrc[x*2+1];
                rdst[x*4+0] = (unsigned char)((lo & 0x0F) * 17);
                rdst[x*4+1] = (unsigned char)(((lo >> 4) & 0x0F) * 17);
                rdst[x*4+2] = (unsigned char)((hi & 0x0F) * 17);
                rdst[x*4+3] = (unsigned char)(((hi >> 4) & 0x0F) * 17);
            }
        }
    } else if (type == GL_UNSIGNED_SHORT_5_5_5_1) {
        const unsigned char* pixelsB = (const unsigned char*)pixels;
        int rowBytes = w * 2;
        int stride = ((rowBytes + rowAlign - 1) / rowAlign) * rowAlign;
        for (int y = 0; y < h; y++) {
            const unsigned char* rsrc = pixelsB + (size_t)y * stride;
            unsigned char* rdst = dst + (size_t)y * w * 4;
            for (int x = 0; x < w; x++) {
                unsigned short v = (unsigned short)(rsrc[x*2+0] | (rsrc[x*2+1] << 8));
                rdst[x*4+0] = (unsigned char)(((v >> 11) & 31) * 255 / 31);
                rdst[x*4+1] = (unsigned char)(((v >> 6) & 31) * 255 / 31);
                rdst[x*4+2] = (unsigned char)(((v >> 1) & 31) * 255 / 31);
                rdst[x*4+3] = (v & 1) ? 255 : 0;
            }
        }
    } else {
        return false;
    }
    return true;
}

static void TexLoadCore(TexObj* t, GLenum format, GLenum type, GLsizei w, GLsizei h, const GLvoid* pixels) {
    if (!t || !pixels || w <= 0 || h <= 0) return;
    t->w = w; t->h = h;
    if (!DecodePixels(t->rgba, format, type, w, h, pixels)) return;
    TexUpload(t);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                  GLint border, GLenum format, GLenum type, const GLvoid* pixels) {
    if (recording()) { pushU32(OP_TEXIMAGE); pushU32(0); pushU32(0); pushU32(0); pushU32(0); return; }
    (void)target; (void)level; (void)internalformat; (void)border;
    if (!s_curTex) return;
    if (!s_textures.count(s_curTex)) s_textures[s_curTex] = new TexObj();
    TexObj* t = s_textures[s_curTex];
    TexLoadCore(t, format, type, width, height, pixels);
}
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels) {
    if (recording()) { pushU32(OP_TEXSUB); pushU32(0); pushU32(0); pushU32(0); pushU32(0); return; }
    (void)target; (void)level;
    if (!s_curTex || !pixels || width <= 0 || height <= 0) return;
    if (!s_textures.count(s_curTex)) s_textures[s_curTex] = new TexObj();
    TexObj* t = s_textures[s_curTex];
    if (t->w <= 0 || t->h <= 0 || t->rgba.empty()) {
        // unknown size -- treat as full re-upload
        TexLoadCore(t, format, type, width, height, pixels);
        return;
    }
    std::vector<unsigned char> subRgba;
    if (!DecodePixels(subRgba, format, type, width, height, pixels)) return;
    if (subRgba.size() != (size_t)width * height * 4) return;
    for (int y = 0; y < height; y++) {
        int dy = yoffset + y;
        if (dy < 0 || dy >= t->h) continue;
        if (xoffset < 0 || xoffset + width > t->w) continue;
        memcpy(&t->rgba[(size_t)dy * t->w * 4 + (size_t)xoffset * 4],
               &subRgba[(size_t)y * width * 4],
               (size_t)width * 4);
    }
    TexUpload(t);
}
void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat,
                            GLsizei width, GLsizei height, GLint border,
                            GLsizei imageSize, const GLvoid* data) {
    (void)target; (void)level; (void)internalformat; (void)width; (void)height;
    (void)border; (void)imageSize; (void)data;
}
void glPixelStorei(GLenum pname, GLint param) {
    if (param > 0 && param <= 8) s_unpackAlign = param;
}

void glBindBuffer(GLenum target, GLuint buffer) {
    if (recording()) { pushU32(OP_BINDBUF); pushU32((unsigned int)buffer); }
    (void)target;
    s_curBuffer = buffer;
    if (buffer && !s_buffers.count(buffer)) s_buffers[buffer] = new BufferObj();
    RingAdd("BD", (unsigned int)buffer, 0, 0, 0);
}
void glGenBuffers(GLsizei n, GLuint* buffers) {
    static GLuint next = 1;
    for (int i = 0; i < n; i++) {
        while (s_buffers.count(next)) next++;
        buffers[i] = next;
        s_buffers[next] = new BufferObj();
        next++;
    }
}
void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    for (int i = 0; i < n; i++) {
        std::map<GLuint, BufferObj*>::iterator it = s_buffers.find(buffers[i]);
        if (it != s_buffers.end()) { delete it->second; s_buffers.erase(it); }
    }
}
void glBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage) {
    if (s_curBuffer == 0) return;
    (void)target; (void)usage;
    BufferObj* b = s_buffers[s_curBuffer];
    if (recording()) { pushU32(OP_BUFFERDATA); pushU32((unsigned int)size); pushU32(0); pushU32(0); pushU32(0); }
    b->data.resize(size > 0 ? size : 0);
    if (size > 0 && data) memcpy(&b->data[0], data, size);
    RingAdd("BUF", (unsigned int)s_curBuffer, (unsigned int)size, 0, 0);
}
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data) {
    (void)target;
    if (s_curBuffer == 0) return;
    BufferObj* b = s_buffers[s_curBuffer];
    if ((size_t)(offset + size) <= b->data.size() && data)
        memcpy(&b->data[offset], data, size);
}

void glEnableClientState(GLenum array) {
    if (recording()) { pushU32(OP_ECLIENT); pushU32((unsigned int)array); }
    RingAdd("EN", (unsigned int)array, 0, 0, 0);
    switch (array) {
        case GL_VERTEX_ARRAY: s_vaEnabled = GL_TRUE; break;
        case GL_COLOR_ARRAY: s_caEnabled = GL_TRUE; break;
        case GL_TEXTURE_COORD_ARRAY: s_taEnabled = GL_TRUE; break;
        case GL_NORMAL_ARRAY: s_naEnabled = GL_TRUE; break;
    }
}
void glDisableClientState(GLenum array) {
    if (recording()) { pushU32(OP_DCLIENT); pushU32((unsigned int)array); }
    RingAdd("DI", (unsigned int)array, 0, 0, 0);
    switch (array) {
        case GL_VERTEX_ARRAY: s_vaEnabled = GL_FALSE; break;
        case GL_COLOR_ARRAY: s_caEnabled = GL_FALSE; break;
        case GL_TEXTURE_COORD_ARRAY: s_taEnabled = GL_FALSE; break;
        case GL_NORMAL_ARRAY: s_naEnabled = GL_FALSE; break;
    }
}
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    if (recording()) { pushU32(OP_VPOINTER); pushU32((unsigned int)s_curBuffer); pushU32((unsigned int)size); pushU32((unsigned int)stride); pushU32((unsigned int)(size_t)pointer); }
    s_vaSize = size; s_vaStride = stride ? stride : 24; s_vaBuf = s_curBuffer; s_vaOff = (GLint)(size_t)pointer;
    if (stride == 0 || stride < 0) s_vaStride = 24;
    RingAdd("VP", (unsigned int)s_curBuffer, (unsigned int)size, (unsigned int)(size_t)pointer, (unsigned int)stride);
}
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    if (recording()) { pushU32(OP_CPOINTER); pushU32((unsigned int)s_curBuffer); pushU32((unsigned int)size); pushU32((unsigned int)stride); pushU32((unsigned int)(size_t)pointer); }
    s_caSize = size; s_caStride = stride ? stride : 24; s_caBuf = s_curBuffer; s_caOff = (GLint)(size_t)pointer;
    RingAdd("CP", (unsigned int)s_curBuffer, (unsigned int)size, (unsigned int)(size_t)pointer, (unsigned int)stride);
}
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer) {
    if (recording()) { pushU32(OP_TPOINTER); pushU32((unsigned int)s_curBuffer); pushU32((unsigned int)size); pushU32((unsigned int)stride); pushU32((unsigned int)(size_t)pointer); }
    s_taSize = size; s_taStride = stride ? stride : 24; s_taBuf = s_curBuffer; s_taOff = (GLint)(size_t)pointer;
    RingAdd("TP", (unsigned int)s_curBuffer, (unsigned int)size, (unsigned int)(size_t)pointer, (unsigned int)stride);
}
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid* pointer) {
    if (recording()) { pushU32(OP_NPOINTER); pushU32(0); pushU32(0); pushU32((unsigned int)stride); pushU32((unsigned int)(size_t)pointer); }
    (void)type; s_naStride = stride ? stride : 24; s_naBuf = s_curBuffer; s_naOff = (GLint)(size_t)pointer;
    RingAdd("NP", (unsigned int)s_curBuffer, 3, (unsigned int)(size_t)pointer, (unsigned int)stride);
}

// draw snapshot taken at draw call time
struct DrawSnap {
    GLenum mode;
    GLint first;
    GLsizei count;
    bool va, ta, ca, na;
    GLuint vBuf, tBuf, cBuf, nBuf;
    GLint vOff, vStride;
    GLint tOff, tStride;
    GLint cOff, cStride;
    GLint nOff, nStride;
};

static inline void GetVertexData(const DrawSnap& s, int vi, float& x, float& y, float& z, float& u, float& v, unsigned char& cr, unsigned char& cg, unsigned char& cb, unsigned char& ca) {
    x = 0; y = 0; z = 0;
    u = 0; v = 0;
    cr = 255; cg = 255; cb = 255; ca = 255;

    if (s.va) {
        const unsigned char* p = 0;
        if (s.vBuf != 0) {
            std::map<GLuint, BufferObj*>::iterator it = s_buffers.find(s.vBuf);
            if (it != s_buffers.end() && !it->second->data.empty()) {
                size_t off = (size_t)s.vOff + (size_t)vi * (size_t)s.vStride;
                if (off + 12 <= it->second->data.size()) p = &it->second->data[off];
            }
        } else if (s.vOff != 0) {
            p = (const unsigned char*)(size_t)s.vOff + (size_t)vi * (size_t)s.vStride;
        }
        if (p) {
            const float* fp = (const float*)p;
            x = fp[0]; y = fp[1]; z = fp[2];
        }
    }

    if (s.ta) {
        const unsigned char* p = 0;
        if (s.tBuf != 0) {
            std::map<GLuint, BufferObj*>::iterator it = s_buffers.find(s.tBuf);
            if (it != s_buffers.end() && !it->second->data.empty()) {
                size_t off = (size_t)s.tOff + (size_t)vi * (size_t)s.tStride;
                if (off + 8 <= it->second->data.size()) p = &it->second->data[off];
            }
        } else if (s.tOff != 0) {
            p = (const unsigned char*)(size_t)s.tOff + (size_t)vi * (size_t)s.tStride;
        }
        if (p) {
            const float* fp = (const float*)p;
            u = fp[0]; v = fp[1];
        }
    }

    if (s.ca) {
        const unsigned char* p = 0;
        if (s.cBuf != 0) {
            std::map<GLuint, BufferObj*>::iterator it = s_buffers.find(s.cBuf);
            if (it != s_buffers.end() && !it->second->data.empty()) {
                size_t off = (size_t)s.cOff + (size_t)vi * (size_t)s.cStride;
                if (off + 4 <= it->second->data.size()) p = &it->second->data[off];
            }
        } else if (s.cOff != 0) {
            p = (const unsigned char*)(size_t)s.cOff + (size_t)vi * (size_t)s.cStride;
        }
        if (p) {
            cr = p[0]; cg = p[1]; cb = p[2]; ca = p[3];
        }
    }
}

static const int VB_MAX_VERTS = (int)(2u*1024u*1024u / 24);

static void DrawListVertices(GLenum mode, int count, const unsigned char* packedVerts, bool hasColor) {
    if (!g_ctx || !g_vb || count <= 0 || !packedVerts) return;
    if (count > VB_MAX_VERTS) {
        if (g_logBudget > 0) { g_logBudget--; LogF("VBOVER f=%d list n=%d max=%d\n", g_frameNo, count, VB_MAX_VERTS); }
        count = VB_MAX_VERTS;
    }
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(g_ctx->Map(g_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    memcpy(m.pData, packedVerts, (size_t)count * 24);
    g_ctx->Unmap(g_vb, 0);
    FrameStat(count);

    float mvp[16], cam[16];
    {
        float P[16], M[16];
        memcpy(P, g_stackProj[g_spProj], 64);
        memcpy(M, g_stackMv[g_spMv], 64);
        float tmp[16]; matMul(tmp, P, M); memcpy(mvp, tmp, 64);
        memcpy(cam, M, 64);
    }
    float tmvp[16], tcam[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) { tmvp[i*4+j] = mvp[j*4+i]; tcam[i*4+j] = cam[j*4+i]; }

    struct __attribute__((packed)) {
        float mvp[16], cam[16], color[4], fogColor[4], fogP[4], misc[4];
    } cb;
    memcpy(cb.mvp, tmvp, 64);
    memcpy(cb.cam, tcam, 64);
    if (hasColor) {
        cb.color[0] = 1.0f; cb.color[1] = 1.0f; cb.color[2] = 1.0f; cb.color[3] = 1.0f;
    } else {
        memcpy(cb.color, s_curColor, 16);
    }
    memcpy(cb.fogColor, s_fogColor, 16);
    bool fogOn = s_capFog != 0;
    cb.fogP[0] = fogOn ? (s_fogMode == GL_LINEAR ? 1.0f : (s_fogMode == GL_EXP2 ? 2.0f : 3.0f)) : 0.0f;
    cb.fogP[1] = s_fogStart;
    cb.fogP[2] = s_fogEnd;
    cb.fogP[3] = s_fogDensity;
    cb.misc[0] = s_capAlpha ? 1.0f : 0.0f;
    cb.misc[1] = s_alphaRef;
    cb.misc[2] = (s_curTex && s_capTex2D) ? 1.0f : 0.0f;
    cb.misc[3] = hasColor ? 1.0f : 0.0f;

    g_ctx->UpdateSubresource(g_cb, 0, 0, &cb, 0, 0);
    ApplyState();

    static UINT stride = 24, offset = 0;
    g_ctx->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);
    g_ctx->IASetInputLayout(g_layout);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11ShaderResourceView* srv = g_whiteSRV;
    int sampIdx = 0;
    if (s_curTex && s_capTex2D) {
        std::map<GLuint, TexObj*>::iterator itt = s_textures.find(s_curTex);
        if (itt != s_textures.end() && itt->second->srv) {
            srv = itt->second->srv;
            sampIdx = (itt->second->nearest ? 0 : 1) * 2 + ((itt->second->clampS || itt->second->clampT) ? 0 : 1);
        }
    }
    g_ctx->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* smp = g_samp[sampIdx];
    g_ctx->PSSetSamplers(0, 1, &smp);

    g_ctx->VSSetShader(g_vs, 0, 0);
    g_ctx->PSSetShader(g_ps, 0, 0);
    g_ctx->VSSetConstantBuffers(0, 1, &g_cb);
    g_ctx->PSSetConstantBuffers(0, 1, &g_cb);

    if (mode == GL_LINES) { g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); g_ctx->Draw(count, 0); }
    else if (mode == GL_LINE_STRIP || mode == GL_LINE_LOOP) { g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP); g_ctx->Draw(count, 0); }
    else if (mode == GL_POINTS) { g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST); g_ctx->Draw(count, 0); }
    else g_ctx->Draw(count, 0);
}

static void PackDrawRange(const DrawSnap& s, int firstIdx, int n) {
    // pack n vertices (from s.first + firstIdx) into scratch VB as canonical 24B vertex
    if (!g_ctx || !g_vb) return;
    if (n <= 0) return;

    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(g_ctx->Map(g_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
        return;
    unsigned char* dst = (unsigned char*)m.pData;

    FrameStat(n);

    for (int i = 0; i < n; i++) {
        int vi = s.first + firstIdx + i;
        float x, y, z, u, v;
        unsigned char cr, cg, cb, ca;
        GetVertexData(s, vi, x, y, z, u, v, cr, cg, cb, ca);
        float* d = (float*)(dst + (size_t)i * 24);
        d[0] = x; d[1] = y; d[2] = z; d[3] = u; d[4] = v;
        d[5] = 0 /* unused pad */;
        unsigned char* dc = dst + (size_t)i * 24 + 20;
        dc[0] = cr; dc[1] = cg; dc[2] = cb; dc[3] = ca;
    }
    g_ctx->Unmap(g_vb, 0);

    // CB
    float mvp[16], cam[16];
    {
        float P[16], M[16];
        memcpy(P, g_stackProj[g_spProj], 64);
        memcpy(M, g_stackMv[g_spMv], 64);
        float tmp[16]; matMul(tmp, P, M); memcpy(mvp, tmp, 64);
        memcpy(cam, M, 64);
    }
    float tmvp[16], tcam[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) { tmvp[i*4+j] = mvp[j*4+i]; tcam[i*4+j] = cam[j*4+i]; }

    struct __attribute__((packed)) {
        float mvp[16], cam[16], color[4], fogColor[4], fogP[4], misc[4];
    } cb;
    memcpy(cb.mvp, tmvp, 64);
    memcpy(cb.cam, tcam, 64);
    if (s.ca) {
        cb.color[0] = 1.0f; cb.color[1] = 1.0f; cb.color[2] = 1.0f; cb.color[3] = 1.0f;
    } else {
        memcpy(cb.color, s_curColor, 16);
    }
    memcpy(cb.fogColor, s_fogColor, 16);
    bool fogOn = s_capFog != 0;
    cb.fogP[0] = fogOn ? (s_fogMode == GL_LINEAR ? 1.0f : (s_fogMode == GL_EXP2 ? 2.0f : 3.0f)) : 0.0f;
    cb.fogP[1] = s_fogStart;
    cb.fogP[2] = s_fogEnd;
    cb.fogP[3] = s_fogDensity;
    cb.misc[0] = s_capAlpha ? 1.0f : 0.0f;
    cb.misc[1] = s_alphaRef;
    cb.misc[2] = (s_curTex && s_capTex2D) ? 1.0f : 0.0f;
    cb.misc[3] = s.ca ? 1.0f : 0.0f;

    g_ctx->UpdateSubresource(g_cb, 0, 0, &cb, 0, 0);

    ApplyState();

    static UINT stride = 24, offset = 0;
    g_ctx->IASetVertexBuffers(0, 1, &g_vb, &stride, &offset);
    g_ctx->IASetInputLayout(g_layout);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // texture
    ID3D11ShaderResourceView* srv = g_whiteSRV;
    int sampIdx = 0;
    if (s_curTex && s_capTex2D) {
        std::map<GLuint, TexObj*>::iterator itt = s_textures.find(s_curTex);
        if (itt != s_textures.end() && itt->second->srv) {
            srv = itt->second->srv;
            sampIdx = (itt->second->nearest ? 0 : 1) * 2 + ((itt->second->clampS || itt->second->clampT) ? 0 : 1);
        }
    }
    g_ctx->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* smp = g_samp[sampIdx];
    g_ctx->PSSetSamplers(0, 1, &smp);

    g_ctx->VSSetShader(g_vs, 0, 0);
    g_ctx->PSSetShader(g_ps, 0, 0);
    g_ctx->VSSetConstantBuffers(0, 1, &g_cb);
    g_ctx->PSSetConstantBuffers(0, 1, &g_cb);

    // expand strips/fans to triangles (rare -- debug only)
    if (s.mode == GL_TRIANGLE_STRIP || s.mode == GL_TRIANGLE_FAN) {
        int tri = n - 2;
        if (tri < 1) return;
        // re-map vertices: reuse scratch via rotate pattern
        g_ctx->Draw(n, 0);  // approximation; strips rare, correct enough for wire debug
        return;
    }

    if (s.mode == GL_LINES) { g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST); g_ctx->Draw(n, 0); }
    else if (s.mode == GL_LINE_STRIP || s.mode == GL_LINE_LOOP) { g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP); g_ctx->Draw(n, 0); if (s.mode == GL_LINE_LOOP) /* ignore closing */; }
    else if (s.mode == GL_POINTS) { g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST); g_ctx->Draw(n, 0); }
    else g_ctx->Draw(n, 0); // TRIANGLES (and QUADS already expanded)
}

static void PackDraw(const DrawSnap& s) {
    if (!g_ctx || !g_vb) return;
    if (s.count <= 0) return;
    if (s.count > VB_MAX_VERTS) {
        if (g_logBudget > 0) { g_logBudget--; LogF("VBOVER2 f=%d n=%d max=%d\n", g_frameNo, (int)s.count, VB_MAX_VERTS); }
        for (int done = 0; done < s.count; done += VB_MAX_VERTS) {
            int n = s.count - done;
            if (n > VB_MAX_VERTS) n = VB_MAX_VERTS;
            PackDrawRange(s, done, n);
        }
        return;
    }
    PackDrawRange(s, 0, s.count);
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (count <= 0) return;

    DrawSnap s;
    memset(&s, 0, sizeof(s));
    s.mode = mode;
    s.first = first;
    s.count = count;
    s.va = s_vaEnabled != 0; s.ta = s_taEnabled != 0; s.ca = s_caEnabled != 0; s.na = false;
    s.vBuf = s_vaBuf; s.tBuf = s_taBuf; s.cBuf = s_caBuf;
    s.vOff = s_vaOff; s.vStride = s_vaStride;
    s.tOff = s_taOff; s.tStride = s_taStride;
    s.cOff = s_caOff; s.cStride = s_caStride;

    if (recording()) {
        ListObj* curList = s_lists[s_recordList];
        size_t vdataOffset = curList->vdata.size();
        size_t byteCount = (size_t)count * 24;
        curList->vdata.resize(vdataOffset + byteCount);
        unsigned char* dst = &curList->vdata[vdataOffset];

        for (int i = 0; i < count; i++) {
            int vi = first + i;
            float x, y, z, u, v;
            unsigned char cr, cg, cb, ca;
            GetVertexData(s, vi, x, y, z, u, v, cr, cg, cb, ca);
            float* d = (float*)(dst + (size_t)i * 24);
            d[0] = x; d[1] = y; d[2] = z; d[3] = u; d[4] = v; d[5] = 0;
            unsigned char* dc = dst + (size_t)i * 24 + 20;
            dc[0] = cr; dc[1] = cg; dc[2] = cb; dc[3] = ca;
        }

        pushU32(OP_DRAW_LIST_VERTICES);
        pushU32((unsigned int)mode);
        pushU32((unsigned int)count);
        pushU32((unsigned int)vdataOffset);
        pushU32((unsigned int)(s.ca ? 1 : 0));
        return;
    }
    if (count > 100 && !s.va) {
        static int s_dumpF = -1;
        static int s_dumpLeft = 6;
        if (s_dumpLeft > 0 && s_dumpF != g_frameNo) {
            s_dumpF = g_frameNo;
            s_dumpLeft--;
            LogF("TRACE framed\n", g_frameNo);
            LogF("TR f=%d target n=%d mode=%u tex=%u va=%d ta=%d ca=%d buf=%u\n", g_frameNo, (int)count, (unsigned)mode, s_curTex, s.va, s.ta, s.ca, (unsigned)s.vBuf);
            RingDump();
        }
    }
    PackDraw(s);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices) {
    (void)mode; (void)count; (void)type; (void)indices;
}

GLuint glGenLists(GLsizei range) {
    static GLuint alloc = 0x00100000;
    GLuint base = alloc;
    alloc += (GLuint)range;
    for (int i = 0; i < range; i++)
        s_lists[base + i] = new ListObj();
    return base;
}
void glDeleteLists(GLuint list, GLsizei range) {
    for (int i = 0; i < range; i++) {
        std::map<GLuint, ListObj*>::iterator it = s_lists.find(list + i);
        if (it != s_lists.end()) { delete it->second; s_lists.erase(it); }
    }
}
void glNewList(GLuint list, GLenum mode) {
    if (g_logBudget > 0) { g_logBudget--; LogF("NL list=%u\n", (unsigned)list); }
    std::map<GLuint, ListObj*>::iterator it = s_lists.find(list);
    if (it == s_lists.end()) { s_lists[list] = new ListObj(); it = s_lists.find(list); }
    it->second->ops.clear();
    it->second->vdata.clear();
    beginRecord(list);
    (void)mode;
}
void glEndList() { if (g_logBudget > 0) { g_logBudget--; LogF("EL op=%u\n", (unsigned)s_recordList); } endRecord(); }
void glCallList(GLuint list) {
    std::map<GLuint, ListObj*>::iterator it = s_lists.find(list);
    if (it == s_lists.end() || it->second->ops.empty()) {
        if (g_logBudget > 0) { g_logBudget--; LogF("CL_MISS f=%d list=%u\n", g_frameNo, (unsigned)list); }
        return;
    }
    if (recording()) { pushU32(OP_CALLLIST); pushU32((unsigned int)list); return; }
    const std::vector<unsigned int>& ops = it->second->ops;
    size_t p = 0;
    const unsigned int* v = &ops[0];
    while (p < ops.size()) {
        unsigned int op = v[p++];
        unsigned int* arg = (unsigned int*)(void*)(v + p);
        switch (op) {
            case OP_MATRIXMODE: glMatrixMode((GLenum)arg[0]); p += 1; break;
            case OP_LOADIDENTITY: glLoadIdentity(); break;
            case OP_LOADMATRIX: glLoadMatrixf((const float*)arg); p += 16; break;
            case OP_MULTMATRIX: glMultMatrixf((const float*)arg); p += 16; break;
            case OP_PUSH: glPushMatrix(); break;
            case OP_POP: glPopMatrix(); break;
            case OP_TRANSLATE: glTranslatef(*(float*)&arg[0], *(float*)&arg[1], *(float*)&arg[2]); p += 3; break;
            case OP_ROTATE: glRotatef(*(float*)&arg[0], *(float*)&arg[1], *(float*)&arg[2], *(float*)&arg[3]); p += 4; break;
            case OP_SCALE: glScalef(*(float*)&arg[0], *(float*)&arg[1], *(float*)&arg[2]); p += 3; break;
            case OP_ORTHO: p += 6; break;
            case OP_COLOR4: glColor4f(*(float*)&arg[0], *(float*)&arg[1], *(float*)&arg[2], *(float*)&arg[3]); p += 4; break;
            case OP_BINDTEX: glBindTexture(GL_TEXTURE_2D, arg[0]); p += 1; break;
            case OP_TEXPARAM: glTexParameteri((GLenum)arg[0], (GLenum)arg[1], (GLint)arg[2]); p += 3; break;
            case OP_TEXIMAGE: p += 4; break;
            case OP_TEXSUB: p += 4; break;
            case OP_PIXELSTORE: p += 1; break;
            case OP_BINDBUF: glBindBuffer(GL_ARRAY_BUFFER, arg[0]); p += 1; break;
            case OP_BUFFERDATA: {
                GLuint save = 0;
                // no-op marker; data persisted at record time
                p += 4; break;
            }
            case OP_VPOINTER: glVertexPointer((GLint)arg[1], GL_FLOAT, (GLsizei)arg[2], (const GLvoid*)(size_t)arg[3]);
                { // restore buffer
                    p += 4; break; }
            case OP_TPOINTER: glTexCoordPointer((GLint)arg[1], GL_FLOAT, (GLsizei)arg[2], (const GLvoid*)(size_t)arg[3]); p += 4; break;
            case OP_CPOINTER: glColorPointer((GLint)arg[1], GL_UNSIGNED_BYTE, (GLsizei)arg[2], (const GLvoid*)(size_t)arg[3]); p += 4; break;
            case OP_NPOINTER: p += 4; break;
            case OP_ECLIENT: glEnableClientState((GLenum)arg[0]); p += 1; break;
            case OP_DCLIENT: glDisableClientState((GLenum)arg[0]); p += 1; break;
            case OP_DRAW: {
                DrawSnap s2;
                memcpy(&s2, arg, sizeof(DrawSnap));
                p += sizeof(DrawSnap) / 4;
                if (g_logBudget > 0 && s2.count >= 1) { g_logBudget--; LogF("LD f=%d m=%d fn=%d n=%d va=%d ta=%d ca=%d buf=%u off=%d st=%d tex=%u\n", g_frameNo, (int)s2.mode, s2.first, s2.count, s2.va, s2.ta, s2.ca, (unsigned)s2.vBuf, s2.vOff, s2.vStride, s_curTex); }
                PackDraw(s2);
                break;
            }
            case OP_DRAW_LIST_VERTICES: {
                GLenum mode = (GLenum)arg[0];
                int count = (int)arg[1];
                size_t vdataOffset = (size_t)arg[2];
                bool hasColor = arg[3] != 0;
                p += 4;
                if (vdataOffset + (size_t)count * 24 <= it->second->vdata.size()) {
                    DrawListVertices(mode, count, &it->second->vdata[vdataOffset], hasColor);
                }
                break;
            }
            case OP_ENABLE: glEnable((GLenum)arg[0]); p += 1; break;
            case OP_DISABLE: glDisable((GLenum)arg[0]); p += 1; break;
            case OP_BLENDFUNC: glBlendFunc((GLenum)arg[0], (GLenum)arg[1]); p += 2; break;
            case OP_ALPHAFUNC: glAlphaFunc((GLenum)arg[0], *(float*)&arg[1]); p += 2; break;
            case OP_DEPTHFUNC: glDepthFunc((GLenum)arg[0]); p += 1; break;
            case OP_SHADEMODEL: glShadeModel((GLenum)arg[0]); p += 1; break;
            case OP_CULLFACE: glCullFace((GLenum)arg[0]); p += 1; break;
            case OP_FOG: {
                GLenum pn = (GLenum)arg[0];
                if (pn == GL_FOG_COLOR) {
                    float c[4] = { *(float*)&arg[1], *(float*)&arg[2], *(float*)&arg[3], *(float*)&arg[4] };
                    glFogfv(pn, c); p += 5;
                } else {
                    glFogf(pn, *(float*)&arg[1]); p += 4;
                }
                break;
            }
            case OP_CLEAR: glClear((GLbitfield)arg[0]); p += 1; break;
            case OP_CLEARCOLOR: glClearColor(*(float*)&arg[0], *(float*)&arg[1], *(float*)&arg[2], *(float*)&arg[3]); p += 4; break;
            case OP_CLEARDEPTH: glClearDepthf(*(float*)&arg[0]); p += 1; break;
            case OP_COLORMASK: glColorMask(arg[0]?1:0, arg[1]?1:0, arg[2]?1:0, arg[3]?1:0); p += 4; break;
            case OP_DEPTHMASK: glDepthMask(arg[0] ? GL_TRUE : GL_FALSE); p += 1; break;
            case OP_VIEWPORT: { GLint vx=(GLint)arg[0],vy=(GLint)arg[1],vw=(GLsizei)arg[2],vh=(GLsizei)arg[3]; static int s_vpReplay=0; if(s_vpReplay<3){LogF("VP-REPLAY %d,%d %dx%d\n",vx,vy,vw,vh);s_vpReplay++;} glViewport(vx,vy,vw,vh); p += 4; break; }
            case OP_SCISSOR: glScissor((GLint)arg[0], (GLint)arg[1], (GLsizei)arg[2], (GLsizei)arg[3]); p += 4; break;
            case OP_DEPTHRANGE: p += 2; break;
            case OP_POLYOFF: { float fa = *(float*)&arg[0], fb = *(float*)&arg[1]; glPolygonOffset(fa, fb); p += 2; break; }
            case OP_POLYMODE: glPolygonMode((GLenum)arg[0], (GLenum)arg[1]); p += 2; break;
            case OP_CALLLIST: glCallList(arg[0]); p += 1; break;
            case OP_LINEWIDTH: p += 4; break;
            case OP_NORMAL3: p += 3; break;
            default: return;
        }
    }
}
void glCallLists(GLsizei n, GLenum type, const GLvoid* lists) {
    if (type == GL_UNSIGNED_INT) {
        const GLuint* li = (const GLuint*)lists;
        for (int i = 0; i < n; i++)
            glCallList(li[i]);
    } else if (type == GL_BYTE || type == GL_UNSIGNED_BYTE) {
        const GLubyte* li = (const GLubyte*)lists;
        for (int i = 0; i < n; i++)
            glCallList(li[i]);
    }
}

void glClear(GLbitfield mask) {
    if (recording()) { pushU32(OP_CLEAR); pushU32((unsigned int)mask); return; }
    if (g_logBudget > 0) { g_logBudget--; LogF("C f=%d mask=%u color=%.2f,%.2f,%.2f,%.2f depth=%.2f\n", g_frameNo, (unsigned)mask, s_clearColor[0], s_clearColor[1], s_clearColor[2], s_clearColor[3], s_clearDepth); }
    if (!g_ctx || !g_rtv) return;
    if (mask & GL_COLOR_BUFFER_BIT) {
        float c[4] = { s_clearColor[0], s_clearColor[1], s_clearColor[2], s_clearColor[3] };
        g_ctx->ClearRenderTargetView(g_rtv, c);
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) && g_dsv) {
        g_ctx->ClearDepthStencilView(g_dsv, D3D11_CLEAR_DEPTH, s_clearDepth, 0);
    }
}
void glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a) {
    if (recording()) { pushU32(OP_CLEARCOLOR); pushU32(*(unsigned int*)&r); pushU32(*(unsigned int*)&g); pushU32(*(unsigned int*)&b); pushU32(*(unsigned int*)&a); return; }
    s_clearColor[0]=r; s_clearColor[1]=g; s_clearColor[2]=b; s_clearColor[3]=a;
}
void glClearDepthf(GLclampf depth) {
    if (recording()) { pushU32(OP_CLEARDEPTH); pushU32(*(unsigned int*)&depth); return; }
    s_clearDepth = depth;
}
void glClearDepth(GLclampd depth) { glClearDepthf((GLclampf)depth); }

GLenum glGetError() {
    GLenum e = s_glError;
    s_glError = GL_NO_ERROR;
    return e;
}
const GLubyte* glGetString(GLenum name) {
    static const GLubyte v[] = "1.1.0";
    static const GLubyte vr[] = "Ninetology D3D11";
    static const GLubyte rn[] = "Adreno (D3D11)";
    static const GLubyte ex[] = "";
    switch (name) {
        case GL_VERSION: return v;
        case GL_VENDOR: return vr;
        case GL_RENDERER: return rn;
        case GL_EXTENSIONS: return ex;
    }
    return (const GLubyte*)("");
}
void glGetFloatv(GLenum pname, GLfloat* params) {
    if (pname == GL_PROJECTION_MATRIX) memcpy(params, g_stackProj[g_spProj], 64);
    else if (pname == GL_MODELVIEW_MATRIX) memcpy(params, g_stackMv[g_spMv], 64);
}
void glGetIntegerv(GLenum pname, GLint* params) {
    if (pname == GL_VIEWPORT) { params[0]=s_viewport[0]; params[1]=s_viewport[1]; params[2]=s_viewport[2]; params[3]=s_viewport[3]; }
    else params[0] = 0;
}
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels) {
    (void)x; (void)y; (void)width; (void)height; (void)format; (void)type;
    if (pixels) memset(pixels, 0, 1);
}
void glFinish() {}
void glFlush() {}

// ------------------------------------------------------------------
// GLEW shim entry points
// ------------------------------------------------------------------
static void glDepthRangefImpl(GLclampf, GLclampf) {}
static void glBindBufferImpl(GLenum, GLuint) {}
void (*__glewDepthRangef)(GLclampf, GLclampf) = glDepthRangefImpl;
void (*__glewBindBuffer)(GLenum, GLuint) = glBindBufferImpl;

GLenum glewInit() { return GL_NO_ERROR; }
GLboolean glewIsSupported(const char* name) { return GL_FALSE; (void)name; }

// ------------------------------------------------------------------
// frame present + wgl stubs
// ------------------------------------------------------------------
static void PresentFrame() {
    if (!g_sc || !g_ctx) return;
    MaybeResize();
    g_frameNo++;
    if (g_logBudget > 0 && (g_frameNo <= 3 || g_frameNo % 20 == 0)) {
        g_logBudget--;
        const float* M = g_frameMv;
        LogF("CAM f=%d draws=%d verts=%d max=%d spMv=%d mvT=%.2f,%.2f,%.2f r0=%.3f,%.3f,%.3f p0=%.3f clr=%.2f,%.2f,%.2f\n",
             g_frameNo, g_frameDraws, g_frameVerts, g_frameMaxVerts, g_spMv,
             M[12], M[13], M[14], M[0], M[1], M[2],
             g_stackProj[g_spProj][0], s_clearColor[0], s_clearColor[1], s_clearColor[2]);
    }
    g_frameDraws = 0; g_frameVerts = 0; g_frameMaxVerts = 0;
    HRESULT hr = g_sc->Present(1, 0);
    (void)hr;
    g_spProj = 0;
    g_spMv = 0;
}

HGLRC WINAPI wglCreateContext(HDC hdc) {
    HWND wnd = WindowFromDC(hdc);
    if (!g_dev && wnd) InitD3D(wnd);
    return (HGLRC)1;
}
BOOL WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc) {
    if (!g_dev) {
        HWND wnd = WindowFromDC(hdc);
        if (wnd) InitD3D(wnd);
    }
    (void)hglrc;
    return g_dev ? TRUE : TRUE;
}
BOOL WINAPI wglDeleteContext(HGLRC hglrc) {
    (void)hglrc;
    return TRUE;
}
PROC WINAPI wglGetProcAddress(LPCSTR name) {
    (void)name;
    return 0;
}
BOOL WINAPI SwapBuffers(HDC hdc) {
    (void)hdc;
    PresentFrame();
    return TRUE;
}

// dllimport override table: wingdi/GL call sites reference __imp_*
extern "C" {
    void* __imp_SwapBuffers = (void*)(void*)SwapBuffers;
    void* __imp_wglCreateContext = (void*)(void*)wglCreateContext;
    void* __imp_wglMakeCurrent = (void*)(void*)wglMakeCurrent;
    void* __imp_wglDeleteContext = (void*)(void*)wglDeleteContext;
    void* __imp_wglGetProcAddress = (void*)(void*)wglGetProcAddress;
}
