/*
 * Font.c
 *
 * Definition for a fixed size font.
 *
 */

#include <stdio.h>

#include "Frame.h"
#include "FrameInt.h"

#ifdef PSX
#include "utils.h"	// prototypes for GetWord()
#endif

#ifdef WIN32

/* The header on a font file */
typedef struct _font_savehdr
{
	UBYTE		aFileType[4];	// Will always be "font"
	UDWORD		version;		// File format version number

	UWORD		height;			// Number of pixels high
	UWORD		spaceWidth;		// Number of pixels gap to leave for a space
	UWORD		baseLine;		// drawing base line

	UWORD		numOffset;		// Number of PROP_OFFSET stored
	UWORD		numChars;		// Number of PROP_CHARS stored
} FONT_SAVEHDR;

/* The header on each character saved */
typedef struct _font_savechar
{
	UWORD	width;		// Pixel width of the character
	UWORD	pitch;		// byte width of the character data
} FONT_SAVECHAR;

#define PALETTEENTRY_BITS 8

/* The current font */
static PROP_FONT	*psCurrFont;
/* The current font colour */
static UDWORD		fontColour;

/* Set the current font */
void fontSet(PROP_FONT *psFont)
{
	ASSERT((PTRVALID(psFont, sizeof(PROP_FONT)),
		"fontSet: Invalid font pointer"));

	psCurrFont = psFont;
}

//extern PROP_FONT	*psWFont;

/* Get the current font */
PROP_FONT *fontGet(void)
{
	return psCurrFont;
}

/* Set the current font colour */
void fontSetColour(UBYTE red, UBYTE green, UBYTE blue)
{
	fontColour = screenGetCacheColour(red,green,blue);
}

/* Set the value to be poked into screen memory for font drawing.
 * The colour value used should be one returned by screenGetCacheColour.
 */
void fontSetCacheColour(UDWORD colour)
{
	fontColour = colour;
}


/* Return the index into the PROP_CHAR array for a character code.
 * If the code isn't printable, return 0 (space).
 */
UWORD fontGetCharIndex(UWORD code)
{
	UDWORD			i;
	PROP_PRINTABLE	*psOffset;

	ASSERT((PTRVALID(psCurrFont, sizeof(PROP_FONT)),
		"fontGetCharIndex: Invalid font pointer"));

	/* If there is no offset data return the code */
	if (psCurrFont->numOffset == 0)
	{
		return code;
	}

	/* Scan through the offset data */
	psOffset = psCurrFont->psOffset;
	for(i=0; i<psCurrFont->numOffset; i++)
	{
		if (code < psOffset->end)
		{
			if (psOffset->printable)
			{
				return (UWORD)(code - psOffset->offset);
			}
			else
			{
				return 0;
			}
		}
		psOffset++;
	}

	/* If we got here we went off the end of the offset table so return space */
	return 0;
}


/* Return the pixel width of a string */
UDWORD fontPixelWidth(STRING *pString)
{
	STRING	*pCurr;
	UDWORD	width;

	ASSERT((PTRVALID(psCurrFont, sizeof(PROP_FONT)),
		"fontPixelWidth: Invalid font pointer"));

	width = 0;
	for(pCurr = pString; *pCurr != '\0'; pCurr ++)
	{
		width += psCurrFont->psChars[fontGetCharIndex(*pCurr)].width;
	}

	return width;
}


/* Print text in the current font at location x,y */
void fontPrint(SDWORD x, SDWORD y, STRING *pFormat, ...)
{
	HRESULT		ddrval;
	STRING		aTxtBuff[1024];
	va_list		pArgs;
	DDSURFACEDESC2	sDDSD;
	UDWORD		endX;
	UBYTE		*pSrc,*pDest,font;
	UWORD		*p16Dest, charInd;
	UDWORD		px,py, bit;
	PROP_CHAR	*psChar;


#ifdef WIN32
	va_start(pArgs, pFormat);
	vsprintf(aTxtBuff, pFormat, pArgs);
#else
	strcpy(aTxtBuff,pFormat);	// playstation does not support parameters

//	fontSet(psWFont);	// always use this font on playstation

#endif

	ASSERT((PTRVALID(psCurrFont, sizeof(PROP_FONT)),
		"fontPrint: Invalid font pointer"));

	/* See if the string is offscreen */
	if ((y < 0) || (y >= (SDWORD)screenHeight - (SDWORD)psCurrFont->height))
	{
		return;
	}
	if (x < - (SDWORD)fontPixelWidth(aTxtBuff) ||
		(x >= (SDWORD)screenWidth))
	{
		return;
	}

	/* Clip the string to the left of the screen */
	pSrc = (UBYTE *)aTxtBuff;
	while (x < 0)
	{
		x += psCurrFont->psChars[fontGetCharIndex(*pSrc)].width;
		pSrc ++;
	}
	if (pSrc != (UBYTE *)aTxtBuff)
	{
		/* Chop off the start of the string */
		pDest = (UBYTE *)aTxtBuff;
		while (*pSrc != '\0')
		{
			*pDest++ = *pSrc++;
		}
		*pDest = '\0';
	}

	/* Clip the string to the right of the screen */
	pSrc = (UBYTE *)aTxtBuff;
	endX = x + psCurrFont->psChars[fontGetCharIndex(*pSrc)].width;
	while (*pSrc != '\0' && endX < screenWidth)
	{
		pSrc++;
		endX += psCurrFont->psChars[fontGetCharIndex(*pSrc)].width;
	}
	if (*pSrc != '\0')
	{
		/* Went off the right of the screen, clip the string */
		*pSrc = '\0';
	}


	sDDSD.dwSize = sizeof(DDSURFACEDESC2);
	ddrval = psBack->lpVtbl->Lock(psBack, NULL, &sDDSD, DDLOCK_WAIT, NULL);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE, "fontPrint: Couldn't lock back buffer"));
		return;
	}


	switch (sBackBufferPixelFormat.dwRGBBitCount)
	{
	case 8:
		/* Go through each scan line of the text */
		for(py = 0; py < psCurrFont->height; py ++)
		{
			/* Scan along the string */
			pDest = (UBYTE *)sDDSD.lpSurface + (y + py) * sDDSD.lPitch + x;
			for(pSrc = (UBYTE *)aTxtBuff; *pSrc != '\0'; pSrc++)
			{
				charInd = fontGetCharIndex(*pSrc);
				if (charInd > 0)
				{
					/* Do the scan line of the current character */
					psChar = psCurrFont->psChars + charInd;
					for(px = 0; px < psChar->width; )
					{
						font = *(psChar->pData + py * psChar->pitch + (px / 8));
						for(bit=0; bit < 8 && px < psChar->width; bit ++, px++)
						{
							if (font & (1 << bit))
							{
								*pDest = (UBYTE)fontColour;
							}
							pDest ++;
						}
					}
				}
				else
				{
					pDest += psCurrFont->spaceWidth;
				}
			}
		}
		break;
	case 16:
		/* Go through each scan line of the text */
		for(py = 0; py < psCurrFont->height; py ++)
		{
			/* Scan along the string */
			p16Dest = (UWORD *)((UBYTE *)sDDSD.lpSurface + (y + py) * sDDSD.lPitch
								+ (x<<1));
			for(pSrc = (UBYTE *)aTxtBuff; *pSrc != '\0'; pSrc++)
			{
				charInd = fontGetCharIndex(*pSrc);
				if (charInd > 0)
				{
					/* Do the scan line of the current character */
					psChar = psCurrFont->psChars + charInd;
					for(px = 0; px < psChar->width; )
					{
						font = *(psChar->pData + py * psChar->pitch + (px / 8));
						for(bit=0; bit < 8 && px < psChar->width; bit ++, px++)
						{
							if (font & (1 << bit))
							{
								*p16Dest = (UWORD)fontColour;
							}
							p16Dest ++;
						}
					}
				}
				else
				{
					p16Dest += psCurrFont->spaceWidth;
				}
			}
		}
		break;
	case 24:
		ASSERT((FALSE, "24 bit text output not implemented"));
		break;
	case 32:
		ASSERT((FALSE, "32 bit text output not implemented"));
		break;
	default:
		ASSERT((FALSE, "Unknown display pixel format"));
		break;
	}

	ddrval = psBack->lpVtbl->Unlock(psBack, sDDSD.lpSurface);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE, "fontPrint: Couldn;t unlock back buffer"));
		return;
	}
}

/* Directly print a single font character from the PROP_CHAR struct */
void fontPrintChar(SDWORD x,SDWORD y, PROP_CHAR *psChar, UDWORD height)
{
	HRESULT		ddrval;
	DDSURFACEDESC2	sDDSD;
	UBYTE		*pDest,font;
	UWORD		*p16Dest;
	UDWORD		px,py, bit;

	ASSERT((PTRVALID(psChar, sizeof(PROP_CHAR)),
		"fontPrintChar: Invalid character pointer"));
	/* The data buffer may well be bigger than this, but the test is easier this way */
	ASSERT((PTRVALID(psChar->pData, height),
		"fontPrintChar: Invalid character data pointer"));

	/* See if the character is on screen */
	if (x + psChar->width < 0 || x >= (SDWORD)screenWidth)
	{
		return;
	}
	if (y + height < 0 || y >= (SDWORD)screenHeight)
	{
		return;
	}

	/* Lock the buffer */
	sDDSD.dwSize = sizeof(DDSURFACEDESC2);
	ddrval = psBack->lpVtbl->Lock(psBack, NULL, &sDDSD, DDLOCK_WAIT, NULL);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE, "fontPrintChar: Couldn't lock back buffer"));
		return;
	}

	/* Specific char prints for the different screen modes */
	switch (sBackBufferPixelFormat.dwRGBBitCount)
	{
	case 8:
		/* Go through each scan line of the text */
		for(py = 0; py < height; py ++)
		{
			pDest = (UBYTE *)sDDSD.lpSurface + (y + py) * sDDSD.lPitch + x;
			for(px = 0; px < psChar->width; )
			{
				font = *(psChar->pData + py * psChar->pitch + (px / 8));
				for(bit=0; bit < 8 && px < psChar->width; bit ++, px++)
				{
					if (font & (1 << bit))
					{
						*pDest = (UBYTE)fontColour;
					}
					pDest ++;
				}
			}
		}
		break;
	case 16:
		/* Go through each scan line of the text */
		for(py = 0; py < height; py ++)
		{
			p16Dest = (UWORD *)((UBYTE *)sDDSD.lpSurface + (y + py) * sDDSD.lPitch
								+ (x<<1));
			for(px = 0; px < psChar->width; )
			{
				font = *(psChar->pData + py * psChar->pitch + (px / 8));
				for(bit=0; bit < 8 && px < psChar->width; bit ++, px++)
				{
					if (font & (1 << bit))
					{
						*p16Dest = (UWORD)fontColour;
					}
					p16Dest ++;
				}
			}
		}
		break;
	case 24:
		ASSERT((FALSE, "24 bit text output not implemented"));
		break;
	case 32:
		ASSERT((FALSE, "32 bit text output not implemented"));
		break;
	default:
		ASSERT((FALSE, "Unknown display pixel format"));
		break;
	}

	ddrval = psBack->lpVtbl->Unlock(psBack, sDDSD.lpSurface);
	if (ddrval != DD_OK)
	{
		ASSERT((FALSE, "screenTextOut: Couldn;t unlock back buffer"));
		return;
	}
}


/* Save font information into a file buffer */
BOOL fontSave(PROP_FONT *psFont, UBYTE **ppFileData, UDWORD *pFileSize)
{
	UDWORD			i,j;
	FONT_SAVEHDR	*psHdr;
	PROP_PRINTABLE	*psCurrP, *psSaveP;
	PROP_CHAR		*psCurrC, *psSaveC;
	UBYTE			*pData, *pSave;
	
	ASSERT((PTRVALID(psFont, sizeof(PROP_FONT)),
		"fontSave: Invalid font pointer"));
	ASSERT((PTRVALID(psFont->psOffset, sizeof(PROP_PRINTABLE)*psFont->numOffset),
		"fontSave: Invalid offset data"));
	ASSERT((PTRVALID(psFont->psChars, sizeof(PROP_CHAR) * psFont->numChars),
		"fontSave: Invalid character data"));

	/* First off calculate the size of the font file */
	*pFileSize = sizeof(FONT_SAVEHDR);
	*pFileSize += sizeof(PROP_PRINTABLE)*psFont->numOffset;
	for(i=0; i<psFont->numChars; i++)
	{
		*pFileSize += sizeof(FONT_SAVECHAR) + psFont->psChars[i].pitch * psFont->height;
	}
	*ppFileData = (UBYTE  *)MALLOC(*pFileSize);
	if (!*ppFileData)
	{
		return FALSE;
	}

	/* Store the header */
	psHdr = (FONT_SAVEHDR *)*ppFileData;
	psHdr->aFileType[0]='f';
	psHdr->aFileType[1]='o';
	psHdr->aFileType[2]='n';
	psHdr->aFileType[3]='t';
	psHdr->version = 1;
	psHdr->height = psFont->height;
	psHdr->spaceWidth = psFont->spaceWidth;
	psHdr->baseLine	= psFont->baseLine;
	psHdr->numOffset = psFont->numOffset;
	psHdr->numChars = psFont->numChars;

	/* Store the offset information */
	psCurrP = psFont->psOffset;
	psSaveP = (PROP_PRINTABLE *)(*ppFileData + sizeof(FONT_SAVEHDR));
	for(i=0; i<psFont->numOffset; i++)
	{
		*psSaveP++ = *psCurrP++;
	}

	/* Store the character data */
	psCurrC = psFont->psChars;
	pSave = (UBYTE *)psSaveP;
	for(i=0; i<psFont->numChars; i++)
	{
		/* Save the character header */
		psSaveC = (PROP_CHAR *)pSave;
		psSaveC->width = psCurrC->width;
		psSaveC->pitch = psCurrC->pitch;

		/* Save the character data */
		pSave += sizeof(UWORD)*2;
		pData = psCurrC->pData;
		for(j=0; j< (UDWORD)psCurrC->pitch * (UDWORD)psFont->height; j++)
		{
			*pSave++ = *pData++;
		}

		psCurrC++;
	}

	ASSERT((pSave == *ppFileData + *pFileSize,
		"fontSave: Incorrect file size"));

	return TRUE;
}
/* Load in a font file */
BOOL fontLoad(UBYTE *pFileData, UDWORD fileSize, PROP_FONT **ppsFont)
{
	UDWORD			i,j;
	FONT_SAVEHDR	*psHdr;
	PROP_PRINTABLE	*psCurrP, *psLoadP;
	PROP_CHAR		*psCurrC, *psLoadC;
	UBYTE			*pData, *pLoad;

	(void)fileSize;
	ASSERT((PTRVALID(pFileData, fileSize),
		"fontLoad: Invalid file data pointer"));

	*ppsFont = NULL;

	/* Check the file type and version */
	psHdr = (FONT_SAVEHDR *)pFileData;
	if (!(psHdr->aFileType[0] == 'f' &&
		  psHdr->aFileType[1] == 'o' &&
		  psHdr->aFileType[2] == 'n' &&
		  psHdr->aFileType[3] == 't'))
	{
		DBERROR(("fontLoad: incorrect file type"));
		goto error;
	}

	if (psHdr->version != 1)
	{
		DBERROR(("fontLoad: incorrect file version"));
		goto error;
	}

	*ppsFont = (PROP_FONT *)MALLOC(sizeof(PROP_FONT));
	if (!*ppsFont)
	{
		DBERROR(("fontLoad: Out of memory"));
		goto error;
	}

	/* Store the header info */
	(*ppsFont)->height = psHdr->height;
	(*ppsFont)->spaceWidth = psHdr->spaceWidth;
	(*ppsFont)->baseLine = psHdr->baseLine;
	(*ppsFont)->numOffset = psHdr->numOffset;
	(*ppsFont)->numChars = psHdr->numChars;

	/* Allocate the offset and character buffers */
	(*ppsFont)->psOffset = (PROP_PRINTABLE *)MALLOC(sizeof(PROP_PRINTABLE)
													* psHdr->numOffset);
	if (!(*ppsFont)->psOffset)
	{
		DBERROR(("fontLoad Out of memory"));
		goto error;
	}
	(*ppsFont)->psChars = (PROP_CHAR *)MALLOC(sizeof(PROP_PRINTABLE)
													* psHdr->numChars);
	if (!(*ppsFont)->psChars)
	{
		DBERROR(("fontLoad Out of memory"));
		goto error;
	}
	memset((*ppsFont)->psChars, 0, sizeof(PROP_PRINTABLE) * psHdr->numChars);

	/* Load the offset data */
	psCurrP = (*ppsFont)->psOffset;
	psLoadP = (PROP_PRINTABLE *)(pFileData + sizeof(FONT_SAVEHDR));
	for(i=0; i<(*ppsFont)->numOffset; i++)
	{
		*psCurrP++ = *psLoadP++;
	}

	/* Load the character data */
	psCurrC = (*ppsFont)->psChars;
	pLoad = (UBYTE *)psLoadP;
	for(i=0; i<(*ppsFont)->numChars; i++)
	{
		/* Load the character header */

		psLoadC = (PROP_CHAR *)pLoad;

		// These are word width ... accessed on a odd-byte boundry ... the Playstation does not like this at all ... so we have to write special code
#ifdef WIN32
		psCurrC->width = psLoadC->width;
		psCurrC->pitch = psLoadC->pitch;
#else
		psCurrC->width = GetWord(&psLoadC->width);
		psCurrC->pitch = GetWord(&psLoadC->pitch);
#endif

//		DBPRINTF(("(%dof%d) font data size %d\n",	i,(*ppsFont)->numChars,(*ppsFont)->height * psCurrC->pitch));
		psCurrC->pData = (UBYTE *)MALLOC((*ppsFont)->height * psCurrC->pitch);
		if (!psCurrC->pData)
		{
			DBERROR(("fontLoad: Out of memory (char)"));
			goto error;
		}

		/* Load the character data */
		pLoad += sizeof(UWORD)*2;
		pData = psCurrC->pData;
		for(j=0; j< (UDWORD)psCurrC->pitch * (UDWORD)(*ppsFont)->height; j++)
		{

	*pData++ = *pLoad++;
		}
		psCurrC++;
	}
	

	return TRUE;

	/* Free up any memory that may have been allocated and exit with error */
error:
	if (*ppsFont)
	{
		if ((*ppsFont)->psOffset)
		{
			FREE((*ppsFont)->psOffset);
		}
		if ((*ppsFont)->psChars)
		{
			for(i=0; i<(*ppsFont)->numChars; i++)
			{
				if ((*ppsFont)->psChars[i].pData)
				{
					FREE((*ppsFont)->psChars[i].pData);
				}
			}
			FREE((*ppsFont)->psChars);
		}
		FREE(*ppsFont);
	}
	*ppsFont = NULL;

	return FALSE;
}


/* Release all the memory used by a font */
void fontFree(PROP_FONT *psFont)
{
	PROP_CHAR	*psChar;
	UDWORD		i;

	psChar = psFont->psChars;
	for(i=0; i< psFont->numChars; i++)
	{
		FREE(psChar->pData);
		psChar++;
	}
	if (psFont->psChars)
	{
		FREE(psFont->psChars);
	}
	if (psFont->psOffset)
	{
		FREE(psFont->psOffset);
	}
	FREE(psFont);
}


#ifdef WIN32		// don't want this on the psx
UBYTE aFontData[PRINTABLE_CHARS][FONT_HEIGHT] =
{
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x18, 0x3c, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x00, 0x00, 0x00, },

	{ 0x18, 0x18, 0x3c, 0x66, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x66, 0x3c, 0x18, 0x18, 0x00, },

	{ 0x00, 0x0e, 0x1b, 0x5b, 0x6e, 0x30, 0x18, 0x0c, 0x76, 0xda, 0xd8, 0x70, 0x00, 0x00, },
	{ 0x00, 0x00, 0x1c, 0x36, 0x36, 0x1c, 0x06, 0xf6, 0x66, 0x66, 0xdc, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x18, 0x30, 0x00, },
	{ 0x00, 0x00, 0x0c, 0x18, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x6c, 0x38, 0xfe, 0x38, 0x6c, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x30, 0x18, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x06, 0x06, 0x00, 0x00, },
	{ 0x00, 0x00, 0x78, 0xcc, 0xec, 0xec, 0xcc, 0xdc, 0xdc, 0xcc, 0x78, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x30, 0x38, 0x3e, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x7e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x60, 0x38, 0x60, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x0c, 0x0c, 0x6c, 0x6c, 0x6c, 0x66, 0xfe, 0x60, 0x60, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x7e, 0x06, 0x06, 0x06, 0x3e, 0x60, 0x60, 0x30, 0x1e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x38, 0x18, 0x0c, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x7e, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0c, 0x0c, 0x0c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x6e, 0x3c, 0x76, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x30, 0x18, 0x1c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x00, 0x00, 0x00, 0x38, 0x38, 0x30, 0x18, 0x00, },
	{ 0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x30, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x7e, 0xc3, 0xc3, 0xf3, 0xdb, 0xdb, 0xf3, 0x03, 0xfe, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x18, 0x3c, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x1e, 0x36, 0x66, 0x66, 0x66, 0x66, 0x66, 0x36, 0x1e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x7e, 0x06, 0x06, 0x06, 0x3e, 0x06, 0x06, 0x06, 0x7e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x7e, 0x06, 0x06, 0x06, 0x3e, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x06, 0x06, 0x76, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x66, 0x66, 0x36, 0x36, 0x1e, 0x36, 0x36, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x7e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0xc6, 0xc6, 0xee, 0xd6, 0xd6, 0xd6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0xc6, 0xc6, 0xce, 0xde, 0xf6, 0xe6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x30, 0x60, 0x00, },
	{ 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x3e, 0x36, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x66, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x66, 0x66, 0x2c, 0x18, 0x18, 0x34, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x7e, 0x60, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x06, 0x7e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, },
	{ 0x00, 0x00, 0x06, 0x06, 0x0c, 0x0c, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x00, 0x00, },
	{ 0x00, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, },
	{ 0x18, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, },
	{ 0x1c, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x3c, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x06, 0x06, 0x06, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x60, 0x60, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x7e, 0x06, 0x06, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x78, 0x0c, 0x0c, 0x0c, 0x7e, 0x0c, 0x0c, 0x0c, 0x0c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x3e, },
	{ 0x00, 0x00, 0x06, 0x06, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x18, 0x18, 0x00, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x30, 0x30, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x1e, },
	{ 0x00, 0x00, 0x06, 0x06, 0x66, 0x66, 0x36, 0x1e, 0x36, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x7e, 0xd6, 0xd6, 0xd6, 0xd6, 0xd6, 0xc6, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x3e, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3e, 0x06, 0x06, 0x06, },
	{ 0x00, 0x00, 0x00, 0x00, 0x7c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, },
	{ 0x00, 0x00, 0x00, 0x00, 0x66, 0x76, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x7c, 0x06, 0x06, 0x3c, 0x60, 0x60, 0x3e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x0c, 0x0c, 0x7e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x78, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0xc6, 0xd6, 0xd6, 0xd6, 0xd6, 0x6c, 0x6c, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x3c, 0x18, 0x3c, 0x66, 0x66, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x30, 0x18, 0x0f, },
	{ 0x00, 0x00, 0x00, 0x00, 0x7e, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x7e, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x30, 0x18, 0x18, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x18, 0x18, 0x30, 0x00, },
	{ 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, },
	{ 0x00, 0x00, 0x0c, 0x18, 0x18, 0x18, 0x30, 0x60, 0x30, 0x18, 0x18, 0x18, 0x0c, 0x00, },
	{ 0x00, 0x00, 0x8e, 0xdb, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, },
	{ 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, },
};
#endif

#endif