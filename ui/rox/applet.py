"""To create a panel applet for ROX-Filer, you should add a file called
AppletRun to your application. This is run when your applet it dragged onto
a panel. It works like the usual AppRun, except that it is passed the XID of
the GtkSocket widget that ROX-Filer creates for you on the panel.

A sample AppletRun might look like this:

#!/usr/bin/env python
import findrox; findrox.version(1, 9, 12)
import rox
import sys
from rox import applet, g

plug = applet.Applet(sys.argv[1])

label = g.Label('Hello\\nWorld!')
plug.add(label)
plug.show_all()

rox.mainloop()
"""

import rox
from rox import g

_root_window = g.gdk.get_default_root_window()

class Applet(g.Plug):
	"""When your AppletRun file is used, create an Applet widget with
	the argument passed to AppletRun. Show the widget to make it appear
	in the panel. toplevel_* functions are called automatically."""
	def __init__(self, xid):
		"""xid is the sys.argv[1] passed to AppletRun."""
		xid = long(xid)
		g.Plug.__init__(self, xid)
		self.socket = g.gdk.window_foreign_new(xid)
		rox.toplevel_ref()
		self.connect('destroy', rox.toplevel_unref)
	
	def position_menu(self, menu):
		"""Use this as the third argument to Menu.popup()."""
		x, y, mods = _root_window.get_pointer()
		pos = self.socket.property_get('_ROX_PANEL_MENU_POS',
						'STRING', g.FALSE)
		if pos: pos = pos[2]
		if pos:
			side, margin = pos.split(',')
			margin = int(margin)
		else:
			side, margin = None, 2

		width, height = g.gdk.screen_width(), g.gdk.screen_height()
		
		req = menu.size_request()

		if side == 'Top':
			y = margin
			x -= 8 + req[0] / 4
		elif side == 'Bottom':
			y = height - margin - req[1]
			x -= 8 + req[0] / 4
		elif side == 'Left':
			x = margin
			y -= 16
		elif side == 'Right':
			x = width - margin - req[0]
			y -= 16
		else:
			x -= req[0] / 2
			y -= 32

		def limit(v, min, max):
			if v < min: return min
			if v > max: return max
			return v

		x = limit(x, 4, width - 4 - req[0])
		y = limit(y, 4, height - 4 - req[1])
		
		return (x, y, True)
