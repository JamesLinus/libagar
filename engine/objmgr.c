/*	$Csoft: objmgr.c,v 1.9 2005/02/22 04:42:04 vedge Exp $	*/

/*
 * Copyright (c) 2003, 2004, 2005 CubeSoft Communications, Inc.
 * <http://www.csoft.org>
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

#include <engine/engine.h>
#include <engine/typesw.h>
#include <engine/view.h>

#include <engine/widget/window.h>
#include <engine/widget/box.h>
#include <engine/widget/vbox.h>
#include <engine/widget/hbox.h>
#include <engine/widget/button.h>
#include <engine/widget/textbox.h>
#include <engine/widget/checkbox.h>
#include <engine/widget/tlist.h>
#include <engine/widget/combo.h>
#include <engine/widget/menu.h>
#include <engine/widget/bitmap.h>
#include <engine/widget/label.h>

#include <string.h>
#include <ctype.h>

#include <engine/mapedit/mapedit.h>

#include "objmgr.h"

struct objent {
	struct object *obj;
	struct window *win;
	TAILQ_ENTRY(objent) objs;
};
static TAILQ_HEAD(,objent) dobjs;
static TAILQ_HEAD(,objent) gobjs;
static int edit_on_create = 1;
static void *current_pobj = NULL;

static void
create_obj(int argc, union evarg *argv)
{
	char name[OBJECT_NAME_MAX];
	struct object_type *t = argv[1].p;
	struct textbox *name_tb = argv[2].p;
	struct tlist *objs_tl = argv[3].p;
	struct window *dlg_win = argv[4].p;
	struct tlist_item *it;
	struct object *pobj;
	void *nobj;

	if ((it = tlist_item_selected(objs_tl)) != NULL) {
		pobj = it->p1;
	} else {
		 pobj = world;
	}
	textbox_copy_string(name_tb, name, sizeof(name));
	view_detach(dlg_win);

	if (name[0] == '\0') {
		unsigned int nameno = 0;
		struct object *ch;
tryname:
		snprintf(name, sizeof(name), "%s #%d", t->type, nameno);
		TAILQ_FOREACH(ch, &pobj->children, cobjs) {
			if (strcmp(ch->name, name) == 0)
				break;
		}
		if (ch != NULL) {
			nameno++;
			goto tryname;
		}
	}

	nobj = Malloc(t->size, M_OBJECT);
	if (t->ops->init != NULL) {
		t->ops->init(nobj, name);
	} else {
		object_init(nobj, t->type, name, NULL);
	}
	object_attach(pobj, nobj);
	object_unlink_datafiles(nobj);
	
	if (edit_on_create &&
	    t->ops->edit != NULL)
		objmgr_open_data(nobj);
}

enum {
	OBJEDIT_EDIT_DATA,
	OBJEDIT_EDIT_GENERIC,
	OBJEDIT_LOAD,
	OBJEDIT_SAVE,
	OBJEDIT_REINIT,
	OBJEDIT_DESTROY,
	OBJEDIT_MOVE_UP,
	OBJEDIT_MOVE_DOWN,
	OBJEDIT_DUP
};

static void
close_obj_generic(int argc, union evarg *argv)
{
	struct window *win = argv[0].p;
	struct objent *oent = argv[1].p;

	view_detach(win);
	TAILQ_REMOVE(&gobjs, oent, objs);
	object_del_dep(&mapedit.pseudo, oent->obj);
	Free(oent, M_MAPEDIT);
	
	/* TODO */
	world_changed = 1;
}

void
objmgr_open_generic(struct object *ob)
{
	struct objent *oent;
	
	TAILQ_FOREACH(oent, &gobjs, objs) {
		if (oent->obj == ob)
			break;
	}
	if (oent != NULL) {
		window_show(oent->win);
		return;
	}
	
	object_add_dep(&mapedit.pseudo, ob);
	
	oent = Malloc(sizeof(struct objent), M_MAPEDIT);
	oent->obj = ob;
	oent->win = object_edit(ob);
	TAILQ_INSERT_HEAD(&gobjs, oent, objs);
	window_show(oent->win);

	event_new(oent->win, "window-close", close_obj_generic, "%p", oent);
	
	/* TODO */
	world_changed = 1;
}

static void
close_obj_data(int argc, union evarg *argv)
{
	struct window *win = argv[0].p;
	struct objent *oent = argv[1].p;

	window_hide(win);
	event_post(NULL, oent->obj, "edit-close", NULL);
	view_detach(win);
	TAILQ_REMOVE(&dobjs, oent, objs);
	object_page_out(oent->obj, OBJECT_DATA);
	object_del_dep(&mapedit.pseudo, oent->obj);
	Free(oent, M_MAPEDIT);

	/* TODO */
	world_changed = 1;
}

void
objmgr_open_data(struct object *ob)
{
	struct objent *oent;
	
	TAILQ_FOREACH(oent, &dobjs, objs) {
		if (oent->obj == ob)
			break;
	}
	if (oent != NULL) {
		window_show(oent->win);
		return;
	}

	if (object_page_in(ob, OBJECT_DATA) == -1) {
		printf("%s: %s\n", ob->name, error_get());
		return;
	}
	object_add_dep(&mapedit.pseudo, ob);

	event_post(NULL, ob, "edit-open", NULL);
	
	oent = Malloc(sizeof(struct objent), M_MAPEDIT);
	oent->obj = ob;
	oent->win = ob->ops->edit(ob);
	TAILQ_INSERT_HEAD(&dobjs, oent, objs);
	window_show(oent->win);

	event_new(oent->win, "window-close", close_obj_data, "%p", oent);

	/* TODO */
	world_changed = 1;
}

static void
obj_op(int argc, union evarg *argv)
{
	struct tlist *tl = argv[1].p;
	struct tlist_item *it;
	int op = argv[2].i;

	TAILQ_FOREACH(it, &tl->items, items) {
		struct object *ob = it->p1;

		if (!it->selected)
			continue;

		switch (op) {
		case OBJEDIT_EDIT_DATA:
			if (ob->ops->edit != NULL) {
				objmgr_open_data(ob);
			} else {
				text_tmsg(MSG_ERROR, 750,
				    _("Object `%s' has no edit operation."),
				    ob->name);
			}
			break;
		case OBJEDIT_EDIT_GENERIC:
			objmgr_open_generic(ob);
			break;
		case OBJEDIT_LOAD:
			if (object_load(ob) == -1) {
				text_msg(MSG_ERROR, "%s: %s", ob->name,
				    error_get());
			} else {
				world_changed = (ob != OBJECT(world));
			}
			break;
		case OBJEDIT_SAVE:
			if (object_save(ob) == -1) {
				text_msg(MSG_ERROR, "%s: %s", ob->name,
				    error_get());
			} else {
				text_tmsg(MSG_INFO, 1000,
				    _("Object `%s' was saved successfully."),
				    ob->name);
				world_changed = (ob != OBJECT(world));
			}
			break;
		case OBJEDIT_DUP:
			{
				struct object *dob;

				if (ob == world ||
				    ob->flags & OBJECT_NON_PERSISTENT) {
					text_msg(MSG_ERROR,
					    _("%s: cannot duplicate."),
					    ob->name);
					break;
				}
				if ((dob = object_duplicate(ob)) == NULL) {
					text_msg(MSG_ERROR, "%s: %s", ob->name,
					    error_get());
				} else {
					world_changed = 1;
				}
			}
			break;
		case OBJEDIT_MOVE_UP:
			object_move_up(ob);
			world_changed = 1;
			break;
		case OBJEDIT_MOVE_DOWN:
			object_move_down(ob);
			world_changed = 1;
			break;
		case OBJEDIT_REINIT:
			if (it->p1 == world) {
				continue;
			}
			if (ob->ops->reinit != NULL) {
				ob->ops->reinit(ob);
				world_changed = 1;
			}
			break;
		case OBJEDIT_DESTROY:
			if (it->p1 == world) {
				continue;
			}
			if (object_in_use(ob)) {
				text_msg(MSG_ERROR, "%s: %s", ob->name,
				    error_get());
				continue;
			}
			if (ob->flags & OBJECT_INDESTRUCTIBLE) {
				text_msg(MSG_ERROR,
				    _("The `%s' object is indestructible."),
				    ob->name);
				continue;
			}
			object_detach(ob);
			object_unlink_datafiles(ob);
			object_destroy(ob);
			if ((ob->flags & OBJECT_STATIC) == 0) {
				Free(ob, M_OBJECT);
			}
			world_changed = 1;
			break;
		}
	}
}

/* Display the object tree. */
static struct tlist_item *
find_objs(struct tlist *tl, struct object *pob, int depth)
{
	char label[TLIST_LABEL_MAX];
	struct object *cob;
	struct tlist_item *it;
	SDL_Surface *icon;

	strlcpy(label, pob->name, sizeof(label));
	if (pob->flags & OBJECT_DATA_RESIDENT) {
		strlcat(label, _(" (resident)"), sizeof(label));
	}
	it = tlist_insert_item(tl, object_icon(pob), label, pob);
	it->depth = depth;
	it->class = "object";

	if (!TAILQ_EMPTY(&pob->children)) {
		it->flags |= TLIST_HAS_CHILDREN;
		if (object_root(pob) == pob)
			it->flags |= TLIST_VISIBLE_CHILDREN;
	}
	if ((it->flags & TLIST_HAS_CHILDREN) &&
	    tlist_visible_children(tl, it)) {
		TAILQ_FOREACH(cob, &pob->children, cobjs)
			find_objs(tl, cob, depth+1);
	}
	return (it);
}

/* Update the object tree display. */
static void
poll_objs(int argc, union evarg *argv)
{
	struct tlist *tl = argv[0].p;
	struct object *pob = argv[1].p;
	struct object *dob = argv[2].p;
	struct tlist_item *it;

	lock_linkage();
	tlist_clear_items(tl);
	find_objs(tl, pob, 0);
	tlist_restore_selections(tl);
	unlock_linkage();

	if (tlist_item_selected(tl) == NULL) {
		TAILQ_FOREACH(it, &tl->items, items) {
			if (it->p1 == dob)
				break;
		}
		if (it != NULL)
			tlist_select(tl, it);
	}
}

static void
load_object(int argc, union evarg *argv)
{
	struct object *o = argv[1].p;

	if (object_load(o) == -1) {
		text_msg(MSG_ERROR, "%s: %s", OBJECT(o)->name, error_get());
	}
}

static void
save_object(int argc, union evarg *argv)
{
	struct object *ob = argv[1].p;

	if (object_save(ob) == -1) {
		text_msg(MSG_ERROR, "%s: %s", ob->name, error_get());
	} else {
		text_tmsg(MSG_INFO, 1000, _("Object `%s' saved successfully."),
		    ob->name);
	}
}

static void
create_obj_dlg(int argc, union evarg *argv)
{
	struct window *win;
	struct object_type *t = argv[1].p;
	struct window *pwin = argv[2].p;
	struct tlist *pobj_tl;
	struct box *bo;
	struct textbox *tb;
	struct checkbox *cb;

	win = window_new(WINDOW_NO_CLOSE|WINDOW_NO_MINIMIZE, NULL);
	window_set_caption(win, _("New %s object"), t->type);
	window_set_position(win, WINDOW_CENTER, 1);

	bo = box_new(win, BOX_VERT, BOX_WFILL);
	{
		label_new(bo, LABEL_STATIC, _("Type: %s"), t->type);
		tb = textbox_new(bo, _("Name: "));
		widget_focus(tb);
	}
	
	bo = box_new(win, BOX_VERT, BOX_WFILL|BOX_HFILL);
	box_set_padding(bo, 0);
	box_set_spacing(bo, 0);
	{
		label_new(bo, LABEL_STATIC, _("Parent object:"));

		pobj_tl = tlist_new(bo, TLIST_POLL|TLIST_TREE);
		tlist_prescale(pobj_tl, "XXXXXXXXXXXXXXXXXXX", 5);
		widget_bind(pobj_tl, "selected", WIDGET_POINTER, &current_pobj);
		event_new(pobj_tl, "tlist-poll", poll_objs, "%p,%p", world,
		    current_pobj);
	}

	bo = box_new(win, BOX_VERT, BOX_WFILL);
	{
		cb = checkbox_new(win, _("Edit now"));
		widget_bind(cb, "state", WIDGET_INT, &edit_on_create);
	}

	bo = box_new(win, BOX_HORIZ, BOX_HOMOGENOUS|BOX_WFILL);
	{
		struct button *btn;
	
		btn = button_new(bo, _("OK"));
		event_new(tb, "textbox-return", create_obj, "%p,%p,%p,%p", t,
		    tb, pobj_tl, win);
		event_new(btn, "button-pushed", create_obj, "%p,%p,%p,%p", t,
		    tb, pobj_tl, win);
		
		btn = button_new(bo, _("Cancel"));
		event_new(btn, "button-pushed", window_generic_detach, "%p",
		    win);
	}

	window_attach(pwin, win);
	window_show(win);
}

/* Create the object editor window. */
struct window *
objmgr_window(void)
{
	struct window *win;
	struct vbox *vb;
	struct textbox *name_tb;
	struct combo *types_com;
	struct tlist *objs_tl;
	struct AGMenu *me;
	struct AGMenuItem *mi, *mi_objs;

	win = window_new(0, "objmgr");
	window_set_caption(win, _("Object manager"));
	window_set_position(win, WINDOW_LOWER_RIGHT, 0);
	window_set_spacing(win, 1);
	
	objs_tl = Malloc(sizeof(struct tlist), M_OBJECT);
	tlist_init(objs_tl, TLIST_POLL|TLIST_MULTI|TLIST_TREE);
	tlist_prescale(objs_tl, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", 10);
	event_new(objs_tl, "tlist-poll", poll_objs, "%p,%p", world, NULL);
	event_new(objs_tl, "tlist-dblclick", obj_op, "%p, %i", objs_tl,
	    OBJEDIT_EDIT_DATA);

	me = menu_new(win);
	mi = menu_add_item(me, _("Objects"));
	{
		int i;

		mi_objs = menu_action(mi, _("Create object"), OBJCREATE_ICON,
		    NULL, NULL);
		for (i = ntypesw-1; i >= 0; i--) {
			char label[32];
			struct object_type *t = &typesw[i];

			strlcpy(label, t->type, sizeof(label));
			label[0] = (char)toupper((int)label[0]);
			menu_action(mi_objs, label, t->icon,
			    create_obj_dlg, "%p,%p", t, win);
		}

		menu_separator(mi);
		menu_action(mi, _("Load state"), OBJLOAD_ICON, load_object,
		    "%p", world);
		menu_action(mi, _("Save state"), OBJSAVE_ICON, save_object,
		    "%p", world);
	}

	vb = vbox_new(win, VBOX_WFILL|VBOX_HFILL);
	vbox_set_padding(vb, 1);
	vbox_set_spacing(vb, 1);
	{
		struct AGMenuItem *mi;

		object_attach(vb, objs_tl);

		mi = tlist_set_popup(objs_tl, "object");
		{
			menu_action(mi, _("Edit object..."), OBJEDIT_ICON,
			    obj_op, "%p, %i", objs_tl, OBJEDIT_EDIT_DATA);
			menu_action(mi, _("Edit generic information..."),
			    OBJGENEDIT_ICON,
			    obj_op, "%p, %i", objs_tl, OBJEDIT_EDIT_GENERIC);

			menu_separator(mi);
			
			menu_action(mi, _("Load"), OBJLOAD_ICON, obj_op,
			    "%p, %i", objs_tl, OBJEDIT_LOAD);
			menu_action(mi, _("Save"), OBJSAVE_ICON, obj_op,
			    "%p, %i", objs_tl, OBJEDIT_SAVE);
			
			menu_separator(mi);
			
			menu_action(mi, _("Duplicate"), OBJDUP_ICON,
			    obj_op, "%p, %i", objs_tl, OBJEDIT_DUP);
			menu_action_kb(mi, _("Move up"), OBJMOVEUP_ICON,
			    SDLK_u, KMOD_SHIFT, obj_op, "%p, %i", objs_tl,
			    OBJEDIT_MOVE_UP);
			menu_action_kb(mi, _("Move down"), OBJMOVEDOWN_ICON,
			    SDLK_d, KMOD_SHIFT, obj_op, "%p, %i", objs_tl,
			    OBJEDIT_MOVE_DOWN);
			
			menu_separator(mi);
			
			menu_action(mi, _("Reinitialize"), OBJREINIT_ICON,
			    obj_op, "%p, %i", objs_tl, OBJEDIT_REINIT);
			menu_action(mi, _("Destroy"), TRASH_ICON,
			    obj_op, "%p, %i", objs_tl, OBJEDIT_DESTROY);
		}
#if 0
		toolbar_add_button(tbar, 0, ICON(OBJEDIT_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_EDIT_DATA);
		toolbar_add_button(tbar, 0, ICON(OBJGENEDIT_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_EDIT_GENERIC);

		toolbar_add_button(tbar, 0, ICON(OBJLOAD_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_LOAD);
		toolbar_add_button(tbar, 0, ICON(OBJSAVE_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_SAVE);

		toolbar_add_button(tbar, 0, ICON(OBJDUP_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_DUP);
		toolbar_add_button(tbar, 0, ICON(OBJMOVEUP_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_MOVE_UP);
		toolbar_add_button(tbar, 0, ICON(OBJMOVEDOWN_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_MOVE_DOWN);
		toolbar_add_button(tbar, 0, ICON(OBJREINIT_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_REINIT);
		toolbar_add_button(tbar, 0, ICON(TRASH_ICON), 0, 0,
		    obj_op, "%p, %i", objs_tl, OBJEDIT_DESTROY);
#endif
	}
	
	window_show(win);
	return (win);
}

void
objmgr_init(void)
{
	TAILQ_INIT(&dobjs);
	TAILQ_INIT(&gobjs);
}

static void
save_changes(int argc, union evarg *argv)
{
	struct window *dlg_win = widget_parent_window(argv[0].p);
	struct object *obj = argv[1].p;
	int then_exit = argv[2].i;

	if (object_save(obj) == -1) {
		text_msg(MSG_ERROR, "%s: %s", obj->name, error_get());
		return;
	}

	dprintf("saved `%s'\n", obj->name);

	if (then_exit) {
		SDL_Event nev;

		nev.type = SDL_USEREVENT;
		SDL_PushEvent(&nev);
	} else {
		text_tmsg(MSG_INFO, 500, _("Object `%s' saved successfully."),
		    obj->name);
	}
}

static void
discard_changes(int argc, union evarg *argv)
{
	struct window *dlg_win = widget_parent_window(argv[0].p);
	struct object *obj = argv[1].p;
	int then_exit = argv[2].i;

	if (then_exit) {
		SDL_Event nev;

		nev.type = SDL_USEREVENT;
		SDL_PushEvent(&nev);
	} else {
		text_tmsg(MSG_INFO, 500, _("Ignoring modifications to `%s'."),
		    obj->name);
	}
}

/*
 * Present the user with a dialog asking whether to save modifications to
 * an object and optionally exit afterwards.
 */
void
objmgr_changed_dlg(void *obj, int then_exit)
{
	struct window *win;
	struct box *bo;
	struct button *b;

	if ((win = window_new(WINDOW_NO_CLOSE|WINDOW_NO_MINIMIZE|WINDOW_MODAL|
			      WINDOW_NO_RESIZE, "objmgr-changed-dlg"))
			      == NULL) {
		return;
	}
	window_set_caption(win, _("Save changes?"));
	label_new(win, LABEL_STATIC, _("The state of `%s' has changed."),
	    OBJECT(obj)->name);
	
	
	bo = box_new(win, BOX_HORIZ, BOX_HOMOGENOUS|VBOX_WFILL);
	{
		b = button_new(bo, _("Save changes"));
		event_new(b, "button-pushed", save_changes, "%p,%i", obj,
		    then_exit);
		widget_focus(b);

		b = button_new(bo, _("Discard changes"));
		event_new(b, "button-pushed", discard_changes, "%p,%i", obj,
		    then_exit);
	}

	bo = box_new(win, BOX_HORIZ, BOX_HOMOGENOUS|VBOX_WFILL);
	{
		b = button_new(bo, _("Cancel"));
		event_new(b, "button-pushed", window_generic_detach, "%p", win);
		WIDGET(b)->flags |= WIDGET_WFILL;
	}

	window_show(win);
}

void
objmgr_destroy(void)
{
	struct objent *oent, *noent;

	for (oent = TAILQ_FIRST(&dobjs);
	     oent != TAILQ_END(&dobjs);
	     oent = noent) {
		noent = TAILQ_NEXT(oent, objs);
		Free(oent, M_MAPEDIT);
	}
	for (oent = TAILQ_FIRST(&gobjs);
	     oent != TAILQ_END(&gobjs);
	     oent = noent) {
		noent = TAILQ_NEXT(oent, objs);
		Free(oent, M_MAPEDIT);
	}
}

/*
 * Close and reopen all edition windows associated with an object, if there
 * are any. This is called automatically from object_load() for objects with
 * the flag OBJECT_REOPEN_ONLOAD.
 */
void
objmgr_reopen(struct object *obj)
{
	struct objent *oent;

	TAILQ_FOREACH(oent, &dobjs, objs) {
		if (oent->obj == obj) {
			dprintf("reopening %s\n", obj->name);

			window_hide(oent->win);
			event_post(NULL, oent->obj, "edit-close", NULL);
			view_detach(oent->win);

			event_post(NULL, oent->obj, "edit-open", NULL);
			oent->win = obj->ops->edit(obj);
			window_show(oent->win);
			event_new(oent->win, "window-close", close_obj_data,
			    "%p", oent);
		}
	}
}

