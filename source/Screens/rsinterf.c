// BetterTelnet
// copyright 1997, 1998, 1999 Rolf Braun

// This is free software under the GNU General Public License (GPL). See the file COPYING
// which comes with the source code and documentation distributions for details.

// based on NCSA Telnet 2.7b5

// #pragma profile on

/* rsinterf.c */

/* A split of RSmac.c to facilitate keeping my sanity --CCP */

#include "rsdefs.h"
#include "vsdata.h"
#include "wind.h"
#include "rsmac.proto.h"
#include "vsinterf.proto.h"
#include "vsintern.proto.h"
#include "rsinterf.proto.h"
#include "menuseg.proto.h"
#include "maclook.proto.h"
#include "wdefpatch.proto.h"	/* 931112, ragge, NADA, KTH */
#include "parse.proto.h"
#include "network.proto.h"
#include "DlogUtils.proto.h"
#include "url.proto.h"
#include "drag.proto.h"
#include "configure.proto.h"
#include "errors.proto.h"
#include "SmartTrackControl.h"
#include "sshglue.proto.h"
#include "general_resrcdefs.h"


static void calculateWindowPosition(WindRec *theScreen,Rect *whereAt, short colsHigh, short colsWide);

/* wdefpatch.c */
extern void drawicon (short id, Rect *dest);

#if GENERATINGPOWERPC
extern Boolean gHasSetWindowContentColor;
#endif

extern WindRec *screens;

extern short MaxRS;
extern RSdata *RSlocal, *RScurrent;
extern Rect	noConst;
extern short RSw,         /* last window used */
    RSa;          /* last attrib used */
extern  short **topLeftCorners;
extern short	NumberOfColorBoxes;
extern short	BoxColorItems[8];
extern RGBColor	BoxColorData[8];

long RScolors[8] =	//these are the old quickdraw constants, 
{					//only used if Telinfo->hasColorQuickDraw is false 
	blackColor,		
	redColor,		
	greenColor,			
	yellowColor,	
	blueColor,		
	magentaColor,	
	cyanColor,			
	whiteColor
};


SIMPLE_UPP(ScrollProc,ControlAction);
SIMPLE_UPP(ActiveScrollProc,ControlAction);

static void HandleDoubleClick(short w, short modifiers);

void	RSunload(void) {}


/*------------------------------------------------------------------------------*/
/* RSselect																		*/
/* Handle the mouse down in the session window.  All we know so far is that it	*/
/* is somewhere in the content window, and it is NOT an option - click.			*/
/* Double clicking now works -- SMB												*/
// And I fixed it so it works correctly.  Grrrr... - JMB
//	WARNING: Make sure RSlocal[w].selected is 1 when doing selections.  If it is
//		zero, the autoscrolling routines will seriously hose the selection drawing.
//		Heed this advice, it took me two hours to find the cause of this bug! - JMB

  /* called on a mouse-down in the text display area of the
	active window. Creates or extends the highlighted selection
	within that window, autoscrolling as appropriate if the user
	drags outside the currently visible part of the display. */
void RSselect( short w, Point pt, EventRecord theEvent)
{
	static	long 	lastClick = 0;
	static 	Point 	lastClickLoc = {0,0};
	GrafPtr tempwndo;
	Point	curr, temp;
	long	clickTime;
	short	shift = (theEvent.modifiers & shiftKey);
	RSsetConst(w);
	tempwndo = RSlocal[w].window;
	
	curr = normalize(pt, w, TRUE);
	clickTime = TickCount();
	
	if  ( ( EqualPt(RSlocal[w].anchor, curr) || EqualPt(RSlocal[w].anchor, RSlocal[w].last) )
			&&  ((clickTime - lastClick) <= GetDblTime())
			&& EqualPt(curr, lastClickLoc)) {
		/* NCSA: SB - check to see if this is a special click */
		/* NCSA: SB - It has to be in the right time interval, and in the same spot */
		curr = RSlocal[w].anchor = RSlocal[w].last = normalize(pt, w,TRUE);
		HandleDoubleClick(w, theEvent.modifiers);
		RSlocal[w].selected = 1;
		lastClick = clickTime;
		lastClickLoc = curr;
		}
	else if (theEvent.modifiers & cmdKey)
	{ // a command click means we should look for a url
		if ((RSlocal[w].selected)&(PointInSelection(curr, w))) //we have a selection already 
			HandleURL(w);
		else
		{ // we need to find the url around this pnt
			if (FindURLAroundPoint(curr, w))
				HandleURL(w);
			else
				SysBeep(1);
		}
	}	
	else {
		lastClick = clickTime;
		lastClickLoc = curr;
		if (RSlocal[w].selected) {
			if (!shift) {
			  /* unhighlight current selection */
				RSinvText(w, RSlocal[ w].anchor, RSlocal[w].last, &noConst);
			  /* start new selection */
				curr = RSlocal[w].last = RSlocal[w].anchor = normalize(pt, w,TRUE);
			}
			else {
				RSsortAnchors(w);
				if ((curr.v < RSlocal[w].anchor.v) || ((curr.v == RSlocal[w].anchor.v) && (curr.h < RSlocal[w].anchor.h))) {
					temp = RSlocal[w].anchor;
					RSlocal[w].anchor = RSlocal[w].last;
					RSlocal[w].last = temp;
					}
				}
		  }
		else
		  {
		  /* start new selection */
			curr = RSlocal[w].anchor = RSlocal[w].last = normalize(pt, w,TRUE);
			RSlocal[w].selected = 1;
			}
			
		while (StillDown())
		  {
		  /* wait for mouse position to change */
			do {
				curr = normalize(getlocalmouse(tempwndo), w,TRUE);
				} while (EqualPt(curr, RSlocal[w].last) && StillDown());
	
		  /* toggle highlight state of text between current and last mouse positions */
			RSinvText(w, curr, RSlocal[w].last, &noConst);
			RSlocal[w].last = curr;
		  } /* while */
		}

	
	if (EqualPt(RSlocal[w].anchor, RSlocal[w].last)) RSlocal[w].selected = 0;
		else RSlocal[w].selected = 1;
	SetMenusForSelection((short)RSlocal[w].selected);
  } /* RSselect */
  
  void FlashSelection(short w)
  {
	short i;
	DELAYLONG finalTick;
	for (i = 0; i < 2; i++) {
		Delay(5, &finalTick);
   	 	RSinvText(w, RSlocal[ w].anchor, RSlocal[w].last, &noConst);
		Delay(5, &finalTick);
    	RSinvText(w, RSlocal[ w].anchor, RSlocal[w].last, &noConst);
	}
  }
  Boolean PointInSelection(Point curr, short w)
  {
  	long beg_offset, end_offset, current_offset;
  	short columns;
  	columns = VSgetcols(w);
  	beg_offset = columns*RSlocal[w].anchor.v + RSlocal[w].anchor.h;
  	end_offset = columns*RSlocal[w].last.v + RSlocal[w].last.h;
  	if (beg_offset == end_offset)
  		return FALSE;
  	current_offset = columns*curr.v + curr.h;
	if ((current_offset >= beg_offset)&&(current_offset <= end_offset))
  		return TRUE;
  	else
  		return FALSE;
  }
  void RSzoom
  (
	GrafPtr window, /* window to zoom */
	short code, /* inZoomIn or inZoomOut */
	short shifted /* bring to front or not */
  )
  /* called after a click in the zoom box, to zoom a terminal window. */
  {
	WStateData	**WSDhdl;
	short		w;
	short		h, v, x1, x2, y1, y2;
	short		width, lines;			// For setting Standard State before zooming
	short		top, left;				// Ditto
	
	SetPort(window);
	w = RSfindvwind(window); /* which window is it, anyway */

	width = VSmaxwidth(w) + 1;
	lines = VSgetlines(w);
	WSDhdl = (WStateData **)((WindowPeek)window)->dataHandle;
	top = (**WSDhdl).userState.top;
	left = (**WSDhdl).userState.left;
	HLock((Handle)WSDhdl);
	SetRect(&((*WSDhdl)->stdState), left, top, RMAXWINDOWWIDTH + left,
				RMAXWINDOWHEIGHT + top);
	HUnlock((Handle)WSDhdl);
	
	/* EraseRect(&window->portRect); */
	ZoomWindow(window, code, shifted);
    EraseRect(&window->portRect);			/* BYU 2.4.15 */

  /* get new window size */
	h = window->portRect.right - window->portRect.left;
	v = window->portRect.bottom - window->portRect.top;

	RSsetsize(w, v, h, -1); /* save new size settings and update scroll bars */
  /* update the visible region of the virtual screen */
	VSgetrgn(w, &x1, &y1, &x2, &y2);
	VSsetrgn(w, x1, y1, (x1 + (h - 16 + CHO) / FWidth -1),
		(y1 + (v - 16 + CVO) / FHeight - 1));
	VSgetrgn(w, &x1, &y1, &x2, &y2);		/* Get new region */
  /* refresh the part which has been revealed, if any */
	VSredraw(w, 0, 0, x2 - x1 + 1, y2 - y1 + 1); 
  /* window contents are now completely valid */
	ValidRect(&window->portRect);
  } /* RSzoom */
  
Boolean RSisInFront(short w)
{
	if (((WindowPtr)RSlocal[w].window) == FrontWindow())
		return TRUE;
	else
		return FALSE;
}


void RSdrawlocker(short w, RgnHandle visRgn)
{
	/* draw locker icon */
	if ( RSlocal[w].left ) {
		short screenIndex = findbyVS(w);
		if ( screenIndex >= 0 && screens[screenIndex].protocol == 4 ) {
			Rect iconRect = (**RSlocal[w].left).contrlRect;
			iconRect.top += 1;
			iconRect.right = iconRect.left;
			iconRect.left = RSlocal[w].window->portRect.left;
			iconRect.bottom = iconRect.top + LOCKWIDTH;
			if ( !visRgn || RectInRgn(&iconRect, visRgn) ) {
				PlotIconID(&iconRect, kAlignNone, kTransformNone, sshicon);
			}
		}
	}
}

short
RSupdatecontent(
	GrafPtr wind,
	RgnHandle updRgn )
{
    short x1, x2, y1, y2;

	RSregnconv /* find bounds of text area needing updating */
	  (
		updRgn,
		&x1, &y1, &x2, &y2,
		RScurrent->fheight, RScurrent->fwidth
	  );

	if (x2 > x1)
	{
		VSredraw(RSfindvwind(wind), x1, y1, x2, y2); /* draw that text */
		// We must reset, less we risk looking UGLY as sin...
		BackPat(PATTERN(qd.white));
		PenPat(PATTERN(qd.black));

		if (TelInfo->haveColorQuickDraw)
		  {
			PmForeColor(0);
			PmBackColor(1);
		  }
		else
		  {
			if (!RSlocal->flipped)
			{
				ForeColor(RScolors[0]);		/* normal foreground */
				BackColor(RScolors[7]);		/* normal Background */
		  	}
		  	else	
			{
				ForeColor(RScolors[7]);		/* normal foreground */
				BackColor(RScolors[0]);		/* normal Background */
		  	}
		  }
	    RSa = -1;
	}
	return(0);
}


short RSupdate
  (
	GrafPtr wind
  )
  /* does updating for the specified window, if it's one of mine.
	Returns zero iff it is. */
  {
    short w, x1, x2, y1, y2;

    w = RSfindvwind(wind);
    if (RSsetwind(w) < 0)
		return(-1); /* not one of mine */

    BeginUpdate(wind);
	if ( !EmptyRgn(wind->visRgn) ) {
		RSupdatecontent(wind, wind->visRgn);
		DrawGrowIcon(wind);
		UpdateControls(wind, wind->visRgn);
		RSdrawlocker(w, wind->visRgn);
	}
    EndUpdate(wind);
	return(0);

  } /* RSupdate */

short RSTextSelected(short w) {		/* BYU 2.4.11 */
  return(RSlocal[w].selected);	/* BYU 2.4.11 */
}								/* BYU 2.4.11 */

void RSskip
  (
	short w,
	Boolean on
  )
  /* sets the "skip" flag for the specified window (whether ignore
	screen updates until further notice). */
  {
	RSlocal[w].skip = on;
  } /* RSskip */


/*
 *  This routine is called when the user presses the grow icon, or when the size of
 *  the window needs to be adjusted (where==NULL, modifiers==0).
 *  It limits the size of the window to a legal range.
 */

short RSsize (GrafPtr window, long *where, long modifiers)
{
	Rect	SizRect;
	long	size;
	short	w, width, lines;
	short	tw, h, v, x1, x2, y1, y2, th;
	Boolean	changeVSSize = false;
	short	screenIndex = 0;
	Boolean	screenIndexValid = false;
	short 	err = noErr;
	short	cwidth;
	short		oldlines;
	short		oldcols;

	if ((w = RSfindvwind(window)) < 0)	/* Not found */
		return (0);
	
	if (modifiers & cmdKey) return (0);
	
	screenIndexValid = (screenIndex = findbyVS(w)) != -1;

/* NONO */
	/* inverted window-resize behaviour */
	/*changeVSSize = (modifiers & optionKey) == optionKey;*/
	changeVSSize = (modifiers & optionKey) == 0;
/* NONO */
#define DONT_DEFAULT_CHANGE_VS_IF_NAWS				// JMB
	// 931112, ragge, NADA, KTH 
	// I think this is the way it should work, if there is naws available it
	// should be used by default, and option toggles behaviour.
	// Maybe it should be user configurable?
#ifndef DONT_DEFAULT_CHANGE_VS_IF_NAWS
	if(screenIndexValid && screens[screenIndex].naws) {
/* NONO */
		/* inverted window-resize behaviour */
		/*changeVSSize = (modifiers & optionKey) != optionKey;*/
		changeVSSize = (modifiers & optionKey) == 0;
/* NONO */
	}
#endif

	SetPort(window);

	width = VSmaxwidth(w) + 1; //VSmaxwidth returns one less than number of columns
	lines = VSgetlines(w);


	if (changeVSSize) {
		th = INFINITY;
		tw = INFINITY-1;
		}
	else {
		tw = RMAXWINDOWWIDTH;
		th = RMAXWINDOWHEIGHT + 1;
		}

	SetRect(&SizRect, 48, 48, tw + 1, th);
	
	if (where)											/* grow icon actions */
		{							
		if (changeVSSize) { /* 931112, ragge, NADA, KTH */
			setupForGrow(window, 1 - CHO, 1 - CVO, FWidth, FHeight);
		}
		size = GrowWindow(window, *(Point *) where, &SizRect);	/* BYU LSC */
		if (changeVSSize) { /* 931112, ragge, NADA, KTH */
			cleanupForGrow(window);
		}

		if (size != 0L)
		  {
			SizeWindow(window, size & 0xffff, (size >> 16) & 0xffff, FALSE);
			h = window->portRect.right - window->portRect.left;
			v = window->portRect.bottom - window->portRect.top;
		  }
		else return(0);							/* user skipped growing */
	  }
	else
	  {									/* just resize the window */
		h = window->portRect.right - window->portRect.left;	/* same width */
		v = (FHeight) * (VSgetlines(w));					/* new height */
		SizeWindow(window, h, v, FALSE);					/* change it */
		} 	

	RSsetsize(w, v, h, screenIndex); /* save new size settings and update scroll bars */

  /* update the visible region of the virtual screen */

	VSgetrgn(w, &x1, &y1, &x2, &y2);
	VSsetrgn(w, x1, y1, (short)((x1 + (h - 16 + CHO) / FWidth - 1)),
		(short)((y1 + (v - 16) / FHeight - 1)));
	VSgetrgn(w, &x1, &y1, &x2, &y2);		/* Get new region */

	DrawControls(window);


	if (changeVSSize) {
		
		oldlines = VSgetlines(w);
		oldcols = VSgetcols(w);

		switch (VSsetlines(w,y2 -y1 +1)) {


		case (-4000): //can't even get enough memory to put VS back to original size
			/* signal this to main program */
			return(-4);
			break;
		
		case (-3000): //no resize: unkown problems, but we put the VS back to original size
			return(0);
			break;
		case (-2000): //no resize: Memory problems, but we put the VS back to original size
			return(-2);
			break;
		default:	//Ok, we can resize; tell host
			cwidth = x2 - x1 + 1;
			if ( cwidth > 255 ) {
				cwidth = 255;
			}
			RScalcwsize(w,cwidth);
			if (screenIndexValid
			 && (oldlines != VSgetlines(w) || oldcols != VSgetcols(w)) ) {
				if (screens[screenIndex].naws) {
					SendNAWSinfo(&screens[screenIndex], cwidth, (y2-y1+1));
				}
				if (screens[screenIndex].protocol == 4) {
					ssh_glue_wresize(&screens[screenIndex]);
				}
			}
			return (0);
			break;
		}
	}

	VSredraw(w, 0, 0, x2 - x1 + 1, y2 - y1 + 1);		/* refresh the part which has been revealed, if any */
	ValidRect(&window->portRect);						/* window contents are now completely valid */

	return (0);
  } /* RSsize */
  
void RSshow( short w)		/* reveals a hidden terminal window. */
{
	VSscrn *theVS;
	if (RSsetwind(w) < 0)
		return;
	theVS = VSwhereis(w);
	RSa = -1;
	VSredraw(w, 0, 0, theVS->maxwidth, theVS->lines);
	ShowWindow(RScurrent->window);
}

Boolean RSsetcolor
	(
	short w, /* window number */
	short n, /* color entry number */
	RGBColor	Color
	)
  /* sets a new value for the specified color entry of a terminal window. */
  {
    if ( !(TelInfo->haveColorQuickDraw) || (RSsetwind(w) < 0) || (n > 15) || (n < 0))
		return(FALSE);

#if GENERATINGPOWERPC
	if ( n == 1 && gHasSetWindowContentColor ) {
		SetWindowContentColor(RScurrent->window, &Color);
	}
#endif
	SetEntryColor(RScurrent->pal, n, &Color);
	SetPort(RScurrent->window);
	InvalRect(&RScurrent->window->portRect);
	return(TRUE);
  } /* RSsetcolor */
  
  void RSsendstring
  (
	short w, /* which terminal window */
	char *ptr, /* pointer to data */
	short len /* length of data */
  )
  /* sends some data to the host along the connection associated
	with the specified window. */
  {
	short temp;

	temp = findbyVS(w);
	if (temp < 0)
		return;
	netpush(screens[temp].port);				/* BYU 2.4.18 - for Diab systems? */		
	netwrite(screens[temp].port, ptr, len);
  } /* RSsendstring */


short RSnewwindow
  (
	RectPtr 	wDims,
	short scrollback, /* number of lines to save off top */
	short width, /* number of characters per text line (80 or 132) */
	short lines, /* number of text lines */
	StringPtr name, /* window name */
	short wrapon, /* autowrap on by default */
	short fnum, /* ID of font to use initially */
	short fsiz, /* size of font to use initially */
	short showit, /* window initially visible or not */
	short goaway, /* NCSA 2.5 */
	short forcesave,		/* NCSA 2.5: force screen save */
  	short screenNumber,
  	short allowBold,
  	short colorBold,
  	short ignoreBeeps,
  	short bfnum,
  	short bfsiz,
  	short bfstyle,
  	short realbold,
  	short oldScrollback,
  	short jump,
  	short realBlink
  )
  /* creates a virtual screen and a window to display it in. */
  {
	GrafPort gp; /* temp port for getting text parameters */
	short w;

	Rect		pRect;
	short		wheight, wwidth;
	WStateData	*wstate;
	WindowPeek	wpeek;
	CTabHandle	ourColorTableHdl;
	int			i;

  /* create the virtual screen */
	w = VSnewscreen(scrollback, (scrollback != 0), /* NCSA 2.5 */
		lines, width, forcesave, ignoreBeeps, oldScrollback, jump, realBlink);	/* NCSA 2.5 */
	if (w < 0) {		/* problems opening the virtual screen -- tell us about it */
		return(-1);
	  	}
	  
	RScurrent = RSlocal + w;

	RScurrent->fnum = fnum;
	RScurrent->fsiz = fsiz;
	RScurrent->bfnum = bfnum;
	RScurrent->bfsiz = bfsiz;
	RScurrent->bfstyle = bfstyle;

	OpenPort(&gp);
	RScurrent->allowBold = allowBold;
	RScurrent->colorBold = colorBold;
	RScurrent->realbold = realbold;
	RSTextFont(fnum,fsiz,0);	/* BYU */
	TextSize(fsiz);
	RSfontmetrics();
	ClosePort(&gp);
	 
	if (wDims->bottom == 0)
		calculateWindowPosition(&screens[screenNumber],wDims,lines,width);

	if ((wDims->right - wDims->left) > RMAXWINDOWWIDTH)
		wDims->right = wDims->left + RMAXWINDOWWIDTH;
	if ((wDims->bottom - wDims->top) > RMAXWINDOWHEIGHT)
		wDims->bottom = wDims->top + RMAXWINDOWHEIGHT;
	wwidth = wDims->right - wDims->left;
	wheight = wDims->bottom - wDims->top;

	if (!RectInRgn(wDims,TelInfo->greyRegion)) //window would be offscreen
		calculateWindowPosition(&screens[screenNumber],wDims,lines,width);

  /* create the window */
	if (!TelInfo->haveColorQuickDraw) {
		RScurrent->window = NewWindow(0L, wDims, name, showit, 8,kInFront, goaway, (long)w);
		RScurrent->pal = NULL;
		if (RScurrent->window == NULL) {
			VSdestroy(w);
			return(-2);
		}
	} else {
		RGBColor scratchRGB;
		
		RScurrent->window = NewCWindow(0L, wDims, name, showit, (short)8,kInFront, goaway, (long)w);
		if (RScurrent->window == NULL) {
			VSdestroy(w);
			return(-2);
		}
		//note: the ANSI colors are in the top 8 of the palette.  The four telnet colors (settable
		//in telnet) are in the lower 4 of the palette.  These 4 are set later by a call from 
		//CreateConnectionFromParams to RSsetColor (ick, but I am not going to add 4 more params to
		//this ungodly function call (CCP 2.7)
		ourColorTableHdl = (CTabHandle) myNewHandle((long) (sizeof(ColorTable) + 
									PALSIZE * sizeof(CSpecArray)));
		if (ourColorTableHdl == NULL) 
		{
			DisposeWindow(RScurrent->window);
			VSdestroy(w);
			return(-2);
		}
		HLock((Handle) ourColorTableHdl);
			
		(*ourColorTableHdl)->ctSize = PALSIZE-1;		// Number of entries minus 1
		(*ourColorTableHdl)->ctFlags = 0;
		
		for (i=0; i <4; i++) //set the ctTable.value field to zero for our four
			(*ourColorTableHdl)->ctTable[i].value = 0;
		
		if (TelInfo->AnsiColors==NULL) 
			return(-2); //BUGG CHANGE THIS ONCE WE ARE WORKING
		
		for (i=0; i < MAXATTR*2; i++) //get the ANSI colors from the palette
		{
			GetEntryColor(TelInfo->AnsiColors, i, &scratchRGB);
			(*ourColorTableHdl)->ctTable[i+4].rgb = scratchRGB;
			(*ourColorTableHdl)->ctTable[i+4].value = 0;
		}
		
		RScurrent->pal = NewPalette(PALSIZE, ourColorTableHdl, pmCourteous, 0);
		DisposeHandle((Handle) ourColorTableHdl);
		if (RScurrent->pal == NULL) 
		{
			DisposeWindow(RScurrent->window);
			VSdestroy(w);
			return(-2);
		}
		SetPalette(RScurrent->window, RScurrent->pal, TRUE); //copy the palette to the window
	} 

	SetPort(RScurrent->window);
	SetOrigin(CHO, CVO);			/* Cheap way to correct left margin problem */

	wpeek = (WindowPeek) RScurrent->window;

	HLock(wpeek->dataHandle);
	wstate = (WStateData *) *wpeek->dataHandle;


	BlockMoveData(&wstate->userState, wDims, 8);
	pRect.top = wDims->top;
	pRect.left = wDims->left;
	pRect.right = pRect.left + RMAXWINDOWWIDTH;
	if (pRect.right > TelInfo->screenRect.right)
		pRect.right = TelInfo->screenRect.right;

	pRect.bottom = pRect.top + RMAXWINDOWHEIGHT;

/*	BlockMoveData(&wstate->stdState, &pRect, 8); uh ? */

  /* create scroll bars for window */
	pRect.top = -1 + CVO;
	pRect.bottom = wheight - 14 + CVO;
	pRect.left = wwidth - 15 + CHO;
	pRect.right = wwidth + CHO;
	RScurrent->scroll = NewControl(RScurrent->window, &pRect, "\p", FALSE,	/* BYU LSC */
		0, 0, 0, 16, 1L);

	if (RScurrent->scroll == 0L) return(-3);

	if ( screens[screenNumber].protocol == 4 ) {
		i = LOCKWIDTH + 1;
	} else {
		i = 0;
	}
	pRect.top = wheight - 15 + CVO;
	pRect.bottom = wheight + CVO;
	pRect.left = -1 + CHO + i;
	pRect.right = wwidth - 14 - i + CHO;
	RScurrent->left = NewControl(RScurrent->window, &pRect, "\p", FALSE,		/* BYU LSC */
		0, 0, 0, 16, 1L);

	if (RScurrent->left == 0L) return(-3);

	RScurrent->skip = 0; /* not skipping output initially */
	RScurrent->max = 0; /* scroll bar settings will be properly initialized by subsequent call to VSsetrgn */
	RScurrent->min = 0;
	RScurrent->current = 0;
	RScurrent->lmax = 0;
	RScurrent->lmin = 0;
	RScurrent->lcurrent = 0;
	RScurrent->selected = 0;	/* no selection initially */
	RScurrent->cursorstate = 0;	/* BYU 2.4.11 - cursor off initially */
	RScurrent->flipped = 0;		/* Initially, the color entries are not flipped */
	RSsetsize(w, wheight, wwidth, screenNumber);

	RSTextFont(RScurrent->fnum,RScurrent->fsiz,0);	/* BYU LSC */
	TextSize(RScurrent->fsiz);				/* 9 point*/
	if (!TelInfo->haveColorQuickDraw)
		TextMode(srcXor);			/* Xor mode*/
	else
		TextMode(srcCopy);

	if (wrapon)
	  /* turn on autowrap */
		VSwrite(w, "\033[?7h",5);

	return(w);
  } /* RSnewwindow */

short RSmouseintext				/* Point is in global coords */
  (
	short w,
	Point myPoint
  )
  /* is myPoint within the text-display area of the specified window. */
  {
	return
		PtInRect(myPoint, &RSlocal[w].textrect); 	/* BYU LSC */
  } /* RSmouseintext */

void RSkillwindow
  (
	short w
  )
  /* closes a terminal window. */
  {
 	WindRecPtr tw;
 	RSdata *temp = RSlocal + w;

 	tw = &screens[findbyVS(w)];
 	--((*topLeftCorners)[tw->positionIndex]); //one less window at this position

 	if (temp->pal != NULL) {
 		DisposePalette(temp->pal);		
 		temp->pal = NULL;
		}
 
	VSdestroy(w);		/* destroy the virtual screen */
	KillControls(RSlocal[w].window);  /* Get rid of those little slidy things */
	DisposeWindow(RSlocal[w].window);	/* Get rid of the actual window */
	RSlocal[w].window = 0L;
	RSw = -1;
  }

RGBColor RSgetcolor
  (
	short w, /* window number */
	short n /* color entry number */
  )
  /* gets the current value for the specified color entry of a terminal window. */
  {
	RGBColor theColor;
	GetEntryColor(RSlocal[w].pal,n,&theColor);
	return theColor;
  
  } /* RSgetcolor */

void	RShide( short w)		/* hides a terminal window. */
{
	if (RSsetwind(w) < 0)
		return;
	
	HideWindow(RScurrent->window);
}

GrafPtr RSgetwindow
  (
	short w
  )
  /* returns a pointer to the Mac window structure for the
	specified terminal window. */
  {
    return(RSlocal[w].window);
  } /* RSgetwindow */

char **RSGetTextSel
  (
	short w, /* window to look at */
	short table /* nonzero for "table" mode, i e
		replace this many (or more) spaces with a single tab. */
  )
  /* returns the contents of the current selection as a handle,
	or nil if there is no selection. */
  {
	char **charh, *charp;
	short maxwid;
	long realsiz;
	Point Anchor,Last;

	if (!RSlocal[w].selected)
		return(0L);	/* No Selection */
	maxwid = VSmaxwidth(w);
	Anchor = RSlocal[w].anchor;
	Last = RSlocal[w].last;
	
	realsiz = Anchor.v - Last.v;
	if (realsiz < 0)
		realsiz = - realsiz;
	realsiz ++;								/* lines 2,3 selected can be 2 lines */
	realsiz *= (maxwid + 2);
	charh = myNewHandle(realsiz);
	if (charh == 0L)
		return((char **) -1L);				/* Boo Boo return */
	HLock((Handle)charh);
	charp = *charh;
	realsiz = VSgettext(w, Anchor.h, Anchor.v, Last.h, Last.v,
		charp, realsiz, "\015", table);
	HUnlock((Handle)charh);
	mySetHandleSize((Handle)charh, realsiz);
	return(charh);
  }  /* RSGetTextSel */

RgnHandle RSGetTextSelRgn(short w)
{
	Rect	temp, temp2;
	Point	lb, ub;
	Point	curr;
	Point	last;
	RgnHandle	rgnH, tempRgn;

	rgnH = NewRgn();
	if (rgnH == nil) {
		return nil;
		}

	tempRgn = NewRgn();
	if (tempRgn == nil) {
		DisposeRgn(rgnH);
		return nil;
		}

	RSsetwind(w);

	curr = RSlocal[w].anchor;
	last = RSlocal[w].last;

  /* normalize coordinates with respect to visible area of virtual screen */
	curr.v -= RScurrent->topline;
	curr.h -= RScurrent->leftmarg;
	last.v -= RScurrent->topline;
	last.h -= RScurrent->leftmarg;

	if (curr.v == last.v)
	  {
	  /* highlighted text all on one line */
		if (curr.h < last.h) /* get bounds the right way round */
		  {
			ub = curr;
			lb = last;
		  }
		else
		  {
			ub = last;
			lb = curr;
		  } /* if */
		MYSETRECT /* set up rectangle bounding area to be highlighted */
		  (
			temp,
			(ub.h + 1) * RScurrent->fwidth,
			ub.v * RScurrent->fheight,
			(lb.h + 1) * RScurrent->fwidth,
			(lb.v + 1) * RScurrent->fheight
		  );
		SectRect(&temp, &noConst, &temp2); /* clip to constraint rectangle */
		RectRgn(rgnH, &temp2);
	  }
	else
	  {
	  /* highlighting across more than one line */
		if (curr.v < last.v)
			ub = curr;
		else
			ub = last;
		if (curr.v > last.v)
			lb = curr;
		else
			lb = last;
		MYSETRECT /* bounds of first (possibly partial) line to be highlighted */
		  (
			temp,
			(ub.h + 1) * RScurrent->fwidth,
			ub.v * RScurrent->fheight,
			RScurrent->width,
			(ub.v + 1) * RScurrent->fheight
		  );
		SectRect(&temp, &noConst, &temp2); /* clip to constraint rectangle */
		RectRgn(rgnH, &temp2);

		MYSETRECT /* bounds of last (possibly partial) line to be highlighted */
		  (
			temp,
			0,
			lb.v * RScurrent->fheight,
			(lb.h + 1) * RScurrent->fwidth,
			(lb.v + 1) * RScurrent->fheight
		  );
		SectRect(&temp, &noConst, &temp2); /* clip to constraint rectangle */
		RectRgn(tempRgn, &temp2);
		UnionRgn(tempRgn, rgnH, rgnH);

		if (lb.v - ub.v > 1) /* highlight extends across more than two lines */
		  {
		  /* highlight complete in-between lines */
			SetRect
			  (
				&temp,
				0,
				(ub.v + 1) * RScurrent->fheight,
				RScurrent->width,
				lb.v * RScurrent->fheight
			  );
			SectRect(&temp, &noConst, &temp2); /* clip to constraint rectangle */
			RectRgn(tempRgn, &temp2);
			UnionRgn(tempRgn, rgnH, rgnH);

		  } /* if */
	  } /* if */

	DisposeRgn(tempRgn);
	
	return rgnH;
}

short RSfindvwind
  (
	GrafPtr wind
  )
  /* returns the number of the virtual screen associated with
	the specified window, or -4 if not found. */
  {
    short
		i = 0;
    while ((RSlocal[i].window != wind) && (i < MaxRS))
		i++;
    if ((RSlocal[i].window == 0L) || (i >= MaxRS))
		return(-4);
	else
		return(i);
  } /* RSfindvwind */

void RSdeactivate
  (
	short w
  )
 /* handles a deactivate event for the specified window. */
  {
	GrafPtr port;
	GetPort(&port);
	SetPort(RSlocal[w].window);

	RSsetConst(w);

  /* deactivate the scroll bars */
	BackColor(whiteColor);

	if (RSlocal[w].scroll != 0L) {
		HideControl(RSlocal[w].scroll);
	}
	if (RSlocal[w].left != 0L) {
		HideControl(RSlocal[w].left);
	}

  /* update the appearance of the grow icon */
	DrawGrowIcon(RSlocal[w].window); 

	if (TelInfo->haveColorQuickDraw)
		PmBackColor(1);
	else
		BackColor(blackColor);
 	SetPort(port);
  } /* RSdeactivate */

void	RScursblink( short w)		/* Blinks the cursor */
{
	GrafPtr	oldwindow;
	long	now = TickCount();
	
	if (now > TelInfo->blinktime) {
		if (VSvalids(w) != 0)			/* BYU 2.4.12 */
			return;						/* BYU 2.4.12 */
		if (!VSIcursorvisible())		/* BYU 2.4.12 */
			return;						/* BYU 2.4.12 - cursor isn't visible */
	
		GetPort(&oldwindow);			/* BYU 2.4.11 */
		TelInfo->blinktime = now + 40;	/* BYU 2.4.11 */
		RSlocal[w].cursorstate ^= 1; 	/* BYU 2.4.11 */
		SetPort(RSlocal[w].window);		/* BYU 2.4.11 */
		InvertRect(&RSlocal[w].cursor);	/* BYU 2.4.11 */
		SetPort(oldwindow);				/* BYU 2.4.11 */
	}
} /* RScursblink */

void RScursblinkon						/* BYU 2.4.18 */
  (										/* BYU 2.4.18 */
    short w								/* BYU 2.4.18 */
  )										/* BYU 2.4.18 */
  /* Blinks the cursor */				/* BYU 2.4.18 */
  {										/* BYU 2.4.18 */
	if (VSvalids(w) != 0)				/* BYU 2.4.12 */
		return;							/* BYU 2.4.12 */
  	if (!VSIcursorvisible()) return;	/* Bri 970610 */
  	if (!RSlocal[w].cursorstate) {		/* BYU 2.4.18 */
		GrafPtr oldwindow;				/* BYU 2.4.18 */
		GetPort(&oldwindow);			/* BYU 2.4.18 */
		RSlocal[w].cursorstate = 1; 	/* BYU 2.4.18 */
		SetPort(RSlocal[w].window);		/* BYU 2.4.18 */
		InvertRect(&RSlocal[w].cursor);	/* BYU 2.4.18 */
		SetPort(oldwindow);				/* BYU 2.4.18 */
	}									/* BYU 2.4.18 */
  } /* RScursblink */					/* BYU 2.4.18 */

void RScursblinkoff						/* BYU 2.4.11 */
  (										/* BYU 2.4.11 */
    short w								/* BYU 2.4.11 */
  )										/* BYU 2.4.11 */
  /* Blinks the cursor */				/* BYU 2.4.11 */
  {										/* BYU 2.4.11 */
	if (VSvalids(w) != 0)				/* BYU 2.4.12 */
		return;							/* BYU 2.4.12 */
  	if (RSlocal[w].cursorstate) {		/* BYU 2.4.11 */
		GrafPtr oldwindow;				/* BYU 2.4.11 */
		GetPort(&oldwindow);			/* BYU 2.4.11 */
		RSlocal[w].cursorstate = 0; 	/* BYU 2.4.11 */
		SetPort(RSlocal[w].window);		/* BYU 2.4.11 */
		InvertRect(&RSlocal[w].cursor);	/* BYU 2.4.11 */
		SetPort(oldwindow);				/* BYU 2.4.11 */
	}									/* BYU 2.4.11 */
  } /* RScursblink */					/* BYU 2.4.11 */

void	RScprompt(short w)
  /* puts up the dialog that lets the user examine and change the color
	settings for the specified window. */
{
	short		scratchshort, ditem;
	Point		ColorBoxPoint;
	DialogPtr	dptr;
	Boolean		UserLikesNewColor;
	RGBColor	scratchRGBcolor;
	
	dptr = GetNewMySmallDialog(ColorDLOG, NULL, kInFront, (void *)ThirdCenterDialog);

	for (scratchshort = 0, NumberOfColorBoxes = 4; scratchshort < NumberOfColorBoxes; scratchshort++)
	{
		RGBColor tempColor;
		tempColor = RSgetcolor(w,scratchshort);
		BoxColorItems[scratchshort] = ColorNF + scratchshort;
		BlockMoveData(&tempColor,&BoxColorData[scratchshort], sizeof(RGBColor));
		UItemAssign( dptr, ColorNF + scratchshort, ColorBoxItemProcUPP);
	}
		
	ColorBoxPoint.h = 0;			// Have the color picker center the box on the main
	ColorBoxPoint.v = 0;			// screen
	
	ditem = 3;	
	while (ditem > 2) {
		ModalDialog(ColorBoxModalProcUPP, &ditem);
		switch (ditem) {
			case	ColorNF:	
			case	ColorNB:	
			case	ColorBF:	
			case	ColorBB:	
				if (TelInfo->haveColorQuickDraw) {
					Str255 askColorString;
					GetIndString(askColorString,MISC_STRINGS,PICK_NEW_COLOR_STRING);
					UserLikesNewColor = GetColor(ColorBoxPoint, askColorString,
						 &BoxColorData[ditem-ColorNF], &scratchRGBcolor);
					if (UserLikesNewColor)
						BoxColorData[ditem-ColorNF] = scratchRGBcolor;
					}
				break;
				
			default:
				break;
			
			} // switch
		} // while

	if (ditem == DLOGCancel) {
		DisposeDialog(dptr);
		return;
		}
		
	for (scratchshort = 0; scratchshort < NumberOfColorBoxes; scratchshort++) 
			RSsetcolor(w,scratchshort,BoxColorData[scratchshort]);
	
	/* force redrawing of entire window contents */
	SetPort(RSlocal[w].window);
	InvalRect(&RSlocal[w].window->portRect);

	DisposeDialog(dptr);
} /* RScprompt */

/*------------------------------------------------------------------------------*/
/* NCSA: SB - RScalcwsize 														*/
/* 		This routine is used to switch between 80 and 132 column mode. All that	*/	
/* 		is passed in is the RS window, and the new width.  This calculates the	*/	
/* 		new window width, resizes the window, and updates everything.  - SMB	*/
/*------------------------------------------------------------------------------*/
void RScalcwsize(short w, short width)
{
	short x1,x2,y1,y2;
	short lines;
	short resizeWidth, resizeHeight;
	Rect ourContent;
	
	RSsetwind(w);
	RScursoff(w);
	VSsetcols(w,(short)(width-1));
	VSgetrgn(w, &x1, &y1, &x2, &y2); /*get current visible region */
	x2= width-1;
	lines = VSgetlines(w);				/* NCSA: SB - trust me, you need this... */
	RScurrent->rwidth =
		RScurrent->width = (x2 - x1 + 1) * RScurrent->fwidth - CHO;
	RScurrent->rheight =
		RScurrent->height= (y2 - y1 + 1) * RScurrent->fheight; 


	if (RScurrent->rwidth > RMAXWINDOWWIDTH - 16 - CHO)
	 	 RScurrent->rwidth = RMAXWINDOWWIDTH - 16 - CHO;
	if (RScurrent->rheight > RMAXWINDOWHEIGHT - 16)
	 	 RScurrent->rheight = RMAXWINDOWHEIGHT - 16;

	ourContent = (*((WindowPeek)(RScurrent->window))->contRgn)->rgnBBox;	
	RScheckmaxwind(&ourContent,RScurrent->rwidth +16,	
				RScurrent->rheight + 16, &resizeWidth, &resizeHeight);		
	RScurrent->rwidth = resizeWidth - 16;
	RScurrent->rheight = resizeHeight - 16;
	SizeWindow
	  (
		RScurrent->window,
		RScurrent->rwidth + 16, RScurrent->rheight+16,
		FALSE
	  ); 
	RSsetsize(w, RScurrent->rheight + 16, RScurrent->rwidth + 16, -1);
	VSgetrgn(w, &x1, &y1, &x2, &y2);
	VSsetrgn(w, x1, y1,
		(short) (x1 + (RScurrent->rwidth ) / RScurrent->fwidth - 1),
		(short) (y1 + (RScurrent->rheight) / RScurrent->fheight - 1));
	VSgetrgn(w, &x1, &y1, &x2, &y2);		/* Get new region */
	
	DrawGrowIcon(RScurrent->window);
	RSdrawlocker(w, RScurrent->window->visRgn);
	VSredraw(w, 0, 0, x2 - x1 + 1, y2 - y1 + 1); /* redraw newly-revealed area, if any */
	ValidRect(&RScurrent->window->portRect); /* no need to do it again */
	DrawControls(RScurrent->window);
	
	RScursoff(w);
}

/* handles a click in a terminal window. */
short RSclick( GrafPtr window, EventRecord theEvent)
{
	ControlHandle ctrlh;
	short w, part, part2, x1, x2, y1, y2;
	Point	where = theEvent.where;
	
	short	shifted = (theEvent.modifiers & shiftKey);
	short	optioned = (theEvent.modifiers & optionKey);
	
	w = 0;
    while ((RSlocal[w].window != window) && (w < MaxRS))	//find VS 
		w++;
    if ((RSlocal[w].window == 0L) || (w >= MaxRS))
		return(-1); /* what the heck is going on here?? */


	SetPort(window);
	GlobalToLocal((Point *) &where);
	part = FindControl(where, window, &ctrlh);		/* BYU LSC */
	if (part != 0)
		switch (part)
		  {
			case inThumb:
				if (gApplicationPrefs->noLiveScrolling) // RAB BetterTelnet 2.0b2
					part2 = TrackControl(ctrlh, where, 0L);
				else part2 = SmartTrackControl(ctrlh, where, ActiveScrollProcUPP);		/* BYU LSC */
				if (part2 == inThumb)
				  {
					part = GetControlValue(ctrlh);
					if (ctrlh == RSlocal[w].scroll)
					  {
					  /* scroll visible region vertically */
						VSgetrgn(w, &x1, &y1, &x2, &y2);
						VSsetrgn(w, x1, part, x2, part + (y2 - y1));
					  }
					else
					  { /* ctrlh must be .left */
					  /* scroll visible region horizontally */
						VSgetrgn(w, &x1, &y1, &x2, &y2);
						VSsetrgn(w, part, y1, part + (x2 - x1), y2);
					  } /* if */
				  } /* if */
				break;
			case inUpButton:
			case inDownButton:
			case inPageUp:
			case inPageDown:
				part2 = TrackControl(ctrlh, where, ScrollProcUPP);	/* BYU LSC */
	/*			InvalRect(&(**RSlocal->scroll).contrlRect); */  /* cheap fix */
				break;
			default:
				break;
		  } /* switch */
	else
	{
		if ((where.h <= RSlocal[w].width)&&(where.v <= RSlocal[w].height))
		{//CCP 2.7 added the above check so that we dont do things when we are in an inactive scrollbar
			if (optioned) 
			{
			  /* send host the appropriate sequences to move the cursor
				to the specified position */
				Point x;
				x = normalize(where, w,FALSE);
				VSpossend(w, x.h, x.v, screens[scrn].echo); /* MAT--we can check here if we want to use normal */
															/* MAT--or EMACS movement. */
			}
			else if (ClickInContent(where,w)) 		/* NCSA: SB - prevent BUS error */
			{
				Boolean	dragged;
	
				(void) DragText(&theEvent, where, w, &dragged);
				if (!dragged) 
					RSselect(w, where, theEvent);
			}
		}
	  } /* if */
	return
		0;
  } /* RSclick */

void RSactivate
  (
	short w
  )
  /* handles an activate event for the specified window. */
  {
	RSsetConst(w);
  /* display the grow icon */
	DrawGrowIcon(RSlocal[w].window);
  /* and activate the scroll bars */
	if (RSlocal[w].scroll != 0L) {
		ShowControl(RSlocal[w].scroll);
	}
	if (RSlocal[w].left != 0L) {
		ShowControl(RSlocal[w].left);
	}
  } /* RSactivate */

/*--------------------------------------------------------------------------*/
/* HandleDoubleClick														*/
/* This is the routine that does the real dirty work.  Since it is not a	*/
/* true TextEdit window, we have to kinda "fake" the double clicking.  By	*/
/* this time, we already know that a double click has taken place, so check	*/
/* the chars to the left and right of our location, and select all chars 	*/
/* that are appropriate	-- SMB												*/
/*--------------------------------------------------------------------------*/
static	void HandleDoubleClick(short w, short modifiers)													
{																				
	Point	leftLoc, rightLoc, curr, oldcurr;													
	long	mySize;															
	char	theChar[5];															
	short	mode = -1, newmode, foundEnd=0;															
	RSsetConst(w);																/* get window dims */							
	leftLoc = RSlocal[w].anchor;									/* these two should be the same */							
	rightLoc = RSlocal[w].last;									
																				
	while(!foundEnd)															/* scan to the right first */														
		{																		
		mySize = VSgettext(w,rightLoc.h, rightLoc.v, rightLoc.h+1, rightLoc.v,	
			theChar,(long)1,"\015",0);									
		if(mySize ==0 || isspace(*theChar))									/* stop if not a letter */			
			foundEnd =1;														
		else rightLoc.h++;														
		}																		
																				
	foundEnd =0;																
	while(!foundEnd)															/* ...and then scan to the left */															
		{																		
		mySize = VSgettext(w,leftLoc.h-1, leftLoc.v, leftLoc.h, leftLoc.v,		
			theChar,(long)1,"\015",0);									
		if(mySize ==0 || isspace(*theChar))										/* STOP! */		
			foundEnd =1;														
		else leftLoc.h--;														
		}																		
																				
	if (leftLoc.h != rightLoc.h) {		/* we selected something */
#if 0
		RSlocal[w].anchor = leftLoc;	/* new left bound */
		RSlocal[w].last = rightLoc;		/* and a matching new right bound */
		RSlocal[w].selected = 1;		/* give me credit for the selection I just made */
		RSinvText(w, RSlocal[w].anchor,		/* time to show it off */
			RSlocal[w].last, &noConst);
#endif
		HiliteThis(w, leftLoc, rightLoc);

		if (modifiers & cmdKey)		// Possible URL selection
			HandleURL(w);
		else {																		
	
			curr.h = 0; curr.v = 0;
	
			while (StillDown()) {
			  /* wait for mouse position to change */
				do {
					oldcurr = curr;
					curr = normalize(getlocalmouse(RSlocal[w].window), w,TRUE);
					} while (EqualPt(curr, oldcurr) && StillDown());
		
				
				if ((curr.v < leftLoc.v) || ((curr.v == leftLoc.v) && (curr.h < leftLoc.h))) {
					newmode = 1;	// up
					}
				else if ((curr.v > leftLoc.v) || ((curr.v == leftLoc.v) && (curr.h > rightLoc.h))) {
					newmode = 2;	// down
					}
				else 
					newmode = -1;	// inside dbl-clicked word
					
				/* toggle highlight state of text between current and last mouse positions */
				if (mode == -1) {
					if (newmode == 2) {
						RSlocal[w].anchor = leftLoc;
						RSinvText(w, curr, rightLoc, &noConst);
						RSlocal[w].last = curr;
						}
					if (newmode == 1) {
						RSlocal[w].anchor = rightLoc;
						RSinvText(w, curr, leftLoc, &noConst);
						RSlocal[w].last = curr;
						}
					}
	
				if (mode == 1) {
					if (newmode == 2) {
						RSlocal[w].anchor = leftLoc;
						RSinvText(w, oldcurr, leftLoc, &noConst);
						RSinvText(w, rightLoc, curr, &noConst);
						RSlocal[w].last = curr;
						}
					if (newmode == -1) {
						RSlocal[w].anchor = leftLoc;
						RSinvText(w, oldcurr, leftLoc, &noConst);
						RSlocal[w].last = rightLoc;
						}
					if (newmode == mode) {
						RSinvText(w, oldcurr, curr, &noConst);
						RSlocal[w].last = curr;
						}
					}
				
				if (mode == 2) {
					if (newmode == 1) {
						RSlocal[w].anchor = rightLoc;
						RSinvText(w, oldcurr, rightLoc, &noConst);
						RSinvText(w, leftLoc, curr, &noConst);
						RSlocal[w].last = curr;
						}
					if (newmode == -1) {
						RSlocal[w].anchor = leftLoc;
						RSinvText(w, oldcurr, rightLoc, &noConst);
						RSlocal[w].last = rightLoc;
						}
					if (newmode == mode) {
						RSinvText(w, oldcurr, curr, &noConst);
						RSlocal[w].last = curr;
						}
					}
					
				mode = newmode;
				} /* while */
			}
		}	
}

Point getlocalmouse(GrafPtr wind)
  /* returns the current mouse position in coordinates local
	to the specified window. Leaves the current grafPort set
	to that window. */
  {
	Point temp;

	SetPort(wind);
	GetMouse(&temp);
	return(temp);
  } /* getlocalmouse */
  
  /*--------------------------------------------------------------------------*/
/* NCSA: SB - ClickInContent												*/
/*	This procedure is a quick check to see if the mouse click is in the		*/
/*	content region of the window.  Normalize the point to be a VS location	*/
/* 	and then see if that is larger than what it should be...				*/
/*	Used by RSClick to see if the click is in the scroll bars, or content..	*/
/*--------------------------------------------------------------------------*/
short ClickInContent(Point where,short w)				/* NCSA: SB */
{														/* NCSA: SB */
	Point x;											/* NCSA: SB */
	x = normalize(where, w,FALSE);							/* NCSA: SB */
	if (x.v >= VSgetlines(w)) return 0;					/* NCSA: SB */
	else return 1;										/* NCSA: SB */
}														/* NCSA: SB */

void RSchangefont(short w, short fnum,long fsiz)
   /*	Set (w) to font fnum, size fsiz; resize window */
{
	Rect pRect;
	short x1, x2, y1, y2, width, lines;
	short srw,srh;
	WStateData *wstate;
	WindowPeek wpeek;
	short resizeWidth, resizeHeight;		/* NCSA: SB */

    RSsetwind(w);
	srw = RScurrent->rwidth;
	srh = RScurrent->rheight;

	if (fnum != -1)
	  {
		RSTextFont(fnum,fsiz,0);	/* BYU */
		RScurrent->fnum = fnum;
	  } /* if */
	
	if (fsiz)
	  {
		TextSize(fsiz);
		RScurrent->fsiz = fsiz;
	  } /* if */
	
	RSfontmetrics();

	width = VSmaxwidth(w) + 1;
	lines = VSgetlines(w);
	
 /* resize window to preserve its dimensions in character cell units */
	
	VSgetrgn(w, &x1, &y1, &x2, &y2);	/* get visible region */
	RScurrent->rwidth =
		RScurrent->width = (x2 - x1 + 1) * RScurrent->fwidth - CHO;
	RScurrent->rheight =
		RScurrent->height= (y2 - y1 + 1) * RScurrent->fheight;

	if (RScurrent->rwidth > RMAXWINDOWWIDTH - 16 - CHO)
	 	 RScurrent->rwidth = RMAXWINDOWWIDTH - 16 - CHO;
	if (RScurrent->rheight > RMAXWINDOWHEIGHT - 16)
	 	 RScurrent->rheight = RMAXWINDOWHEIGHT - 16;
	
	RScheckmaxwind(&RScurrent->window->portRect,RScurrent->rwidth +16,	/* NCSA: SB */
		RScurrent->rheight + 16, &resizeWidth, &resizeHeight);			/* NCSA: SB */


	SizeWindow
	  (
		RScurrent->window,
		RScurrent->rwidth + 16, RScurrent->rheight+16,
		FALSE
	  ); /*  TRUE if done right */
	RSsetsize(w, RScurrent->rheight + 16, RScurrent->rwidth + 16, -1);

	wpeek = (WindowPeek) RScurrent->window;

	HLock(wpeek->dataHandle);
	wstate = (WStateData *) *wpeek->dataHandle;

	BlockMoveData(&pRect, &wstate->stdState, 8);
	pRect.right = pRect.left + RMAXWINDOWWIDTH;
	if (pRect.right > TelInfo->screenRect.right)
		pRect.right = TelInfo->screenRect.right;
	pRect.bottom = pRect.top + RMAXWINDOWHEIGHT;
	BlockMoveData(&wstate->stdState, &pRect, 8);

	VSgetrgn(w, &x1, &y1, &x2, &y2);
	VSsetrgn(w, x1, y1,
		(short) (x1 + (RScurrent->rwidth ) / RScurrent->fwidth - 1),
		(short) (y1 + (RScurrent->rheight) / RScurrent->fheight - 1));
	VSgetrgn(w, &x1, &y1, &x2, &y2);		/* Get new region */
	
	DrawGrowIcon(RScurrent->window);
	RSdrawlocker(w, RScurrent->window->visRgn);
	VSredraw(w, 0, 0, x2 - x1 + 1, y2 - y1 + 1); /* redraw newly-revealed area, if any */
	ValidRect(&RScurrent->window->portRect); /* no need to do it again */
	DrawControls(RScurrent->window);
  } /* RSchangefont */

void RSchangebold
	(
		short w,
		short allowBold,
		short colorBold,
		short inversebold
	)
{
	RSsetwind(w);
	RScurrent->allowBold = allowBold;
	RScurrent->colorBold = colorBold;
	RScurrent->bfstyle = inversebold;
	VSredraw(screens[scrn].vs,0,0,VSmaxwidth(screens[scrn].vs),VSgetlines(screens[scrn].vs)-1);
}

short RSgetfont
  (
	short w, /* which window */
	short *pfnum, /* where to return font ID */
	short *pfsiz /* where to return font size */
  )
  /* returns the current font ID and size setting for the specified window. */
  {
    if (0 > RSsetwind(w))
		return -1;
	*pfnum = RScurrent->fnum;
	*pfsiz = RScurrent->fsiz;
	return(0);
  } /* RSgetfont */

void RSfontmetrics
  (
	void
  )
  /* calculates various metrics for drawing text with selected font
	and size in current grafport into *RScurrent. */
  {
	FontInfo finforec;
	GrafPtr myGP;
	StyleField txFace;
 
	GetPort(&myGP); 
	GetFontInfo(&finforec);
	RScurrent->fascent = finforec.ascent;
	RScurrent->fheight = finforec.ascent + finforec.descent + finforec.leading /* +1 */;
	RScurrent->monospaced = (CharWidth('W') == CharWidth('i'));   
	txFace = myGP->txFace;
	TextFace( 0 );
	RScurrent->fwidth = CharWidth('W'); 
	TextFace( bold );
	RScurrent->boldislarger = ( CharWidth('W') != RScurrent->fwidth);
	TextFace( txFace );

/*	SetFontInfo(&finforec);*/
}

pascal void ScrollProc(ControlHandle control, short part)
  /* scroll-tracking routine which does continuous scrolling of visible
	 region. */
  {
	short w, kind, x1, y2, x2, y1, value;

	kind = RSfindscroll(control, &w);
	VSgetrgn(w, &x1, &y1, &x2, &y2);

	if (kind == 2)
	  { /* horizontal scroll bar */
		switch (part)
		  {
			case inUpButton:							/* Up is left */
				VSscrolleft(w, 1);
				break;
			case inDownButton:							/* Down is right */
				VSscrolright(w, 1);
				break;
			case inPageUp:
				VSscrolleft(w, x2 - x1); /* scroll a whole windowful */
				break;
			case inPageDown:
				VSscrolright(w, x2 - x1); /* scroll a whole windowful */
				break;
			default:
				break;
		  } /* switch */
	  }
	else if (kind == 1)
	  { /* vertical scroll bar */
		switch (part)
		  {
			case inUpButton:
				VSscrolback(w, 1);
				break;
			case inDownButton:
				VSscrolforward(w, 1);
				break;
			case inPageUp:
				VSscrolback(w, y2 - y1); /* scroll a whole windowful */
				break;
			case inPageDown:
				VSscrolforward(w, y2 - y1); /* scroll a whole windowful */
				break;
			default:
				break;
		  } /* switch */
	  } /* if */
  } /* ScrollProc */

pascal void ActiveScrollProc(ControlHandle control, short part)
  /* scroll-tracking routine which does continuous scrolling of visible
	 region. */
  {
	short w, kind, x1, y2, x2, y1, value;

	kind = RSfindscroll(control, &w);
	VSgetrgn(w, &x1, &y1, &x2, &y2);

	if (kind == 2)
	  { /* horizontal scroll bar */
		switch (part)
		  {
			case inThumb:
				value = GetControlValue(control);
				  /* scroll visible region horizontally */
					VSsetrgn(w, value, y1, value + (x2 - x1), y2);
					break;
			default:
				break;
		  } /* switch */
	  }
	else if (kind == 1)
	  { /* vertical scroll bar */
		switch (part)
		  {
			case inThumb:
					value = GetControlValue(control);
					  /* scroll visible region vertically */
						VSsetrgn(w, x1, value, x2, value + (y2 - y1));
						break;
			default:
				break;
		  } /* switch */
	  } /* if */
  } /* ActiveScrollProc */

void UnHiliteSelection(short w)
{
	RSinvText(w, RSlocal[ w].anchor, RSlocal[w].last, &noConst);
	RSlocal[w].selected = FALSE;
}
void HiliteThis(short w, Point begin, Point end)
{
	if (RSlocal[w].selected)
		UnHiliteSelection(w);

	RSlocal[w].anchor.v = begin.v;
	RSlocal[w].anchor.h = begin.h;
	RSlocal[w].last.v = end.v;
	RSlocal[w].last.h = end.h;
	
	RSinvText(w, RSlocal[ w].anchor, RSlocal[w].last, &noConst);
	RSlocal[w].selected = TRUE;
}

void calculateWindowPosition(WindRec *theScreen,Rect *whereAt, short colsHigh, short colsWide)
{
	
	short offset, currentCount = 0, lastIndex = 0;
	Boolean done = FALSE, tooFarRight = FALSE;
	short w = theScreen->vs;
	Boolean wideCount = 0;
	theScreen->positionIndex = 0;
	while (!done)
	{
		
		while (((*topLeftCorners)[theScreen->positionIndex] > currentCount)&& //find an empty spot
				(theScreen->positionIndex < MaxSess - 1))
			theScreen->positionIndex++;
		
		offset = ((gApplicationPrefs->StaggerWindows == TRUE) ? 
					gApplicationPrefs->StaggerWindowsOffset : 1) * (theScreen->positionIndex);
		whereAt->top = GetMBarHeight() + 25 + offset;
		whereAt->left  = 10 + offset;
		if (!tooFarRight)
			whereAt->left += (currentCount-wideCount)*gApplicationPrefs->StaggerWindowsOffset;
		else
			wideCount += currentCount - 1;
		whereAt->bottom= 30000 + offset;
		whereAt->right = 30000 + offset;
		tooFarRight = (whereAt->left + (colsWide + 1)*RScurrent->fwidth + 16 - CHO >
					TelInfo->screenRect.right);
		if (tooFarRight || (whereAt->top + (colsHigh + 1)*RScurrent->fwidth + 16 - CHO > 
										TelInfo->screenRect.bottom))
		{ // we are off screen
			if (theScreen->positionIndex == 0)
				return; //the window is bigger than the screensize; return;
			
			currentCount++; // go through again, pick spot with least number already at it
			lastIndex = theScreen->positionIndex;
			theScreen->positionIndex = 0; 		
		}
		else
			done = TRUE;
	}	
	((*topLeftCorners)[theScreen->positionIndex])++; //add our window to the number at this spot
}

void RSUpdatePalette(void)  //called when ANSI colors have changed, and we need to update each
{							//windows palette
	GrafPtr oldPort;
	int screenIndex;

	GetPort(&oldPort);
	for (screenIndex = 0; screenIndex < TelInfo->numwindows; screenIndex++)
	{
		if ((screens[screenIndex].active == CNXN_ACTIVE)||
			(screens[screenIndex].active == CNXN_OPENING))
		{
			if (screens[screenIndex].ANSIgraphics)
			{
				if (RSsetwind(screens[screenIndex].vs) >= 0)
				{
					int i;
					for (i = 0; i < 16; i++) {
							RGBColor tempColor;
							GetEntryColor(TelInfo->AnsiColors, i, &tempColor);
#if GENERATINGPOWERPC
							if ( i == 1 && gHasSetWindowContentColor ) {
								SetWindowContentColor(RScurrent->window, &tempColor);
							}
#endif
							SetEntryColor(RScurrent->pal,i+4, &tempColor); //set the new color
					}
					SetPort(screens[screenIndex].wind);
					InvalRect(&RScurrent->window->portRect); //force a redraw
				}
			}
		}
	}
	SetPort(oldPort);
}

void RSchangeboldfont(short w, short fnum)
{
    RSsetwind(w);
	RScurrent->bfnum = fnum;
	VSredraw(screens[scrn].vs,0,0,VSmaxwidth(screens[scrn].vs),VSgetlines(screens[scrn].vs)-1);
} /* RSchangeboldfont */

short RSgetboldfont
  (
	short w, /* which window */
	short *pfnum /* where to return font ID */
  )
  /* returns the current font ID and size setting for the specified window. */
  {
    if (0 > RSsetwind(w))
		return -1;
	*pfnum = RScurrent->bfnum;
	return(0);
  } /* RSgetboldfont */
