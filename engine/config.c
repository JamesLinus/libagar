/*	$Csoft: config.c,v 1.94 2003/07/25 22:01:33 vedge Exp $	    */

/*
 * Copyright (c) 2002, 2003 CubeSoft Communications, Inc.
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

#include <config/sharedir.h>
#include <config/ttfdir.h>
#include <engine/compat/asprintf.h>

#include <engine/engine.h>
#include <engine/config.h>
#include <engine/view.h>
#include <engine/map.h>
#include <engine/prop.h>

#include <engine/widget/window.h>
#include <engine/widget/vbox.h>
#include <engine/widget/hbox.h>
#include <engine/widget/label.h>
#include <engine/widget/button.h>
#include <engine/widget/radio.h>
#include <engine/widget/checkbox.h>
#include <engine/widget/textbox.h>
#include <engine/widget/keycodes.h>
#include <engine/widget/tlist.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

const struct version config_ver = {
	"agar config",
	5, 0
};

static int config_notify = 0;			/* GUI notifications */

static void
config_change_path(int argc, union evarg *argv)
{
	struct textbox *tbox = argv[0].p;
	char *varname = argv[1].s;
	char *s;

	s = textbox_string(tbox);
	prop_set_string(config, varname, "%s", s);
	free(s);

	WIDGET(tbox)->flags &= ~(WIDGET_FOCUSED);
}

static void
config_change_res(int argc, union evarg *argv)
{
	struct textbox *tbox = argv[0].p;
	char *varname = argv[1].s;
	int i;

	i = textbox_int(tbox);
	prop_set_int(config, varname, i);
}

static void
config_save(int argc, union evarg *argv)
{
	if (object_save(config) == -1)
		text_msg(MSG_ERROR, "%s", error_get());
}


static void
config_prop_modified(int argc, union evarg *argv)
{
	struct prop *prop = argv[1].p;

	if (strcmp(prop->key, "view.full-screen") == 0) {
		if (view != NULL) {
			SDL_Event vexp;

			if ((prop->data.i &&
			    (!(view->v->flags & SDL_FULLSCREEN))) ||
			   (!prop->data.i &&
			    ((view->v->flags & SDL_FULLSCREEN)))) {
				SDL_WM_ToggleFullScreen(view->v);
				vexp.type = SDL_VIDEOEXPOSE;
				SDL_PushEvent(&vexp);
			}
		}
	} else if (strcmp(prop->key, "view.opengl") == 0 &&
	    prop->data.i && config_notify) {
		text_msg(MSG_WARNING,
		    _("Save the configuration and restart %s for OpenGL mode "
		      "to take effect"),
		    proginfo->progname);
	} else if (strcmp(prop->key, "view.async-blits") == 0 &&
	    prop->data.i && config_notify) {
		text_msg(MSG_WARNING,
		    _("Save the configuration and restart %s for async blits "
		      "to take effect"),
		    proginfo->progname);
	} else if (strcmp(prop->key, "input.unicode") == 0) {
		if (SDL_EnableUNICODE(prop->data.i)) {
			dprintf("disabled unicode translation\n");
		} else {
			dprintf("enabled unicode translation\n");
		}
	}
}

void
config_init(struct config *con)
{
	char *udatadir;
	struct passwd *pwd;
	struct stat sta;

	object_init(con, "object", "config", NULL);
	OBJECT(con)->flags |= OBJECT_RELOAD_PROPS;
	OBJECT(con)->save_pfx = NULL;

	prop_set_bool(con, "view.full-screen", 0);
	prop_set_bool(con, "view.async-blits", 0);
	prop_set_bool(con, "view.opengl", 0);
	prop_set_uint16(con, "view.w", 800);
	prop_set_uint16(con, "view.h", 600);
	prop_set_uint16(con, "view.min-w", 320);
	prop_set_uint16(con, "view.min-h", 240);
	prop_set_uint8(con, "view.depth", 32);
	
	prop_set_bool(con, "font-engine", 1);
	prop_set_string(con, "font-engine.default-font", "zekton");
	prop_set_int(con, "font-engine.default-size", 12);
	prop_set_int(con, "font-engine.default-style", 0);

	prop_set_bool(con, "input.unicode", 1);
	prop_set_bool(con, "input.joysticks", 1);

	pwd = getpwuid(getuid());
	Asprintf(&udatadir, "%s/.%s", pwd->pw_dir, proginfo->progname);

	prop_set_string(con, "save-path", "%s", udatadir);
	prop_set_string(con, "den-path", "%s", SHAREDIR);
	prop_set_string(con, "load-path", "%s:%s", udatadir, SHAREDIR);
	prop_set_string(con, "font-path", "%s/fonts:%s", udatadir, TTFDIR);

	if (stat(udatadir, &sta) != 0 &&
	    mkdir(udatadir, 0700) != 0) {
		fatal("%s: %s", udatadir, strerror(errno));
	}

	event_new(con, "prop-modified", config_prop_modified, NULL);
	free(udatadir);
}

void
config_window(struct config *con)
{
	struct window *win;
	struct vbox *vb;
	struct hbox *hb;
	struct button *button;
	struct textbox *tbox;
	struct checkbox *cbox;

	win = window_new("config-engine-settings");
	window_set_caption(win, _("Engine settings"));
	window_set_closure(win, WINDOW_HIDE);

	vb = vbox_new(win, 0);
	vbox_set_spacing(vb, 2);
	vbox_set_padding(vb, 20);
	{
		const struct {
			char *name;
			char *descr;
		} settings[] = {
			{ "view.full-screen", N_("Full screen") },
			{ "view.async-blits", N_("Asynchronous blits") },
#ifdef HAVE_OPENGL
			{ "view.opengl", N_("OpenGL rendering context") },
#endif
			{ "input.unicode", N_("Unicode keyboard translation") }
		};
		const int nsettings = sizeof(settings) / sizeof(settings[0]);
		int i;

		for (i = 0; i < nsettings; i++) {
			cbox = checkbox_new(vb, "%s", _(settings[i].descr));
			widget_bind(cbox, "state", WIDGET_PROP, config,
			    settings[i].name);
		}

#ifdef DEBUG
		/* Thread unsafe, but not very dangerous. */
		cbox = checkbox_new(vb, _("Debugging"));
		widget_bind(cbox, "state", WIDGET_INT, NULL, &engine_debug);
#endif
		cbox = checkbox_new(vb, _("Idle time prediction"));
		widget_bind(cbox, "state", WIDGET_INT, NULL, &event_idle);
	}
	
	vb = vbox_new(win, VBOX_WFILL);
	{
		char path[MAXPATHLEN];

		tbox = textbox_new(vb, _("Data save dir: "));
		prop_copy_string(config, "save-path", path, sizeof(path));
		textbox_printf(tbox, "%s", path);
		event_new(tbox, "textbox-return", config_change_path, "%s",
		    "save-path");
	
		tbox = textbox_new(vb, _("Data load path: "));
		prop_copy_string(config, "load-path", path, sizeof(path));
		textbox_printf(tbox, "%s", path);
		event_new(tbox, "textbox-return", config_change_path, "%s",
		    "load-path");
	
		tbox = textbox_new(vb, _("Font path: "));
		prop_copy_string(config, "font-path", path, sizeof(path));
		textbox_printf(tbox, "%s", path);
		event_new(tbox, "textbox-return", config_change_path, "%s",
		    "font-path");
		
		tbox = textbox_new(vb, _("Den path: "));
		prop_copy_string(config, "den-path", path, sizeof(path));
		textbox_printf(tbox, "%s", path);
		event_new(tbox, "textbox-return", config_change_path, "%s",
		    "den-path");
	}

	hb = hbox_new(win, HBOX_WFILL|HBOX_HOMOGENOUS);
	{
		/* XXX propose some default resolutions. */

		tbox = textbox_new(hb, _("Width: "));
		textbox_printf(tbox, "%d", prop_get_uint16(config, "view.w"));
		event_new(tbox, "textbox-return", config_change_res, "%s",
		    "view.w");
		WIDGET(tbox)->flags &= ~(WIDGET_WFILL);

		tbox = textbox_new(hb, _("Height: "));
		textbox_printf(tbox, "%d", prop_get_uint16(config, "view.h"));
		event_new(tbox, "textbox-return", config_change_res, "%s",
		    "view.h");
		WIDGET(tbox)->flags &= ~(WIDGET_WFILL);
	}

	hb = hbox_new(win, HBOX_HOMOGENOUS|HBOX_WFILL|HBOX_HFILL);
	hbox_set_spacing(hb, 0);
	hbox_set_padding(hb, 0);
	{
		button = button_new(hb, _("Close"));
		event_new(button, "button-pushed", window_generic_hide, 
		    "%p", win);
		widget_focus(button);

		button = button_new(hb, _("Save"));
		event_new(button, "button-pushed", config_save, NULL);
	}
	config->settings = win;
	config_notify++;
}

/* Copy the full pathname to a data file to a fixed-size buffer. */
int
config_search_file(const char *path_key, const char *name, const char *ext,
    char *path, size_t path_len)
{
	char file[MAXPATHLEN];
	struct stat sta;
	char *dir, *last;

	prop_copy_string(config, path_key, path, path_len);

	for (dir = strtok_r(path, ":", &last);
	     dir != NULL;
	     dir = strtok_r(NULL, ":", &last)) {
		strlcpy(file, dir, sizeof(file));

		if (name[0] != '/') {
			strlcat(file, "/", sizeof(file));
		}
		strlcat(file, name, sizeof(file));
		strlcat(file, ".", sizeof(file));
		strlcat(file, ext, sizeof(file));
		if (stat(file, &sta) == 0) {
			if (strlcpy(path, file, path_len) >= path_len) {
				error_set(_("The search path is too big."));
				return (-1);
			}
			return (0);
		}
	}
	error_set(_("Cannot find `%s.%s' in <%s>."), name, ext, path);
	return (-1);
}

