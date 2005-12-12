// qtractorAudioClip.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorAudioClip.h"
#include "qtractorAudioFile.h"
#include "qtractorAudioBuffer.h"
#include "qtractorAudioEngine.h"
#include "qtractorAudioPeak.h"

#include "qtractorSessionDocument.h"

#include <qfileinfo.h>
#include <qpainter.h>


//----------------------------------------------------------------------
// class qtractorAudioClip -- Audio file/buffer clip.
//

// Constructor.
qtractorAudioClip::qtractorAudioClip ( qtractorTrack *pTrack )
	: qtractorClip(pTrack)
{
	m_pPeak = NULL;
	m_pBuff = new qtractorAudioBuffer(pTrack->session()->sampleRate());
}


// Destructor.
qtractorAudioClip::~qtractorAudioClip (void)
{
	if (m_pPeak)
		delete m_pPeak;
	if (m_pBuff)
		delete m_pBuff;
}


// The main use method.
bool qtractorAudioClip::open ( const QString& sFilename )
{
	if (track() == NULL)
		return false;
	if (track()->session() == NULL)
		return false;
	if (track()->session()->audioPeakFactory() == NULL)
		return false;

	if (m_pPeak)
		delete m_pPeak;

	m_pPeak = track()->session()->audioPeakFactory()->createPeak(sFilename,
		track()->session()->sampleRate());
	if (m_pPeak == NULL)
		return false;

	if (!m_pBuff->open(sFilename)) {
		delete m_pPeak;
		m_pPeak = NULL;
		return false;
	}

	qtractorClip::setClipName(QFileInfo(sFilename).baseName());
	qtractorClip::setClipLength(m_pBuff->frames());

	return true;
}


// Intra-clip frame positioning.
void qtractorAudioClip::seek ( unsigned long iOffset )
{
	m_pBuff->seek(iOffset);
}


// Reset clip state.
void qtractorAudioClip::reset (void)
{
	m_pBuff->reset();
}


// Audio clip special process cycle executive.
void qtractorAudioClip::process ( float fGain,
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (track()->bus());
	if (pAudioBus == NULL)
		return;

	// Get the next bunch from the clip...
	unsigned long iClipStart = clipStart();
	unsigned long iClipEnd   = iClipStart + clipLength();
	if (iFrameStart < iClipStart && iFrameEnd > iClipStart) {
		m_pBuff->readMix(pAudioBus->out(), pAudioBus->channels(),
			iFrameEnd - iClipStart, iClipStart - iFrameStart, fGain);
	} else if (iFrameStart >= iClipStart && iFrameStart < iClipEnd) {
		m_pBuff->readMix(pAudioBus->out(), pAudioBus->channels(),
			iFrameEnd - iFrameStart, 0, fGain);
	}
}


// Audio clip paint method.
void qtractorAudioClip::drawClip ( QPainter *pPainter, const QRect& clipRect,
	unsigned long iClipOffset )
{
	qtractorSession *pSession = track()->session();
	if (pSession == NULL)
		return;

	// Fill clip background...
#if 0
	pPainter->fillRect(clipRect, track()->background());
#else
	pPainter->setPen(track()->background().dark());
	pPainter->setBrush(track()->background());
	pPainter->drawRect(clipRect);
#endif

	// Cache some peak data...
	if (m_pPeak == NULL)
		return;
	unsigned int iPeriod = m_pPeak->period();
	if (iPeriod < 1)
		return;
	unsigned int iChannels = m_pPeak->channels();
	if (iChannels < 1)
		return;

	// Draw peak chart...
	unsigned long iframe = (iClipOffset / iPeriod);
	unsigned long nframes
		= (pSession->frameFromPixel(clipRect.width()) / iPeriod) + 2;
	if (nframes > m_pPeak->frames())
		nframes = m_pPeak->frames();
	qtractorAudioPeakFrame *pframes
		= new qtractorAudioPeakFrame [iChannels * nframes];

	// Grab them in...
	m_pPeak->getPeak(pframes, iframe, nframes);

	int h1 = (clipRect.height() / iChannels);
	int h2 = (h1 / 2);
	int n, i, j, x, y;
	int kdelta = (pSession->frameFromPixel(1) / iPeriod);

	if (kdelta < 1) {
		// Polygon mode...
		int ymax, ymin, yrms;
		unsigned int iPolyPoints = (nframes << 1);
		QPointArray **pPolyMax = new QPointArray* [iChannels];
		QPointArray **pPolyRms = new QPointArray* [iChannels];
		for (i = 0; i < (int) iChannels; i++) {
			pPolyMax[i] = new QPointArray(iPolyPoints);
			pPolyRms[i] = new QPointArray(iPolyPoints);
		}
		// Build polygonal vertexes...
		j = 0;
		for (n = 0; n < (int) nframes; n++) {
			y = clipRect.y() + h2;
			x = clipRect.x() + pSession->pixelFromFrame(n * iPeriod);
			for (i = 0; i < (int) iChannels; i++, j++) {
				ymax = (h2 * pframes[j].peakMax) / 255;
				ymin = (h2 * pframes[j].peakMin) / 255;
				yrms = (h2 * pframes[j].peakRms) / 255;
				pPolyMax[i]->setPoint(n, x, y + ymax);
				pPolyMax[i]->setPoint(iPolyPoints - n - 1, x, y - ymin);
				pPolyRms[i]->setPoint(n, x, y + yrms);
				pPolyRms[i]->setPoint(iPolyPoints - n - 1, x, y - yrms);
				y += h1;
			}
		}
		// Draw (and free) the polygon...
		for (i = 0; i < (int) iChannels; i++) {
			pPainter->setBrush(track()->foreground());
			pPainter->drawPolygon(*pPolyMax[i]);
			pPainter->setBrush(track()->foreground().light());
			pPainter->drawPolygon(*pPolyRms[i]);
			delete pPolyMax[i];
			delete pPolyRms[i];
		}
		delete [] pPolyMax;
		delete [] pPolyRms;
		// Done on polygon mode.
	} else {
		// Bar-accumulated mode.
		int v, k;
		int *ymax = new int [iChannels];
		int *ymin = new int [iChannels];
		int *yrms = new int [iChannels];
		for (i = 0; i < (int) iChannels; i++)
			ymax[i] = ymin[i] = yrms[i] = 0;
		j = k = 0;
		x = clipRect.x();
		for (n = 0; n < (int) nframes; n++) {
			y = clipRect.y() + h2;
			if (kdelta < 1)
				x = clipRect.x() + pSession->pixelFromFrame(n * iPeriod);
			for (i = 0; i < (int) iChannels; i++, j++) {
				v = (h2 * pframes[j].peakMax) / 255;
				if (ymax[i] < v) ymax[i] = v;
				v = (h2 * pframes[j].peakMin) / 255;
				if (ymin[i] < v) ymin[i] = v;
				v = (h2 * pframes[j].peakRms) / 255;
				if (yrms[i] < v) yrms[i] = v;
				if (kdelta < 1) {
					pPainter->setPen(track()->foreground());
					pPainter->drawLine(x, y - ymin[i], x, y + ymax[i]);
					pPainter->setPen(track()->foreground().light());
					pPainter->drawLine(x, y - yrms[i], x, y + yrms[i]);
					ymax[i] = ymin[i] = yrms[i] = 0;
					y += h1;
				}
			}
			if (kdelta < 1) {
				kdelta = pSession->frameFromPixel(x - clipRect.x())
					/ (++k * iPeriod);
			}
			kdelta--;
		}
		// Free (ab)used arrays.
		delete [] yrms;
		delete [] ymin;
		delete [] ymax;
		// Done on bar-accumulated mode.
	}

	// Our peak buffer at large.
	delete [] pframes;
}


// Virtual document element methods.
bool qtractorAudioClip::loadClipElement (
	qtractorSessionDocument * /* pDocument */, QDomElement *pElement )
{
	// Load track children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {
		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;
		// Load track state..
		if (eChild.tagName() == "filename") {
			if (!qtractorAudioClip::open(eChild.text()))
				return false;
		}
	}

	return true;
}


bool qtractorAudioClip::saveClipElement (
	qtractorSessionDocument *pDocument, QDomElement *pElement )
{
	if (m_pPeak == NULL)
		return false;

	QDomElement eAudioClip = pDocument->document()->createElement("audio-clip");
	pDocument->saveTextElement("filename", m_pPeak->filename(), &eAudioClip);
	pElement->appendChild(eAudioClip);

	return true;
}


// end of qtractorAudioClip.cpp
