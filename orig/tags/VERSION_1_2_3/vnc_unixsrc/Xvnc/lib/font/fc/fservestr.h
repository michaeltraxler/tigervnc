/* $XConsortium: fservestr.h,v 1.13 95/06/09 22:16:29 gildea Exp $ */
/*
 * Copyright 1990 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Network Computing Devices not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Network Computing
 * Devices makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * NETWORK COMPUTING DEVICES DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL NETWORK COMPUTING DEVICES BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  	Dave Lemke, Network Computing Devices, Inc
 */

#ifndef _FSERVESTR_H_
#define _FSERVESTR_H_

#include	"fserve.h"
#include	"fsio.h"

/*
 * font server data structures
 */
/*
 * font server private storage
 */


typedef struct _fs_font {
    CharInfoPtr pDefault;
    CharInfoPtr encoding;
    CharInfoPtr inkMetrics;
}           FSFontRec, *FSFontPtr;

/* FS special data for the font */
typedef struct _fs_font_data {
    long        fontid;
    int		generation;	/* FS generation when opened */
    FontPathElementPtr fpe;
    unsigned long glyphs_to_get;	/* # glyphs remaining to be gotten */

    /* Following data needed in case font needs to be reopened. */
    int		namelen;
    char       *name;
    fsBitmapFormat	format;
    fsBitmapFormatMask	fmask;
}           FSFontDataRec;

typedef struct fs_clients_depending {
    pointer	client;
    struct fs_clients_depending *next;
}	FSClientsDependingRec, *FSClientsDependingPtr;

/* OpenFont specific data for blocked request */
typedef struct _fs_blocked_font {
    FontPtr     pfont;
    long        fontid;
    int         state;		/* how many of the replies have landed */
    int         errcode;
    int         flags;
    fsBitmapFormat format;
    FSClientsDependingPtr	clients_depending;
}           FSBlockedFontRec;

/* LoadGlyphs data for blocked request */
typedef struct _fs_blocked_glyphs {
    FontPtr     pfont;
    int		num_expected_ranges;
    fsRange     *expected_ranges;
    int         errcode;
    Bool        done;
    FSClientsDependingPtr	clients_depending;
}           FSBlockedGlyphRec;

/* LoadExtents data for blocked request */
typedef struct _fs_blocked_extents {
    FontPtr     pfont;
    fsRange    *expected_ranges;
    int         nranges;
    Bool        done;
    unsigned long nextents;
    fsXCharInfo *extents;
}           FSBlockedExtentRec;

/* LoadBitmaps data for blocked request */
typedef struct _fs_blocked_bitmaps {
    FontPtr     pfont;
    fsRange    *expected_ranges;
    int         nranges;
    Bool        done;
    unsigned long size;
    unsigned long nglyphs;
    fsOffset32   *offsets;
    pointer     gdata;
}           FSBlockedBitmapRec;

/* state for blocked ListFonts */
typedef struct _fs_blocked_list {
    FontNamesPtr names;
    int         patlen;
    int         errcode;
    Bool        done;
}           FSBlockedListRec;

/* state for blocked ListFontsWithInfo */
typedef struct _fs_blocked_list_info {
    int         status;
    char       *name;
    int         namelen;
    FontInfoPtr pfi;
    int         remaining;
    int         errcode;
}           FSBlockedListInfoRec;

/* state for blocked request */
typedef struct _fs_block_data {
    int			    type;	/* Open Font, LoadGlyphs, ListFonts,
					 * ListWithInfo */
    pointer		    client;	    /* who wants it */
    int			    sequence_number;/* expected */
    fsGenericReply	    header;
    pointer		    data;	    /* type specific data */
    struct _fs_block_data   *depending;	    /* clients depending on this one */
    struct _fs_block_data   *next;
}           FSBlockDataRec;

/* state for reconnected to dead font server */
typedef struct _fs_reconnect {
    int	    i;
} FSReconnectRec, *FSReconnectPtr;


#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define fsCat(x,y) x##_##y
#else
#define fsCat(x,y) x/**/_/**/y
#endif


/* copy XCharInfo parts of a protocol reply into a xCharInfo */

#define fsUnpack_XCharInfo(packet, structure) \
    (structure)->leftSideBearing = fsCat(packet,left); \
    (structure)->rightSideBearing = fsCat(packet,right); \
    (structure)->characterWidth = fsCat(packet,width); \
    (structure)->ascent = fsCat(packet,ascent); \
    (structure)->descent = fsCat(packet,descent); \
    (structure)->attributes = fsCat(packet,attributes)


/* copy XFontInfoHeader parts of a protocol reply into a FontInfoRec */

#define fsUnpack_XFontInfoHeader(packet, structure) \
    (structure)->allExist = ((packet)->font_header_flags & FontInfoAllCharsExist) != 0; \
    (structure)->drawDirection = \
        ((packet)->font_header_draw_direction == LeftToRightDrawDirection) ? \
	LeftToRight : RightToLeft; \
    (structure)->inkInside = ((packet)->font_header_flags & FontInfoInkInside) != 0; \
 \
    (structure)->firstRow = (packet)->font_hdr_char_range_min_char_high; \
    (structure)->firstCol = (packet)->font_hdr_char_range_min_char_low; \
    (structure)->lastRow = (packet)->font_hdr_char_range_max_char_high; \
    (structure)->lastCol = (packet)->font_hdr_char_range_max_char_low; \
    (structure)->defaultCh = (packet)->font_header_default_char_low \
                           + ((packet)->font_header_default_char_high << 8); \
 \
    (structure)->fontDescent = (packet)->font_header_font_descent; \
    (structure)->fontAscent = (packet)->font_header_font_ascent; \
 \
    fsUnpack_XCharInfo((packet)->font_header_min_bounds, &(structure)->minbounds); \
    fsUnpack_XCharInfo((packet)->font_header_min_bounds, &(structure)->ink_minbounds); \
    fsUnpack_XCharInfo((packet)->font_header_max_bounds, &(structure)->maxbounds); \
    fsUnpack_XCharInfo((packet)->font_header_max_bounds, &(structure)->ink_maxbounds)


#endif				/* _FSERVESTR_H_ */
