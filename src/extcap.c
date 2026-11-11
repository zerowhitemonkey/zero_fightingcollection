// SNKHD - cattura dello schermo per i giochi PC esterni (Windows, DXGI Desktop Duplication).
// La finestra dell'app resta sopra il gioco (esclusa dalla cattura con WDA_EXCLUDEFROMCAPTURE) e ridisegna
// l'area del gioco catturata con SCREEN TYPE / MONITOR / SCANLINES / BLUR come per i giochi emulati.
#ifdef _WIN32
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <string.h>
#include "gfxb.h"
#include "extcap.h"

static IDXGIOutputDuplication *dupl;
static RECT out_rc;              // area del monitor catturato (coordinate del desktop)
static unsigned cap_tex;         // texture con l'area del gioco
static int cap_w, cap_h, have_frame;
static RECT last_src;
static HWND self_hwnd;
static DWORD retry_at;

static int dupl_open(void) {
    ID3D11Device *dev = (ID3D11Device*)gfxb_d3d_device();
    if (!dev || !self_hwnd) return 0;
    IDXGIDevice *xdev = NULL; IDXGIAdapter *ad = NULL;
    if (FAILED(ID3D11Device_QueryInterface(dev, &IID_IDXGIDevice, (void**)&xdev))) return 0;
    IDXGIDevice_GetAdapter(xdev, &ad);
    IDXGIDevice_Release(xdev);
    if (!ad) return 0;
    // uscita (monitor) che contiene il centro della finestra dell'app
    RECT wr; GetWindowRect(self_hwnd, &wr);
    POINT c = { (wr.left + wr.right) / 2, (wr.top + wr.bottom) / 2 };
    IDXGIOutput *o = NULL, *pick = NULL;
    for (UINT i = 0; IDXGIAdapter_EnumOutputs(ad, i, &o) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_OUTPUT_DESC od; IDXGIOutput_GetDesc(o, &od);
        if (!pick && PtInRect(&od.DesktopCoordinates, c)) { pick = o; out_rc = od.DesktopCoordinates; continue; }
        IDXGIOutput_Release(o);
    }
    IDXGIAdapter_Release(ad);
    if (!pick) return 0;
    IDXGIOutput1 *o1 = NULL;
    HRESULT hr = IDXGIOutput_QueryInterface(pick, &IID_IDXGIOutput1, (void**)&o1);
    IDXGIOutput_Release(pick);
    if (FAILED(hr) || !o1) return 0;
    hr = IDXGIOutput1_DuplicateOutput(o1, (IUnknown*)dev, &dupl);
    IDXGIOutput1_Release(o1);
    return SUCCEEDED(hr) && dupl;
}

static void dupl_close(void) {
    if (dupl) { IDXGIOutputDuplication_Release(dupl); dupl = NULL; }
}

int extcap_open(HWND self, char *err, int errlen) {
    self_hwnd = self;
    have_frame = 0; cap_w = cap_h = 0; retry_at = 0;
    memset(&last_src, 0, sizeof last_src);
    if (!dupl_open()) { if (err) lstrcpynA(err, "DuplicateOutput non disponibile", errlen); return 0; }
    return 1;
}

void extcap_monitor(RECT *r) { *r = out_rc; }

void extcap_close(void) {
    dupl_close();
    if (cap_tex) { gfxb_tex_free(cap_tex); cap_tex = 0; }
    cap_w = cap_h = 0; have_frame = 0;
}

// src = area da catturare (coordinate del desktop). Ritorna 1 se la texture contiene un'immagine valida.
int extcap_frame(const RECT *src, unsigned *tex, int *w, int *h) {
    if (!dupl) {
        if (GetTickCount() < retry_at) goto done;
        if (!dupl_open()) { retry_at = GetTickCount() + 500; goto done; }
    }
    RECT s = *src;
    // limiti del monitor catturato
    if (s.left < out_rc.left) s.left = out_rc.left;
    if (s.top < out_rc.top) s.top = out_rc.top;
    if (s.right > out_rc.right) s.right = out_rc.right;
    if (s.bottom > out_rc.bottom) s.bottom = out_rc.bottom;
    int sw = s.right - s.left, sh = s.bottom - s.top;
    if (sw < 16 || sh < 16) goto done;
    if (memcmp(&s, &last_src, sizeof s)) { have_frame = 0; last_src = s; }   // area cambiata: aspetta un'immagine nuova

    DXGI_OUTDUPL_FRAME_INFO fi; IDXGIResource *res = NULL;
    HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(dupl, 0, &fi, &res);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        // nessun aggiornamento del desktop: alla prima richiesta si forza la copia dell'immagine corrente
        if (have_frame) goto done;
        hr = IDXGIOutputDuplication_AcquireNextFrame(dupl, 20, &fi, &res);
        if (FAILED(hr)) goto done;
    }
    if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) { dupl_close(); retry_at = GetTickCount() + 200; goto done; }
    if (FAILED(hr)) goto done;
    {
        ID3D11Texture2D *dt = NULL;
        IDXGIResource_QueryInterface(res, &IID_ID3D11Texture2D, (void**)&dt);
        IDXGIResource_Release(res);
        if (dt) {
            if (!cap_tex || cap_w != sw || cap_h != sh) {
                if (cap_tex) gfxb_tex_free(cap_tex);
                cap_tex = gfxb_tex_create_gpu(sw, sh);
                cap_w = sw; cap_h = sh;
            }
            ID3D11Texture2D *ct = (ID3D11Texture2D*)gfxb_tex_resource(cap_tex);
            if (ct) {
                D3D11_BOX b = { (UINT)(s.left - out_rc.left), (UINT)(s.top - out_rc.top), 0,
                                (UINT)(s.right - out_rc.left), (UINT)(s.bottom - out_rc.top), 1 };
                ID3D11DeviceContext_CopySubresourceRegion((ID3D11DeviceContext*)gfxb_d3d_context(),
                    (ID3D11Resource*)ct, 0, 0, 0, 0, (ID3D11Resource*)dt, 0, &b);
                have_frame = 1;
            }
            ID3D11Texture2D_Release(dt);
        }
        IDXGIOutputDuplication_ReleaseFrame(dupl);
    }
done:
    *tex = cap_tex; *w = cap_w; *h = cap_h;
    return have_frame && cap_tex;
}
#endif
