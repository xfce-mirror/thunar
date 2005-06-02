"""To use ROX-Lib2 you need to copy the findrox.py script into your application
directory and import that before anything else. This module will locate
ROX-Lib2 and add ROX-Lib2/python to sys.path. If ROX-Lib2 is not found, it
will display a suitable error and quit.

Since the name of the gtk2 module can vary, it is best to import it from rox,
where it is named 'g'.

The AppRun script of a simple application might look like this:

	#!/usr/bin/env python
	import findrox; findrox.version(1, 9, 12)
	import rox

	window = rox.Window()
	window.set_title('My window')
	window.show()

	rox.mainloop()

This program creates and displays a window. The rox.Window widget keeps
track of how many toplevel windows are open. rox.mainloop() will return
when the last one is closed.

'rox.app_dir' is set to the absolute pathname of your application (extracted
from sys.argv).

The builtin names True and False are defined to 1 and 0, if your version of
python is old enough not to include them already.
"""

import sys, os, codecs

_to_utf8 = codecs.getencoder('utf-8')

roxlib_version = (1, 9, 17)

_path = os.path.realpath(sys.argv[0])
app_dir = os.path.dirname(_path)
if _path.endswith('/AppRun') or _path.endswith('/AppletRun'):
	sys.argv[0] = os.path.dirname(_path)

# In python2.3 there is a bool type. Later versions of 2.2 use ints, but
# early versions don't support them at all, so create them here.
try:
	True
except:
	import __builtin__
	__builtin__.False = 0
	__builtin__.True = 1

try:
	iter
except:
	sys.stderr.write('Sorry, you need to have python 2.2, and it \n'
			 'must be the default version. You may be able to \n'
			 'change the first line of your program\'s AppRun \n'
			 'file to end \'python2.2\' as a workaround.\n')
	raise SystemExit(1)

import i18n

_roxlib_dir = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
_ = i18n.translation(os.path.join(_roxlib_dir, 'Messages'))

_old_sys_path = sys.path[:]

try:
	zhost = 'zero-install.sourceforge.net'
	zpath = '/uri/0install/' + zhost
	if not os.getenv('ROXLIB_DISABLE_ZEROINSTALL') and os.path.exists(zpath):
		zpath = os.path.join(zpath, 'libs/pygtk-2/platform/latest')
		if not os.path.exists(zpath):
			os.system('0refresh ' + zhost)
		if os.path.exists(zpath):
			sys.path.insert(0, zpath +
				'/lib/python%d.%d/site-packages' % sys.version_info[:2])
	import pygtk; pygtk.require('2.0')
	import gtk._gtk		# If this fails, don't leave a half-inited gtk
	import gtk; g = gtk	# Don't syntax error for python1.5
	assert g.Window		# Ensure not 1.2 bindings
except:
	try:
		# Try again without Zero Install
		sys.path = _old_sys_path
		if 'gtk' in sys.modules:
			del sys.modules['gtk']
		try:
			# Try to support 1.99.12, at least to show an error
			import pygtk; pygtk.require('2.0')
		except:
			print "Warning: very old pygtk!"
		import gtk; g = gtk	# Don't syntax error for python1.5
		assert g.Window		# Ensure not 1.2 bindings
	except:
		sys.stderr.write(_('The pygtk2 package (1.99.13 or later) must be '
			   'installed to use this program:\n'
			   'http://rox.sourceforge.net/rox_lib.php3\n'))
		raise

# Put argv back the way it was, now that Gtk has initialised
sys.argv[0] = _path

def _warn_old_findrox():
	try:
		import findrox
	except:
		return	# Don't worry too much if it's missing
	if not hasattr(findrox, 'version'):
		print >>sys.stderr, "WARNING from ROX-Lib: the version of " \
			"findrox.py used by this application (%s) is very " \
			"old and may cause problems." % app_dir
_warn_old_findrox()

# For backwards compatibility. Use True and False in new code.
TRUE = True
FALSE = False

class UserAbort(Exception):
	"""Raised when the user aborts an operation, eg by clicking on Cancel
	or pressing Escape."""
	def __init__(self, message = None):
		Exception.__init__(self,
			message or _("Operation aborted at user's request"))

def alert(message):
	"Display message in an error box. Return when the user closes the box."
	toplevel_ref()
	box = g.MessageDialog(None, 0, g.MESSAGE_ERROR, g.BUTTONS_OK, message)
	box.set_position(g.WIN_POS_CENTER)
	box.set_title(_('Error'))
	box.run()
	box.destroy()
	toplevel_unref()

def bug(message = "A bug has been detected in this program. Please report "
		  "the problem to the authors."):
	"Display an error message and offer a debugging prompt."
	try:
		raise Exception(message)
	except:
		type, value, tb = sys.exc_info()
		import debug
		debug.show_exception(type, value, tb, auto_details = True)

def croak(message):
	"""Display message in an error box, then quit the program, returning
	with a non-zero exit status."""
	alert(message)
	sys.exit(1)

def info(message):
	"Display informational message. Returns when the user closes the box."
	toplevel_ref()
	box = g.MessageDialog(None, 0, g.MESSAGE_INFO, g.BUTTONS_OK, message)
	box.set_position(g.WIN_POS_CENTER)
	box.set_title(_('Information'))
	box.run()
	box.destroy()
	toplevel_unref()

def confirm(message, stock_icon, action = None):
	"""Display a <Cancel>/<Action> dialog. Result is true if the user
	chooses the action, false otherwise. If action is given then that
	is used as the text instead of the default for the stock item. Eg:
	if rox.confirm('Really delete everything?', g.STOCK_DELETE): delete()
	"""
	toplevel_ref()
	box = g.MessageDialog(None, 0, g.MESSAGE_QUESTION,
				g.BUTTONS_CANCEL, message)
	if action:
		button = ButtonMixed(stock_icon, action)
	else:
		button = g.Button(stock = stock_icon)
	button.set_flags(g.CAN_DEFAULT)
	button.show()
	box.add_action_widget(button, g.RESPONSE_OK)
	box.set_position(g.WIN_POS_CENTER)
	box.set_title(_('Confirm:'))
	box.set_default_response(g.RESPONSE_OK)
	resp = box.run()
	box.destroy()
	toplevel_unref()
	return resp == g.RESPONSE_OK

def report_exception():
	"""Display the current python exception in an error box, returning
	when the user closes the box. This is useful in the 'except' clause
	of a 'try' block. Uses rox.debug.show_exception()."""
	type, value, tb = sys.exc_info()
	import debug
	debug.show_exception(type, value, tb)

_icon_path = os.path.join(app_dir, '.DirIcon')
_window_icon = None
if os.path.exists(_icon_path):
	try:
		g.window_set_default_icon_list(g.gdk.pixbuf_new_from_file(_icon_path))
	except:
		# Older pygtk
		_window_icon = g.gdk.pixbuf_new_from_file(_icon_path)
del _icon_path

class Window(g.Window):
	"""This works in exactly the same way as a GtkWindow, except that
	it calls the toplevel_(un)ref functions for you automatically,
	and sets the window icon to <app_dir>/.DirIcon if it exists."""
	def __init__(*args, **kwargs):
		apply(g.Window.__init__, args, kwargs)
		toplevel_ref()
		args[0].connect('destroy', toplevel_unref)

		if _window_icon:
			args[0].set_icon(_window_icon)

class Dialog(g.Dialog):
	"""This works in exactly the same way as a GtkDialog, except that
	it calls the toplevel_(un)ref functions for you automatically."""
	def __init__(*args, **kwargs):
		apply(g.Dialog.__init__, args, kwargs)
		toplevel_ref()
		args[0].connect('destroy', toplevel_unref)

class ButtonMixed(g.Button):
	"""A button with a standard stock icon, but any label. This is useful
	when you want to express a concept similar to one of the stock ones."""
	def __init__(self, stock, message):
		"""Specify the icon and text for the new button. The text
		may specify the mnemonic for the widget by putting a _ before
		the letter, eg:
		button = ButtonMixed(g.STOCK_DELETE, '_Delete message')."""
		g.Button.__init__(self)
	
		label = g.Label('')
		label.set_text_with_mnemonic(message)
		label.set_mnemonic_widget(self)

		image = g.image_new_from_stock(stock, g.ICON_SIZE_BUTTON)
		box = g.HBox(FALSE, 2)
		align = g.Alignment(0.5, 0.5, 0.0, 0.0)

		box.pack_start(image, FALSE, FALSE, 0)
		box.pack_end(label, FALSE, FALSE, 0)

		self.add(align)
		align.add(box)
		align.show_all()

_toplevel_windows = 0
_in_mainloops = 0
def mainloop():
	"""This is a wrapper around the gtk2.mainloop function. It only runs
	the loop if there are top level references, and exits when
	rox.toplevel_unref() reduces the count to zero."""
	global _toplevel_windows, _in_mainloops

	_in_mainloops = _in_mainloops + 1	# Python1.5 syntax
	try:
		while _toplevel_windows:
			g.main()
	finally:
		_in_mainloops = _in_mainloops - 1

def toplevel_ref():
	"""Increment the toplevel ref count. rox.mainloop() won't exit until
	toplevel_unref() is called the same number of times."""
	global _toplevel_windows
	_toplevel_windows = _toplevel_windows + 1

def toplevel_unref(*unused):
	"""Decrement the toplevel ref count. If this is called while in
	rox.mainloop() and the count has reached zero, then rox.mainloop()
	will exit. Ignores any arguments passed in, so you can use it
	easily as a callback function."""
	global _toplevel_windows
	assert _toplevel_windows > 0
	_toplevel_windows = _toplevel_windows - 1
	if _toplevel_windows == 0 and _in_mainloops:
		g.main_quit()

_host_name = None
def our_host_name():
	"""Try to return the canonical name for this computer. This is used
	in the drag-and-drop protocol to work out whether a drop is coming from
	a remote machine (and therefore has to be fetched differently)."""
	from socket import getfqdn
	global _host_name
	if _host_name:
		return _host_name
	try:
		_host_name = getfqdn()
	except:
		_host_name = 'localhost'
		alert("ROX-Lib socket.getfqdn() failed!")
	return _host_name

def escape(uri):
	"Convert each space to %20, etc"
	import re
	return re.sub('[^:-_./a-zA-Z0-9]',
		lambda match: '%%%02x' % ord(match.group(0)),
		_to_utf8(uri)[0])

def unescape(uri):
	"Convert each %20 to a space, etc"
	if '%' not in uri: return uri
	import re
	return re.sub('%[0-9a-fA-F][0-9a-fA-F]',
		lambda match: chr(int(match.group(0)[1:], 16)),
		uri)

def get_local_path(uri):
	"""Convert 'uri' to a local path and return, if possible. If 'uri'
	is a resource on a remote machine, return None. URI is in the escaped form
	(%20 for space)."""
	if not uri:
		return None

	if uri[0] == '/':
		if uri[1:2] != '/':
			return unescape(uri)	# A normal Unix pathname
		i = uri.find('/', 2)
		if i == -1:
			return None	# //something
		if i == 2:
			return unescape(uri[2:])	# ///path
		remote_host = uri[2:i]
		if remote_host == our_host_name():
			return unescape(uri[i:])	# //localhost/path
		# //otherhost/path
	elif uri[:5].lower() == 'file:':
		if uri[5:6] == '/':
			return get_local_path(uri[5:])
	elif uri[:2] == './' or uri[:3] == '../':
		return unescape(uri)
	return None

app_options = None
def setup_app_options(program, leaf = 'Options.xml', site = None):
	"""Most applications only have one set of options. This function can be
	used to set up the default group. 'program' is the name of the
	directory to use and 'leaf' is the name of the file used to store the
	group. You can refer to the group using rox.app_options.

	If site is given, the basedir module is used for saving options (the
	new system). Otherwise, the deprecated choices module is used.

	See rox.options.OptionGroup."""
	global app_options
	assert not app_options
	from options import OptionGroup
	app_options = OptionGroup(program, leaf, site)

_options_box = None
def edit_options(options_file = None):
	"""Edit the app_options (set using setup_app_options()) using the GUI
	specified in 'options_file' (default <app_dir>/Options.xml).
	If this function is called again while the box is still open, the
	old box will be redisplayed to the user."""
	assert app_options

	global _options_box
	if _options_box:
		_options_box.present()
		return
	
	if not options_file:
		options_file = os.path.join(app_dir, 'Options.xml')
	
	import OptionsBox
	_options_box = OptionsBox.OptionsBox(app_options, options_file)

	def closed(widget):
		global _options_box
		assert _options_box == widget
		_options_box = None
	_options_box.connect('destroy', closed)
	_options_box.open()

try:
	import xml
except:
	alert(_("You do not have the Python 'xml' module installed, which "
	        "ROX-Lib2 requires. You need to install python-xmlbase "
	        "(this is a small package; the full PyXML package is not "
	        "required)."))

if g.pygtk_version[:2] == (1, 99) and g.pygtk_version[2] < 12:
	# 1.99.12 is really too old too, but RH8.0 uses it so we'll have
	# to work around any problems...
	sys.stderr.write('Your version of pygtk (%d.%d.%d) is too old. '
	      'Things might not work correctly.' % g.pygtk_version)
