/**
 * Bresenham Curve Rasterizing Algorithms
 * @author alois zingl
 * @version V20.15 april 2020
 * @copyright MIT open-source license software
 * @url https://github.com/zingl/Bresenham
 * @author  Zingl Alois
 */

#include <stdlib.h>
#include <functional>
#include <assert.h>
#include <cmath>
#include <algorithm>
namespace bresenhamrasterization
{

void plotLine(int x0, int y0, int x1, int y1, std::function<void(int, int)> setPixel)
{
   int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
   int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
   int err = dx + dy, e2; /* error value e_xy */

   for (;;)
   { /* loop */
      setPixel(x0, y0);
      e2 = 2 * err;
      if (e2 >= dy)
      { /* e_xy+e_x > 0 */
         if (x0 == x1)
            break;
         err += dy;
         x0 += sx;
      }
      if (e2 <= dx)
      { /* e_xy+e_y < 0 */
         if (y0 == y1)
            break;
         err += dx;
         y0 += sy;
      }
   }
}

void plotLine3d(int x0, int y0, int z0, int x1, int y1, int z1, std::function<void(int, int, int)> setPixel)
{
   int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
   int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
   int dz = abs(z1 - z0), sz = z0 < z1 ? 1 : -1;
   int dm = dx > dy && dx > dz ? dx : dy > dz ? dy
                                              : dz,
       i = dm;            /* max diff */
   x1 = y1 = z1 = dm / 2; /* error offset */

   for (;;)
   {
      setPixel(x0, y0, z0);
      if (i-- == 0)
         break;
      x1 -= dx;
      if (x1 < 0)
      {
         x1 += dm;
         x0 += sx;
      }
      y1 -= dy;
      if (y1 < 0)
      {
         y1 += dm;
         y0 += sy;
      }
      z1 -= dz;
      if (z1 < 0)
      {
         z1 += dm;
         z0 += sz;
      }
   }
}

#ifdef FIX_CONVERT_TO_FLOAT
void plotEllipse(int xm, int ym, int a, int b, std::function<void(int, int)> setPixel)
{
   int x = -a, y = 0;                                        /* II. quadrant from bottom left to top right */
   long e2 = (long)b * b, err = (long)x * (2 * e2 + x) + e2; /* error of 1.step */

   do
   {
      setPixel(xm - x, ym + y); /*   I. Quadrant */
      setPixel(xm + x, ym + y); /*  II. Quadrant */
      setPixel(xm + x, ym - y); /* III. Quadrant */
      setPixel(xm - x, ym - y); /*  IV. Quadrant */
      e2 = 2 * err;
      if (e2 >= (x * 2 + 1) * (long)b * b) /* e_xy+e_x > 0 */
         err += (++x * 2 + 1) * (long)b * b;
      if (e2 <= (y * 2 + 1) * (long)a * a) /* e_xy+e_y < 0 */
         err += (++y * 2 + 1) * (long)a * a;
   } while (x <= 0);

   while (y++ < b)
   {                        /* too early stop of flat ellipses a=1, */
      setPixel(xm, ym + y); /* -> finish tip of ellipse */
      setPixel(xm, ym - y);
   }
}

void plotOptimizedEllipse(int xm, int ym, int a, int b, std::function<void(int, int)> setPixel)
{
   long x = -a, y = 0;                      /* II. quadrant from bottom left to top right */
   long e2 = b, dx = (1 + 2 * x) * e2 * e2; /* error increment  */
   long dy = x * x, err = dx + dy;          /* error of 1.step */

   do
   {
      setPixel(xm - x, ym + y); /*   I. Quadrant */
      setPixel(xm + x, ym + y); /*  II. Quadrant */
      setPixel(xm + x, ym - y); /* III. Quadrant */
      setPixel(xm - x, ym - y); /*  IV. Quadrant */
      e2 = 2 * err;
      if (e2 >= dx)
      {
         x++;
         err += dx += 2 * (long)b * b;
      } /* x step */
      if (e2 <= dy)
      {
         y++;
         err += dy += 2 * (long)a * a;
      } /* y step */
   } while (x <= 0);

   while (y++ < b)
   {                        /* too early stop for flat ellipses with a=1, */
      setPixel(xm, ym + y); /* -> finish tip of ellipse */
      setPixel(xm, ym - y);
   }
}

void plotCircle(int xm, int ym, int r, std::function<void(int, int)> setPixel)
{
   int x = -r, y = 0, err = 2 - 2 * r; /* bottom left to top right */
   do
   {
      setPixel(xm - x, ym + y); /*   I. Quadrant +x +y */
      setPixel(xm - y, ym - x); /*  II. Quadrant -x +y */
      setPixel(xm + x, ym - y); /* III. Quadrant -x -y */
      setPixel(xm + y, ym + x); /*  IV. Quadrant +x -y */
      r = err;
      if (r <= y)
         err += ++y * 2 + 1; /* e_xy+e_y < 0 */
      if (r > x || err > y)  /* e_xy+e_x > 0 or no 2nd y-step */
         err += ++x * 2 + 1; /* -> x-step now */
   } while (x < 0);
}

void plotEllipseRect(int x0, int y0, int x1, int y1, std::function<void(int, int)> setPixel)
{                                                                /* rectangular parameter enclosing the ellipse */
   long a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;          /* diameter */
   double dx = 4 * (1.0 - a) * b * b, dy = 4 * (b1 + 1) * a * a; /* error increment */
   double err = dx + dy + b1 * a * a, e2;                        /* error of 1.step */

   if (x0 > x1)
   {
      x0 = x1;
      x1 += a;
   } /* if called with swapped points */
   if (y0 > y1)
      y0 = y1; /* .. exchange them */
   y0 += (b + 1) / 2;
   y1 = y0 - b1; /* starting pixel */
   a = 8 * a * a;
   b1 = 8 * b * b;

   do
   {
      setPixel(x1, y0); /*   I. Quadrant */
      setPixel(x0, y0); /*  II. Quadrant */
      setPixel(x0, y1); /* III. Quadrant */
      setPixel(x1, y1); /*  IV. Quadrant */
      e2 = 2 * err;
      if (e2 <= dy)
      {
         y0++;
         y1--;
         err += dy += a;
      } /* y step */
      if (e2 >= dx || 2 * err > dy)
      {
         x0++;
         x1--;
         err += dx += b1;
      } /* x step */
   } while (x0 <= x1);

   while (y0 - y1 <= b)
   {                        /* too early stop of flat ellipses a=1 */
      setPixel(x0 - 1, y0); /* -> finish tip of ellipse */
      setPixel(x1 + 1, y0++);
      setPixel(x0 - 1, y1);
      setPixel(x1 + 1, y1--);
   }
}

void plotQuadBezierSeg(int x0, int y0, int x1, int y1, int x2, int y2, std::function<void(int, int)> setPixel)
{ /* plot a limited quadratic Bezier segment */
   int sx = x2 - x1, sy = y2 - y1;
   long xx = x0 - x1, yy = y0 - y1, xy;         /* relative values for checks */
   double dx, dy, err, cur = xx * sy - yy * sx; /* curvature */

   assert(xx * sx <= 0 && yy * sy <= 0); /* sign of gradient must not change */

   if (sx * (long)sx + sy * (long)sy > xx * xx + yy * yy)
   { /* begin with longer part */
      x2 = x0;
      x0 = sx + x1;
      y2 = y0;
      y0 = sy + y1;
      cur = -cur; /* swap P0 P2 */
   }
   if (cur != 0)
   { /* no straight line */
      xx += sx;
      xx *= sx = x0 < x2 ? 1 : -1; /* x step direction */
      yy += sy;
      yy *= sy = y0 < y2 ? 1 : -1; /* y step direction */
      xy = 2 * xx * yy;
      xx *= xx;
      yy *= yy; /* differences 2nd degree */
      if (cur * sx * sy < 0)
      { /* negated curvature? */
         xx = -xx;
         yy = -yy;
         xy = -xy;
         cur = -cur;
      }
      dx = 4.0 * sy * cur * (x1 - x0) + xx - xy; /* differences 1st degree */
      dy = 4.0 * sx * cur * (y0 - y1) + yy - xy;
      xx += xx;
      yy += yy;
      err = dx + dy + xy; /* error 1st step */
      do
      {
         setPixel(x0, y0); /* plot curve */
         if (x0 == x2 && y0 == y2)
            return;         /* last pixel -> curve finished */
         y1 = 2 * err < dx; /* save value for test of y step */
         if (2 * err > dy)
         {
            x0 += sx;
            dx -= xy;
            err += dy += yy;
         } /* x step */
         if (y1)
         {
            y0 += sy;
            dy -= xy;
            err += dx += xx;
         } /* y step */
      } while (dy < 0 && dx > 0); /* gradient negates -> algorithm fails */
   }
   plotLine(x0, y0, x2, y2, setPixel); /* plot remaining part to end */
}

void plotQuadBezier(int x0, int y0, int x1, int y1, int x2, int y2, std::function<void(int, int)> setPixel)
{ /* plot any quadratic Bezier curve */
   int x = x0 - x1, y = y0 - y1;
   double t = x0 - 2 * x1 + x2, r;

   if ((long)x * (x2 - x1) > 0)
   {                               /* horizontal cut at P4? */
      if ((long)y * (y2 - y1) > 0) /* vertical cut at P6 too? */
         if (fabs((y0 - 2 * y1 + y2) / t * x) > abs(y))
         { /* which first? */
            x0 = x2;
            x2 = x + x1;
            y0 = y2;
            y2 = y + y1; /* swap points */
         } /* now horizontal cut at P4 comes first */
      t = (x0 - x1) / t;
      r = (1 - t) * ((1 - t) * y0 + 2.0 * t * y1) + t * t * y2; /* By(t=P4) */
      t = (x0 * x2 - x1 * x1) * t / (x0 - x1);                  /* gradient dP4/dx=0 */
      x = floor(t + 0.5);
      y = floor(r + 0.5);
      r = (y1 - y0) * (t - x0) / (x1 - x0) + y0; /* intersect P3 | P0 P1 */
      plotQuadBezierSeg(x0, y0, x, floor(r + 0.5), x, y, setPixel);
      r = (y1 - y2) * (t - x2) / (x1 - x2) + y2; /* intersect P4 | P1 P2 */
      x0 = x1 = x;
      y0 = y;
      y1 = floor(r + 0.5); /* P0 = P4, P1 = P8 */
   }
   if ((long)(y0 - y1) * (y2 - y1) > 0)
   { /* vertical cut at P6? */
      t = y0 - 2 * y1 + y2;
      t = (y0 - y1) / t;
      r = (1 - t) * ((1 - t) * x0 + 2.0 * t * x1) + t * t * x2; /* Bx(t=P6) */
      t = (y0 * y2 - y1 * y1) * t / (y0 - y1);                  /* gradient dP6/dy=0 */
      x = floor(r + 0.5);
      y = floor(t + 0.5);
      r = (x1 - x0) * (t - y0) / (y1 - y0) + x0; /* intersect P6 | P0 P1 */
      plotQuadBezierSeg(x0, y0, floor(r + 0.5), y, x, y, setPixel);
      r = (x1 - x2) * (t - y2) / (y1 - y2) + x2; /* intersect P7 | P1 P2 */
      x0 = x;
      x1 = floor(r + 0.5);
      y0 = y1 = y; /* P0 = P6, P1 = P7 */
   }
   plotQuadBezierSeg(x0, y0, x1, y1, x2, y2, setPixel); /* remaining part */
}

void plotQuadRationalBezierSeg(int x0, int y0, int x1, int y1,
                               int x2, int y2, float w, std::function<void(int, int)> setPixel)
{                                  /* plot a limited rational Bezier segment, squared weight */
   int sx = x2 - x1, sy = y2 - y1; /* relative values for checks */
   double dx = x0 - x2, dy = y0 - y2, xx = x0 - x1, yy = y0 - y1;
   double xy = xx * sy + yy * sx, cur = xx * sy - yy * sx, err; /* curvature */

   assert(xx * sx <= 0.0 && yy * sy <= 0.0); /* sign of gradient must not change */

   if (cur != 0.0 && w > 0.0)
   { /* no straight line */
      if (sx * (long)sx + sy * (long)sy > xx * xx + yy * yy)
      { /* begin with longer part */
         x2 = x0;
         x0 -= dx;
         y2 = y0;
         y0 -= dy;
         cur = -cur; /* swap P0 P2 */
      }
      xx = 2.0 * (4.0 * w * sx * xx + dx * dx); /* differences 2nd degree */
      yy = 2.0 * (4.0 * w * sy * yy + dy * dy);
      sx = x0 < x2 ? 1 : -1; /* x step direction */
      sy = y0 < y2 ? 1 : -1; /* y step direction */
      xy = -2.0 * sx * sy * (2.0 * w * xy + dx * dy);

      if (cur * sx * sy < 0.0)
      { /* negated curvature? */
         xx = -xx;
         yy = -yy;
         xy = -xy;
         cur = -cur;
      }
      dx = 4.0 * w * (x1 - x0) * sy * cur + xx / 2.0 + xy; /* differences 1st degree */
      dy = 4.0 * w * (y0 - y1) * sx * cur + yy / 2.0 + xy;

      if (w < 0.5 && (dy > xy || dx < xy))
      { /* flat ellipse, algorithm fails */
         cur = (w + 1.0) / 2.0;
         w = sqrt(w);
         xy = 1.0 / (w + 1.0);
         sx = floor((x0 + 2.0 * w * x1 + x2) * xy / 2.0 + 0.5); /* subdivide curve in half */
         sy = floor((y0 + 2.0 * w * y1 + y2) * xy / 2.0 + 0.5);
         dx = floor((w * x1 + x0) * xy + 0.5);
         dy = floor((y1 * w + y0) * xy + 0.5);
         plotQuadRationalBezierSeg(x0, y0, dx, dy, sx, sy, cur, setPixel); /* plot separately */
         dx = floor((w * x1 + x2) * xy + 0.5);
         dy = floor((y1 * w + y2) * xy + 0.5);
         plotQuadRationalBezierSeg(sx, sy, dx, dy, x2, y2, cur, setPixel);
         return;
      }
      err = dx + dy - xy; /* error 1.step */
      do
      {
         setPixel(x0, y0); /* plot curve */
         if (x0 == x2 && y0 == y2)
            return; /* last pixel -> curve finished */
         x1 = 2 * err > dy;
         y1 = 2 * (err + yy) < -dy; /* save value for test of x step */
         if (2 * err < dx || y1)
         {
            y0 += sy;
            dy += xy;
            err += dx += xx;
         } /* y step */
         if (2 * err > dx || x1)
         {
            x0 += sx;
            dx += xy;
            err += dy += yy;
         } /* x step */
      } while (dy <= xy && dx >= xy); /* gradient negates -> algorithm fails */
   }
   plotLine(x0, y0, x2, y2, setPixel); /* plot remaining needle to end */
}

void plotQuadRationalBezier(int x0, int y0, int x1, int y1,
                            int x2, int y2, float w, std::function<void(int, int)> setPixel)
{ /* plot any quadratic rational Bezier curve */
   int x = x0 - 2 * x1 + x2, y = y0 - 2 * y1 + y2;
   double xx = x0 - x1, yy = y0 - y1, ww, t, q;

   assert(w >= 0.0);

   if (xx * (x2 - x1) > 0)
   {                          /* horizontal cut at P4? */
      if (yy * (y2 - y1) > 0) /* vertical cut at P6 too? */
         if (fabs(xx * y) > fabs(yy * x))
         { /* which first? */
            x0 = x2;
            x2 = xx + x1;
            y0 = y2;
            y2 = yy + y1; /* swap points */
         } /* now horizontal cut at P4 comes first */
      if (x0 == x2 || w == 1.0)
         t = (x0 - x1) / (double)x;
      else
      { /* non-rational or rational case */
         q = sqrt(4.0 * w * w * (x0 - x1) * (x2 - x1) + (x2 - x0) * (long)(x2 - x0));
         if (x1 < x0)
            q = -q;
         t = (2.0 * w * (x0 - x1) - x0 + x2 + q) / (2.0 * (1.0 - w) * (x2 - x0)); /* t at P4 */
      }
      q = 1.0 / (2.0 * t * (1.0 - t) * (w - 1.0) + 1.0);                          /* sub-divide at t */
      xx = (t * t * (x0 - 2.0 * w * x1 + x2) + 2.0 * t * (w * x1 - x0) + x0) * q; /* = P4 */
      yy = (t * t * (y0 - 2.0 * w * y1 + y2) + 2.0 * t * (w * y1 - y0) + y0) * q;
      ww = t * (w - 1.0) + 1.0;
      ww *= ww * q;                                /* squared weight P3 */
      w = ((1.0 - t) * (w - 1.0) + 1.0) * sqrt(q); /* weight P8 */
      x = floor(xx + 0.5);
      y = floor(yy + 0.5);                         /* P4 */
      yy = (xx - x0) * (y1 - y0) / (x1 - x0) + y0; /* intersect P3 | P0 P1 */
      plotQuadRationalBezierSeg(x0, y0, x, floor(yy + 0.5), x, y, ww, setPixel);
      yy = (xx - x2) * (y1 - y2) / (x1 - x2) + y2; /* intersect P4 | P1 P2 */
      y1 = floor(yy + 0.5);
      x0 = x1 = x;
      y0 = y; /* P0 = P4, P1 = P8 */
   }
   if ((y0 - y1) * (long)(y2 - y1) > 0)
   { /* vertical cut at P6? */
      if (y0 == y2 || w == 1.0)
         t = (y0 - y1) / (y0 - 2.0 * y1 + y2);
      else
      { /* non-rational or rational case */
         q = sqrt(4.0 * w * w * (y0 - y1) * (y2 - y1) + (y2 - y0) * (long)(y2 - y0));
         if (y1 < y0)
            q = -q;
         t = (2.0 * w * (y0 - y1) - y0 + y2 + q) / (2.0 * (1.0 - w) * (y2 - y0)); /* t at P6 */
      }
      q = 1.0 / (2.0 * t * (1.0 - t) * (w - 1.0) + 1.0);                          /* sub-divide at t */
      xx = (t * t * (x0 - 2.0 * w * x1 + x2) + 2.0 * t * (w * x1 - x0) + x0) * q; /* = P6 */
      yy = (t * t * (y0 - 2.0 * w * y1 + y2) + 2.0 * t * (w * y1 - y0) + y0) * q;
      ww = t * (w - 1.0) + 1.0;
      ww *= ww * q;                                /* squared weight P5 */
      w = ((1.0 - t) * (w - 1.0) + 1.0) * sqrt(q); /* weight P7 */
      x = floor(xx + 0.5);
      y = floor(yy + 0.5);                         /* P6 */
      xx = (x1 - x0) * (yy - y0) / (y1 - y0) + x0; /* intersect P6 | P0 P1 */
      plotQuadRationalBezierSeg(x0, y0, floor(xx + 0.5), y, x, y, ww, setPixel);
      xx = (x1 - x2) * (yy - y2) / (y1 - y2) + x2; /* intersect P7 | P1 P2 */
      x1 = floor(xx + 0.5);
      x0 = x;
      y0 = y1 = y; /* P0 = P6, P1 = P7 */
   }
   plotQuadRationalBezierSeg(x0, y0, x1, y1, x2, y2, w * w, setPixel); /* remaining */
}

void plotRotatedEllipse(int x, int y, int a, int b, float angle, std::function<void(int, int)> setPixel)
{ /* plot ellipse rotated by angle (radian) */
   float xd = (long)a * a, yd = (long)b * b;
   float s = sin(angle), zd = (xd - yd) * s;       /* ellipse rotation */
   xd = sqrt(xd - zd * s), yd = sqrt(yd + zd * s); /* surrounding rectangle */
   a = xd + 0.5;
   b = yd + 0.5;
   zd = zd * a * b / (xd * yd); /* scale to integer */
   plotRotatedEllipseRect(x - a, y - b, x + a, y + b, (long)(4 * zd * cos(angle)), setPixel);
}

void plotRotatedEllipseRect(int x0, int y0, int x1, int y1, long zd, std::function<void(int, int)> setPixel)
{ /* rectangle enclosing the ellipse, integer rotation angle */
   int xd = x1 - x0, yd = y1 - y0;
   float w = xd * (long)yd;
   if (zd == 0)
      return plotEllipseRect(x0, y0, x1, y1, setPixel); /* looks nicer */
   if (w != 0.0)
      w = (w - zd) / (w + w);    /* squared weight of P1 */
   assert(w <= 1.0 && w >= 0.0); /* limit angle to |zd|<=xd*yd */
   xd = floor(xd * w + 0.5);
   yd = floor(yd * w + 0.5); /* snap xe,ye to int */
   plotQuadRationalBezierSeg(x0, y0 + yd, x0, y0, x0 + xd, y0, 1.0 - w, setPixel);
   plotQuadRationalBezierSeg(x0, y0 + yd, x0, y1, x1 - xd, y1, w, setPixel);
   plotQuadRationalBezierSeg(x1, y1 - yd, x1, y1, x1 - xd, y1, 1.0 - w, setPixel);
   plotQuadRationalBezierSeg(x1, y1 - yd, x1, y0, x0 + xd, y0, w, setPixel);
}

void plotCubicBezierSeg(int x0, int y0, float x1, float y1,
                        float x2, float y2, int x3, int y3, std::function<void(int, int)> setPixel)
{ /* plot limited cubic Bezier segment */
   int f, fx, fy, leg = 1;
   int sx = x0 < x3 ? 1 : -1, sy = y0 < y3 ? 1 : -1; /* step direction */
   float xc = -fabs(x0 + x1 - x2 - x3), xa = xc - 4 * sx * (x1 - x2), xb = sx * (x0 - x1 - x2 + x3);
   float yc = -fabs(y0 + y1 - y2 - y3), ya = yc - 4 * sy * (y1 - y2), yb = sy * (y0 - y1 - y2 + y3);
   double ab, ac, bc, cb, xx, xy, yy, dx, dy, ex, *pxy, EP = 0.01;
   /* check for curve restrains */
   /* slope P0-P1 == P2-P3    and  (P0-P3 == P1-P2      or   no slope change) */
   assert((x1 - x0) * (x2 - x3) < EP && ((x3 - x0) * (x1 - x2) < EP || xb * xb < xa * xc + EP));
   assert((y1 - y0) * (y2 - y3) < EP && ((y3 - y0) * (y1 - y2) < EP || yb * yb < ya * yc + EP));

   if (xa == 0 && ya == 0)
   { /* quadratic Bezier */
      sx = floor((3 * x1 - x0 + 1) / 2);
      sy = floor((3 * y1 - y0 + 1) / 2); /* new midpoint */
      return plotQuadBezierSeg(x0, y0, sx, sy, x3, y3, setPixel);
   }
   x1 = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + 1; /* line lengths */
   x2 = (x2 - x3) * (x2 - x3) + (y2 - y3) * (y2 - y3) + 1;
   do
   { /* loop over both ends */
      ab = xa * yb - xb * ya;
      ac = xa * yc - xc * ya;
      bc = xb * yc - xc * yb;
      ex = ab * (ab + ac - 3 * bc) + ac * ac; /* P0 part of self-intersection loop? */
      f = ex > 0 ? 1 : sqrt(1 + 1024 / x1);   /* calculate resolution */
      ab *= f;
      ac *= f;
      bc *= f;
      ex *= f * f; /* increase resolution */
      xy = 9 * (ab + ac + bc) / 8;
      cb = 8 * (xa - ya); /* init differences of 1st degree */
      dx = 27 * (8 * ab * (yb * yb - ya * yc) + ex * (ya + 2 * yb + yc)) / 64 - ya * ya * (xy - ya);
      dy = 27 * (8 * ab * (xb * xb - xa * xc) - ex * (xa + 2 * xb + xc)) / 64 - xa * xa * (xy + xa);
      /* init differences of 2nd degree */
      xx = 3 * (3 * ab * (3 * yb * yb - ya * ya - 2 * ya * yc) - ya * (3 * ac * (ya + yb) + ya * cb)) / 4;
      yy = 3 * (3 * ab * (3 * xb * xb - xa * xa - 2 * xa * xc) - xa * (3 * ac * (xa + xb) + xa * cb)) / 4;
      xy = xa * ya * (6 * ab + 6 * ac - 3 * bc + cb);
      ac = ya * ya;
      cb = xa * xa;
      xy = 3 * (xy + 9 * f * (cb * yb * yc - xb * xc * ac) - 18 * xb * yb * ab) / 8;

      if (ex < 0)
      { /* negate values if inside self-intersection loop */
         dx = -dx;
         dy = -dy;
         xx = -xx;
         yy = -yy;
         xy = -xy;
         ac = -ac;
         cb = -cb;
      } /* init differences of 3rd degree */
      ab = 6 * ya * ac;
      ac = -6 * xa * ac;
      bc = 6 * ya * cb;
      cb = -6 * xa * cb;
      dx += xy;
      ex = dx + dy;
      dy += xy; /* error of 1st step */

      for (pxy = &xy, fx = fy = f; x0 != x3 && y0 != y3;)
      {
         setPixel(x0, y0); /* plot curve */
         do
         { /* move sub-steps of one pixel */
            if (dx > *pxy || dy < *pxy)
               goto exit;     /* confusing values */
            y1 = 2 * ex - dy; /* save value for test of y step */
            if (2 * ex >= dx)
            { /* x sub-step */
               fx--;
               ex += dx += xx;
               dy += xy += ac;
               yy += bc;
               xx += ab;
            }
            if (y1 <= 0)
            { /* y sub-step */
               fy--;
               ex += dy += yy;
               dx += xy += bc;
               xx += ac;
               yy += cb;
            }
         } while (fx > 0 && fy > 0); /* pixel complete? */
         if (2 * fx <= f)
         {
            x0 += sx;
            fx += f;
         } /* x step */
         if (2 * fy <= f)
         {
            y0 += sy;
            fy += f;
         } /* y step */
         if (pxy == &xy && dx < 0 && dy > 0)
            pxy = &EP; /* pixel ahead valid */
      }
   exit:
      xx = x0;
      x0 = x3;
      x3 = xx;
      sx = -sx;
      xb = -xb; /* swap legs */
      yy = y0;
      y0 = y3;
      y3 = yy;
      sy = -sy;
      yb = -yb;
      x1 = x2;
   } while (leg--); /* try other end */
   plotLine(x0, y0, x3, y3, setPixel); /* remaining part in case of cusp or crunode */
}

void plotCubicBezier(int x0, int y0, int x1, int y1,
                     int x2, int y2, int x3, int y3, std::function<void(int, int)> setPixel)
{ /* plot any cubic Bezier curve */
   int n = 0, i = 0;
   long xc = x0 + x1 - x2 - x3, xa = xc - 4 * (x1 - x2);
   long xb = x0 - x1 - x2 + x3, xd = xb + 4 * (x1 + x2);
   long yc = y0 + y1 - y2 - y3, ya = yc - 4 * (y1 - y2);
   long yb = y0 - y1 - y2 + y3, yd = yb + 4 * (y1 + y2);
   float fx0 = x0, fx1, fx2, fx3, fy0 = y0, fy1, fy2, fy3;
   double t1 = xb * xb - xa * xc, t2, t[5];
   /* sub-divide curve at gradient sign changes */
   if (xa == 0)
   { /* horizontal */
      if (abs(xc) < 2 * abs(xb))
         t[n++] = xc / (2.0 * xb); /* one change */
   }
   else if (t1 > 0.0)
   { /* two changes */
      t2 = sqrt(t1);
      t1 = (xb - t2) / xa;
      if (fabs(t1) < 1.0)
         t[n++] = t1;
      t1 = (xb + t2) / xa;
      if (fabs(t1) < 1.0)
         t[n++] = t1;
   }
   t1 = yb * yb - ya * yc;
   if (ya == 0)
   { /* vertical */
      if (abs(yc) < 2 * abs(yb))
         t[n++] = yc / (2.0 * yb); /* one change */
   }
   else if (t1 > 0.0)
   { /* two changes */
      t2 = sqrt(t1);
      t1 = (yb - t2) / ya;
      if (fabs(t1) < 1.0)
         t[n++] = t1;
      t1 = (yb + t2) / ya;
      if (fabs(t1) < 1.0)
         t[n++] = t1;
   }
   for (i = 1; i < n; i++) /* bubble sort of 4 points */
      if ((t1 = t[i - 1]) > t[i])
      {
         t[i - 1] = t[i];
         t[i] = t1;
         i = 0;
      }

   t1 = -1.0;
   t[n] = 1.0; /* begin / end point */
   for (i = 0; i <= n; i++)
   {             /* plot each segment separately */
      t2 = t[i]; /* sub-divide at t[i-1], t[i] */
      fx1 = (t1 * (t1 * xb - 2 * xc) - t2 * (t1 * (t1 * xa - 2 * xb) + xc) + xd) / 8 - fx0;
      fy1 = (t1 * (t1 * yb - 2 * yc) - t2 * (t1 * (t1 * ya - 2 * yb) + yc) + yd) / 8 - fy0;
      fx2 = (t2 * (t2 * xb - 2 * xc) - t1 * (t2 * (t2 * xa - 2 * xb) + xc) + xd) / 8 - fx0;
      fy2 = (t2 * (t2 * yb - 2 * yc) - t1 * (t2 * (t2 * ya - 2 * yb) + yc) + yd) / 8 - fy0;
      fx0 -= fx3 = (t2 * (t2 * (3 * xb - t2 * xa) - 3 * xc) + xd) / 8;
      fy0 -= fy3 = (t2 * (t2 * (3 * yb - t2 * ya) - 3 * yc) + yd) / 8;
      x3 = floor(fx3 + 0.5);
      y3 = floor(fy3 + 0.5); /* scale bounds to int */
      if (fx0 != 0.0)
      {
         fx1 *= fx0 = (x0 - x3) / fx0;
         fx2 *= fx0;
      }
      if (fy0 != 0.0)
      {
         fy1 *= fy0 = (y0 - y3) / fy0;
         fy2 *= fy0;
      }
      if (x0 != x3 || y0 != y3) /* segment t1 - t2 */
         plotCubicBezierSeg(x0, y0, x0 + fx1, y0 + fy1, x0 + fx2, y0 + fy2, x3, y3, setPixel);
      x0 = x3;
      y0 = y3;
      fx0 = fx3;
      fy0 = fy3;
      t1 = t2;
   }
}
#endif
void plotLineAA(int x0, int y0, int x1, int y1, std::function<void(int, int, long)> setPixelAA)
{ /* draw a black (0) anti-aliased line on white (255) background */
   int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, x2;
   long dx = abs(x1 - x0), dy = abs(y1 - y0), err = dx * dx + dy * dy;
   long e2 = err == 0 ? 1 : (long)(0xffff7fl / sqrt((double)err)); /* multiplication factor */

   dx *= e2;
   dy *= e2;
   err = dx - dy; /* error value e_xy */
   for (;;)
   { /* pixel loop */
      setPixelAA(x0, y0, abs(err - dx + dy) >> 16);
      e2 = err;
      x2 = x0;
      if (2 * e2 >= -dx)
      { /* x step */
         if (x0 == x1)
            break;
         if (e2 + dy < 0xff0000l)
            setPixelAA(x0, y0 + sy, (e2 + dy) >> 16);
         err -= dy;
         x0 += sx;
      }
      if (2 * e2 <= dy)
      { /* y step */
         if (y0 == y1)
            break;
         if (dx - e2 < 0xff0000l)
            setPixelAA(x2 + sx, y0, (dx - e2) >> 16);
         err += dx;
         y0 += sy;
      }
   }
}

void plotCircleAA(int xm, int ym, int r, std::function<void(int, int, long)> setPixelAA)
{                                  /* draw a black anti-aliased circle on white background */
   int x = -r, y = 0;              /* II. quadrant from bottom left to top right */
   int i, x2, e2, err = 2 - 2 * r; /* error of 1.step */
   r = 1 - err;
   do
   {
      i = 255 * abs(err - 2 * (x + y) - 2) / r; /* get blend value of pixel */
      setPixelAA(xm - x, ym + y, i);            /*   I. Quadrant */
      setPixelAA(xm - y, ym - x, i);            /*  II. Quadrant */
      setPixelAA(xm + x, ym - y, i);            /* III. Quadrant */
      setPixelAA(xm + y, ym + x, i);            /*  IV. Quadrant */
      e2 = err;
      x2 = x; /* remember values */
      if (err + y > 0)
      {                                   /* x step */
         i = 255 * (err - 2 * x - 1) / r; /* outward pixel */
         if (i < 256)
         {
            setPixelAA(xm - x, ym + y + 1, i);
            setPixelAA(xm - y - 1, ym - x, i);
            setPixelAA(xm + x, ym - y - 1, i);
            setPixelAA(xm + y + 1, ym + x, i);
         }
         err += ++x * 2 + 1;
      }
      if (e2 + x2 <= 0)
      {                                  /* y step */
         i = 255 * (2 * y + 3 - e2) / r; /* inward pixel */
         if (i < 256)
         {
            setPixelAA(xm - x2 - 1, ym + y, i);
            setPixelAA(xm - y, ym - x2 - 1, i);
            setPixelAA(xm + x2 + 1, ym - y, i);
            setPixelAA(xm + y, ym + x2 + 1, i);
         }
         err += ++y * 2 + 1;
      }
   } while (x < 0);
}
#ifdef FIX_CONVERT_TO_FLOAT

void plotEllipseRectAA(int x0, int y0, int x1, int y1, std::function<void(int, int, long)> setPixelAA)
{                                                               /* draw a black anti-aliased rectangular ellipse on white background */
   long a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;         /* diameter */
   float dx = 4 * (a - 1.0) * b * b, dy = 4 * (b1 + 1) * a * a; /* error increment */
   float ed, i, err = b1 * a * a - dx + dy;                     /* error of 1.step */
   bool f;

   if (a == 0 || b == 0)
      return plotLineAA(x0, y0, x1, y1, setPixelAA);
   if (x0 > x1)
   {
      x0 = x1;
      x1 += a;
   } /* if called with swapped points */
   if (y0 > y1)
      y0 = y1; /* .. exchange them */
   y0 += (b + 1) / 2;
   y1 = y0 - b1; /* starting pixel */
   a = 8 * a * a;
   b1 = 8 * b * b;

   for (;;)
   { /* approximate ed=sqrt(dx*dx+dy*dy) */
      i = std::min(dx, dy);
      ed = std::max(dx, dy);
      if (y0 == y1 + 1 && err > dy && a > b1)
         ed = 255 * 4. / a; /* x-tip */
      else
         ed = 255 / (ed + 2 * ed * i * i / (4 * ed * ed + i * i)); /* approximation */
      i = ed * fabs(err + dx - dy);                                /* get intensity value by pixel error */
      setPixelAA(x0, y0, i);
      setPixelAA(x0, y1, i);
      setPixelAA(x1, y0, i);
      setPixelAA(x1, y1, i);

      if (f = 2 * err + dy >= 0)
      { /* x step, remember condition */
         if (x0 >= x1)
            break;
         i = ed * (err + dx);
         if (i < 255)
         {
            setPixelAA(x0, y0 + 1, i);
            setPixelAA(x0, y1 - 1, i);
            setPixelAA(x1, y0 + 1, i);
            setPixelAA(x1, y1 - 1, i);
         } /* do error increment later since values are still needed */
      }
      if (2 * err <= dx)
      { /* y step */
         i = ed * (dy - err);
         if (i < 255)
         {
            setPixelAA(x0 + 1, y0, i);
            setPixelAA(x1 - 1, y0, i);
            setPixelAA(x0 + 1, y1, i);
            setPixelAA(x1 - 1, y1, i);
         }
         y0++;
         y1--;
         err += dy += a;
      }
      if (f)
      {
         x0++;
         x1--;
         err -= dx -= b1;
      } /* x error increment */
   }
   if (--x0 == x1++) /* too early stop of flat ellipses */
      while (y0 - y1 < b)
      {
         i = 255 * 4 * fabs(err + dx) / b1; /* -> finish tip of ellipse */
         setPixelAA(x0, ++y0, i);
         setPixelAA(x1, y0, i);
         setPixelAA(x0, --y1, i);
         setPixelAA(x1, y1, i);
         err += dy += a;
      }
}

void plotQuadBezierSegAA(int x0, int y0, int x1, int y1, int x2, int y2, std::function<void(int, int, long)> setPixelAA)
{ /* draw an limited anti-aliased quadratic Bezier segment */
   int sx = x2 - x1, sy = y2 - y1;
   long xx = x0 - x1, yy = y0 - y1, xy;             /* relative values for checks */
   double dx, dy, err, ed, cur = xx * sy - yy * sx; /* curvature */

   assert(xx * sx <= 0 && yy * sy <= 0); /* sign of gradient must not change */

   if (sx * (long)sx + sy * (long)sy > xx * xx + yy * yy)
   { /* begin with longer part */
      x2 = x0;
      x0 = sx + x1;
      y2 = y0;
      y0 = sy + y1;
      cur = -cur; /* swap P0 P2 */
   }
   if (cur != 0)
   { /* no straight line */
      xx += sx;
      xx *= sx = x0 < x2 ? 1 : -1; /* x step direction */
      yy += sy;
      yy *= sy = y0 < y2 ? 1 : -1; /* y step direction */
      xy = 2 * xx * yy;
      xx *= xx;
      yy *= yy; /* differences 2nd degree */
      if (cur * sx * sy < 0)
      { /* negated curvature? */
         xx = -xx;
         yy = -yy;
         xy = -xy;
         cur = -cur;
      }
      dx = 4.0 * sy * (x1 - x0) * cur + xx - xy; /* differences 1st degree */
      dy = 4.0 * sx * (y0 - y1) * cur + yy - xy;
      xx += xx;
      yy += yy;
      err = dx + dy + xy; /* error 1st step */
      do
      {
         cur = fmin(dx + xy, -xy - dy);
         ed = fmax(dx + xy, -xy - dy); /* approximate error distance */
         ed += 2 * ed * cur * cur / (4 * ed * ed + cur * cur);
         setPixelAA(x0, y0, 255 * fabs(err - dx - dy - xy) / ed); /* plot curve */
         if (x0 == x2 || y0 == y2)
            break; /* last pixel -> curve finished */
         x1 = x0;
         cur = dx - err;
         y1 = 2 * err + dy < 0;
         if (2 * err + dx > 0)
         { /* x step */
            if (err - dy < ed)
               setPixelAA(x0, y0 + sy, 255 * fabs(err - dy) / ed);
            x0 += sx;
            dx -= xy;
            err += dy += yy;
         }
         if (y1)
         { /* y step */
            if (cur < ed)
               setPixelAA(x1 + sx, y0, 255 * fabs(cur) / ed);
            y0 += sy;
            dy -= xy;
            err += dx += xx;
         }
      } while (dy < dx); /* gradient negates -> close curves */
   }
   plotLineAA(x0, y0, x2, y2, setPixelAA); /* plot remaining needle to end */
}

void plotQuadRationalBezierSegAA(int x0, int y0, int x1, int y1,
                                 int x2, int y2, float w, std::function<void(int, int, long)> setPixelAA)
{                                  /* draw an anti-aliased rational quadratic Bezier segment, squared weight */
   int sx = x2 - x1, sy = y2 - y1; /* relative values for checks */
   double dx = x0 - x2, dy = y0 - y2, xx = x0 - x1, yy = y0 - y1;
   double xy = xx * sy + yy * sx, cur = xx * sy - yy * sx, err, ed; /* curvature */
   bool f;

   assert(xx * sx <= 0.0 && yy * sy <= 0.0); /* sign of gradient must not change */

   if (cur != 0.0 && w > 0.0)
   { /* no straight line */
      if (sx * (long)sx + sy * (long)sy > xx * xx + yy * yy)
      { /* begin with longer part */
         x2 = x0;
         x0 -= dx;
         y2 = y0;
         y0 -= dy;
         cur = -cur; /* swap P0 P2 */
      }
      xx = 2.0 * (4.0 * w * sx * xx + dx * dx); /* differences 2nd degree */
      yy = 2.0 * (4.0 * w * sy * yy + dy * dy);
      sx = x0 < x2 ? 1 : -1; /* x step direction */
      sy = y0 < y2 ? 1 : -1; /* y step direction */
      xy = -2.0 * sx * sy * (2.0 * w * xy + dx * dy);

      if (cur * sx * sy < 0)
      { /* negated curvature? */
         xx = -xx;
         yy = -yy;
         cur = -cur;
         xy = -xy;
      }
      dx = 4.0 * w * (x1 - x0) * sy * cur + xx / 2.0 + xy; /* differences 1st degree */
      dy = 4.0 * w * (y0 - y1) * sx * cur + yy / 2.0 + xy;

      if (w < 0.5 && dy > dx)
      { /* flat ellipse, algorithm fails */
         cur = (w + 1.0) / 2.0;
         w = sqrt(w);
         xy = 1.0 / (w + 1.0);
         sx = floor((x0 + 2.0 * w * x1 + x2) * xy / 2.0 + 0.5); /* subdivide curve in half  */
         sy = floor((y0 + 2.0 * w * y1 + y2) * xy / 2.0 + 0.5);
         dx = floor((w * x1 + x0) * xy + 0.5);
         dy = floor((y1 * w + y0) * xy + 0.5);
         plotQuadRationalBezierSegAA(x0, y0, dx, dy, sx, sy, cur, setPixelAA); /* plot apart */
         dx = floor((w * x1 + x2) * xy + 0.5);
         dy = floor((y1 * w + y2) * xy + 0.5);
         return plotQuadRationalBezierSegAA(sx, sy, dx, dy, x2, y2, cur, setPixelAA);
      }
      err = dx + dy - xy; /* error 1st step */
      do
      { /* pixel loop */
         cur = fmin(dx - xy, xy - dy);
         ed = fmax(dx - xy, xy - dy);
         ed += 2 * ed * cur * cur / (4. * ed * ed + cur * cur); /* approximate error distance */
         x1 = 255 * fabs(err - dx - dy + xy) / ed;              /* get blend value by pixel error */
         if (x1 < 256)
            setPixelAA(x0, y0, x1); /* plot curve */
         if (f = 2 * err + dy < 0)
         { /* y step */
            if (y0 == y2)
               return; /* last pixel -> curve finished */
            if (dx - err < ed)
               setPixelAA(x0 + sx, y0, 255 * fabs(dx - err) / ed);
         }
         if (2 * err + dx > 0)
         { /* x step */
            if (x0 == x2)
               return; /* last pixel -> curve finished */
            if (err - dy < ed)
               setPixelAA(x0, y0 + sy, 255 * fabs(err - dy) / ed);
            x0 += sx;
            dx += xy;
            err += dy += yy;
         }
         if (f)
         {
            y0 += sy;
            dy += xy;
            err += dx += xx;
         } /* y step */
      } while (dy < dx); /* gradient negates -> algorithm fails */
   }
   plotLineAA(x0, y0, x2, y2, setPixelAA); /* plot remaining needle to end */
}

void plotCubicBezierSegAA(int x0, int y0, float x1, float y1,
                          float x2, float y2, int x3, int y3,
                          std::function<void(int, int, long)> setPixelAA)
{ /* plot limited anti-aliased cubic Bezier segment */
   int f, fx, fy, leg = 1;
   int sx = x0 < x3 ? 1 : -1, sy = y0 < y3 ? 1 : -1; /* step direction */
   float xc = -fabs(x0 + x1 - x2 - x3), xa = xc - 4 * sx * (x1 - x2), xb = sx * (x0 - x1 - x2 + x3);
   float yc = -fabs(y0 + y1 - y2 - y3), ya = yc - 4 * sy * (y1 - y2), yb = sy * (y0 - y1 - y2 + y3);
   double ab, ac, bc, ba, xx, xy, yy, dx, dy, ex, px, py, ed, ip, EP = 0.01;

   /* check for curve restrains */
   /* slope P0-P1 == P2-P3     and  (P0-P3 == P1-P2      or  no slope change) */
   assert((x1 - x0) * (x2 - x3) < EP && ((x3 - x0) * (x1 - x2) < EP || xb * xb < xa * xc + EP));
   assert((y1 - y0) * (y2 - y3) < EP && ((y3 - y0) * (y1 - y2) < EP || yb * yb < ya * yc + EP));

   if (xa == 0 && ya == 0)
   { /* quadratic Bezier */
      sx = floor((3 * x1 - x0 + 1) / 2);
      sy = floor((3 * y1 - y0 + 1) / 2); /* new midpoint */
      return plotQuadBezierSegAA(x0, y0, sx, sy, x3, y3, setPixelAA);
   }
   x1 = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + 1; /* line lengths */
   x2 = (x2 - x3) * (x2 - x3) + (y2 - y3) * (y2 - y3) + 1;
   do
   { /* loop over both ends */
      ab = xa * yb - xb * ya;
      ac = xa * yc - xc * ya;
      bc = xb * yc - xc * yb;
      ip = 4 * ab * bc - ac * ac;             /* self intersection loop at all? */
      ex = ab * (ab + ac - 3 * bc) + ac * ac; /* P0 part of self-intersection loop? */
      f = ex > 0 ? 1 : sqrt(1 + 1024 / x1);   /* calculate resolution */
      ab *= f;
      ac *= f;
      bc *= f;
      ex *= f * f; /* increase resolution */
      xy = 9 * (ab + ac + bc) / 8;
      ba = 8 * (xa - ya); /* init differences of 1st degree */
      dx = 27 * (8 * ab * (yb * yb - ya * yc) + ex * (ya + 2 * yb + yc)) / 64 - ya * ya * (xy - ya);
      dy = 27 * (8 * ab * (xb * xb - xa * xc) - ex * (xa + 2 * xb + xc)) / 64 - xa * xa * (xy + xa);
      /* init differences of 2nd degree */
      xx = 3 * (3 * ab * (3 * yb * yb - ya * ya - 2 * ya * yc) - ya * (3 * ac * (ya + yb) + ya * ba)) / 4;
      yy = 3 * (3 * ab * (3 * xb * xb - xa * xa - 2 * xa * xc) - xa * (3 * ac * (xa + xb) + xa * ba)) / 4;
      xy = xa * ya * (6 * ab + 6 * ac - 3 * bc + ba);
      ac = ya * ya;
      ba = xa * xa;
      xy = 3 * (xy + 9 * f * (ba * yb * yc - xb * xc * ac) - 18 * xb * yb * ab) / 8;

      if (ex < 0)
      { /* negate values if inside self-intersection loop */
         dx = -dx;
         dy = -dy;
         xx = -xx;
         yy = -yy;
         xy = -xy;
         ac = -ac;
         ba = -ba;
      } /* init differences of 3rd degree */
      ab = 6 * ya * ac;
      ac = -6 * xa * ac;
      bc = 6 * ya * ba;
      ba = -6 * xa * ba;
      dx += xy;
      ex = dx + dy;
      dy += xy; /* error of 1st step */

      for (fx = fy = f; x0 != x3 && y0 != y3;)
      {
         y1 = fmin(fabs(xy - dx), fabs(dy - xy));
         ed = fmax(fabs(xy - dx), fabs(dy - xy)); /* approximate error distance */
         ed = f * (ed + 2 * ed * y1 * y1 / (4 * ed * ed + y1 * y1));
         y1 = 255 * fabs(ex - (f - fx + 1) * dx - (f - fy + 1) * dy + f * xy) / ed;
         if (y1 < 256)
            setPixelAA(x0, y0, y1);                         /* plot curve */
         px = fabs(ex - (f - fx + 1) * dx + (fy - 1) * dy); /* pixel intensity x move */
         py = fabs(ex + (fx - 1) * dx - (f - fy + 1) * dy); /* pixel intensity y move */
         y2 = y0;
         do
         { /* move sub-steps of one pixel */
            if (dx + xx > xy || dy + yy < xy)
               goto exit;     /* two x or y steps */
            y1 = 2 * ex + dx; /* save value for test of y step */
            if (2 * ex + dy > 0)
            { /* x sub-step */
               fx--;
               ex += dx += xx;
               dy += xy += ac;
               yy += bc;
               xx += ab;
            }
            else if (y1 > 0)
               goto exit; /* tiny nearly cusp */
            if (y1 <= 0)
            { /* y sub-step */
               fy--;
               ex += dy += yy;
               dx += xy += bc;
               xx += ac;
               yy += ba;
            }
         } while (fx > 0 && fy > 0); /* pixel complete? */
         if (2 * fy <= f)
         { /* x+ anti-aliasing pixel */
            if (py < ed)
               setPixelAA(x0 + sx, y0, 255 * py / ed); /* plot curve */
            y0 += sy;
            fy += f; /* y step */
         }
         if (2 * fx <= f)
         { /* y+ anti-aliasing pixel */
            if (px < ed)
               setPixelAA(x0, y2 + sy, 255 * px / ed); /* plot curve */
            x0 += sx;
            fx += f; /* x step */
         }
      }
      break; /* finish curve by line */
   exit:
      if (2 * ex < dy && 2 * fy <= f + 2)
      { /* round x+ approximation pixel */
         if (py < ed)
            setPixelAA(x0 + sx, y0, 255 * py / ed); /* plot curve */
         y0 += sy;
      }
      if (2 * ex > dx && 2 * fx <= f + 2)
      { /* round y+ approximation pixel */
         if (px < ed)
            setPixelAA(x0, y2 + sy, 255 * px / ed); /* plot curve */
         x0 += sx;
      }
      xx = x0;
      x0 = x3;
      x3 = xx;
      sx = -sx;
      xb = -xb; /* swap legs */
      yy = y0;
      y0 = y3;
      y3 = yy;
      sy = -sy;
      yb = -yb;
      x1 = x2;
   } while (leg--); /* try other end */
   plotLineAA(x0, y0, x3, y3, setPixelAA); /* remaining part in case of cusp or crunode */
}
#endif

void plotLineWidth(int x0, int y0, int x1, int y1, float wd, std::function<void(int, int, long)> setPixelAA)
{ /* plot an anti-aliased line of width wd */
   int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
   int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
   int err = dx - dy, e2, x2, y2; /* error value e_xy */
   float ed = dx + dy == 0 ? 1.0f : (float)sqrt((double)(dx * dx) + (double)(dy * dy));

   for (wd = (wd + 1) / 2;;)
   { /* pixel loop */
      setPixelAA(x0, y0, std::max(0, (int)(255.0f * ((float)abs(err - dx + dy) / ed - wd + 1))));
      e2 = err;
      x2 = x0;
      if (2 * e2 >= -dx)
      { /* x step */
         for (e2 += dy, y2 = y0; e2 < (int)(ed * wd) && (y1 != y2 || dx > dy); e2 += dx)
            setPixelAA(x0, y2 += sy, std::max(0, (int)(255.0f * ((float)abs(e2) / ed - wd + 1))));
         if (x0 == x1)
            break;
         e2 = err;
         err -= dy;
         x0 += sx;
      }
      if (2 * e2 <= dy)
      { /* y step */
         for (e2 = dx - e2; e2 < (int)(ed * wd) && (x1 != x2 || dx < dy); e2 += dy)
            setPixelAA(x2 += sx, y0, std::max(0, (int)(255.0f * ((float)abs(e2) / ed - wd + 1))));
         if (y0 == y1)
            break;
         err += dx;
         y0 += sy;
      }
   }
}

#ifdef FIX_CONVERT_TO_FLOAT

void plotQuadSpline(int n, int x[], int y[], std::function<void(int, int)> setPixel)
{ /* plot quadratic spline, destroys input arrays x,y */
#define M_MAX 6
   float mi = 1, m[M_MAX]; /* diagonal constants of matrix */
   int i, x0, y0, x1, y1, x2 = x[n], y2 = y[n];

   assert(n > 1); /* need at least 3 points P[0]..P[n] */

   x[1] = x0 = 8 * x[1] - 2 * x[0]; /* first row of matrix */
   y[1] = y0 = 8 * y[1] - 2 * y[0];

   for (i = 2; i < n; i++)
   { /* forward sweep */
      if (i - 2 < M_MAX)
         m[i - 2] = mi = 1.0 / (6.0 - mi);
      x[i] = x0 = floor(8 * x[i] - x0 * mi + 0.5); /* store yi */
      y[i] = y0 = floor(8 * y[i] - y0 * mi + 0.5);
   }
   x1 = floor((x0 - 2 * x2) / (5.0 - mi) + 0.5); /* correction last row */
   y1 = floor((y0 - 2 * y2) / (5.0 - mi) + 0.5);

   for (i = n - 2; i > 0; i--)
   { /* back substitution */
      if (i <= M_MAX)
         mi = m[i - 1];
      x0 = floor((x[i] - x1) * mi + 0.5); /* next corner */
      y0 = floor((y[i] - y1) * mi + 0.5);
      plotQuadBezier((x0 + x1) / 2, (y0 + y1) / 2, x1, y1, x2, y2, setPixel);
      x2 = (x0 + x1) / 2;
      x1 = x0;
      y2 = (y0 + y1) / 2;
      y1 = y0;
   }
   plotQuadBezier(x[0], y[0], x1, y1, x2, y2, setPixel);
}

void plotCubicSpline(int n, int x[], int y[], std::function<void(int, int)> setPixel)
{ /* plot cubic spline, destroys input arrays x,y */
#define M_MAX 6
   float mi = 0.25, m[M_MAX]; /* diagonal constants of matrix */
   int x3 = x[n - 1], y3 = y[n - 1], x4 = x[n], y4 = y[n];
   int i, x0, y0, x1, y1, x2, y2;

   assert(n > 2); /* need at least 4 points P[0]..P[n] */

   x[1] = x0 = 12 * x[1] - 3 * x[0]; /* first row of matrix */
   y[1] = y0 = 12 * y[1] - 3 * y[0];

   for (i = 2; i < n; i++)
   { /* foreward sweep */
      if (i - 2 < M_MAX)
         m[i - 2] = mi = 0.25 / (2.0 - mi);
      x[i] = x0 = floor(12 * x[i] - 2 * x0 * mi + 0.5);
      y[i] = y0 = floor(12 * y[i] - 2 * y0 * mi + 0.5);
   }
   x2 = floor((x0 - 3 * x4) / (7 - 4 * mi) + 0.5); /* correct last row */
   y2 = floor((y0 - 3 * y4) / (7 - 4 * mi) + 0.5);
   plotCubicBezier(x3, y3, (x2 + x4) / 2, (y2 + y4) / 2, x4, y4, x4, y4, setPixel);

   if (n - 3 < M_MAX)
      mi = m[n - 3];
   x1 = floor((x[n - 2] - 2 * x2) * mi + 0.5);
   y1 = floor((y[n - 2] - 2 * y2) * mi + 0.5);
   for (i = n - 3; i > 0; i--)
   { /* back substitution */
      if (i <= M_MAX)
         mi = m[i - 1];
      x0 = floor((x[i] - 2 * x1) * mi + 0.5);
      y0 = floor((y[i] - 2 * y1) * mi + 0.5);
      x4 = floor((x0 + 4 * x1 + x2 + 3) / 6.0); /* reconstruct P[i] */
      y4 = floor((y0 + 4 * y1 + y2 + 3) / 6.0);
      plotCubicBezier(x4, y4,
                      floor((2 * x1 + x2) / 3 + 0.5), floor((2 * y1 + y2) / 3 + 0.5),
                      floor((x1 + 2 * x2) / 3 + 0.5), floor((y1 + 2 * y2) / 3 + 0.5),
                      x3, y3, setPixel);
      x3 = x4;
      y3 = y4;
      x2 = x1;
      y2 = y1;
      x1 = x0;
      y1 = y0;
   }
   x0 = x[0];
   x4 = floor((3 * x0 + 7 * x1 + 2 * x2 + 6) / 12.0); /* reconstruct P[1] */
   y0 = y[0];
   y4 = floor((3 * y0 + 7 * y1 + 2 * y2 + 6) / 12.0);
   plotCubicBezier(x4, y4, floor((2 * x1 + x2) / 3 + 0.5), floor((2 * y1 + y2) / 3 + 0.5),
                   floor((x1 + 2 * x2) / 3 + 0.5), floor((y1 + 2 * y2) / 3 + 0.5), x3, y3, setPixel);
   plotCubicBezier(x0, y0, x0, y0, (x0 + x1) / 2, (y0 + y1) / 2, x4, y4, setPixel);
}
#endif
}