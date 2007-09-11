/*
 * Copyright (c) 2002-2007 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <core/core.h>
#include <core/view.h>

#include "ucombo.h"

#include "primitive.h"

AG_UCombo *
AG_UComboNew(void *parent, Uint flags)
{
	AG_UCombo *com;

	com = Malloc(sizeof(AG_UCombo), M_OBJECT);
	AG_UComboInit(com, flags);
	AG_ObjectAttach(parent, com);
	if (flags & AG_UCOMBO_FOCUS) {
		AG_WidgetFocus(com);
	}
	return (com);
}

AG_UCombo *
AG_UComboNewPolled(void *parent, Uint flags, AG_EventFn fn, const char *fmt,
    ...)
{
	AG_UCombo *com;
	AG_Event *ev;

	com = Malloc(sizeof(AG_UCombo), M_OBJECT);
	AG_UComboInit(com, flags);
	AG_ObjectAttach(parent, com);

	com->list->flags |= AG_TLIST_POLL;
	ev = AG_SetEvent(com->list, "tlist-poll", fn, NULL);
	AG_EVENT_GET_ARGS(ev, fmt);
	
	if (flags & AG_UCOMBO_FOCUS) {
		AG_WidgetFocus(com);
	}
	return (com);
}

static void
Collapse(AG_UCombo *com)
{
	AG_WidgetBinding *stateb;
	int *state;

	if (com->panel == NULL) {
		return;
	}
	com->wSaved = WIDGET(com->panel)->w;
	com->hSaved = WIDGET(com->panel)->h;
	
	AG_WindowHide(com->panel);
	AG_ObjectDetach(com->list);
	AG_ViewDetach(com->panel);
	com->panel = NULL;
	
	stateb = AG_WidgetGetBinding(com->button, "state", &state);
	*state = 0;
	AG_WidgetBindingChanged(stateb);
	AG_WidgetUnlockBinding(stateb);
}

static void
ModalClose(AG_Event *event)
{
	AG_UCombo *com = AG_PTR(1);

	if (com->panel != NULL)
		Collapse(com);
}

static void
Expand(AG_Event *event)
{
	AG_UCombo *com = AG_PTR(1);
	int expand = AG_INT(2);
	AG_SizeReq rList;
	int x, y, w, h;

	if (expand) {
		com->panel = AG_WindowNew(AG_WINDOW_MODAL|AG_WINDOW_NOTITLE);
		AG_WindowSetPadding(com->panel, 0, 0, 0, 0);
		AG_ObjectAttach(com->panel, com->list);
	
		if (com->wSaved > 0) {
			w = com->wSaved;
			h = com->hSaved;
		} else {
			AG_TlistSizeHintPixels(com->list,
			    com->wPreList, com->hPreList);
			AG_WidgetSizeReq(com->list, &rList);
			w = rList.w + agColorsBorderSize*2;
			h = rList.h + agColorsBorderSize*2;
		}
		x = WIDGET(com)->cx + WIDGET(com)->w - w;
		y = WIDGET(com)->cy;

		AG_SetEvent(com->panel, "window-modal-close",
		    ModalClose, "%p", com);
		AG_WindowSetGeometry(com->panel, x, y, w, h);
		AG_WindowShow(com->panel);
	} else {
		Collapse(com);
	}
}

static void
SelectedItem(AG_Event *event)
{
	AG_Tlist *tl = AG_SELF();
	AG_UCombo *com = AG_PTR(1);
	AG_TlistItem *it;

	AG_MutexLock(&tl->lock);
	if ((it = AG_TlistSelectedItem(tl)) != NULL) {
		it->selected++;
		AG_ButtonText(com->button, "%s", it->text);
		AG_PostEvent(NULL, com, "ucombo-selected", "%p", it);
	}
	AG_MutexUnlock(&tl->lock);

	Collapse(com);
}

void
AG_UComboInit(AG_UCombo *com, Uint flags)
{
	Uint wflags = AG_WIDGET_UNFOCUSED_BUTTONUP;

	if (flags & AG_UCOMBO_HFILL) { wflags |= AG_WIDGET_HFILL; }
	if (flags & AG_UCOMBO_VFILL) { wflags |= AG_WIDGET_VFILL; }

	AG_WidgetInit(com, &agUComboOps, wflags);
	com->panel = NULL;
	com->wSaved = 0;
	com->hSaved = 0;
	com->hPreList = 4;
	AG_TextSize("XXXXXXXX", &com->wPreList, NULL);

	com->button = AG_ButtonNew(com, AG_BUTTON_STICKY, _("..."));
	AG_ButtonSetPadding(com->button, 1,1,1,1);
	AG_WidgetSetFocusable(com->button, 0);
	AG_SetEvent(com->button, "button-pushed", Expand, "%p", com);
	
	com->list = Malloc(sizeof(AG_Tlist), M_OBJECT);
	AG_TlistInit(com->list, AG_TLIST_EXPAND);
	AG_SetEvent(com->list, "tlist-changed", SelectedItem, "%p", com);
}

void
AG_UComboSizeHint(AG_UCombo *com, const char *text, int h)
{
	AG_TextSize(text, &com->wPreList, NULL);
	com->hPreList = h;
}

void
AG_UComboSizeHintPixels(AG_UCombo *com, int w, int h)
{
	com->wPreList = w;
	com->hPreList = h;
}

static void
Destroy(void *p)
{
	AG_UCombo *com = p;

	if (com->panel != NULL) {
		AG_WindowHide(com->panel);
		AG_ObjectDetach(com->list);
		AG_ViewDetach(com->panel);
	}
	AG_ObjectDestroy(com->list);
	Free(com->list, M_OBJECT);
	AG_WidgetDestroy(com);
}

static void
SizeRequest(void *p, AG_SizeReq *r)
{
	AG_UCombo *com = p;
	AG_SizeReq rButton;

	AG_WidgetSizeReq(com->button, &rButton);
	r->w = rButton.w;
	r->h = rButton.h;
}

static int
SizeAllocate(void *p, const AG_SizeAlloc *a)
{
	AG_UCombo *com = p;
	AG_SizeAlloc aButton;

	aButton.x = 0;
	aButton.y = 0;
	aButton.w = a->w;
	aButton.h = a->h;
	AG_WidgetSizeAlloc(com->button, &aButton);
	return (0);
}

const AG_WidgetOps agUComboOps = {
	{
		"AG_Widget:AG_UCombo",
		sizeof(AG_UCombo),
		{ 0,0 },
		NULL,			/* init */
		NULL,			/* reinit */
		Destroy,
		NULL,			/* load */
		NULL,			/* save */
		NULL			/* edit */
	},
	NULL,			/* draw */
	SizeRequest,
	SizeAllocate
};

