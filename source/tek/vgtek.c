// BetterTelnet
// copyright 1997, 1998, 1999 Rolf Braun

// This is free software under the GNU General Public License (GPL). See the file COPYING
// which comes with the source code and documentation distributions for details.

// based on NCSA Telnet 2.7b5

/*
 *	vgtek.c by Aaron Contorer 1987 for NCSA
 *	bugfixes by Tim Krauskopf 1988 for NCSA
 *	TEK4105 support by Dave Whittington 1990 (for NCSA, of course)
 *
 *	Takes Tektronix codes as input; sends output to real graphics devices.
 *
 *	CHANGES TO MAKE:
 *	create a function to make sure a window is attached to a real window.
 *	  Calling program will call this whenever switching between active windows.
 *	Pass virtual window number to RG driver so it can call back.
 */

#include "event.proto.h"
#include "maclook.proto.h"
#include "wind.h"

#include "vsdata.h"
#include "vsinterf.proto.h"

#include "vgfont.h"
#include "vgtek.proto.h"
#include "rg0.proto.h"
#include "rgmp.proto.h"
#include "tekrgmac.proto.h"
#include "tekdefs.h"	/* NCSA: sb - all defines are now here, for easy access */
#include "tekstor.proto.h"

static RGLINK RG[TEK_DEVICE_MAX] = {
	RGMnewwin,		RGMdevname,		RGMinit,		RGMgin,
	RGMpencolor,	RGMclrscr,		RGMclose,		RGMpoint,
	RGMdrawline,	RGMinfo,		RGMpagedone,	RGMdataline,
	RGMcharmode,	RGMgmode,		RGMtmode,		RGMshowcur,
	RGMlockcur,		RGMhidecur,		RGMbell,		RGMuncover,

	RGMPnewwin,		RGMPdevname,	RGMPinit,		RG0returnshort,
	RGMPpencolor,	RG0returnshort,	RGMPclose,		RGMPpoint,
	RGMPdrawline,	RGMPinfo,		RG0oneshort,	RGMPdataline,
	RGMPcharmode,	RG0void,		RG0void,		RG0void,
	RG0void,		RG0void,		RG0oneshort,	RG0oneshort
};

extern WindRec *screens;

static fontnum(short vw, short n);
static storexy(short vw, short x, short y);
static joinup(short hi, short lo, short e);
static newcoord(short vw);
static linefeed(short vw);
static clipvec(short vw, short xa, short ya, short xb, short yb);
static	short	VGcheck(short dnum);

static struct VGWINTYPE *VGwin[MAXVG];  /* BYU - virtual window descriptors */

static char state[MAXVG],savstate[MAXVG];
	/* save state in a parallel array for speed */
static TEKSTOREP VGstore[MAXVG]; /* the store where data for this window is kept */
static char storing[MAXVG]; /* are we currently saving data from this window */
static short drawing[MAXVG]; /* redrawing or not? */

static short charxset[NUMSIZES] = {56,51,34,31,112,168};
static short charyset[NUMSIZES] = {88,82,53,48,176,264};

/*******************************************************************/

void	TEKMAINunload(void) {}

short donothing(void)	/* BYU LSC */
{
	return 0;
}

/*
 *	Set font for window 'vw' to size 'n'.
 *	Sizes are 0..3 in Tek 4014 standard.
 *	Sizes 4 & 5 are used internally for Tek 4105 emulation.
 */
static fontnum(short vw, short n)
{
	if ((n < 0) || (n >= NUMSIZES)) return(-1);
	VGwin[vw]->fontnum = n;
	VGwin[vw]->charx = charxset[n];
	VGwin[vw]->chary = charyset[n];
	return(0);
}


static storexy(short vw, short x, short y)
/* set graphics x and y position */
{
	VGwin[vw]->curx = x;
	VGwin[vw]->cury = y;
}


static joinup(short hi, short lo, short e)
/* returns the number represented by the 3 pieces */
{
#if 1
	return (((hi & 31) << 7) | ((lo & 31) << 2) | (e & 3));
#else
	return (((hi /* & 31 */ ) << 7) | ((lo /* & 31 */ ) << 2) | (e /* & 3 */));
#endif
} /* end joinup() */


static newcoord(short vw)
/*
 *	Replace x,y with nx,ny
 */
{
	VGwin[vw]->hiy = VGwin[vw]->nhiy;
	VGwin[vw]->hix = VGwin[vw]->nhix;
	VGwin[vw]->loy = VGwin[vw]->nloy;
	VGwin[vw]->lox = VGwin[vw]->nlox;
	VGwin[vw]->ey  = VGwin[vw]->ney;
	VGwin[vw]->ex  = VGwin[vw]->nex;

	VGwin[vw]->curx = joinup(VGwin[vw]->nhix,VGwin[vw]->nlox,VGwin[vw]->nex);
	VGwin[vw]->cury = joinup(VGwin[vw]->nhiy,VGwin[vw]->nloy,VGwin[vw]->ney);
}


static linefeed(short vw) 
/* 
 *	Perform a linefeed & cr (CHARTALL units) in specified window.
 */
{
/*	short y = joinup(VGwin[vw]->hiy,VGwin[vw]->loy,VGwin[vw]->ey);*/
	short y = VGwin[vw]->cury;
	short x;

	if (y > VGwin[vw]->chary) y -= VGwin[vw]->chary;
	else
	{
		y = 3119 - VGwin[vw]->chary;
		VGwin[vw]->textcol = 2048 - VGwin[vw]->textcol;
	}
	x = VGwin[vw]->textcol;
	storexy(vw,x,y);
}

/*
 *	Draw a vector in vw's window from x0,y0 to x1,y1.
 *	Zoom the vector to the current visible window,
 *	and clip it before drawing it.
 *	Uses Liang-Barsky algorithm from ACM Transactions on Graphics,
 *		Vol. 3, No. 1, January 1984, p. 7.
 *
 *  Note: since QuickDraw on the Mac already handles clipping, we
 *		  will not do any processing here.
 *  14may91dsw
 *
 */
static clipvec(short vw, short xa, short ya, short xb, short yb)
{
	short	t,b,l,r;
	struct VGWINTYPE *vp;
	long	hscale, vscale;
	
	vp = VGwin[vw];		/* BYU mod */

	hscale = INXMAX / (long) vp->winwide;
	vscale = INYMAX / (long) vp->wintall;
	
	t = vp->wintop;
	b = vp->winbot;
	l = vp->winleft;
	r = vp->winright;

	(*RG[vp->RGdevice].drawline) (vp->RGnum,
		(short) ((long)(xa - l) * INXMAX / (long) vp->winwide),
		(short) ((long)(ya- b) * INYMAX / (long) vp->wintall),
		(short) ((long)(xb - l) * INXMAX / (long) vp->winwide),
		(short) ((long)(yb- b) * INYMAX / (long) vp->wintall)
		);
} /* end clipvec() */


/*
 *	Draw a stroked character at the current cursor location.
 *	Uses simple 8-directional moving, 8-directional drawing.
 *
 *	Modified 17jul90dsw: TEK4105 character set added.
 */
short	drawc(short vw, short c) /* character to draw */
{
	short		x,y,savex,savey;
	short		strokex,strokey;
	short		n;						/* number of times to perform command */
	char	*pstroke;				/* pointer into stroke data */
	short		hmag,vmag;
	short		xdir,ydir;
	short		height;

	xdir = 1;
	ydir = 0;

	if (c == 10)
	{
		linefeed(vw);
		return(0);
	}

	if (c == 7)
	{
		(*RG[VGwin[vw]->RGdevice].bell) (VGwin[vw]->RGnum);
		TEKunstore(VGstore[vw]);
		return(0);
	}

	savey = y = VGwin[vw]->cury;
	savex = x = VGwin[vw]->curx;

	if (c == 8)
	{
		if (savex <= VGwin[vw]->textcol) return(0);
		savex -= VGwin[vw]->charx;
		if (savex < VGwin[vw]->textcol) savex = VGwin[vw]->textcol;
		VGwin[vw]->cury = savey;
		VGwin[vw]->curx = savex;
		return(0);
	}

	if (VGwin[vw]->TEKtype)
	{
		height = VGwin[vw]->TEKSize;
		if (c > 126)
		{
			height = 1;
			(*RG[VGwin[vw]->RGdevice].pencolor)(VGwin[vw]->RGnum,VGwin[vw]->pencolor);
		}
		else
			(*RG[VGwin[vw]->RGdevice].pencolor)(VGwin[vw]->RGnum,VGwin[vw]->TEKIndex);
		hmag = (height*8);
		vmag = (height*8);
		
		xdir = 0;
		switch(VGwin[vw]->TEKRot)
		{
			case 0:
				xdir = 1;
				break;
			case 90:
				ydir = 1;
				break;
			case 180:
				xdir = -1;
				break;
			case 270:
				ydir = -1;
				break;
		}
	}
	else
	{
		hmag = VGwin[vw]->charx / 10;
		vmag = VGwin[vw]->chary / 10;
	}

	if ((c < 32) || (c > 137))
		return(0);					// Is this return value correct?
	c -= 32;

	/* bounds checking to avoid crash... */
	//pstroke = (VGwin[vw]->TEKtype) ? VGTEKfont[c] : VGfont[c];
	if (VGwin[vw]->TEKtype) {
		pstroke = (c < (sizeof(VGTEKfont) / sizeof(VGTEKfont[0]))) ? VGTEKfont[c] : "";
	} else {
		pstroke = (c < (sizeof(VGfont) / sizeof(VGfont[0]))) ? VGfont[c] : "";
	}

	while (*pstroke)
	{
		strokex = x;
		strokey = y;
		n = (*(pstroke++) - 48);	/* run length */
		c = *(pstroke++);			/* direction code */

		switch(c) 	/* horizontal movement: positive = right */
		{
		case 'e': case 'd': case 'c': case 'y': case 'h': case 'n':
			x += (n * hmag) * xdir;
			y += (n * hmag) * ydir;
			break;
		case 'q': case 'a': case 'z': case 'r': case 'f': case 'v':
			x -= (n * hmag) * xdir;
			y -= (n * hmag) * ydir;
		}

		switch(c)	/* vertical movement: positive = up */
		{
		case 'q': case 'w': case 'e': case 'r': case 't': case 'y':
			x -= (n * vmag) * ydir;
			y += (n * vmag) * xdir;
			break;
		case 'z': case 'x': case 'c': case 'v': case 'b': case 'n':
			x += (n * vmag) * ydir;
			y -= (n * vmag) * xdir;
		}

		switch(c)	/* draw or move */
		{
		case 'r': case 't': case 'y': case 'f': case 'h':
		case 'v': case 'b': case 'n':
			clipvec (vw,strokex,strokey,x,y);
			break;
		}
	
	} /* end while not at end of string */

	/* Update cursor location to next char position */
	savex += VGwin[vw]->charx * xdir;
	savey += VGwin[vw]->charx * ydir;
	if ((savex < 0) || (savex > 4095) || (savey < 0) || (savey > 3119))
	{
		savex = savex < 0 ? 0 : savex > 4095 ? 4095 : savex;
		savey = savey < 0 ? 0 : savey > 3119 ? 3119 : savey;
	}

	if (VGwin[vw]->TEKtype)
		(*RG[VGwin[vw]->RGdevice].pencolor)(VGwin[vw]->RGnum,VGwin[vw]->pencolor);

	VGwin[vw]->cury = savey;
	VGwin[vw]->curx = savex;
}

/***********************************************************
************************************************************
***														 ***
**	All routines given below may be called by the user	  **
**	program.  No routines given above may be called from  **
**	the user program.									  **
***														 ***
************************************************************
***********************************************************/

/*	Initialize the whole VG environment.  Should be called ONCE
 *	at program startup before using the VG routines.
 */
void	VGinit(void)
{
	short i;

	for (i = 0; i < MAXVG; i++) {
		VGwin[i] = nil;
		}

	for (i=0; i<TEK_DEVICE_MAX; i++) (*RG[i].init)();		/* BYU LSC */
}

short VGgetVS(short theVGnum)				/* NCSA 2.5: give us the VS */
{											/* NCSA 2.5 */
	if (VGcheck(theVGnum)) {
		return -1;
		}
		
	return VGwin[theVGnum]->theVS;			/* NCSA 2.5 */
}											/* NCSA 2.5 */

static	short	VGcheck(short dnum)
{
	if ((dnum >= MAXVG) || (dnum < 0)) return -1;

	if (VGwin[dnum] == nil) return -1;

	return 0;
}

/*
 *	create a new VG window and return its number.
 *	New window will be attached to specified real device.
 *	Returns -1 if unable to create a new VG window.
 *
 *	Modified 16jul90dsw: Support selection of 4105 or 4014.
 */
short VGnewwin
  (
	short device, /* number of RG device to use */
	short theVS
  )
{
	short	vw = 0;
	short	theScrn;

	theScrn = findbyVS(theVS);
	if (theScrn < 0)
		return(-1);

	while ((vw < MAXVG) && (VGwin[vw] != nil)) vw++;
	if (vw == MAXVG)
		return(-1);
		
	VGwin[vw] = (struct VGWINTYPE *) myNewPtr(sizeof(struct VGWINTYPE));
	if (VGwin[vw] == nil) {
		return -1;
		}
			
	VGstore[vw] = newTEKstore();
	if (VGstore[vw] == nil) {
		DisposePtr((Ptr)VGwin[vw]);
		VGwin[vw] = nil;
		return -1;
		}
	
	VGwin[vw]->id = 'VGWN';
	VGwin[vw]->RGdevice = device;
	VGwin[vw]->RGnum = (*RG[device].newwin)();

	VGwin[vw]->TEKtype = screens[theScrn].tektype;	// 0 = 4014, 1 = 4105

	if (VGwin[vw]->RGnum < 0)
	{
		/* no windows available on device */
		DisposePtr((Ptr)VGwin[vw]);
		VGwin[vw] = nil;
		freeTEKstore(VGstore[vw]);
		return(-1);
	}
			
	VGwin[vw]->mode = ALPHA;
	VGwin[vw]->TEKPanel = (pointlist) NULL;
	VGwin[vw]->theVS = theVS;
	state[vw] = DONE;
	storing[vw] = TRUE;
	VGwin[vw]->textcol = 0;
	drawing[vw] = 1;
	fontnum(vw,0);
	(*RG[device].pencolor)(VGwin[vw]->RGnum,1);

	storexy(vw,0,3071);
#if 1										/* BYU LSC */
	VGzoom(vw,0,0,4095,3119);				/* BYU LSC - important */
#else										/* BYU LSC */
	VGzoom(vw,0,0,INXMAX-1,INYMAX-1);
#endif										/* BYU LSC */
	return(vw);
}

/*	Release its real graphics device and its store. */
void	VGclose(short vw)
{
	if (VGcheck(vw)) {
		return;
		}

	(*RG[VGwin[vw]->RGdevice].close)(VGwin[vw]->RGnum);
	freeTEKstore(VGstore[vw]);
	DisposePtr((Ptr)VGwin[vw]);
	VGwin[vw] = nil;
}

/*	Detach window from its current device and attach it to the
 *	specified device.  Returns negative number if unable to do so.
 *	Sample application:  switching an image from #9 to Hercules.
 *	Must redraw after calling this.
 */
short	VGdevice(short vw, short dev)
{
	short newwin;

	if (VGcheck(vw)) {
		return -1;
		}

	newwin = (*RG[dev].newwin)();
	if (newwin<0) return(newwin);	/* unable to open new window */

	(*RG[VGwin[vw]->RGdevice].close)(VGwin[vw]->RGnum);
	VGwin[vw]->RGdevice = dev;
	VGwin[vw]->RGnum = newwin;
	VGwin[vw]->pencolor = 1;
	VGwin[vw]->TEKBackground = 0;
	fontnum(vw,1);
	return(0);
}

/*	Clear the store associated with window vw.  
 *	All contents are lost.
 *	User program can call this whenever desired.
 *	Automatically called after receipt of Tek page command. */
void	VGclrstor(short vw)
{
	if (VGcheck(vw)) {
		return;
		}

	freeTEKstore(VGstore[vw]);
	VGstore[vw] = newTEKstore();
		/* Don't have to check for errors --	*/
		/* there was definitely enough memory.	*/
}

/*	Successively call the function pointed to by 'func' for each
 *	character stored from window vw.  Each character will
 *	be passed in integer form as the only parameter.  A value of -1
 *	will be passed on the last call to indicate the end of the data.
 */
void	VGdumpstore(short vw, short (*func )(short))
{
	short		data;
	TEKSTOREP	st;

	if (VGcheck(vw)) {
		return;
		}

	st = VGstore[vw];
	topTEKstore(st);
	while ((data = nextTEKitem(st)) != -1) (*func)(data);
	(*func)(-1);
}

/*	This is the main Tek emulator process.  Pass it the window and
 *	the latest input character, and it will take care of the rest.
 *	Calls RG functions as well as local zoom and character drawing
 *	functions.
 *
 *	Modified 16jul90dsw:
 *		Added 4105 support.
 */
void VGdraw(short vw, char c)			/* the latest input char */
{
	short		sn;
	char		cmd;
	char		value;
	char		goagain;	/* true means go thru the function a second time */
	char		temp[80];
	RgnHandle	PanelRgn;
	struct	VGWINTYPE *vp;
	pointlist	temppoint;

	if (VGcheck(vw)) {
		return;
	}

	vp = VGwin[vw];		/* BYU */

 	sn = findbyVS(vp->theVS);
 	if (sn < 0)
 		return;

	temp[0] = c;
	temp[1] = (char) 0;

	/*** MAIN LOOP ***/
 	do
	{
		c &= 0x7f;				/* BYU 2.4.15 */
 		cmd = (c >> 5) & 0x03;
		value = c & 0x1f;
		goagain = FALSE;

		switch(state[vw])
		{
		case HIY: /* beginning of a vector */
			vp->nhiy = vp->hiy;
			vp->nhix = vp->hix;
			vp->nloy = vp->loy;
			vp->nlox = vp->lox;
			vp->ney  = vp->ey;
			vp->nex  = vp->ex;
			
			switch(cmd)
			{
			case 0:
				if (value == 27)		/* escape sequence */
				{
					state[vw] = ESCOUT;
					savstate[vw] = HIY;
				}
				else if (value < 27)	/* ignore */
				{
					break;
				}
				else
				{
					state[vw] = CANCEL;
					goagain = TRUE;
				}
				break;
			case 1:						/* hiy */
				vp->nhiy = value;
				state[vw] = EXTRA;
				break;
			case 2:						/* lox */
				vp->nlox = value;
				state[vw] = DONE;
				break;
			case 3:						/* extra or loy */
				vp->nloy = value;
				state[vw] = LOY;
				break;
			}
			break;		
		case ESCOUT:
			if ((value != 13) && (value != 10) && (value != 27) && (value != '~'))
			{
				state[vw] = savstate[vw];		/* skip all EOL-type characters */
				goagain = TRUE;
			}
			break;
		case EXTRA:	/* got hiy; expecting extra or loy */
			switch(cmd)
			{
			case 0:
				if (value == 27)		/* escape sequence */
				{
					state[vw] = ESCOUT;
					savstate[vw] = EXTRA;
				}
				else if (value < 27)	/* ignore */
				{
					break;
				}
				else
				{
					state[vw] = DONE;
					goagain = TRUE;
				}
				break;
			case 1:						/* hix */
				vp->nhix = value;
				state[vw] = LOX;
				break;
			case 2:						/* lox */
				vp->nlox = value;
				state[vw] = DONE;
				break;
			case 3:						/* extra or loy */
				vp->nloy = value;
				state[vw] = LOY;
				break;
			}
			break;
		case LOY: /* got extra or loy; next may be loy or something else */
			switch(cmd)
			{
			case 0:
				if (value == 27)		/* escape sequence */
				{
					state[vw] = ESCOUT;
					savstate[vw] = LOY;
				}
				else if (value < 27)	/* ignore */
				{
					break;
				}
				else
				{
					state[vw] = DONE;
					goagain = TRUE;
				}
				break;
			case 1: /* hix */
				vp->nhix = value;
				state[vw] = LOX;
				break;
			case 2: /* lox */
				vp->nlox = value;
				state[vw] = DONE;
				break;
			case 3: /* this is loy; previous loy was really extra */
				vp->ney = (vp->nloy >> 2) & 3;
				vp->nex = vp->nloy & 3;
				vp->nloy = value;
				state[vw] = HIX;
				break;
			}
			break;
		case HIX:						/* hix or lox */
			switch(cmd)
			{
			case 0:
				if (value == 27)		/* escape sequence */
				{
					state[vw] = ESCOUT;
					savstate[vw] = HIX;
				}
				else if (value < 27)	/* ignore */
				{
					break;
				}
				else
				{
					state[vw] = DONE;
					goagain = TRUE;
				}
				break;
			case 1:						/* hix */
				vp->nhix = value;
				state[vw] = LOX;
				break;
			case 2:						/* lox */
				vp->nlox = value;
				state[vw] = DONE;
				break;
			}
		 	break;
	
		case LOX:						/* must be lox */
			switch(cmd)
			{
			case 0:
				if (value == 27)		/* escape sequence */
				{
					state[vw] = ESCOUT;
					savstate[vw] = LOX;
				}
				else if (value < 27)	/* ignore */
				{
					break;
				}
				else
				{
					state[vw] = DONE;
					goagain = TRUE;
				}
				break;
			case 2:
				vp->nlox = value;
				state[vw] = DONE;
				break;
			}
			break;
	
		case ENTERVEC:
			if (c == 7) vp->mode = DRAW;
			if (c < 32)
			{
				state[vw] = DONE;
				goagain = TRUE;
				vp->mode = DONE;
				break;
			}
			state[vw] = HIY;
			vp->mode = TEMPMOVE;
			vp->modesave = DRAW;
			goagain = TRUE;
			break;
		case RS:
			switch (c)
			{
			case ' ':				/* pen up */
				vp->modesave = vp->mode;
				vp->mode = TEMPMOVE;
				break;
			case 'P':				/* pen down */
				vp->mode = DRAW;
				break;
			case 'D':				/* move up */
				vp->cury++;
				break;
			case 'E':
				vp->cury++;
				vp->curx++;
				break;
			case 'A':
				vp->curx++;
				break;
			case 'I':
				vp->curx++;
				vp->cury--;
				break;
			case 'H':
				vp->cury--;
				break;
			case 'J':
				vp->curx--;
				vp->cury--;
				break;
			case 'B':
				vp->curx--;
				break;
			case 'F':
				vp->cury++;
				vp->curx--;
				break;
			case 27:
				savstate[vw] = RS;
				state[vw] = ESCOUT;
				break;
			default:
/*				storexy(vw,vp->curx,vp->cury);*/
				state[vw] = CANCEL;
				goagain = TRUE;
				break;
			}
			if (vp->mode == DRAW)
				clipvec(vw,vp->curx,vp->cury,vp->curx,vp->cury);
			break;
		case CMD0: /* *->CMD0: get 1st letter of cmd */
			switch(c)
			{
			case 29:					/* GS, start draw */
				state[vw] = DONE;
				goagain = TRUE;
				break;
			case '%':
				state[vw] = TERMSTAT;
				break;
			case '8':
				fontnum(vw,0);
				state[vw] = DONE;
				break;
			case '9':
				fontnum(vw,1);
				state[vw] = DONE;
				break;
			case ':':
				fontnum(vw,2);
				state[vw] = DONE;
				break;
			case ';':
				fontnum(vw,3);
				state[vw] = DONE;
				break;
			case 12: /* form feed = clrscr */
				if (sn >= 0 && screens[sn].tekclear) {
					VGpage(vw);
					VGclrstor(vw);
				}
				break;
			case 'L':
				state[vw] = SOMEL;
				break;
			case 'K':
				state[vw] = IGNORE;
				break;
			case 'M':
				state[vw] = SOMEM;
				break;
			case 'R':
				state[vw] = SOMER;
				break;
			case 'T':
				state[vw] = SOMET;
				break;
			case 26:
				(*RG[vp->RGdevice].gin)(vp->RGnum);
				TEKunstore(VGstore[vw]);
				TEKunstore(VGstore[vw]);
				break;
			case 10:
			case 13:
			case 27:
			case '~':
				savstate[vw] = DONE;
				state[vw] = ESCOUT;
				break;			/* completely ignore these after ESC */
			default:
				state[vw] = DONE;
			}
			break;
		case TERMSTAT:
			switch(c)
			{
				case '!':
					state[vw] = INTEGER;		/* Drop the next integer */
					savstate[vw] = DONE;
					break;
			}
			break;
		case SOMER:
			switch(c)
			{
				case 'A':
					state[vw] = INTEGER;
					savstate[vw] = VIEWAT;
					break;
				default:
					state[vw] = DONE;
			}
			break;
		case VIEWAT:
			state[vw] = INTEGER;
			savstate[vw] = VIEWAT2;
			goagain = TRUE;
			break;
		case VIEWAT2:
			vp->TEKBackground = vp->intin < 0 ? 0 : vp->intin > 7 ? 7 : vp->intin;
			state[vw] = INTEGER;
			savstate[vw] = DONE;
			goagain = TRUE;
			break;
		case SOMET:				/* Got ESC T; now handle 3rd char. */
			switch(c)
			{
			case 'C':			/* GCURSOR */
				vp->intin = 3;
				state[vw] = STARTDISC;
				break;
			case 'D':
				vp->intin = 2;
				state[vw] = STARTDISC;
				break;
			case 'F':			/* set dialog area color map */
				state[vw] = JUNKARRAY;
				break;
			case 'G':			/* set surface color map */
				state[vw] = INTEGER;
				savstate[vw] = JUNKARRAY;
				break;
			default:
				state[vw] = DONE;
			}			
			break;
		case JUNKARRAY:			/* This character is the beginning of an integer
									array to be discarded.  Get array size. */
			savstate[vw] = STARTDISC;
			state[vw] = INTEGER;
			break;					
		case STARTDISC:			/* Begin discarding integers. */
			vp->count = vp->intin + 1;
			goagain = TRUE;
			state[vw] = DISCARDING;
			break;
		case DISCARDING:
			/* We are in the process of discarding an integer array. */
			goagain = TRUE;
			if (!(--(vp->count))) state[vw] = DONE;
			else if (vp->count == 1)
			{
				state[vw] = INTEGER;
				savstate[vw] = DONE;
			}
			else
			{
				state[vw] = INTEGER;
				savstate[vw] = DISCARDING;
			}
			break;
		case INTEGER:
			if (c & 0x40)
			{
				vp->intin = c & 0x3f;
				state[vw] = INTEGER1;
			}
			else
			{
				vp->intin = c & 0x0f;
				if (!(c & 0x10)) vp->intin *= -1;
				state[vw] = savstate[vw];
			}
			break;
		case INTEGER1:
			if (c & 0x40)
			{
				vp->intin = (vp->intin << 6) | (c & 0x3f);
				state[vw] = INTEGER2;
			}
			else
			{
				vp->intin = (vp->intin << 4) | (c & 0x0f);
				if (!(c & 0x10)) vp->intin *= -1;
				state[vw] = savstate[vw];
			}
			break;
		case INTEGER2:
			vp->intin = (vp->intin << 4) | (c & 0x0f);
			if (!(c & 0x10)) vp->intin *= -1;
			state[vw] = savstate[vw];
			break;
		case IGNORE:			/* ignore next char; it's not supported */
			state[vw] = DONE;
			break;
		case IGNORE2:			/* ignore next 2 chars */
			state[vw] = IGNORE;
			break;
		case SOMEL:				/* now process 2nd letter */
			switch(c)
			{
			case 'E':					/* END PANEL 25jul90dsw */
				if (vp->TEKtype)
				{
					if (vp->TEKPanel)
					{
						if ((vp->current->x != vp->savx) ||
							(vp->current->y != vp->savy))
						{
							temppoint = (pointlist) myNewPtr(sizeof(point));
							temppoint->x = vp->savx;
							temppoint->y = vp->savy;
							temppoint->next = (pointlist) NULL;
							vp->current->next = temppoint;
						}
						temppoint = vp->current = vp->TEKPanel;
						vp->savx = vp->curx = vp->current->x;
						vp->savy = vp->cury = vp->current->y;
						vp->current = vp->current->next;
						DisposePtr((Ptr) temppoint);
						PanelRgn = NewRgn();
						OpenRgn();
						while (vp->current)
						{
							clipvec(vw,vp->curx,vp->cury,
									vp->current->x,vp->current->y);
							temppoint = vp->current;
							vp->curx = vp->current->x;
							vp->cury = vp->current->y;
							vp->current = vp->current->next;
							DisposePtr((Ptr) temppoint);
						}
						CloseRgn(PanelRgn);
						if (vp->TEKPattern <= 0)
							(*RG[vp->RGdevice].pencolor)(vp->RGnum,-vp->TEKPattern);
						PaintRgn(PanelRgn);
				/*		if (vp->TEKOutline) 
							FrameRgn(PanelRgn); */
						DisposeRgn(PanelRgn);
						(*RG[vp->RGdevice].pencolor)(vp->RGnum,vp->pencolor);
						vp->TEKPanel = (pointlist) NULL;
						vp->curx = vp->savx;
						vp->cury = vp->savy;
					}
				}
				state[vw] = DONE;
				break;
			case 'F':					/* MOVE */
				vp->modesave = vp->mode;
				vp->mode = TEMPMOVE;
				state[vw] = HIY;
				break;
			case 'G':					/* DRAW */
				vp->modesave = vp->mode;
				vp->mode = TEMPDRAW;
				state[vw] = HIY;
				break;
			case 'H':					/* MARKER */
				vp->modesave = vp->mode;
				vp->mode = TEMPMARK;
				state[vw] = HIY;
				break;
			case 'I':					/* DAINDEX 24jul90dsw*/
				state[vw] = STARTDISC;
				vp->intin = 3;
				break;
			case 'L':
				state[vw] = INTEGER;
				savstate[vw] = DONE;
				break;
			case 'P':					/* BEGIN PANEL 17jul90dsw */
				if (vp->TEKtype)		/* 4105 only */
				{
					state[vw] = HIY;
					vp->mode = PANEL;
				}
				else
					state[vw] = DONE;
				break;
			case 'T':					/* GTEXT 17jul90dsw */
				if (vp->TEKtype)		/* 4105 only */
				{
					savstate[vw] = GTEXT;
					state[vw] = INTEGER;
				}
				else
					state[vw] = DONE;
				break;
			default:
				state[vw] = DONE;
			}
			break;
		case SOMEM:
			switch(c)
			{
			case 'C':					/* set graphtext size */
				savstate[vw] = GTSIZE0;
				state[vw] = INTEGER;
				break;
			case 'L':					/* set line index */
				savstate[vw] = COLORINT;
				state[vw] = INTEGER;
				break;
			case 'M':					/* MARKERTYPE 17jul90dsw */
				if (vp->TEKtype)
				{
					savstate[vw] = MARKER;
					state[vw] = INTEGER;
				}
				else
					state[vw] = DONE;
				break;
			case 'N':					/* GTPATH 17jul90dsw */
				if (vp->TEKtype)
				{
					savstate[vw] = GTPATH;
					state[vw] = INTEGER;
				}
				else
					state[vw] = DONE;
				break;
			case 'P':					/* FillPattern 17jul90dsw */
				if (vp->TEKtype)
				{
					savstate[vw] = FPATTERN;
					state[vw] = INTEGER;
				}
				else
					state[vw] = DONE;
				break;
			case 'R':					/* GTROT 17jul90dsw */
				if (vp->TEKtype)
				{
					savstate[vw] = GTROT;
					state[vw] = INTEGER;
				}
				else
					state[vw] = DONE;
				break;
			case 'T':					/* GTINDEX 17jul90dsw */
				if (vp->TEKtype)
				{
					savstate[vw] = GTINDEX;
					state[vw] = INTEGER;
				}
				else
					state[vw] = DONE;
				break;
			case 'V':
				if (vp->TEKtype)
				{
					state[vw] = INTEGER;
					savstate[vw] = DONE;
				}
				else
					state[vw] = DONE;
				break;
			default:
				state[vw] = DONE;
			}
			break;
		case COLORINT:				/* set line index; have integer */
			vp->pencolor = vp->intin;
			(*RG[vp->RGdevice].pencolor)(vp->RGnum,vp->intin);
			state[vw] = CANCEL;
			goagain = TRUE;			/* we ignored current char; now process it */
			break;
		case GTSIZE0:				/* discard the first integer; get the 2nd */
			state[vw] = INTEGER;	/* get the important middle integer */
			savstate[vw] = GTSIZE1;
			goagain = TRUE;
			break;
		case GTSIZE1:				/* integer is the height */
			if (vp->TEKtype)
			{
				if (vp->intin < 88) vp->TEKSize = 1;
				else if ((vp->intin > 87) && (vp->intin < 149)) vp->TEKSize = 2;
				else if ((vp->intin > 148) && (vp->intin < 209)) vp->TEKSize = 3;
				else if (vp->intin > 208) vp->TEKSize = vp->intin / 61;
				VGwin[vw]->charx = (vp->TEKSize * 52);
				VGwin[vw]->chary = (vp->TEKSize * 64);
			}
			else
			{
				if (vp->intin < 88)
					fontnum(vw,0);
				else if (vp->intin < 149)
					fontnum(vw,4);
				else
					fontnum(vw,5);
			}
			state[vw] = INTEGER;	/* discard last integer */
			savstate[vw] = DONE;
			goagain = TRUE;
			break;
		case GTEXT:					/* TEK4105 GraphText output.  17jul90dsw */
			if (vp->intin > 0)
			{
				drawc(vw,(short) c);	/* Draw the character */
				vp->intin--;		/* One less character in the string... */
			}
			else
			{
				goagain = TRUE;
				state[vw] = DONE;
				newcoord(vw);
			}
			break;
		case MARKER:				/* TEK4105 Set marker type.  17jul90dsw */
			vp->TEKMarker = vp->intin;
			if (vp->TEKMarker > 10) vp->TEKMarker = 10;
			if (vp->TEKMarker <  0) vp->TEKMarker = 0;
			state[vw] = DONE;
			goagain = TRUE;
			break;
		case GTPATH:
			vp->TEKPath = vp->intin;
			state[vw] = DONE;
			goagain = TRUE;
			break;
		case FPATTERN:
			vp->TEKPattern = (vp->intin <  -7) ?  -7 :
							 (vp->intin > 149) ? 149 : vp->intin;
			state[vw] = DONE;
			goagain = TRUE;
			break;
		case GTROT:
			vp->TEKRot = vp->intin;
			state[vw] = INTEGER;
			savstate[vw] = GTROT1;
			goagain = TRUE;
			break;
		case GTROT1:
			vp->TEKRot = (vp->TEKRot) << (vp->intin);
			vp->TEKRot = ((vp->TEKRot + 45) / 90) * 90;
			state[vw] = DONE;
			goagain = TRUE;
			break;
		case GTINDEX:
			vp->TEKIndex = (vp->intin < 0) ? 0 : (vp->intin > 7) ? 7 : vp->intin;
			state[vw] = DONE;
			goagain = TRUE;
			break;
		case PANEL:
			vp->TEKOutline = (vp->intin == 0) ? 0 : 1;
			temppoint = (pointlist) myNewPtr(sizeof(point));
			if (vp->TEKPanel)
			{
				if ((vp->current->x != vp->savx) && (vp->current->y != vp->savy))
				{
					temppoint->x = vp->savx;
					temppoint->y = vp->savy;
					vp->current->next = temppoint;
					vp->current = temppoint;
					temppoint = (pointlist) myNewPtr(sizeof(point));
				}
				vp->current->next = temppoint;
			}
			else vp->TEKPanel = temppoint;
			vp->current = temppoint;
			vp->current->x = vp->savx = joinup(vp->nhix,vp->nlox,vp->nex);
			vp->current->y = vp->savy = joinup(vp->nhiy,vp->nloy,vp->ney);
			vp->current->next = (pointlist) NULL;
			state[vw] = INTEGER;
			savstate[vw] = PANEL;
			vp->mode = DONE;
			newcoord(vw);
			state[vw] = DONE;
			goagain = TRUE;
			break;
		case DONE:					/* ready for anything */
			switch(c)
			{
			case 31:				/* US - enter ALPHA mode */
				vp->mode = ALPHA; 
				state[vw] = CANCEL;
				break;
			case 30:
				state[vw] = RS;
				break;
			case 28:
					vp->mode = MARK;
					state[vw] = HIY;
				break;
			case 29:				/* GS - enter VECTOR mode */
				state[vw] = ENTERVEC;
				break;
			case 27:
				state[vw] = CMD0;
				break;
			default:
				if (vp->mode == ALPHA)
				{
					state[vw] = DONE;
					if (! vp->TEKtype)
						drawc(vw,(short) c);
					else
					{
						VSwrite(vp->theVS,&c,1);
						TEKunstore(VGstore[vw]);
					}
					return;
				}
				else if ((vp->mode == DRAW) && cmd)
				{
					state[vw] = HIY;
					goagain = TRUE;
				}
				else if ((vp->mode == MARK) && cmd)
				{
					state[vw] = HIY;
					goagain = TRUE;
				}
				else if ((vp->mode == DRAW) && ((c == 13) || (c == 10)))
				{
					/* break drawing mode on CRLF */
					vp->mode = ALPHA; 
					state[vw] = CANCEL;
				}
				else
				{
					state[vw] = DONE;			/* do nothing */
					return;
				}
			}
		}
	
		if (state[vw] == DONE)
		{
			if (vp->mode == PANEL)
			{
				vp->mode = DONE;
				state[vw] = INTEGER;
				savstate[vw] = PANEL;
			}
			else if ((vp->TEKPanel) && ((vp->mode == DRAW) || (vp->mode == TEMPDRAW)
					|| (vp->mode == MARK) || (vp->mode == TEMPMARK) ||
						(vp->mode == TEMPMOVE)))
			{
				temppoint = (pointlist) myNewPtr(sizeof(point));
				vp->current->next = temppoint;
				vp->current = temppoint;
				vp->current->x = joinup(vp->nhix,vp->nlox,vp->nex);
				vp->current->y = joinup(vp->nhiy,vp->nloy,vp->ney);
				vp->current->next = (pointlist) NULL;
				if ((vp->mode == TEMPDRAW) || (vp->mode == TEMPMOVE) ||
					(vp->mode == TEMPMARK))
					vp->mode = vp->modesave;
				newcoord(vw);
			}
			else if (vp->mode == TEMPMOVE)
			{
				vp->mode = vp->modesave;
				newcoord(vw);
			}
			else if ((vp->mode == DRAW) || (vp->mode == TEMPDRAW))
			{
				clipvec(vw,vp->curx,vp->cury,
					joinup(vp->nhix,vp->nlox,vp->nex),
					joinup(vp->nhiy,vp->nloy,vp->ney));
				newcoord(vw);
				if (vp->mode == TEMPDRAW) vp->mode = vp->modesave;
			}
			else if ((vp->mode == MARK) || (vp->mode == TEMPMARK))
			{
				newcoord(vw);
				if (vp->TEKtype) drawc(vw,127 + vp->TEKMarker);
				newcoord(vw);
				if (vp->mode == TEMPMARK) vp->mode = vp->modesave;
			}
		}

		if (state[vw] == CANCEL) state[vw] = DONE;
	} while (goagain);
	return;
}

/*  Clear screen and have a few other effects:
 *	- Return graphics to home position (0,3071)
 *	- Switch to alpha mode
 *	This is a standard Tek command; don't look at me.
 */
void VGpage(short vw)
{
	if (VGcheck(vw)) {
		return;
		}

	if (VGwin[vw]->TEKtype)
		(*RG[VGwin[vw]->RGdevice].pencolor)(VGwin[vw]->RGnum,VGwin[vw]->TEKBackground);
	else
		(*RG[VGwin[vw]->RGdevice].pencolor)(VGwin[vw]->RGnum,0);
	(*RG[VGwin[vw]->RGdevice].clrscr)(VGwin[vw]->RGnum);
	(*RG[VGwin[vw]->RGdevice].pencolor)(VGwin[vw]->RGnum,1);
	VGwin[vw]->mode = ALPHA;
	state[vw] = DONE;
	VGwin[vw]->textcol = 0;
	fontnum(vw,0);
	storexy(vw,0,3071);
}

/*	Redraw window 'vw' in pieces to window 'dest'.
 *	Must call this function repeatedly to draw whole image.
 *	Only draws part of the image at a time, to yield CPU power.
 *	Returns 0 if needs to be called more, or 1 if the image
 *	is complete.  Another call would result in the redraw beginning again.
 *	User should clear screen before beginning redraw.
 */
short VGpred(short vw, short dest)
{
	short		data;
	TEKSTOREP	st;
	short		count = 0;
	
	if (VGcheck(vw)) {
		return -1;
		}

	st = VGstore[vw];
	
	if (drawing[vw])		/* wasn't redrawing */
	{
		topTEKstore(VGstore[vw]);
		drawing[vw] = 0;	/* redraw incomplete */
	}

	while (++count < PREDCOUNT && ((data = nextTEKitem(st)) != -1))
		VGdraw(dest,data);

	if (data == -1) drawing[vw] = 1; 	/* redraw complete */
	return(drawing[vw]);
}

/*	Abort VGpred redrawing of specified window.
	Must call this routine if you decide not to complete the redraw. */
void VGstopred(short vw)
{
	if (VGcheck(vw)) {
		return;
		}

	drawing[vw] = 1;
}

/*	Redraw the contents of window 'vw' to window 'dest'.
 *	Does not yield CPU until done.
 *	User should clear the screen before calling this, to avoid 
 *	a messy display. */
void VGredraw(short vw, short dest)
{
	short	data;

	if (VGcheck(vw)) {
		return;
		}

	topTEKstore(VGstore[vw]);
	while ((data = nextTEKitem(VGstore[vw])) != -1) VGdraw(dest,data);
}
 
/*	Send interesting information about the virtual window down to
 *	its RG, so that the RG can make VG calls and display zoom values
 */
void	VGgiveinfo(short vw)
{
	if (VGcheck(vw)) {
		return;
		}

	(*RG[VGwin[vw]->RGdevice].info)(VGwin[vw]->RGnum,
		vw,
		VGwin[vw]->winbot,
		VGwin[vw]->winleft,
		VGwin[vw]->wintop,
		VGwin[vw]->winright);
}

/*	Set new borders for zoom/pan region.
 *	x0,y0 is lower left; x1,y1 is upper right.
 *	User should redraw after calling this.
 */
void	VGzoom(short vw, short x0, short y0, short x1, short y1)
{
	if (VGcheck(vw)) {
		return;
		}

	VGwin[vw]->winbot = y0;
	VGwin[vw]->winleft = x0;
	VGwin[vw]->wintop = y1;
	VGwin[vw]->winright = x1;
	VGwin[vw]->wintall = y1 - y0 + 1;
	VGwin[vw]->winwide = x1 - x0 + 1;
	VGgiveinfo(vw);
}

void	VGwhatzoom(short vw, short *px0, short *py0, short *px1, short *py1)
{
	if (VGcheck(vw)) {
		return;
		}

	*py0 = VGwin[vw]->winbot;
	*px0 = VGwin[vw]->winleft;
	*py1 = VGwin[vw]->wintop;
	*px1 = VGwin[vw]->winright;
}

/*	Set zoom/pan borders for window 'dest' equal to those for window 'src'.
 *	User should redraw window 'dest' after calling this.
 */
void	VGzcpy(short src, short dest)
{
	VGzoom(dest,VGwin[src]->winleft, VGwin[src]->winbot,
	VGwin[src]->winright, VGwin[src]->wintop);
}

/*	Close virtual window.
/*	Draw the data pointed to by 'data' of length 'count'
 *	on window vw, and add it to the store for that window.
 *	This is THE way for user program to pass Tektronix data.
 */
short	VGwrite(short vw, char *data, short count)
{
	char *c = data;
	char *end = &(data[count]);
	char storeit;

	if (VGcheck(vw)) {
		return -1;
		}

	storeit = storing[vw];
	
	while (c != end)
	{
		if (*c == 24)				/* ASC CAN character */
			return(c-data+1);
		if (storeit) addTEKstore(VGstore[vw],*c);
		VGdraw(vw,*c++);

	}
	return(count);
}

/*	Return a pointer to a human-readable string
 *	which describes the specified real device
 */
char *	VGrgname(short rgdev)
{
	return(*RG[rgdev].devname)();
}

/*	Put the specified real device into text mode */
void	VGtmode(short rgdev)
{
	(*RG[rgdev].tmode)();
}

/*	Translate data for output as GIN report.
 *
 *	User indicates VW number and x,y coordinates of the GIN cursor.
 *	Coordinate space is 0-4095, 0-4095 with 0,0 at the bottom left of
 *	the real window and 4095,4095 at the upper right of the real window.
 *	'c' is the character to be returned as the keypress.
 *	'a' is a pointer to an array of 5 characters.  The 5 chars must
 *	be transmitted by the user to the remote host as the GIN report. */
void VGgindata( short vw,
	unsigned short x,		/* NCSA: SB - UNSIGNED data */
	unsigned short y,		/* NCSA: SB - "          "  */
	char c, char *a)
{
	long	x2,y2;
	
	if (VGcheck(vw)) {
		return;
		}

	x2 = ((x * VGwin[vw]->winwide) / INXMAX + VGwin[vw]->winleft) >> 2;
	y2 = ((y * VGwin[vw]->wintall) / INYMAX + VGwin[vw]->winbot) >> 2;

	a[0] = c;
	a[1] = 0x20 | ((x2 & 0x03E0) >> 5);
	a[2] = 0x20 | (x2 & 0x001F);
	a[3] = 0x20 | ((y2 & 0x03E0) >> 5);
	a[4] = 0x20 | (y2 & 0x001F);
}
