"""All ROX applications that can save documents should use drag-and-drop saving.
The document itself should use the Saveable mix-in class and override some of the
methods to actually do the save.

If you want to save a selection then you can create a new object specially for
the purpose and pass that to the SaveBox."""

import os, sys
import rox
from rox import alert, info, g, _, filer, escape
from rox import choices, get_local_path

gdk = g.gdk

TARGET_XDS = 0
TARGET_RAW = 1

def _write_xds_property(context, value):
	win = context.source_window
	if value:
		win.property_change('XdndDirectSave0', 'text/plain', 8,
					gdk.PROP_MODE_REPLACE,
					value)
	else:
		win.property_delete('XdndDirectSave0')

def _read_xds_property(context, delete):
	"""Returns a UTF-8 encoded, non-escaped, URI."""
	win = context.source_window
	retval = win.property_get('XdndDirectSave0', 'text/plain', delete)
	if retval:
		return retval[2]
	return None
	
def image_for_type(type):
	'Search <Choices> for a suitable icon. Returns a pixbuf, or None.'
	from icon_theme import rox_theme
	media, subtype = type.split('/', 1)
	path = choices.load('MIME-icons', media + '_' + subtype + '.png')
	if not path:
		icon = 'mime-%s:%s' % (media, subtype)
		try:
			path = rox_theme.lookup_icon(icon, 48)
			if not path:
				icon = 'mime-%s' % media
				path = rox_theme.lookup_icon(icon, 48)
		except:
			print "Error loading MIME icon"
	if not path:
		path = choices.load('MIME-icons', media + '.png')
	if path:
		return gdk.pixbuf_new_from_file(path)
	else:
		return None

def _report_save_error():
	"Report a AbortSave nicely, otherwise use report_exception()"
	value = sys.exc_info()[1]
	if isinstance(value, AbortSave):
		value.show()
	else:
		rox.report_exception()

class AbortSave(rox.UserAbort):
	"""Raise this to cancel a save. If a message is given, it is displayed
	in a normal alert box (not in the report_exception style). If the
	message is None, no message is shown (you should have already shown
	it!)"""
	def __init__(self, message):
		self.message = message
		rox.UserAbort.__init__(self, message)
	
	def show(self):
		if self.message:
			rox.alert(self.message)

class Saveable:
	"""This class describes the interface that an object must provide
	to work with the SaveBox/SaveArea widgets. Inherit from it if you
	want to save. All methods can be overridden, but normally only
	save_to_stream() needs to be. You can also set save_last_stat to
	the result of os.stat(filename) when loading a file to make ROX-Lib
	restore permissions and warn about other programs editing the file."""

	save_last_stat = None

	def set_uri(self, uri):
		"""When the data is safely saved somewhere this is called
		with its new name. Mark your data as unmodified and update
		the filename for next time. Saving to another application
		won't call this method. Default method does nothing.
		The URI may be in the form of a URI or a local path.
		It is UTF-8, not escaped (% really means %)."""
		pass

	def save_to_stream(self, stream):
		"""Write the data to save to the stream. When saving to a
		local file, stream will be the actual file, otherwise it is a
		cStringIO object."""
		raise Exception('You forgot to write the save_to_stream() method...'
				'silly programmer!')

	def save_to_file(self, path):
		"""Write data to file. Raise an exception on error.
		The default creates a temporary file, uses save_to_stream() to
		write to it, then renames it over the original. If the temporary file
		can't be created, it writes directly over the original."""

		# Ensure the directory exists...
		parent_dir = os.path.dirname(path)
		if not os.path.isdir(parent_dir):
			from rox import fileutils
			try:
				fileutils.makedirs(parent_dir)
			except OSError:
				raise AbortSave(None)	# (message already shown)
		
		import random
		tmp = 'tmp-' + `random.randrange(1000000)`
		tmp = os.path.join(parent_dir, tmp)

		def open_private(path):
			return os.fdopen(os.open(path, os.O_CREAT | os.O_WRONLY, 0600), 'wb')
		
		try:
			stream = open_private(tmp)
		except:
			# Can't create backup... try a direct write
			tmp = None
			stream = open_private(path)
		try:
			try:
				self.save_to_stream(stream)
			finally:
				stream.close()
			if tmp:
				os.rename(tmp, path)
		except:
			_report_save_error()
			if tmp and os.path.exists(tmp):
				if os.path.getsize(tmp) == 0 or \
				   rox.confirm(_("Delete temporary file '%s'?") % tmp,
				   		g.STOCK_DELETE):
					os.unlink(tmp)
			raise AbortSave(None)
		self.save_set_permissions(path)
		filer.examine(path)

	def save_to_selection(self, selection_data):
		"""Write data to the selection. The default method uses save_to_stream()."""
		from cStringIO import StringIO
		stream = StringIO()
		self.save_to_stream(stream)
		selection_data.set(selection_data.target, 8, stream.getvalue())

	save_mode = None	# For backwards compat
	def save_set_permissions(self, path):
		"""The default save_to_file() creates files with the mode 0600
		(user read/write only). After saving has finished, it calls this
		method to set the final permissions. The save_set_permissions():
		- sets it to 0666 masked with the umask (if save_mode is None), or
		- sets it to save_last_stat.st_mode (not masked) otherwise."""
		if self.save_last_stat is not None:
			save_mode = self.save_last_stat.st_mode
		else:
			save_mode = self.save_mode
		
		if save_mode is not None:
			os.chmod(path, save_mode)
		else:
			mask = os.umask(0077)	# Get the current umask
			os.umask(mask)		# Set it back how it was
			os.chmod(path, 0666 & ~mask)
	
	def save_done(self):
		"""Time to close the savebox. Default method does nothing."""
		pass

	def discard(self):
		"""Discard button clicked, or document safely saved. Only called if a SaveBox 
		was created with discard=1.
		The user doesn't want the document any more, even if it's modified and unsaved.
		Delete it."""
		raise Exception("Sorry... my programmer forgot to tell me how to handle Discard!")
	
	save_to_stream._rox_default = 1
	save_to_file._rox_default = 1
	save_to_selection._rox_default = 1
	def can_save_to_file(self):
		"""Indicates whether we have a working save_to_stream or save_to_file
		method (ie, whether we can save to files). Default method checks that
		one of these two methods has been overridden."""
		if not hasattr(self.save_to_stream, '_rox_default'):
			return 1	# Have user-provided save_to_stream
		if not hasattr(self.save_to_file, '_rox_default'):
			return 1	# Have user-provided save_to_file
		return 0
	def can_save_to_selection(self):
		"""Indicates whether we have a working save_to_stream or save_to_selection
		method (ie, whether we can save to selections). Default methods checks that
		one of these two methods has been overridden."""
		if not hasattr(self.save_to_stream, '_rox_default'):
			return 1	# Have user-provided save_to_stream
		if not hasattr(self.save_to_selection, '_rox_default'):
			return 1	# Have user-provided save_to_file
		return 0
	
	def save_cancelled(self):
		"""If you multitask during a save (using a recursive mainloop) then the
		user may click on the Cancel button. This function gets called if so, and
		should cause the recursive mainloop to return."""
		raise Exception("Lazy programmer error: can't abort save!")

class SaveArea(g.VBox):
	"""A SaveArea contains the widgets used in a save box. You can use
	this to put a savebox area in a larger window."""
	
	document = None		# The Saveable with the data
	entry = None
	initial_uri = None	# The pathname supplied to the constructor
	
	def __init__(self, document, uri, type):
		"""'document' must be a subclass of Saveable.
		'uri' is the file's current location, or a simple name (eg 'TextFile')
		if it has never been saved.
		'type' is the MIME-type to use (eg 'text/plain').
		"""
		g.VBox.__init__(self, False, 0)

		self.document = document
		self.initial_uri = uri

		drag_area = self._create_drag_area(type)
		self.pack_start(drag_area, True, True, 0)
		drag_area.show_all()

		entry = g.Entry()
		entry.connect('activate', lambda w: self.save_to_file_in_entry())
		self.entry = entry
		self.pack_start(entry, False, True, 4)
		entry.show()

		entry.set_text(uri)
	
	def _set_icon(self, type):
		pixbuf = image_for_type(type)
		if pixbuf:
			self.icon.set_from_pixbuf(pixbuf)
		else:
			self.icon.set_from_stock(g.STOCK_MISSING_IMAGE, g.ICON_SIZE_DND)

	def _create_drag_area(self, type):
		align = g.Alignment()
		align.set(.5, .5, 0, 0)

		self.drag_box = g.EventBox()
		self.drag_box.set_border_width(4)
		self.drag_box.add_events(gdk.BUTTON_PRESS_MASK)
		align.add(self.drag_box)

		self.icon = g.Image()
		self._set_icon(type)

		self._set_drag_source(type)
		self.drag_box.connect('drag_begin', self.drag_begin)
		self.drag_box.connect('drag_end', self.drag_end)
		self.drag_box.connect('drag_data_get', self.drag_data_get)
		self.drag_in_progress = 0

		self.drag_box.add(self.icon)

		return align

	def set_type(self, type, icon = None):
		"""Change the icon and drag target to 'type'.
		If 'icon' is given (as a GtkImage) then that icon is used,
		otherwise an appropriate icon for the type is used."""
		if icon:
			self.icon.set_from_pixbuf(icon.get_pixbuf())
		else:
			self._set_icon(type)
		self._set_drag_source(type)
	
	def _set_drag_source(self, type):
		if self.document.can_save_to_file():
			targets = [('XdndDirectSave0', 0, TARGET_XDS)]
		else:
			targets = []
		if self.document.can_save_to_selection():
			targets = targets + [(type, 0, TARGET_RAW),
				  ('application/octet-stream', 0, TARGET_RAW)]

		if not targets:
			raise Exception("Document %s can't save!" % self.document)
		self.drag_box.drag_source_set(gdk.BUTTON1_MASK | gdk.BUTTON3_MASK,
					      targets,
					      gdk.ACTION_COPY | gdk.ACTION_MOVE)
	
	def save_to_file_in_entry(self):
		"""Call this when the user clicks on an OK button you provide."""
		uri = self.entry.get_text()
		path = get_local_path(escape(uri))

		if path:
			if not self.confirm_new_path(path):
				return
			try:
				self.set_sensitive(False)
				try:
					self.document.save_to_file(path)
				finally:
					self.set_sensitive(True)
				self.set_uri(uri)
				self.save_done()
			except:
				_report_save_error()
		else:
			rox.info(_("Drag the icon to a directory viewer\n"
				   "(or enter a full pathname)"))
	
	def drag_begin(self, drag_box, context):
		self.drag_in_progress = 1
		self.destroy_on_drag_end = 0
		self.using_xds = 0
		self.data_sent = 0

		try:
			pixbuf = self.icon.get_pixbuf()
			if pixbuf:
				drag_box.drag_source_set_icon_pixbuf(pixbuf)
		except:
			# This can happen if we set the broken image...
			import traceback
			traceback.print_exc()

		uri = self.entry.get_text()
		if uri:
			i = uri.rfind('/')
			if (i == -1):
				leaf = uri
			else:
				leaf = uri[i + 1:]
		else:
			leaf = _('Unnamed')
		_write_xds_property(context, leaf)
	
	def drag_data_get(self, widget, context, selection_data, info, time):
		if info == TARGET_RAW:
			try:
				self.set_sensitive(False)
				try:
					self.document.save_to_selection(selection_data)
				finally:
					self.set_sensitive(True)
			except:
				_report_save_error()
				_write_xds_property(context, None)
				return

			self.data_sent = 1
			_write_xds_property(context, None)
			
			if self.drag_in_progress:
				self.destroy_on_drag_end = 1
			else:
				self.save_done()
			return
		elif info != TARGET_XDS:
			_write_xds_property(context, None)
			alert("Bad target requested!")
			return

		# Using XDS:
		#
		# Get the path that the destination app wants us to save to.
		# If it's local, save and return Success
		#			  (or Error if save fails)
		# If it's remote, return Failure (remote may try another method)
		# If no URI is given, return Error
		to_send = 'E'
		uri = _read_xds_property(context, False)
		if uri:
			path = get_local_path(escape(uri))
			if path:
				if not self.confirm_new_path(path):
					to_send = 'E'
				else:
					try:
						self.set_sensitive(False)
						try:
							self.document.save_to_file(path)
						finally:
							self.set_sensitive(True)
						self.data_sent = True
					except:
						_report_save_error()
						self.data_sent = False
					if self.data_sent:
						to_send = 'S'
				# (else Error)
			else:
				to_send = 'F'	# Non-local transfer
		else:
			alert("Remote application wants to use " +
				  "Direct Save, but I can't read the " +
				  "XdndDirectSave0 (type text/plain) " +
				  "property.")

		selection_data.set(selection_data.target, 8, to_send)
	
		if to_send != 'E':
			_write_xds_property(context, None)
			self.set_uri(uri)
		if self.data_sent:
			self.save_done()
	
	def confirm_new_path(self, path):
		"""User wants to save to this path. If it's different to the original path,
		check that it doesn't exist and ask for confirmation if it does.
		If document.save_last_stat is set, compare with os.stat for an existing file
		and warn about changes.
		Returns true to go ahead with the save."""
		if not os.path.exists(path):
			return True
		if path == self.initial_uri:
			if self.document.save_last_stat is None:
				return True		# OK. Nothing to compare with.
			last = self.document.save_last_stat
			stat = os.stat(path)
			msg = []
			if stat.st_mode != last.st_mode:
				msg.append(_("Permissions changed from %o to %o.") % \
						(last.st_mode, stat.st_mode))
			if stat.st_size != last.st_size:
				msg.append(_("Size was %d bytes; now %d bytes.") % \
						(last.st_size, stat.st_size))
			if stat.st_mtime != last.st_mtime:
				msg.append(_("Modification time changed."))
			if not msg:
				return True		# No change detected
			return rox.confirm("File '%s' edited by another program since last load/save. "
					   "Really save (discarding other changes)?\n\n%s" %
					   (path, '\n'.join(msg)), g.STOCK_DELETE)
		return rox.confirm(_("File '%s' already exists -- overwrite it?") % path,
				   g.STOCK_DELETE, _('_Overwrite'))
	
	def set_uri(self, uri):
		"""Data is safely saved somewhere. Update the document's URI and save_last_stat (for local saves).
		URI is not escaped. Internal."""
		path = get_local_path(escape(uri))
		if path is not None:
			self.document.save_last_stat = os.stat(path)	# Record for next time
		self.document.set_uri(path or uri)
	
	def drag_end(self, widget, context):
		self.drag_in_progress = 0
		if self.destroy_on_drag_end:
			self.save_done()

	def save_done(self):
		self.document.save_done()

class SaveBox(g.Dialog):
	"""A SaveBox is a GtkDialog that contains a SaveArea and, optionally, a Discard button.
	Calls rox.toplevel_(un)ref automatically.
	"""
	save_area = None

	def __init__(self, document, uri, type = 'text/plain', discard = False):
		"""See SaveArea.__init__.
		If discard is True then an extra discard button is added to the dialog."""
		g.Dialog.__init__(self)
		self.set_has_separator(False)

		self.add_button(g.STOCK_CANCEL, g.RESPONSE_CANCEL)
		self.add_button(g.STOCK_SAVE, g.RESPONSE_OK)
		self.set_default_response(g.RESPONSE_OK)

		if discard:
			discard_area = g.HButtonBox()

			def discard_clicked(event):
				document.discard()
				self.destroy()
			button = rox.ButtonMixed(g.STOCK_DELETE, _('_Discard'))
			discard_area.pack_start(button, False, True, 2)
			button.connect('clicked', discard_clicked)
			button.unset_flags(g.CAN_FOCUS)
			button.set_flags(g.CAN_DEFAULT)
			self.vbox.pack_end(discard_area, False, True, 0)
			self.vbox.reorder_child(discard_area, 0)
			
			discard_area.show_all()

		self.set_title(_('Save As:'))
		self.set_position(g.WIN_POS_MOUSE)
		self.set_wmclass('savebox', 'Savebox')
		self.set_border_width(1)

		# Might as well make use of the new nested scopes ;-)
		self.set_save_in_progress(0)
		class BoxedArea(SaveArea):
			def set_uri(area, uri):
				SaveArea.set_uri(area, uri)
				if discard:
					document.discard()
			def save_done(area):
				document.save_done()
				self.destroy()

			def set_sensitive(area, sensitive):
				if self.window:
					# Might have been destroyed by now...
					self.set_save_in_progress(not sensitive)
					SaveArea.set_sensitive(area, sensitive)
		save_area = BoxedArea(document, uri, type)
		self.save_area = save_area

		save_area.show_all()
		self.build_main_area()

		i = uri.rfind('/')
		i = i + 1
		# Have to do this here, or the selection gets messed up
		save_area.entry.grab_focus()
		g.Editable.select_region(save_area.entry, i, -1) # PyGtk bug
		#save_area.entry.select_region(i, -1)

		def got_response(widget, response):
			if self.save_in_progress:
				try:
					document.save_cancelled()
				except:
					rox.report_exception()
				return
			if response == g.RESPONSE_CANCEL:
				self.destroy()
			elif response == g.RESPONSE_OK:
				self.save_area.save_to_file_in_entry()
			elif response == g.RESPONSE_DELETE_EVENT:
				pass
			else:
				raise Exception('Unknown response!')
		self.connect('response', got_response)

		rox.toplevel_ref()
		self.connect('destroy', lambda w: rox.toplevel_unref())
	
	def set_type(self, type, icon = None):
		"""See SaveArea's method of the same name."""
		self.save_area.set_type(type, icon)

	def build_main_area(self):
		"""Place self.save_area somewhere in self.vbox. Override this
		for more complicated layouts."""
		self.vbox.add(self.save_area)
	
	def set_save_in_progress(self, in_progress):
		"""Called when saving starts and ends. Shade/unshade any widgets as
		required. Make sure you call the default method too!
		Not called if box is destroyed from a recursive mainloop inside
		a save_to_* function."""
		self.set_response_sensitive(g.RESPONSE_OK, not in_progress)
		self.save_in_progress = in_progress

class StringSaver(SaveBox, Saveable):
	"""A very simple SaveBox which saves the string passed to its constructor."""
	def __init__(self, string, name):
		"""'string' is the string to save. 'name' is the default filename"""
		SaveBox.__init__(self, self, name, 'text/plain')
		self.string = string
	
	def save_to_stream(self, stream):
		stream.write(self.string)

class SaveFilter(Saveable):
	"""This Saveable runs a process in the background to generate the
	save data. Any python streams can be used as the input to and
	output from the process.
	
	The output from the subprocess is saved to the output stream (either
	directly, for fileno() streams, or via another temporary file).

	If the process returns a non-zero exit status or writes to stderr,
	the save fails (messages written to stderr are displayed).
	"""

	command = None
	stdin = None

	def set_stdin(self, stream):
		"""Use 'stream' as stdin for the process. If stream is not a
		seekable fileno() stream then it is copied to a temporary file
		at this point. If None, the child process will get /dev/null on
		stdin."""
		if stream is not None:
			if hasattr(stream, 'fileno') and hasattr(stream, 'seek'):
				self.stdin = stream
			else:
				import tempfile
				import shutil
				self.stdin = tempfile.TemporaryFile()
				shutil.copyfileobj(stream, self.stdin)
		else:
			self.stdin = None
	
	def save_to_stream(self, stream):
		from processes import PipeThroughCommand

		assert not hasattr(self, 'child_run')	# No longer supported

		self.process = PipeThroughCommand(self.command, self.stdin, stream)
		self.process.wait()
		self.process = None
	
	def save_cancelled(self):
		"""Send SIGTERM to the child processes."""
		if self.process:
			self.killed = 1
			self.process.kill()
