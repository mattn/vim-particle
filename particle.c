#include <windows.h>
#include <stdio.h>
#include <time.h>

typedef struct {
  HWND hwnd;
  int x;
  int y;
  int dx;
  int dy;
  int r;
} particle;

static particle p[5] = {0};
static HBRUSH hb = NULL;

LRESULT CALLBACK
WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  HDC hdc;
  RECT rc;
  PAINTSTRUCT ps;

  switch (uMsg) {
    case WM_PAINT:
      hdc = BeginPaint(hwnd, &ps);
      GetClientRect(hwnd, &rc);
      FillRect(hdc, &rc, hb);
      EndPaint(hwnd, &ps);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CALLBACK
UpdateProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
  int i, r;
  for (i = 0; i < sizeof(p)/sizeof(p[0]); i++) {
    p[i].x += p[i].dx;
    p[i].y += p[i].dy;
    p[i].dy++;
    p[i].r++;
    if (p[i].r == 20) {
      DestroyWindow(p[i].hwnd);
      break;
    }
    r = 20 - abs(p[i].r);
    HRGN hrgn = CreateEllipticRgn(0, 0, r, r);
    if (hrgn == NULL)
      continue;
    SetWindowRgn(p[i].hwnd, hrgn, TRUE);
    MoveWindow(p[i].hwnd, p[i].x, p[i].y, r, r, TRUE);
  }
}

int WINAPI
WinMain(HINSTANCE hinst, HINSTANCE hinstPrev, LPSTR lpszCmdLine, int nCmdShow) {
  TCHAR app[] = TEXT("particle");
  MSG msg;
  WNDCLASSEX wc;
  int i;
  int exit_code = 1;
  int argc;
  LPWSTR *argv;
  int r = 0xff, g = 0xff, b = 0xff;
  GUITHREADINFO ti = {0};
  RECT rc = {0};
  POINT pt;
  HWND hwnd;

  srand((unsigned)time(NULL));

  argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  hwnd = argc >= 1 ? (HWND) _wtoi64(argv[1]) : NULL;
  if (hwnd != NULL) {
    GetWindowRect(hwnd, &rc);
    pt.x = (rc.left + rc.right) / 2 + rand() % 200 - 100;
    pt.y = (rc.top + rc.bottom) / 2 + rand() % 200 - 100;
  } else {
    GetCursorPos(&pt);
  }
  if (argc >= 2) {
    swscanf(argv[2], L"%02x%02x%02x", &r, &g, &b);
  } else {
    r = rand() % 255;
    g = rand() % 255;
    b = rand() % 255;
  }

  wc.cbSize        = sizeof(WNDCLASSEX);
  wc.style         = 0;
  wc.lpfnWndProc   = WindowProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = hinst;
  wc.hIcon         = (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  wc.hCursor       = (HCURSOR)LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
  wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = app;
  wc.hIconSm       = (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
  if (!RegisterClassEx(&wc)) goto leave;

  hb = CreateSolidBrush(RGB(r, g, b));
  for (i = 0; i < sizeof(p)/sizeof(p[0]); i++) {
    p[i].hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
        app, app, WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, NULL, NULL, hinst, NULL);
    if (p[i].hwnd == NULL) goto leave;
    SetLayeredWindowAttributes(p[i].hwnd, RGB(0xFF, 0xFF, 0xFF), 70, LWA_ALPHA);

    p[i].x = pt.x;
    p[i].y = pt.y;
    p[i].dx = (rand() % 10) - 5;
    p[i].dy = -10 - (rand() % 10);
    p[i].r = -20;

    ShowWindow(p[i].hwnd, nCmdShow);
    UpdateWindow(p[i].hwnd);
  }
  SetTimer(NULL, 0, 1, UpdateProc);
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  exit_code = (int)msg.wParam;

leave:
  if (argv) LocalFree(argv);
  return exit_code;
}

/* vim:set et sw=2: */
