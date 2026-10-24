/* Offline NBIS test: nbis_matrix <scale> <flags> f1.pgm f2.pgm ...
 * Loads PGMs, optionally upscales (bilinear), detects minutiae like vfs0090
 * does and prints bozorth3 score matrix. */
#include "fpi-image.h"
#include "fpi-print.h"
#include "fp-print-private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bozorth_probe_init (struct xyt_struct *);
int bozorth_to_gallery (int, struct xyt_struct *, struct xyt_struct *);

static FpImage *
load_pgm (const char *path, double scale, int flags)
{
  FILE *f = fopen (path, "rb");
  int w, h, max;
  if (!f || fscanf (f, "P5 %d %d %d", &w, &h, &max) != 3)
    exit (1);
  fgetc (f);
  guint8 *px = g_malloc (w * h);
  if (fread (px, 1, w * h, f) != (size_t) (w * h))
    exit (1);
  fclose (f);

  int W = (int) (w * scale), H = (int) (h * scale);
  FpImage *img = fp_image_new (W, H);
  img->flags = flags;
  img->data = g_malloc (W * H);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      {
        double sx = x / scale, sy = y / scale;
        int x0 = (int) sx, y0 = (int) sy;
        int x1 = MIN (x0 + 1, w - 1), y1 = MIN (y0 + 1, h - 1);
        double fx = sx - x0, fy = sy - y0;
        double v = px[y0 * w + x0] * (1 - fx) * (1 - fy) + px[y0 * w + x1] * fx * (1 - fy) +
                   px[y1 * w + x0] * (1 - fx) * fy + px[y1 * w + x1] * fx * fy;
        img->data[y * W + x] = (guint8) v;
      }
  g_free (px);
  return img;
}

static void
detected (GObject *src, GAsyncResult *res, gpointer loop)
{
  GError *err = NULL;
  if (!fp_image_detect_minutiae_finish (FP_IMAGE (src), res, &err))
    g_printerr ("minutiae failed: %s\n", err->message);
  g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
  double scale = atof (argv[1]);
  int flags = atoi (argv[2]);
  int n = argc - 3;
  struct xyt_struct **xyt = g_new0 (struct xyt_struct *, n);
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  for (int i = 0; i < n; i++)
    {
      FpImage *img = load_pgm (argv[i + 3], scale < 0 ? 1 : scale, flags);
      if (scale < 0)
        img = fpi_image_resize (img, (guint) -scale, (guint) -scale);
      fp_image_detect_minutiae (img, NULL, detected, loop);
      g_main_loop_run (loop);
      FpPrint *p = g_object_new (FP_TYPE_PRINT, "driver", "vfs0090", "device-id", "x", NULL);
      fpi_print_set_type (p, FPI_PRINT_NBIS);
      GError *err = NULL;
      if (!fpi_print_add_from_image (p, img, &err))
        {
          printf ("%s: %s\n", argv[i + 3], err->message);
          continue;
        }
      xyt[i] = g_ptr_array_index (p->prints, 0);
      printf ("%s: %u minutiae\n", argv[i + 3], fp_image_get_minutiae (img)->len);
    }

  printf ("score matrix (probe row vs gallery col):\n");
  for (int i = 0; i < n; i++)
    {
      if (!xyt[i])
        continue;
      int plen = bozorth_probe_init (xyt[i]);
      for (int j = 0; j < n; j++)
        printf ("%5d", (i == j || !xyt[j]) ? -1 : bozorth_to_gallery (plen, xyt[i], xyt[j]));
      printf ("\n");
    }
  return 0;
}
