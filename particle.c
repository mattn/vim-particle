#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <process.h>

#define MAX_GROUPS 64
#define DEFAULT_PER_GROUP 3
#define MAX_PER_GROUP 10
#define WM_SPAWN (WM_USER + 1)
#define PARTICLE_SIZE 10
#define STAR_SIZE 30
#define POOP_SIZE 30

typedef struct {
  HWND hwnd;
  int x, y, dx, dy;
} particle;

typedef struct {
  particle p[MAX_PER_GROUP];
  HDC hdcMem;
  HBITMAP hbmp;
  int active;
} pgroup;

typedef struct {
  int sx, sy, sw, sh;
  BYTE r, g, b, a;
} spawn_params;

static pgroup groups[MAX_GROUPS];
static int g_per_group = DEFAULT_PER_GROUP;
static HWND g_target;
static HINSTANCE g_hinst;
static DWORD g_mainThread;
static TCHAR g_class[] = TEXT("particle");
static UINT_PTR g_timer;
static int g_active_count;
static int g_mode; /* 0=normal, 1=star, 2=poop */

static void
CreateParticleBitmap(pgroup *g, BYTE r, BYTE b, BYTE gb, BYTE alpha) {
  BITMAPINFO bmi = {0};
  HDC hdcScreen;
  DWORD *bits;
  int cx, cy;
  float rx, ry, dx, dy, dist;
  int w = PARTICLE_SIZE, h = PARTICLE_SIZE;

  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  hdcScreen = GetDC(NULL);
  g->hdcMem = CreateCompatibleDC(hdcScreen);
  g->hbmp = CreateDIBSection(g->hdcMem, &bmi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
  SelectObject(g->hdcMem, g->hbmp);
  ReleaseDC(NULL, hdcScreen);

  rx = w / 2.0f;
  ry = h / 2.0f;
  for (cy = 0; cy < h; cy++) {
    for (cx = 0; cx < w; cx++) {
      dx = (cx - rx + 0.5f) / rx;
      dy = (cy - ry + 0.5f) / ry;
      dist = dx * dx + dy * dy;
      if (dist <= 1.0f) {
        BYTE a = (BYTE)(alpha * (1.0f - dist));
        bits[cy * w + cx] = ((DWORD)a << 24)
          | ((DWORD)(r * a / 255) << 16)
          | ((DWORD)(b * a / 255) << 8)
          | ((DWORD)(gb * a / 255));
      }
    }
  }
}

static void
CreateStarBitmap(pgroup *g, BYTE r, BYTE gb, BYTE b, BYTE alpha) {
  BITMAPINFO bmi = {0};
  HDC hdcScreen;
  DWORD *bits;
  int cx, cy, i;
  int w = STAR_SIZE, h = STAR_SIZE;
  float center = w / 2.0f;
  float outer_r = center * 0.95f;
  float inner_r = outer_r * 0.38f;
  float vx[10], vy[10];

  for (i = 0; i < 5; i++) {
    float ao = -3.14159265f/2.0f + i * 2*3.14159265f/5.0f;
    float ai = -3.14159265f/2.0f + (i + 0.5f) * 2*3.14159265f/5.0f;
    vx[i*2]   = center + outer_r * cosf(ao);
    vy[i*2]   = center + outer_r * sinf(ao);
    vx[i*2+1] = center + inner_r * cosf(ai);
    vy[i*2+1] = center + inner_r * sinf(ai);
  }

  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  hdcScreen = GetDC(NULL);
  g->hdcMem = CreateCompatibleDC(hdcScreen);
  g->hbmp = CreateDIBSection(g->hdcMem, &bmi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
  SelectObject(g->hdcMem, g->hbmp);
  ReleaseDC(NULL, hdcScreen);

  for (cy = 0; cy < h; cy++) {
    for (cx = 0; cx < w; cx++) {
      float px = cx + 0.5f, py = cy + 0.5f;
      int inside = 0, j = 9;
      for (i = 0; i < 10; i++) {
        if ((vy[i] > py) != (vy[j] > py) &&
            px < (vx[j]-vx[i]) * (py-vy[i]) / (vy[j]-vy[i]) + vx[i])
          inside = !inside;
        j = i;
      }
      if (inside) {
        BYTE a = alpha;
        bits[cy * w + cx] = ((DWORD)a << 24)
          | ((DWORD)(r * a / 255) << 16)
          | ((DWORD)(gb * a / 255) << 8)
          | ((DWORD)(b * a / 255));
      }
    }
  }
}

static void
CreatePoopBitmap(pgroup *g, BYTE r, BYTE gb, BYTE b, BYTE alpha) {
  BITMAPINFO bmi = {0};
  HDC hdcScreen;
  DWORD *bits;
  int cx, cy;
  int w = POOP_SIZE, h = POOP_SIZE;

  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  hdcScreen = GetDC(NULL);
  g->hdcMem = CreateCompatibleDC(hdcScreen);
  g->hbmp = CreateDIBSection(g->hdcMem, &bmi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
  SelectObject(g->hdcMem, g->hbmp);
  ReleaseDC(NULL, hdcScreen);

  for (cy = 0; cy < h; cy++) {
    for (cx = 0; cx < w; cx++) {
      float px = (cx + 0.5f) / w, py = (cy + 0.5f) / h;
      float dx, dy;
      int inside = 0;
      /* bottom */
      dx = (px - 0.50f) / 0.42f; dy = (py - 0.82f) / 0.18f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      /* middle */
      dx = (px - 0.50f) / 0.33f; dy = (py - 0.60f) / 0.20f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      /* top */
      dx = (px - 0.50f) / 0.23f; dy = (py - 0.40f) / 0.18f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      /* tip */
      dx = (px - 0.55f) / 0.12f; dy = (py - 0.23f) / 0.12f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      if (inside) {
        BYTE a = alpha;
        bits[cy * w + cx] = ((DWORD)a << 24)
          | ((DWORD)(r * a / 255) << 16)
          | ((DWORD)(gb * a / 255) << 8)
          | ((DWORD)(b * a / 255));
      }
    }
  }
}

static int
particle_size(void) {
  if (g_mode == 1) return STAR_SIZE;
  if (g_mode == 2) return POOP_SIZE;
  return PARTICLE_SIZE;
}

static void
ApplyLayered(HWND hwnd, int x, int y, HDC hdcMem) {
  HDC hdcScreen;
  POINT ptPos, ptSrc = {0, 0};
  int psz = particle_size();
  SIZE sz = {psz, psz};
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

  ptPos.x = x;
  ptPos.y = y;
  hdcScreen = GetDC(NULL);
  UpdateLayeredWindow(hwnd, hdcScreen, &ptPos, &sz, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
  ReleaseDC(NULL, hdcScreen);
}

static void
FreeGroupBitmap(pgroup *g) {
  if (g->hbmp) { DeleteObject(g->hbmp); g->hbmp = NULL; }
  if (g->hdcMem) { DeleteDC(g->hdcMem); g->hdcMem = NULL; }
}

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CALLBACK
UpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
  int i, j;
  for (i = 0; i < MAX_GROUPS; i++) {
    int done;
    if (!groups[i].active) continue;
    done = 1;
    for (j = 0; j < g_per_group; j++) {
      if (groups[i].p[j].hwnd == NULL) continue;
      groups[i].p[j].x += groups[i].p[j].dx;
      groups[i].p[j].y += groups[i].p[j].dy;
      groups[i].p[j].dy += g_mode == 1 ? 1 : 2;
      if (groups[i].p[j].dy >= 30) {
        DestroyWindow(groups[i].p[j].hwnd);
        groups[i].p[j].hwnd = NULL;
        continue;
      }
      done = 0;
      SetWindowPos(groups[i].p[j].hwnd, NULL,
          groups[i].p[j].x, groups[i].p[j].y, 0, 0,
          SWP_NOSIZE|SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOACTIVATE);
    }
    if (done) {
      FreeGroupBitmap(&groups[i]);
      groups[i].active = 0;
      g_active_count--;
    }
  }
  if (g_active_count <= 0) {
    KillTimer(NULL, g_timer);
    g_timer = 0;
    g_active_count = 0;
  }
}

static void
SpawnGroup(spawn_params *sp) {
  int i, gi;
  POINT pt = {0};
  RECT rc;

  for (gi = 0; gi < MAX_GROUPS; gi++) {
    if (!groups[gi].active) break;
  }
  if (gi == MAX_GROUPS) { free(sp); return; }

  if (sp->sw > 0 && sp->sh > 0) {
    GetWindowRect(g_target, &rc);
    pt.x = (rc.right - rc.left) / sp->sw * sp->sx;
    pt.y = (rc.bottom - rc.top) / sp->sh * sp->sy;
    ClientToScreen(g_target, &pt);
  } else {
    if (GetWindowRect(g_target, &rc)) {
      pt.x = (rc.left + rc.right) / 2 + rand() % 200 - 100;
      pt.y = (rc.top + rc.bottom) / 2 + rand() % 200 - 100;
      ClientToScreen(g_target, &pt);
    }
  }

  if (g_mode == 1)
    CreateStarBitmap(&groups[gi], sp->r, sp->g, sp->b, sp->a);
  else if (g_mode == 2)
    CreatePoopBitmap(&groups[gi], sp->r, sp->g, sp->b, sp->a);
  else
    CreateParticleBitmap(&groups[gi], sp->r, sp->g, sp->b, sp->a);
  groups[gi].active = 1;
  g_active_count++;

  {
    int psz = particle_size();
    for (i = 0; i < g_per_group; i++) {
      groups[gi].p[i].hwnd = CreateWindowEx(
          WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
          g_class, g_class, WS_POPUP, pt.x, pt.y,
          psz, psz,
          NULL, NULL, g_hinst, NULL);
      groups[gi].p[i].x = pt.x;
      groups[gi].p[i].y = pt.y;
      groups[gi].p[i].dx = (rand() % 10) - 5;
      groups[gi].p[i].dy = -10 - (rand() % 10);
      ApplyLayered(groups[gi].p[i].hwnd, pt.x, pt.y, groups[gi].hdcMem);
      ShowWindow(groups[gi].p[i].hwnd, SW_SHOWNOACTIVATE);
    }
  }

  if (!g_timer)
    g_timer = SetTimer(NULL, 0, 1, UpdateProc);

  free(sp);
}

unsigned __stdcall
ReaderThread(void *arg) {
  char buf[256];
  while (fgets(buf, sizeof(buf), stdin)) {
    int x = 0, y = 0, w = 0, h = 0;
    unsigned int r = 0xff, g = 0xff, b = 0xff, a = 70;
    char color[8] = "ffffff";
    spawn_params *sp;
    if (sscanf(buf, "%d,%d,%d,%d %6s %u", &x, &y, &w, &h, color, &a) >= 4) {
      sscanf(color, "%02x%02x%02x", &r, &g, &b);
      if (a > 255) a = 255;
      sp = (spawn_params*)malloc(sizeof(spawn_params));
      sp->sx = x;
      sp->sy = y;
      sp->sw = w;
      sp->sh = h;
      sp->r = (BYTE)r;
      sp->g = (BYTE)g;
      sp->b = (BYTE)b;
      sp->a = (BYTE)a;
      PostThreadMessage(g_mainThread, WM_SPAWN, 0, (LPARAM)sp);
    }
  }
  PostThreadMessage(g_mainThread, WM_QUIT, 0, 0);
  return 0;
}

int
main(int argc, char *argv[]) {
  WNDCLASSEX wc;
  MSG msg;

  g_hinst = GetModuleHandle(NULL);
  g_mainThread = GetCurrentThreadId();
  srand((unsigned)time(NULL));

  {
    int i;
    for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
        int n = atoi(argv[++i]);
        if (n >= 1 && n <= MAX_PER_GROUP)
          g_per_group = n;
      } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
        g_target = (HWND)(intptr_t)atoll(argv[++i]);
      } else if (strcmp(argv[i], "-mode") == 0 && i + 1 < argc) {
        i++;
        if (strcmp(argv[i], "star") == 0) g_mode = 1;
        else if (strcmp(argv[i], "unko") == 0) g_mode = 2;
      }
    }
  }
  if (g_target == NULL)
    g_target = GetForegroundWindow();

  wc.cbSize        = sizeof(WNDCLASSEX);
  wc.style         = 0;
  wc.lpfnWndProc   = WindowProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = g_hinst;
  wc.hIcon         = (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  wc.hCursor       = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = g_class;
  wc.hIconSm       = (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  if (!RegisterClassEx(&wc)) return 1;

  _beginthreadex(NULL, 0, ReaderThread, NULL, 0, NULL);

  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    if (msg.message == WM_SPAWN) {
      SpawnGroup((spawn_params*)msg.lParam);
    }
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}

/* vim:set et sw=2: */
