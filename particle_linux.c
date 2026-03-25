#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>

#define MAX_GROUPS 64
#define DEFAULT_PER_GROUP 3
#define MAX_PER_GROUP 10
#define PARTICLE_SIZE 10
#define STAR_SIZE 30
#define POOP_SIZE 30

typedef struct {
  Window win;
  int x, y, dx, dy;
  int alive;
} particle;

typedef struct {
  particle p[MAX_PER_GROUP];
  unsigned int *bitmap;
  int bmp_size;
  int active;
} pgroup;

typedef struct {
  int sx, sy, sw, sh;
  unsigned char r, g, b, a;
} spawn_params;

static pgroup groups[MAX_GROUPS];
static int g_per_group = DEFAULT_PER_GROUP;
static Window g_target;
static Display *g_dpy;
static int g_screen;
static Visual *g_visual;
static Colormap g_colormap;
static int g_depth;
static int g_active_count;
static int g_mode; /* 0=normal, 1=star, 2=poop */
static int g_pipe[2]; /* pipe for reader thread -> main thread */

static int
find_argb_visual(Display *dpy, int screen, Visual **vis_out, int *depth_out) {
  XVisualInfo vinfo_template;
  XVisualInfo *vinfo_list;
  int n, i;

  vinfo_template.screen = screen;
  vinfo_template.depth = 32;
  vinfo_template.class = TrueColor;
  vinfo_list = XGetVisualInfo(dpy,
      VisualScreenMask | VisualDepthMask | VisualClassMask,
      &vinfo_template, &n);
  if (!vinfo_list) return 0;

  for (i = 0; i < n; i++) {
    XRenderPictFormat *fmt;
    fmt = XRenderFindVisualFormat(dpy, vinfo_list[i].visual);
    if (fmt && fmt->type == PictTypeDirect && fmt->direct.alphaMask) {
      *vis_out = vinfo_list[i].visual;
      *depth_out = 32;
      XFree(vinfo_list);
      return 1;
    }
  }
  XFree(vinfo_list);
  return 0;
}

static void
create_particle_bitmap(pgroup *g, unsigned char r, unsigned char gc,
    unsigned char b, unsigned char alpha) {
  int w = PARTICLE_SIZE, h = PARTICLE_SIZE;
  int cx, cy;
  float rx, ry, dx, dy, dist;

  g->bmp_size = w;
  g->bitmap = (unsigned int *)calloc(w * h, sizeof(unsigned int));

  rx = w / 2.0f;
  ry = h / 2.0f;
  for (cy = 0; cy < h; cy++) {
    for (cx = 0; cx < w; cx++) {
      dx = (cx - rx + 0.5f) / rx;
      dy = (cy - ry + 0.5f) / ry;
      dist = dx * dx + dy * dy;
      if (dist <= 1.0f) {
        unsigned char a = (unsigned char)(alpha * (1.0f - dist));
        g->bitmap[cy * w + cx] = ((unsigned int)a << 24)
          | ((unsigned int)(r * a / 255) << 16)
          | ((unsigned int)(gc * a / 255) << 8)
          | ((unsigned int)(b * a / 255));
      }
    }
  }
}

static void
create_star_bitmap(pgroup *g, unsigned char r, unsigned char gc,
    unsigned char b, unsigned char alpha) {
  int w = STAR_SIZE, h = STAR_SIZE;
  int cx, cy, i;
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

  g->bmp_size = w;
  g->bitmap = (unsigned int *)calloc(w * h, sizeof(unsigned int));

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
        unsigned char a = alpha;
        g->bitmap[cy * w + cx] = ((unsigned int)a << 24)
          | ((unsigned int)(r * a / 255) << 16)
          | ((unsigned int)(gc * a / 255) << 8)
          | ((unsigned int)(b * a / 255));
      }
    }
  }
}

static void
create_poop_bitmap(pgroup *g, unsigned char r, unsigned char gc,
    unsigned char b, unsigned char alpha) {
  int w = POOP_SIZE, h = POOP_SIZE;
  int cx, cy;

  g->bmp_size = w;
  g->bitmap = (unsigned int *)calloc(w * h, sizeof(unsigned int));

  for (cy = 0; cy < h; cy++) {
    for (cx = 0; cx < w; cx++) {
      float px = (cx + 0.5f) / w, py = (cy + 0.5f) / h;
      float dx, dy;
      int inside = 0;
      dx = (px - 0.50f) / 0.42f; dy = (py - 0.82f) / 0.18f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      dx = (px - 0.50f) / 0.33f; dy = (py - 0.60f) / 0.20f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      dx = (px - 0.50f) / 0.23f; dy = (py - 0.40f) / 0.18f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      dx = (px - 0.55f) / 0.12f; dy = (py - 0.23f) / 0.12f;
      if (dx*dx + dy*dy <= 1.0f) inside = 1;
      if (inside) {
        unsigned char a = alpha;
        g->bitmap[cy * w + cx] = ((unsigned int)a << 24)
          | ((unsigned int)(r * a / 255) << 16)
          | ((unsigned int)(gc * a / 255) << 8)
          | ((unsigned int)(b * a / 255));
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
apply_window_image(Window win, unsigned int *bitmap, int size) {
  XImage *img;
  GC gc;

  img = XCreateImage(g_dpy, g_visual, g_depth, ZPixmap, 0,
      (char *)bitmap, size, size, 32, 0);
  if (!img) return;

  gc = XCreateGC(g_dpy, win, 0, NULL);
  XPutImage(g_dpy, win, gc, img, 0, 0, 0, 0, size, size);
  XFreeGC(g_dpy, gc);

  img->data = NULL;
  XDestroyImage(img);
}

static void
free_group(pgroup *g) {
  if (g->bitmap) { free(g->bitmap); g->bitmap = NULL; }
}

static void
update_particles(void) {
  int i, j;
  for (i = 0; i < MAX_GROUPS; i++) {
    int done;
    if (!groups[i].active) continue;
    done = 1;
    for (j = 0; j < g_per_group; j++) {
      if (!groups[i].p[j].alive) continue;
      groups[i].p[j].x += groups[i].p[j].dx;
      groups[i].p[j].y += groups[i].p[j].dy;
      groups[i].p[j].dy += g_mode == 1 ? 1 : 2;
      if (groups[i].p[j].dy >= 30) {
        XDestroyWindow(g_dpy, groups[i].p[j].win);
        groups[i].p[j].alive = 0;
        continue;
      }
      done = 0;
      XMoveWindow(g_dpy, groups[i].p[j].win,
          groups[i].p[j].x, groups[i].p[j].y);
    }
    if (done) {
      free_group(&groups[i]);
      groups[i].active = 0;
      g_active_count--;
    }
  }
  XFlush(g_dpy);
}

static void
spawn_group(spawn_params *sp) {
  int i, gi;
  int px, py;
  XSetWindowAttributes attrs;
  int psz;

  for (gi = 0; gi < MAX_GROUPS; gi++) {
    if (!groups[gi].active) break;
  }
  if (gi == MAX_GROUPS) { free(sp); return; }

  if (sp->sw > 0 && sp->sh > 0) {
    XWindowAttributes wa;
    int rx, ry;
    Window child;
    XGetWindowAttributes(g_dpy, g_target, &wa);
    XTranslateCoordinates(g_dpy, g_target, RootWindow(g_dpy, g_screen),
        0, 0, &rx, &ry, &child);
    px = rx + wa.width * sp->sx / sp->sw;
    py = ry + wa.height * sp->sy / sp->sh;
  } else {
    XWindowAttributes wa;
    int rx, ry;
    Window child;
    XGetWindowAttributes(g_dpy, g_target, &wa);
    XTranslateCoordinates(g_dpy, g_target, RootWindow(g_dpy, g_screen),
        0, 0, &rx, &ry, &child);
    px = rx + wa.width / 2 + rand() % 200 - 100;
    py = ry + wa.height / 2 + rand() % 200 - 100;
  }

  if (g_mode == 1)
    create_star_bitmap(&groups[gi], sp->r, sp->g, sp->b, sp->a);
  else if (g_mode == 2)
    create_poop_bitmap(&groups[gi], sp->r, sp->g, sp->b, sp->a);
  else
    create_particle_bitmap(&groups[gi], sp->r, sp->g, sp->b, sp->a);
  groups[gi].active = 1;
  g_active_count++;

  psz = particle_size();
  attrs.override_redirect = True;
  attrs.colormap = g_colormap;
  attrs.border_pixel = 0;
  attrs.background_pixel = 0;

  for (i = 0; i < g_per_group; i++) {
    groups[gi].p[i].win = XCreateWindow(g_dpy,
        RootWindow(g_dpy, g_screen),
        px, py, psz, psz, 0,
        g_depth, InputOutput, g_visual,
        CWOverrideRedirect | CWColormap | CWBorderPixel | CWBackPixel,
        &attrs);

    groups[gi].p[i].x = px;
    groups[gi].p[i].y = py;
    groups[gi].p[i].dx = (rand() % 10) - 5;
    groups[gi].p[i].dy = -10 - (rand() % 10);
    groups[gi].p[i].alive = 1;

    XMapWindow(g_dpy, groups[gi].p[i].win);
    apply_window_image(groups[gi].p[i].win,
        groups[gi].bitmap, groups[gi].bmp_size);
  }

  XFlush(g_dpy);
  free(sp);
}

static void *
reader_thread(void *arg) {
  char buf[256];
  (void)arg;

  while (fgets(buf, sizeof(buf), stdin)) {
    int x = 0, y = 0, w = 0, h = 0;
    unsigned int r = 0xff, g = 0xff, b = 0xff, a = 70;
    char color[8] = "ffffff";
    spawn_params *sp;
    if (sscanf(buf, "%d,%d,%d,%d %6s %u", &x, &y, &w, &h, color, &a) >= 4) {
      sscanf(color, "%02x%02x%02x", &r, &g, &b);
      if (a > 255) a = 255;
      sp = (spawn_params *)malloc(sizeof(spawn_params));
      sp->sx = x;
      sp->sy = y;
      sp->sw = w;
      sp->sh = h;
      sp->r = (unsigned char)r;
      sp->g = (unsigned char)g;
      sp->b = (unsigned char)b;
      sp->a = (unsigned char)a;
      write(g_pipe[1], &sp, sizeof(sp));
    }
  }
  /* signal quit: write NULL pointer */
  {
    spawn_params *null_sp = NULL;
    write(g_pipe[1], &null_sp, sizeof(null_sp));
  }
  return NULL;
}

int
main(int argc, char *argv[]) {
  pthread_t reader;
  int running = 1;
  int xfd;

  srand((unsigned)time(NULL));

  {
    int i;
    for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
        int n = atoi(argv[++i]);
        if (n >= 1 && n <= MAX_PER_GROUP)
          g_per_group = n;
      } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
        g_target = (Window)atoll(argv[++i]);
      } else if (strcmp(argv[i], "-mode") == 0 && i + 1 < argc) {
        i++;
        if (strcmp(argv[i], "star") == 0) g_mode = 1;
        else if (strcmp(argv[i], "unko") == 0) g_mode = 2;
      }
    }
  }

  g_dpy = XOpenDisplay(NULL);
  if (!g_dpy) {
    fprintf(stderr, "Cannot open display\n");
    return 1;
  }
  g_screen = DefaultScreen(g_dpy);

  if (!find_argb_visual(g_dpy, g_screen, &g_visual, &g_depth)) {
    /* fallback: use default visual (no transparency) */
    g_visual = DefaultVisual(g_dpy, g_screen);
    g_depth = DefaultDepth(g_dpy, g_screen);
    g_colormap = DefaultColormap(g_dpy, g_screen);
  } else {
    g_colormap = XCreateColormap(g_dpy, RootWindow(g_dpy, g_screen),
        g_visual, AllocNone);
  }

  if (g_target == 0)
    g_target = RootWindow(g_dpy, g_screen);

  if (pipe(g_pipe) < 0) {
    fprintf(stderr, "pipe failed\n");
    return 1;
  }
  fcntl(g_pipe[0], F_SETFL, O_NONBLOCK);

  pthread_create(&reader, NULL, reader_thread, NULL);

  xfd = ConnectionNumber(g_dpy);

  while (running) {
    struct pollfd fds[2];
    int timeout_ms = g_active_count > 0 ? 16 : -1; /* ~60fps when active */

    fds[0].fd = xfd;
    fds[0].events = POLLIN;
    fds[1].fd = g_pipe[0];
    fds[1].events = POLLIN;

    poll(fds, 2, timeout_ms);

    /* drain X events */
    while (XPending(g_dpy)) {
      XEvent ev;
      XNextEvent(g_dpy, &ev);
    }

    /* handle pipe messages (spawn requests) */
    if (fds[1].revents & POLLIN) {
      spawn_params *sp;
      while (read(g_pipe[0], &sp, sizeof(sp)) == (ssize_t)sizeof(sp)) {
        if (sp == NULL) {
          running = 0;
          break;
        }
        spawn_group(sp);
      }
    }

    /* update particle positions */
    if (g_active_count > 0) {
      update_particles();
    }
  }

  /* cleanup */
  {
    int i, j;
    for (i = 0; i < MAX_GROUPS; i++) {
      if (!groups[i].active) continue;
      for (j = 0; j < g_per_group; j++) {
        if (groups[i].p[j].alive)
          XDestroyWindow(g_dpy, groups[i].p[j].win);
      }
      free_group(&groups[i]);
    }
  }

  XCloseDisplay(g_dpy);
  close(g_pipe[0]);
  close(g_pipe[1]);

  return 0;
}

/* vim:set et sw=2: */
