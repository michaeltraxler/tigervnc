/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright (C) 2009 Pierre Ossman for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdio.h>
#include "XserverDesktop.h"
#include "vncHooks.h"
#include "vncExtInit.h"
#include "xorg-version.h"

extern "C" {
#define class c_class
#define private c_private
#define public c_public
#include "scrnintstr.h"
#include "windowstr.h"
#include "gcstruct.h"
#include "regionstr.h"
#include "dixfontstr.h"
#include "colormapst.h"
#ifdef RENDER
#include "picturestr.h"
#endif
#ifdef RANDR
#include "randrstr.h"
#endif

#undef class
#undef private
#undef public
}

#include "RegionHelper.h"

#define DBGPRINT(x) //(fprintf x)

// MAX_RECTS_PER_OP is the maximum number of rectangles we generate from
// operations like Polylines and PolySegment.  If the operation is more complex
// than this, we simply use the bounding box.  Ideally it would be a
// command-line option, but that would involve an extra malloc each time, so we
// fix it here.
#define MAX_RECTS_PER_OP 5

// vncHooksScreenRec and vncHooksGCRec contain pointers to the original
// functions which we "wrap" in order to hook the screen changes.  The screen
// functions are each wrapped individually, while the GC "funcs" and "ops" are
// wrapped as a unit.

typedef struct {
  XserverDesktop* desktop;

  CloseScreenProcPtr           CloseScreen;
  CreateGCProcPtr              CreateGC;
  CopyWindowProcPtr            CopyWindow;
  ClearToBackgroundProcPtr     ClearToBackground;
#if XORG < 110
  RestoreAreasProcPtr          RestoreAreas;
#endif
  InstallColormapProcPtr       InstallColormap;
  StoreColorsProcPtr           StoreColors;
  DisplayCursorProcPtr         DisplayCursor;
  ScreenBlockHandlerProcPtr    BlockHandler;
#ifdef RENDER
  CompositeProcPtr             Composite;
#endif
#ifdef RANDR
  RRSetConfigProcPtr           RandRSetConfig;
#endif
} vncHooksScreenRec, *vncHooksScreenPtr;

typedef struct {
    GCFuncs *wrappedFuncs;
    GCOps *wrappedOps;
} vncHooksGCRec, *vncHooksGCPtr;

#if XORG == 15
static DevPrivateKey vncHooksScreenPrivateKey = &vncHooksScreenPrivateKey;
static DevPrivateKey vncHooksGCPrivateKey = &vncHooksGCPrivateKey;
#elif XORG < 19
static int vncHooksScreenPrivateKeyIndex;
static int vncHooksGCPrivateKeyIndex;
static DevPrivateKey vncHooksScreenPrivateKey = &vncHooksScreenPrivateKeyIndex;
static DevPrivateKey vncHooksGCPrivateKey = &vncHooksGCPrivateKeyIndex;
#else
static DevPrivateKeyRec vncHooksScreenKeyRec;
static DevPrivateKeyRec vncHooksGCKeyRec;
#define vncHooksScreenPrivateKey (&vncHooksScreenKeyRec)
#define vncHooksGCPrivateKey (&vncHooksGCKeyRec)
#endif

#define vncHooksScreenPrivate(pScreen) \
        (vncHooksScreenPtr) dixLookupPrivate(&(pScreen)->devPrivates, \
                                             vncHooksScreenPrivateKey)
#define vncHooksGCPrivate(pGC) \
        (vncHooksGCPtr) dixLookupPrivate(&(pGC)->devPrivates, \
                                         vncHooksGCPrivateKey)

// screen functions

static Bool vncHooksCloseScreen(int i, ScreenPtr pScreen);
static Bool vncHooksCreateGC(GCPtr pGC);
static void vncHooksCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg,
                               RegionPtr pOldRegion);
static void vncHooksClearToBackground(WindowPtr pWin, int x, int y, int w,
                                      int h, Bool generateExposures);
#if XORG < 110
static RegionPtr vncHooksRestoreAreas(WindowPtr pWin, RegionPtr prgnExposed);
#endif
static void vncHooksInstallColormap(ColormapPtr pColormap);
static void vncHooksStoreColors(ColormapPtr pColormap, int ndef,
                                xColorItem* pdef);
static Bool vncHooksDisplayCursor(
#if XORG >= 16
				  DeviceIntPtr pDev,
#endif
				  ScreenPtr pScreen, CursorPtr cursor);
static void vncHooksBlockHandler(int i, pointer blockData, pointer pTimeout,
                                 pointer pReadmask);
#ifdef RENDER
static void vncHooksComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, 
			      PicturePtr pDst, INT16 xSrc, INT16 ySrc, INT16 xMask, 
			      INT16 yMask, INT16 xDst, INT16 yDst, CARD16 width, CARD16 height);
#endif
#ifdef RANDR
static Bool vncHooksRandRSetConfig(ScreenPtr pScreen, Rotation rotation,
                                   int rate, RRScreenSizePtr pSize);
#endif

// GC "funcs"

static void vncHooksValidateGC(GCPtr pGC, unsigned long changes,
                               DrawablePtr pDrawable);
static void vncHooksChangeGC(GCPtr pGC, unsigned long mask);
static void vncHooksCopyGC(GCPtr src, unsigned long mask, GCPtr dst);
static void vncHooksDestroyGC(GCPtr pGC);
static void vncHooksChangeClip(GCPtr pGC, int type, pointer pValue,int nrects);
static void vncHooksDestroyClip(GCPtr pGC);
static void vncHooksCopyClip(GCPtr dst, GCPtr src);

static GCFuncs vncHooksGCFuncs = {
  vncHooksValidateGC, vncHooksChangeGC, vncHooksCopyGC, vncHooksDestroyGC,
  vncHooksChangeClip, vncHooksDestroyClip, vncHooksCopyClip,
};

// GC "ops"

static void vncHooksFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nInit,
                              DDXPointPtr pptInit, int *pwidthInit,
                              int fSorted);
static void vncHooksSetSpans(DrawablePtr pDrawable, GCPtr pGC, char *psrc,
                             DDXPointPtr ppt, int *pwidth, int nspans,
                             int fSorted);
static void vncHooksPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth,
                             int x, int y, int w, int h, int leftPad,
                             int format, char *pBits);
static RegionPtr vncHooksCopyArea(DrawablePtr pSrc, DrawablePtr pDst,
                                  GCPtr pGC, int srcx, int srcy, int w, int h,
                                  int dstx, int dsty);
static RegionPtr vncHooksCopyPlane(DrawablePtr pSrc, DrawablePtr pDst,
                                   GCPtr pGC, int srcx, int srcy, int w, int h,
                                   int dstx, int dsty, unsigned long plane);
static void vncHooksPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
                              int npt, xPoint *pts);
static void vncHooksPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode,
                              int npt, DDXPointPtr ppts);
static void vncHooksPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg,
                                xSegment *segs);
static void vncHooksPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                                  xRectangle *rects);
static void vncHooksPolyArc(DrawablePtr pDrawable, GCPtr pGC, int narcs,
                            xArc *arcs);
static void vncHooksFillPolygon(DrawablePtr pDrawable, GCPtr pGC, int shape,
                                int mode, int count, DDXPointPtr pts);
static void vncHooksPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                                 xRectangle *rects);
static void vncHooksPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int narcs,
                                xArc *arcs);
static int vncHooksPolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                             int count, char *chars);
static int vncHooksPolyText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                              int count, unsigned short *chars);
static void vncHooksImageText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                               int count, char *chars);
static void vncHooksImageText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                                int count, unsigned short *chars);
static void vncHooksImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x,
                                  int y, unsigned int nglyph,
                                  CharInfoPtr *ppci, pointer pglyphBase);
static void vncHooksPolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x,
                                 int y, unsigned int nglyph,
                                 CharInfoPtr *ppci, pointer pglyphBase);
static void vncHooksPushPixels(GCPtr pGC, PixmapPtr pBitMap,
                               DrawablePtr pDrawable, int w, int h, int x,
                               int y);

static GCOps vncHooksGCOps = {
  vncHooksFillSpans, vncHooksSetSpans, vncHooksPutImage, vncHooksCopyArea,
  vncHooksCopyPlane, vncHooksPolyPoint, vncHooksPolylines, vncHooksPolySegment,
  vncHooksPolyRectangle, vncHooksPolyArc, vncHooksFillPolygon,
  vncHooksPolyFillRect, vncHooksPolyFillArc, vncHooksPolyText8,
  vncHooksPolyText16, vncHooksImageText8, vncHooksImageText16,
  vncHooksImageGlyphBlt, vncHooksPolyGlyphBlt, vncHooksPushPixels
};



/////////////////////////////////////////////////////////////////////////////
// vncHooksInit() is called at initialisation time and every time the server
// resets.  It is called once for each screen, but the indexes are only
// allocated once for each server generation.

Bool vncHooksInit(ScreenPtr pScreen, XserverDesktop* desktop)
{
  vncHooksScreenPtr vncHooksScreen;

#if XORG < 19
  if (!dixRequestPrivate(vncHooksScreenPrivateKey, sizeof(vncHooksScreenRec))) {
    ErrorF("vncHooksInit: Allocation of vncHooksScreen failed\n");
    return FALSE;
  }
  if (!dixRequestPrivate(vncHooksGCPrivateKey, sizeof(vncHooksGCRec))) {
    ErrorF("vncHooksInit: Allocation of vncHooksGCRec failed\n");
    return FALSE;
  }

#else
  if (!dixRegisterPrivateKey(&vncHooksScreenKeyRec, PRIVATE_SCREEN,
      sizeof(vncHooksScreenRec))) {
    ErrorF("vncHooksInit: Allocation of vncHooksScreen failed\n");
    return FALSE;
  }
  if (!dixRegisterPrivateKey(&vncHooksGCKeyRec, PRIVATE_GC,
      sizeof(vncHooksGCRec))) {
    ErrorF("vncHooksInit: Allocation of vncHooksGCRec failed\n");
    return FALSE;
  }

#endif

  vncHooksScreen = vncHooksScreenPrivate(pScreen);

  vncHooksScreen->desktop = desktop;

  vncHooksScreen->CloseScreen = pScreen->CloseScreen;
  vncHooksScreen->CreateGC = pScreen->CreateGC;
  vncHooksScreen->CopyWindow = pScreen->CopyWindow;
  vncHooksScreen->ClearToBackground = pScreen->ClearToBackground;
#if XORG < 110
  vncHooksScreen->RestoreAreas = pScreen->RestoreAreas;
#endif
  vncHooksScreen->InstallColormap = pScreen->InstallColormap;
  vncHooksScreen->StoreColors = pScreen->StoreColors;
  vncHooksScreen->DisplayCursor = pScreen->DisplayCursor;
  vncHooksScreen->BlockHandler = pScreen->BlockHandler;
#ifdef RENDER
  PictureScreenPtr ps;
  ps = GetPictureScreenIfSet(pScreen);
  if (ps) {
    vncHooksScreen->Composite = ps->Composite;
  }
#endif
#ifdef RANDR
  rrScrPrivPtr rp;
  rp = rrGetScrPriv(pScreen);
  if (rp) {
    vncHooksScreen->RandRSetConfig = rp->rrSetConfig;
  }
#endif

  pScreen->CloseScreen = vncHooksCloseScreen;
  pScreen->CreateGC = vncHooksCreateGC;
  pScreen->CopyWindow = vncHooksCopyWindow;
  pScreen->ClearToBackground = vncHooksClearToBackground;
#if XORG < 110
  pScreen->RestoreAreas = vncHooksRestoreAreas;
#endif
  pScreen->InstallColormap = vncHooksInstallColormap;
  pScreen->StoreColors = vncHooksStoreColors;
  pScreen->DisplayCursor = vncHooksDisplayCursor;
  pScreen->BlockHandler = vncHooksBlockHandler;
#ifdef RENDER
  if (ps) {
    ps->Composite = vncHooksComposite;
  }
#endif
#ifdef RANDR
  if (rp) {
    rp->rrSetConfig = vncHooksRandRSetConfig;
  }
#endif

  return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
//
// screen functions
//

// SCREEN_UNWRAP and SCREEN_REWRAP unwrap and rewrap the given screen function.
// It would be nice to do this with a C++ class, but each function is of a
// distinct type, so it would have to use templates, and it's not worth that
// much pain.

#define SCREEN_UNWRAP(scrn,field)                                         \
  ScreenPtr pScreen = scrn;                                               \
  vncHooksScreenPtr vncHooksScreen = vncHooksScreenPrivate(pScreen);      \
  pScreen->field = vncHooksScreen->field;                                 \
  DBGPRINT((stderr,"vncHooks" #field " called\n"));

#define SCREEN_REWRAP(field) pScreen->field = vncHooks##field;


// CloseScreen - unwrap the screen functions and call the original CloseScreen
// function

static Bool vncHooksCloseScreen(int i, ScreenPtr pScreen_)
{
  SCREEN_UNWRAP(pScreen_, CloseScreen);

  pScreen->CreateGC = vncHooksScreen->CreateGC;
  pScreen->CopyWindow = vncHooksScreen->CopyWindow;
  pScreen->ClearToBackground = vncHooksScreen->ClearToBackground;
#if XORG < 110
  pScreen->RestoreAreas = vncHooksScreen->RestoreAreas;
#endif
  pScreen->InstallColormap = vncHooksScreen->InstallColormap;
  pScreen->StoreColors = vncHooksScreen->StoreColors;
  pScreen->DisplayCursor = vncHooksScreen->DisplayCursor;
  pScreen->BlockHandler = vncHooksScreen->BlockHandler;
#ifdef RENDER
  PictureScreenPtr ps;
  ps = GetPictureScreenIfSet(pScreen);
  if (ps) {
    ps->Composite = vncHooksScreen->Composite;
  }
#endif
#ifdef RANDR
  rrScrPrivPtr rp;
  rp = rrGetScrPriv(pScreen);
  if (rp) {
    rp->rrSetConfig = vncHooksScreen->RandRSetConfig;
  }
#endif

  DBGPRINT((stderr,"vncHooksCloseScreen: unwrapped screen functions\n"));

  return (*pScreen->CloseScreen)(i, pScreen);
}

// CreateGC - wrap the "GC funcs"

static Bool vncHooksCreateGC(GCPtr pGC)
{
  SCREEN_UNWRAP(pGC->pScreen, CreateGC);

  vncHooksGCPtr vncHooksGC = vncHooksGCPrivate(pGC);

  Bool ret = (*pScreen->CreateGC) (pGC);

  vncHooksGC->wrappedOps = 0;
  vncHooksGC->wrappedFuncs = pGC->funcs;
  pGC->funcs = &vncHooksGCFuncs;

  SCREEN_REWRAP(CreateGC);

  return ret;
}

// CopyWindow - destination of the copy is the old region, clipped by
// borderClip, translated by the delta.  This call only does the copy - it
// doesn't affect any other bits.

static void vncHooksCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg,
                               RegionPtr pOldRegion)
{
  SCREEN_UNWRAP(pWin->drawable.pScreen, CopyWindow);

  int dx, dy;
  RegionHelper copied(pScreen, pOldRegion);

  BoxRec screen_box = {0, 0, pScreen->width, pScreen->height};
  RegionHelper screen_rgn(pScreen, &screen_box, 1);

  dx = pWin->drawable.x - ptOldOrg.x;
  dy = pWin->drawable.y - ptOldOrg.y;

  // RFB tracks copies in terms of destination rectangle, not source.
  // We also need to copy with changes to the Window's clipping region.
  // Finally, make sure we don't get copies to or from regions outside
  // the framebuffer.
  REGION_INTERSECT(pScreen, copied.reg, copied.reg, screen_rgn.reg);
  REGION_TRANSLATE(pScreen, copied.reg, dx, dy);
  REGION_INTERSECT(pScreen, copied.reg, copied.reg, screen_rgn.reg);
  REGION_INTERSECT(pScreen, copied.reg, copied.reg, &pWin->borderClip);

  (*pScreen->CopyWindow) (pWin, ptOldOrg, pOldRegion);

  if (REGION_NOTEMPTY(pScreen, copied.reg))
    vncHooksScreen->desktop->add_copied(copied.reg, dx, dy);

  SCREEN_REWRAP(CopyWindow);
}

// ClearToBackground - changed region is the given rectangle, clipped by
// clipList, but only if generateExposures is false.

static void vncHooksClearToBackground(WindowPtr pWin, int x, int y, int w,
                                      int h, Bool generateExposures)
{
  SCREEN_UNWRAP(pWin->drawable.pScreen, ClearToBackground);

  BoxRec box;
  box.x1 = x + pWin->drawable.x;
  box.y1 = y + pWin->drawable.y;
  box.x2 = w ? (box.x1 + w) : (pWin->drawable.x + pWin->drawable.width);
  box.y2 = h ? (box.y1 + h) : (pWin->drawable.y + pWin->drawable.height);

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, &pWin->clipList);

  (*pScreen->ClearToBackground) (pWin, x, y, w, h, generateExposures);

  if (!generateExposures) {
    vncHooksScreen->desktop->add_changed(changed.reg);
  }

  SCREEN_REWRAP(ClearToBackground);
}

#if XORG < 110
// RestoreAreas - changed region is the given region

static RegionPtr vncHooksRestoreAreas(WindowPtr pWin, RegionPtr pRegion)
{
  SCREEN_UNWRAP(pWin->drawable.pScreen, RestoreAreas);

  RegionHelper changed(pScreen, pRegion);

  RegionPtr result = (*pScreen->RestoreAreas) (pWin, pRegion);

  vncHooksScreen->desktop->add_changed(changed.reg);

  SCREEN_REWRAP(RestoreAreas);

  return result;
}
#endif

// InstallColormap - get the new colormap

static void vncHooksInstallColormap(ColormapPtr pColormap)
{
  SCREEN_UNWRAP(pColormap->pScreen, InstallColormap);

  (*pScreen->InstallColormap) (pColormap);

  vncHooksScreen->desktop->setColormap(pColormap);

  SCREEN_REWRAP(InstallColormap);
}

// StoreColors - get the colormap changes

static void vncHooksStoreColors(ColormapPtr pColormap, int ndef,
                                xColorItem* pdef)
{
  SCREEN_UNWRAP(pColormap->pScreen, StoreColors);

  (*pScreen->StoreColors) (pColormap, ndef, pdef);

  vncHooksScreen->desktop->setColourMapEntries(pColormap, ndef, pdef);

  SCREEN_REWRAP(StoreColors);
}

// DisplayCursor - get the cursor shape

static Bool vncHooksDisplayCursor(
#if XORG >= 16
				  DeviceIntPtr pDev,
#endif
				  ScreenPtr pScreen_, CursorPtr cursor)
{
  SCREEN_UNWRAP(pScreen_, DisplayCursor);

  Bool ret = (*pScreen->DisplayCursor) (
#if XORG >= 16
					pDev,
#endif
					pScreen, cursor);
#if XORG >= 16
  /*
   * XXX DIX calls this function with NULL argument to remove cursor sprite from
   * screen. Should we handle this in setCursor as well?
   */
  if (cursor != NullCursor) {
#endif
    vncHooksScreen->desktop->setCursor(cursor);
#if XORG >= 16
  }
#endif

  SCREEN_REWRAP(DisplayCursor);

  return ret;
}

// BlockHandler - ignore any changes during the block handler - it's likely
// these are just drawing the cursor.

static void vncHooksBlockHandler(int i, pointer blockData, pointer pTimeout,
                                 pointer pReadmask)
{
  SCREEN_UNWRAP(screenInfo.screens[i], BlockHandler);

  vncHooksScreen->desktop->ignoreHooks(true);

  (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);

  vncHooksScreen->desktop->ignoreHooks(false);

  SCREEN_REWRAP(BlockHandler);
}

// Composite - needed for RENDER

#ifdef RENDER
void vncHooksComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, 
		       PicturePtr pDst, INT16 xSrc, INT16 ySrc, INT16 xMask, 
		       INT16 yMask, INT16 xDst, INT16 yDst, CARD16 width, CARD16 height)
{
  ScreenPtr pScreen = pDst->pDrawable->pScreen;
  vncHooksScreenPtr vncHooksScreen = vncHooksScreenPrivate(pScreen);
  BoxRec box;
  PictureScreenPtr ps = GetPictureScreen(pScreen);
  rfb::Rect rect1, rect2;

  rect1.setXYWH(pDst->pDrawable->x + xDst,
		pDst->pDrawable->y + yDst,
		width,
		height);
      
  rect2 = rect1.intersect(vncHooksScreen->desktop->getRect());
  if (!rect2.is_empty()) {
    box.x1 = rect2.tl.x;
    box.y1 = rect2.tl.y;
    box.x2 = rect2.br.x;
    box.y2 = rect2.br.y;
    RegionHelper changed(pScreen, &box, 0);
    vncHooksScreen->desktop->add_changed(changed.reg);
  }

  ps->Composite = vncHooksScreen->Composite;
  (*ps->Composite)(op, pSrc, pMask, pDst, xSrc, ySrc,
		   xMask, yMask, xDst, yDst, width, height);
  ps->Composite = vncHooksComposite;
}

#endif /* RENDER */

// RandRSetConfig - follow any framebuffer changes

#ifdef RANDR

static Bool vncHooksRandRSetConfig(ScreenPtr pScreen, Rotation rotation,
                                   int rate, RRScreenSizePtr pSize)
{
  vncHooksScreenPtr vncHooksScreen = vncHooksScreenPrivate(pScreen);
  rrScrPrivPtr rp = rrGetScrPriv(pScreen);
  Bool ret;
  RegionRec reg;
  BoxRec box;

  rp->rrSetConfig = vncHooksScreen->RandRSetConfig;
  ret = (*rp->rrSetConfig)(pScreen, rotation, rate, pSize);
  rp->rrSetConfig = vncHooksRandRSetConfig;

  if (!ret)
    return FALSE;

  // Let the RFB core know of the new dimensions and framebuffer
  vncHooksScreen->desktop->setFramebuffer(pScreen->width, pScreen->height,
                                          vncFbptr[pScreen->myNum],
                                          vncFbstride[pScreen->myNum]);

  // Mark entire screen as changed
  box.x1 = 0;
  box.y1 = 0;
  box.x2 = pScreen->width;
  box.y2 = pScreen->height;
  REGION_INIT(pScreen, &reg, &box, 1);

  vncHooksScreen->desktop->add_changed(&reg);

  return TRUE;
}

#endif /* RANDR */

/////////////////////////////////////////////////////////////////////////////
//
// GC "funcs"
//

// GCFuncUnwrapper is a helper class which unwraps the GC funcs and ops in its
// constructor and rewraps them in its destructor.

class GCFuncUnwrapper {
public:
  GCFuncUnwrapper(GCPtr pGC_) : pGC(pGC_) {
    vncHooksGC = vncHooksGCPrivate(pGC);
    pGC->funcs = vncHooksGC->wrappedFuncs;
    if (vncHooksGC->wrappedOps)
      pGC->ops = vncHooksGC->wrappedOps;
  }
  ~GCFuncUnwrapper() {
    vncHooksGC->wrappedFuncs = pGC->funcs;
    pGC->funcs = &vncHooksGCFuncs;
    if (vncHooksGC->wrappedOps) {
      vncHooksGC->wrappedOps = pGC->ops;
      pGC->ops = &vncHooksGCOps;
    }
  }
  GCPtr pGC;
  vncHooksGCPtr vncHooksGC;
};


// ValidateGC - wrap the "ops" if a viewable window

static void vncHooksValidateGC(GCPtr pGC, unsigned long changes,
                               DrawablePtr pDrawable)
{
  GCFuncUnwrapper u(pGC);

  DBGPRINT((stderr,"vncHooksValidateGC called\n"));

  (*pGC->funcs->ValidateGC) (pGC, changes, pDrawable);

  u.vncHooksGC->wrappedOps = 0;
  if (pDrawable->type == DRAWABLE_WINDOW && ((WindowPtr) pDrawable)->viewable) {
    u.vncHooksGC->wrappedOps = pGC->ops;
    DBGPRINT((stderr,"vncHooksValidateGC: wrapped GC ops\n"));
  }    
}

// Other GC funcs - just unwrap and call on

static void vncHooksChangeGC(GCPtr pGC, unsigned long mask) {
  GCFuncUnwrapper u(pGC);
  (*pGC->funcs->ChangeGC) (pGC, mask);
}
static void vncHooksCopyGC(GCPtr src, unsigned long mask, GCPtr dst) {
  GCFuncUnwrapper u(dst);
  (*dst->funcs->CopyGC) (src, mask, dst);
}
static void vncHooksDestroyGC(GCPtr pGC) {
  GCFuncUnwrapper u(pGC);
  (*pGC->funcs->DestroyGC) (pGC);
}
static void vncHooksChangeClip(GCPtr pGC, int type, pointer pValue, int nrects)
{
  GCFuncUnwrapper u(pGC);
  (*pGC->funcs->ChangeClip) (pGC, type, pValue, nrects);
}
static void vncHooksDestroyClip(GCPtr pGC) {
  GCFuncUnwrapper u(pGC);
  (*pGC->funcs->DestroyClip) (pGC);
}
static void vncHooksCopyClip(GCPtr dst, GCPtr src) {
  GCFuncUnwrapper u(dst);
  (*dst->funcs->CopyClip) (dst, src);
}


/////////////////////////////////////////////////////////////////////////////
//
// GC "ops"
//

// GCOpUnwrapper is a helper class which unwraps the GC funcs and ops in its
// constructor and rewraps them in its destructor.

class GCOpUnwrapper {
public:
  GCOpUnwrapper(DrawablePtr pDrawable, GCPtr pGC_)
    : pGC(pGC_), pScreen(pDrawable->pScreen)
  {
    vncHooksGC = vncHooksGCPrivate(pGC);
    oldFuncs = pGC->funcs;
    pGC->funcs = vncHooksGC->wrappedFuncs;
    pGC->ops = vncHooksGC->wrappedOps;
  }
  ~GCOpUnwrapper() {
    vncHooksGC->wrappedOps = pGC->ops;
    pGC->funcs = oldFuncs;
    pGC->ops = &vncHooksGCOps;
  }
  GCPtr pGC;
  vncHooksGCPtr vncHooksGC;
  GCFuncs* oldFuncs;
  ScreenPtr pScreen;
};

#define GC_OP_UNWRAPPER(pDrawable, pGC, name)                             \
  GCOpUnwrapper u(pDrawable, pGC);                                        \
  ScreenPtr pScreen = (pDrawable)->pScreen;                               \
  vncHooksScreenPtr vncHooksScreen = vncHooksScreenPrivate(pScreen);      \
  DBGPRINT((stderr,"vncHooks" #name " called\n"));


// FillSpans - changed region is the whole of borderClip.  This is pessimistic,
// but I believe this function is rarely used so it doesn't matter.

static void vncHooksFillSpans(DrawablePtr pDrawable, GCPtr pGC, int nInit,
                              DDXPointPtr pptInit, int *pwidthInit,
                              int fSorted)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, FillSpans);

  RegionHelper changed(pScreen, &((WindowPtr)pDrawable)->borderClip);

  (*pGC->ops->FillSpans) (pDrawable, pGC, nInit, pptInit, pwidthInit, fSorted);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// SetSpans - changed region is the whole of borderClip.  This is pessimistic,
// but I believe this function is rarely used so it doesn't matter.

static void vncHooksSetSpans(DrawablePtr pDrawable, GCPtr pGC, char *psrc,
                             DDXPointPtr ppt, int *pwidth, int nspans,
                             int fSorted)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, SetSpans);

  RegionHelper changed(pScreen, &((WindowPtr)pDrawable)->borderClip);

  (*pGC->ops->SetSpans) (pDrawable, pGC, psrc, ppt, pwidth, nspans, fSorted);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PutImage - changed region is the given rectangle, clipped by pCompositeClip

static void vncHooksPutImage(DrawablePtr pDrawable, GCPtr pGC, int depth,
                             int x, int y, int w, int h, int leftPad,
                             int format, char *pBits)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PutImage);

  BoxRec box;
  box.x1 = x + pDrawable->x;
  box.y1 = y + pDrawable->y;
  box.x2 = box.x1 + w;
  box.y2 = box.y1 + h;

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PutImage) (pDrawable, pGC, depth, x, y, w, h, leftPad, format,
                         pBits);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// CopyArea - destination of the copy is the dest rectangle, clipped by
// pCompositeClip.  Any parts of the destination which cannot be copied from
// the source (could be all of it) go into the changed region.

static RegionPtr vncHooksCopyArea(DrawablePtr pSrc, DrawablePtr pDst,
                                  GCPtr pGC, int srcx, int srcy, int w, int h,
                                  int dstx, int dsty)
{
  GC_OP_UNWRAPPER(pDst, pGC, CopyArea);

  BoxRec box;
  box.x1 = dstx + pDst->x;
  box.y1 = dsty + pDst->y;
  box.x2 = box.x1 + w;
  box.y2 = box.y1 + h;

  RegionHelper dst(pScreen, &box, 0);
  REGION_INTERSECT(pScreen, dst.reg, dst.reg, pGC->pCompositeClip);

  RegionHelper src(pScreen);

  if ((pSrc->type == DRAWABLE_WINDOW) && (pSrc->pScreen == pScreen)) {
    box.x1 = srcx + pSrc->x;
    box.y1 = srcy + pSrc->y;
    box.x2 = box.x1 + w;
    box.y2 = box.y1 + h;

    src.init(&box, 0);
    REGION_INTERSECT(pScreen, src.reg, src.reg, &((WindowPtr)pSrc)->clipList);
    REGION_TRANSLATE(pScreen, src.reg,
                     dstx + pDst->x - srcx - pSrc->x,
                     dsty + pDst->y - srcy - pSrc->y);
  } else {
    src.init(NullBox, 0);
  }

  RegionHelper changed(pScreen, NullBox, 0);
  REGION_SUBTRACT(pScreen, changed.reg, dst.reg, src.reg);
  REGION_INTERSECT(pScreen, dst.reg, dst.reg, src.reg);

  RegionPtr rgn = (*pGC->ops->CopyArea) (pSrc, pDst, pGC, srcx, srcy, w, h,
                                         dstx, dsty);

  if (REGION_NOTEMPTY(pScreen, dst.reg))
    vncHooksScreen->desktop->add_copied(dst.reg,
                                        dstx + pDst->x - srcx - pSrc->x,
                                        dsty + pDst->y - srcy - pSrc->y);

  if (REGION_NOTEMPTY(pScreen, changed.reg))
    vncHooksScreen->desktop->add_changed(changed.reg);

  return rgn;
}


// CopyPlane - changed region is the destination rectangle, clipped by
// pCompositeClip

static RegionPtr vncHooksCopyPlane(DrawablePtr pSrc, DrawablePtr pDst,
                                   GCPtr pGC, int srcx, int srcy, int w, int h,
                                   int dstx, int dsty, unsigned long plane)
{
  GC_OP_UNWRAPPER(pDst, pGC, CopyPlane);

  BoxRec box;
  box.x1 = dstx + pDst->x;
  box.y1 = dsty + pDst->y;
  box.x2 = box.x1 + w;
  box.y2 = box.y1 + h;

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  RegionPtr rgn = (*pGC->ops->CopyPlane) (pSrc, pDst, pGC, srcx, srcy, w, h,
                                          dstx, dsty, plane);
  vncHooksScreen->desktop->add_changed(changed.reg);

  return rgn;
}

// PolyPoint - changed region is the bounding rect, clipped by pCompositeClip

static void vncHooksPolyPoint(DrawablePtr pDrawable, GCPtr pGC, int mode,
                              int npt, xPoint *pts)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyPoint);

  if (npt == 0) {
    (*pGC->ops->PolyPoint) (pDrawable, pGC, mode, npt, pts);
    return;
  }

  int minX = pts[0].x;
  int maxX = pts[0].x;
  int minY = pts[0].y;
  int maxY = pts[0].y;

  if (mode == CoordModePrevious) {
    int x = pts[0].x;
    int y = pts[0].y;

    for (int i = 1; i < npt; i++) {
      x += pts[i].x;
      y += pts[i].y;
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
    }
  } else {
    for (int i = 1; i < npt; i++) {
      if (pts[i].x < minX) minX = pts[i].x;
      if (pts[i].x > maxX) maxX = pts[i].x;
      if (pts[i].y < minY) minY = pts[i].y;
      if (pts[i].y > maxY) maxY = pts[i].y;
    }
  }

  BoxRec box;
  box.x1 = minX + pDrawable->x;
  box.y1 = minY + pDrawable->y;
  box.x2 = maxX + 1 + pDrawable->x;
  box.y2 = maxY + 1 + pDrawable->y;

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PolyPoint) (pDrawable, pGC, mode, npt, pts);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// Polylines - changed region is the union of the bounding rects of each line,
// clipped by pCompositeClip.  If there are more than MAX_RECTS_PER_OP lines,
// just use the bounding rect of all the lines.

static void vncHooksPolylines(DrawablePtr pDrawable, GCPtr pGC, int mode,
                              int npt, DDXPointPtr ppts)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, Polylines);

  if (npt == 0) {
    (*pGC->ops->Polylines) (pDrawable, pGC, mode, npt, ppts);
    return;
  }

  int nRegRects = npt - 1;
  xRectangle regRects[MAX_RECTS_PER_OP];

  int lw = pGC->lineWidth;
  if (lw == 0) lw = 1;

  if (npt == 1)
  {
    // a single point
    nRegRects = 1;
    regRects[0].x = pDrawable->x + ppts[0].x - lw;
    regRects[0].y = pDrawable->y + ppts[0].y - lw;
    regRects[0].width = 2*lw;
    regRects[0].height = 2*lw;
  }
  else
  {
    /*
     * mitered joins can project quite a way from
     * the line end; the 11 degree miter limit limits
     * this extension to lw / (2 * tan(11/2)), rounded up
     * and converted to int yields 6 * lw
     */

    int extra = lw / 2;
    if (pGC->joinStyle == JoinMiter) {
      extra = 6 * lw;
    }

    int prevX, prevY, curX, curY;
    int rectX1, rectY1, rectX2, rectY2;
    int minX, minY, maxX, maxY;

    prevX = ppts[0].x + pDrawable->x;
    prevY = ppts[0].y + pDrawable->y;
    minX = maxX = prevX;
    minY = maxY = prevY;

    for (int i = 0; i < nRegRects; i++) {
      if (mode == CoordModeOrigin) {
        curX = pDrawable->x + ppts[i+1].x;
        curY = pDrawable->y + ppts[i+1].y;
      } else {
        curX = prevX + ppts[i+1].x;
        curY = prevY + ppts[i+1].y;
      }

      if (prevX > curX) {
        rectX1 = curX - extra;
        rectX2 = prevX + extra + 1;
      } else {
        rectX1 = prevX - extra;
        rectX2 = curX + extra + 1;
      }

      if (prevY > curY) {
        rectY1 = curY - extra;
        rectY2 = prevY + extra + 1;
      } else {
        rectY1 = prevY - extra;
        rectY2 = curY + extra + 1;
      }

      if (nRegRects <= MAX_RECTS_PER_OP) {
        regRects[i].x = rectX1;
        regRects[i].y = rectY1;
        regRects[i].width = rectX2 - rectX1;
        regRects[i].height = rectY2 - rectY1;
      } else {
        if (rectX1 < minX) minX = rectX1;
        if (rectY1 < minY) minY = rectY1;
        if (rectX2 > maxX) maxX = rectX2;
        if (rectY2 > maxY) maxY = rectY2;
      }

      prevX = curX;
      prevY = curY;
    }

    if (nRegRects > MAX_RECTS_PER_OP) {
      regRects[0].x = minX;
      regRects[0].y = minY;
      regRects[0].width = maxX - minX;
      regRects[0].height = maxY - minY;
      nRegRects = 1;
    }
  }

  RegionHelper changed(pScreen, nRegRects, regRects);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->Polylines) (pDrawable, pGC, mode, npt, ppts);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PolySegment - changed region is the union of the bounding rects of each
// segment, clipped by pCompositeClip.  If there are more than MAX_RECTS_PER_OP
// segments, just use the bounding rect of all the segments.

static void vncHooksPolySegment(DrawablePtr pDrawable, GCPtr pGC, int nseg,
                                xSegment *segs)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolySegment);

  if (nseg == 0) {
    (*pGC->ops->PolySegment) (pDrawable, pGC, nseg, segs);
    return;
  }

  xRectangle regRects[MAX_RECTS_PER_OP];
  int nRegRects = nseg;

  int lw = pGC->lineWidth;
  int extra = lw / 2;

  int rectX1, rectY1, rectX2, rectY2;
  int minX, minY, maxX, maxY;

  minX = maxX = segs[0].x1;
  minY = maxY = segs[0].y1;

  for (int i = 0; i < nseg; i++) {
    if (segs[i].x1 > segs[i].x2) {
      rectX1 = pDrawable->x + segs[i].x2 - extra;
      rectX2 = pDrawable->x + segs[i].x1 + extra + 1;
    } else {
      rectX1 = pDrawable->x + segs[i].x1 - extra;
      rectX2 = pDrawable->x + segs[i].x2 + extra + 1;
    }

    if (segs[i].y1 > segs[i].y2) {
      rectY1 = pDrawable->y + segs[i].y2 - extra;
      rectY2 = pDrawable->y + segs[i].y1 + extra + 1;
    } else {
      rectY1 = pDrawable->y + segs[i].y1 - extra;
      rectY2 = pDrawable->y + segs[i].y2 + extra + 1;
    }

    if (nseg <= MAX_RECTS_PER_OP) {
      regRects[i].x = rectX1;
      regRects[i].y = rectY1;
      regRects[i].width = rectX2 - rectX1;
      regRects[i].height = rectY2 - rectY1;
    } else {
      if (rectX1 < minX) minX = rectX1;
      if (rectY1 < minY) minY = rectY1;
      if (rectX2 > maxX) maxX = rectX2;
      if (rectY2 > maxY) maxY = rectY2;
    }
  }

  if (nseg > MAX_RECTS_PER_OP) {
    regRects[0].x = minX;
    regRects[0].y = minY;
    regRects[0].width = maxX - minX;
    regRects[0].height = maxY - minY;
    nRegRects = 1;
  }

  RegionHelper changed(pScreen, nRegRects, regRects);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PolySegment) (pDrawable, pGC, nseg, segs);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PolyRectangle - changed region is the union of the bounding rects around
// each side of the outline rectangles, clipped by pCompositeClip.  If there
// are more than MAX_RECTS_PER_OP rectangles, just use the bounding rect of all
// the rectangles.

static void vncHooksPolyRectangle(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                                  xRectangle *rects)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyRectangle);

  if (nrects == 0) {
    (*pGC->ops->PolyRectangle) (pDrawable, pGC, nrects, rects);
    return;
  }

  xRectangle regRects[MAX_RECTS_PER_OP*4];
  int nRegRects = nrects * 4;

  int lw = pGC->lineWidth;
  int extra = lw / 2;

  int rectX1, rectY1, rectX2, rectY2;
  int minX, minY, maxX, maxY;

  minX = maxX = rects[0].x;
  minY = maxY = rects[0].y;

  for (int i = 0; i < nrects; i++) {
    if (nrects <= MAX_RECTS_PER_OP) {
      regRects[i*4].x = rects[i].x - extra + pDrawable->x;
      regRects[i*4].y = rects[i].y - extra + pDrawable->y;
      regRects[i*4].width = rects[i].width + 1 + 2 * extra;
      regRects[i*4].height = 1 + 2 * extra;

      regRects[i*4+1].x = rects[i].x - extra + pDrawable->x;
      regRects[i*4+1].y = rects[i].y - extra + pDrawable->y;
      regRects[i*4+1].width = 1 + 2 * extra;
      regRects[i*4+1].height = rects[i].height + 1 + 2 * extra;

      regRects[i*4+2].x = rects[i].x + rects[i].width - extra + pDrawable->x;
      regRects[i*4+2].y = rects[i].y - extra + pDrawable->y;
      regRects[i*4+2].width = 1 + 2 * extra;
      regRects[i*4+2].height = rects[i].height + 1 + 2 * extra;

      regRects[i*4+3].x = rects[i].x - extra + pDrawable->x;
      regRects[i*4+3].y = rects[i].y + rects[i].height - extra + pDrawable->y;
      regRects[i*4+3].width = rects[i].width + 1 + 2 * extra;
      regRects[i*4+3].height = 1 + 2 * extra;
    } else {
      rectX1 = pDrawable->x + rects[i].x - extra;
      rectY1 = pDrawable->y + rects[i].y - extra;
      rectX2 = pDrawable->x + rects[i].x + rects[i].width + extra+1;
      rectY2 = pDrawable->y + rects[i].y + rects[i].height + extra+1;
      if (rectX1 < minX) minX = rectX1;
      if (rectY1 < minY) minY = rectY1;
      if (rectX2 > maxX) maxX = rectX2;
      if (rectY2 > maxY) maxY = rectY2;
    }
  }

  if (nrects > MAX_RECTS_PER_OP) {
    regRects[0].x = minX;
    regRects[0].y = minY;
    regRects[0].width = maxX - minX;
    regRects[0].height = maxY - minY;
    nRegRects = 1;
  }

  RegionHelper changed(pScreen, nRegRects, regRects);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PolyRectangle) (pDrawable, pGC, nrects, rects);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PolyArc - changed region is the union of bounding rects around each arc,
// clipped by pCompositeClip.  If there are more than MAX_RECTS_PER_OP
// arcs, just use the bounding rect of all the arcs.

static void vncHooksPolyArc(DrawablePtr pDrawable, GCPtr pGC, int narcs,
                            xArc *arcs)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyArc);

  if (narcs == 0) {
    (*pGC->ops->PolyArc) (pDrawable, pGC, narcs, arcs);
    return;
  }

  xRectangle regRects[MAX_RECTS_PER_OP];
  int nRegRects = narcs;

  int lw = pGC->lineWidth;
  if (lw == 0) lw = 1;
  int extra = lw / 2;

  int rectX1, rectY1, rectX2, rectY2;
  int minX, minY, maxX, maxY;

  minX = maxX = arcs[0].x;
  minY = maxY = arcs[0].y;

  for (int i = 0; i < narcs; i++) {
    if (narcs <= MAX_RECTS_PER_OP) {
      regRects[i].x = arcs[i].x - extra + pDrawable->x;
      regRects[i].y = arcs[i].y - extra + pDrawable->y;
      regRects[i].width = arcs[i].width + lw;
      regRects[i].height = arcs[i].height + lw;
    } else {
      rectX1 = pDrawable->x + arcs[i].x - extra;
      rectY1 = pDrawable->y + arcs[i].y - extra;
      rectX2 = pDrawable->x + arcs[i].x + arcs[i].width + lw;
      rectY2 = pDrawable->y + arcs[i].y + arcs[i].height + lw;
      if (rectX1 < minX) minX = rectX1;
      if (rectY1 < minY) minY = rectY1;
      if (rectX2 > maxX) maxX = rectX2;
      if (rectY2 > maxY) maxY = rectY2;
    }
  }

  if (narcs > MAX_RECTS_PER_OP) {
    regRects[0].x = minX;
    regRects[0].y = minY;
    regRects[0].width = maxX - minX;
    regRects[0].height = maxY - minY;
    nRegRects = 1;
  }

  RegionHelper changed(pScreen, nRegRects, regRects);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PolyArc) (pDrawable, pGC, narcs, arcs);

  vncHooksScreen->desktop->add_changed(changed.reg);
}


// FillPolygon - changed region is the bounding rect around the polygon,
// clipped by pCompositeClip

static void vncHooksFillPolygon(DrawablePtr pDrawable, GCPtr pGC, int shape,
                                int mode, int count, DDXPointPtr pts)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, FillPolygon);

  if (count == 0) {
    (*pGC->ops->FillPolygon) (pDrawable, pGC, shape, mode, count, pts);
    return;
  }

  int minX = pts[0].x;
  int maxX = pts[0].x;
  int minY = pts[0].y;
  int maxY = pts[0].y;

  if (mode == CoordModePrevious) {
    int x = pts[0].x;
    int y = pts[0].y;

    for (int i = 1; i < count; i++) {
      x += pts[i].x;
      y += pts[i].y;
      if (x < minX) minX = x;
      if (x > maxX) maxX = x;
      if (y < minY) minY = y;
      if (y > maxY) maxY = y;
    }
  } else {
    for (int i = 1; i < count; i++) {
      if (pts[i].x < minX) minX = pts[i].x;
      if (pts[i].x > maxX) maxX = pts[i].x;
      if (pts[i].y < minY) minY = pts[i].y;
      if (pts[i].y > maxY) maxY = pts[i].y;
    }
  }

  BoxRec box;
  box.x1 = minX + pDrawable->x;
  box.y1 = minY + pDrawable->y;
  box.x2 = maxX + 1 + pDrawable->x;
  box.y2 = maxY + 1 + pDrawable->y;

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->FillPolygon) (pDrawable, pGC, shape, mode, count, pts);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PolyFillRect - changed region is the union of the rectangles, clipped by
// pCompositeClip.  If there are more than MAX_RECTS_PER_OP rectangles, just
// use the bounding rect of all the rectangles.

static void vncHooksPolyFillRect(DrawablePtr pDrawable, GCPtr pGC, int nrects,
                                 xRectangle *rects)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyFillRect);

  if (nrects == 0) {
    (*pGC->ops->PolyFillRect) (pDrawable, pGC, nrects, rects);
    return;
  }

  xRectangle regRects[MAX_RECTS_PER_OP];
  int nRegRects = nrects;
  int rectX1, rectY1, rectX2, rectY2;
  int minX, minY, maxX, maxY;
  minX = maxX = rects[0].x;
  minY = maxY = rects[0].y;

  for (int i = 0; i < nrects; i++) {
    if (nrects <= MAX_RECTS_PER_OP) {
      regRects[i].x = rects[i].x + pDrawable->x;
      regRects[i].y = rects[i].y + pDrawable->y;
      regRects[i].width = rects[i].width;
      regRects[i].height = rects[i].height;
    } else {
      rectX1 = pDrawable->x + rects[i].x;
      rectY1 = pDrawable->y + rects[i].y;
      rectX2 = pDrawable->x + rects[i].x + rects[i].width;
      rectY2 = pDrawable->y + rects[i].y + rects[i].height;
      if (rectX1 < minX) minX = rectX1;
      if (rectY1 < minY) minY = rectY1;
      if (rectX2 > maxX) maxX = rectX2;
      if (rectY2 > maxY) maxY = rectY2;
    }
  }

  if (nrects > MAX_RECTS_PER_OP) {
    regRects[0].x = minX;
    regRects[0].y = minY;
    regRects[0].width = maxX - minX;
    regRects[0].height = maxY - minY;
    nRegRects = 1;
  }

  RegionHelper changed(pScreen, nRegRects, regRects);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PolyFillRect) (pDrawable, pGC, nrects, rects);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PolyFillArc - changed region is the union of bounding rects around each arc,
// clipped by pCompositeClip.  If there are more than MAX_RECTS_PER_OP arcs,
// just use the bounding rect of all the arcs.

static void vncHooksPolyFillArc(DrawablePtr pDrawable, GCPtr pGC, int narcs,
                                xArc *arcs)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyFillArc);

  if (narcs == 0) {
    (*pGC->ops->PolyFillArc) (pDrawable, pGC, narcs, arcs);
    return;
  }

  xRectangle regRects[MAX_RECTS_PER_OP];
  int nRegRects = narcs;

  int lw = pGC->lineWidth;
  if (lw == 0) lw = 1;
  int extra = lw / 2;

  int rectX1, rectY1, rectX2, rectY2;
  int minX, minY, maxX, maxY;

  minX = maxX = arcs[0].x;
  minY = maxY = arcs[0].y;

  for (int i = 0; i < narcs; i++) {
    if (narcs <= MAX_RECTS_PER_OP) {
      regRects[i].x = arcs[i].x - extra + pDrawable->x;
      regRects[i].y = arcs[i].y - extra + pDrawable->y;
      regRects[i].width = arcs[i].width + lw;
      regRects[i].height = arcs[i].height + lw;
    } else {
      rectX1 = pDrawable->x + arcs[i].x - extra;
      rectY1 = pDrawable->y + arcs[i].y - extra;
      rectX2 = pDrawable->x + arcs[i].x + arcs[i].width + lw;
      rectY2 = pDrawable->y + arcs[i].y + arcs[i].height + lw;
      if (rectX1 < minX) minX = rectX1;
      if (rectY1 < minY) minY = rectY1;
      if (rectX2 > maxX) maxX = rectX2;
      if (rectY2 > maxY) maxY = rectY2;
    }
  }

  if (narcs > MAX_RECTS_PER_OP) {
    regRects[0].x = minX;
    regRects[0].y = minY;
    regRects[0].width = maxX - minX;
    regRects[0].height = maxY - minY;
    nRegRects = 1;
  }

  RegionHelper changed(pScreen, nRegRects, regRects);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PolyFillArc) (pDrawable, pGC, narcs, arcs);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// GetTextBoundingRect - calculate a bounding rectangle around n chars of a
// font.  Not particularly accurate, but good enough.

static void GetTextBoundingRect(DrawablePtr pDrawable, FontPtr font, int x,
                                int y, int nchars, BoxPtr box)
{
  int ascent = __rfbmax(FONTASCENT(font), FONTMAXBOUNDS(font, ascent));
  int descent = __rfbmax(FONTDESCENT(font), FONTMAXBOUNDS(font, descent));
  int charWidth = __rfbmax(FONTMAXBOUNDS(font,rightSideBearing),
                           FONTMAXBOUNDS(font,characterWidth));

  box->x1 = pDrawable->x + x;
  box->y1 = pDrawable->y + y - ascent;
  box->x2 = box->x1 + charWidth * nchars;
  box->y2 = box->y1 + ascent + descent;

  if (FONTMINBOUNDS(font,leftSideBearing) < 0)
    box->x1 += FONTMINBOUNDS(font,leftSideBearing);
}

// PolyText8 - changed region is bounding rect around count chars, clipped by
// pCompositeClip

static int vncHooksPolyText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                             int count, char *chars)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyText8);

  if (count == 0)
    return (*pGC->ops->PolyText8) (pDrawable, pGC, x, y, count, chars);

  BoxRec box;
  GetTextBoundingRect(pDrawable, pGC->font, x, y, count, &box);

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  int ret = (*pGC->ops->PolyText8) (pDrawable, pGC, x, y, count, chars);

  vncHooksScreen->desktop->add_changed(changed.reg);

  return ret;
}

// PolyText16 - changed region is bounding rect around count chars, clipped by
// pCompositeClip

static int vncHooksPolyText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                              int count, unsigned short *chars)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyText16);

  if (count == 0)
    return (*pGC->ops->PolyText16) (pDrawable, pGC, x, y, count, chars);

  BoxRec box;
  GetTextBoundingRect(pDrawable, pGC->font, x, y, count, &box);

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  int ret = (*pGC->ops->PolyText16) (pDrawable, pGC, x, y, count, chars);

  vncHooksScreen->desktop->add_changed(changed.reg);

  return ret;
}

// ImageText8 - changed region is bounding rect around count chars, clipped by
// pCompositeClip

static void vncHooksImageText8(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                               int count, char *chars)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, ImageText8);

  if (count == 0) {
    (*pGC->ops->ImageText8) (pDrawable, pGC, x, y, count, chars);
    return;
  }

  BoxRec box;
  GetTextBoundingRect(pDrawable, pGC->font, x, y, count, &box);

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->ImageText8) (pDrawable, pGC, x, y, count, chars);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// ImageText16 - changed region is bounding rect around count chars, clipped by
// pCompositeClip

static void vncHooksImageText16(DrawablePtr pDrawable, GCPtr pGC, int x, int y,
                                int count, unsigned short *chars)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, ImageText16);

  if (count == 0) {
    (*pGC->ops->ImageText16) (pDrawable, pGC, x, y, count, chars);
    return;
  }

  BoxRec box;
  GetTextBoundingRect(pDrawable, pGC->font, x, y, count, &box);

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->ImageText16) (pDrawable, pGC, x, y, count, chars);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// ImageGlyphBlt - changed region is bounding rect around nglyph chars, clipped
// by pCompositeClip

static void vncHooksImageGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x,
                                  int y, unsigned int nglyph,
                                  CharInfoPtr *ppci, pointer pglyphBase)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, ImageGlyphBlt);

  if (nglyph == 0) {
    (*pGC->ops->ImageGlyphBlt) (pDrawable, pGC, x, y, nglyph, ppci,pglyphBase);
    return;
  }

  BoxRec box;
  GetTextBoundingRect(pDrawable, pGC->font, x, y, nglyph, &box);

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->ImageGlyphBlt) (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PolyGlyphBlt - changed region is bounding rect around nglyph chars, clipped
// by pCompositeClip

static void vncHooksPolyGlyphBlt(DrawablePtr pDrawable, GCPtr pGC, int x,
                                 int y, unsigned int nglyph,
                                 CharInfoPtr *ppci, pointer pglyphBase)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PolyGlyphBlt);

  if (nglyph == 0) {
    (*pGC->ops->PolyGlyphBlt) (pDrawable, pGC, x, y, nglyph, ppci,pglyphBase);
    return;
  }

  BoxRec box;
  GetTextBoundingRect(pDrawable, pGC->font, x, y, nglyph, &box);

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PolyGlyphBlt) (pDrawable, pGC, x, y, nglyph, ppci, pglyphBase);

  vncHooksScreen->desktop->add_changed(changed.reg);
}

// PushPixels - changed region is the given rectangle, clipped by
// pCompositeClip

static void vncHooksPushPixels(GCPtr pGC, PixmapPtr pBitMap,
                               DrawablePtr pDrawable, int w, int h, int x,
                               int y)
{
  GC_OP_UNWRAPPER(pDrawable, pGC, PushPixels);

  BoxRec box;
  box.x1 = x + pDrawable->x;
  box.y1 = y + pDrawable->y;
  box.x2 = box.x1 + w;
  box.y2 = box.y1 + h;

  RegionHelper changed(pScreen, &box, 0);

  REGION_INTERSECT(pScreen, changed.reg, changed.reg, pGC->pCompositeClip);

  (*pGC->ops->PushPixels) (pGC, pBitMap, pDrawable, w, h, x, y);

  vncHooksScreen->desktop->add_changed(changed.reg);
}
