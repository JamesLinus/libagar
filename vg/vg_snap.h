/*	Public domain	*/

#ifndef _AGAR_VG_SNAP_H_
#define _AGAR_VG_SNAP_H_

#ifdef _AGAR_INTERNAL
#include <gui/toolbar.h>
#else
#include <agar/gui/toolbar.h>
#endif

#include "begin_code.h"

enum vg_snap_mode {
	VG_FREE_POSITIONING,	/* No positional restriction */
	VG_GRID,		/* Snap to grid */
	VG_ENDPOINT,		/* Snap to endpoints */
	VG_ENDPOINT_DISTANCE,	/* Snap to given distance from endpoint */
	VG_CLOSEST_POINT,	/* Snap to closest point on entity */
	VG_CENTER_POINT,	/* Snap to center points */
	VG_MIDDLE_POINT,	/* Snap to middle points */
	VG_INTERSECTIONS_AUTO,	/* Snap to intersections automatically */
	VG_INTERSECTIONS_MANUAL	/* Snap to intersections manually */
};

struct ag_menu;
struct ag_menu_item;

__BEGIN_DECLS
void		   VG_SnapPoint(struct vg_view *, float *, float *);
void	 	   VG_DrawGrid(struct vg_view *);
struct ag_toolbar *VG_SnapToolbar(void *, struct vg_view *,
                                  enum ag_toolbar_type);
void		   VG_SnapMenu(struct ag_menu *, struct ag_menu_item *,
		               struct vg_view *);
__END_DECLS

#include "close_code.h"
#endif /* _AGAR_VG_SNAP_H_ */
