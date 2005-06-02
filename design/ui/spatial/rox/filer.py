"""An easy way to get ROX-Filer to do things."""

# Note: do a double-fork in case it's an old version of the filer
# and doesn't automatically background itself.
def _spawn(argv):
	from os import fork, _exit, execvp, waitpid
	child = fork()
	if child == 0:
		# We are the child
		child = fork()
		if child == 0:
			# Grandchild
			try:
				execvp(argv[0], argv)
			except:
				pass
			print "Warning: exec('%s') failed!" % argv[0]
			_exit(1)
		elif child == -1:
			print "Error: fork() failed!"
		_exit(1)
	elif child == -1:
		print "Error: fork() failed!"
	waitpid(child, 0)

def spawn_rox(args):
	"""Run rox (either from PATH or through Zero Install) with the
	given arguments."""
	import os.path
	for bindir in os.environ.get('PATH', '').split(':'):
		path = os.path.join(bindir, 'rox')
		if os.path.isfile(path):
			_spawn(('rox',) + args)
			return
	if os.path.exists('/uri/0install/rox.sourceforge.net'):
		_spawn(('/bin/0run', 'rox.sourceforge.net/rox 2002-01-01') + args)
	else:
		print "Didn't find rox in PATH, and Zero Install not present. Trying 'rox' anyway..."
		_spawn(('rox',) + args)

def open_dir(dir):
	"Open 'dir' in a new filer window."
	spawn_rox(('-d', dir))

def examine(file):
	"""'file' may have changed (maybe you just created it, for example). Update
	any filer views of it."""
	spawn_rox(('-x', file))

def show_file(file):
	"""Open a directory and draw the user's attention to this file. Useful for
	'Up' toolbar buttons that show where a file is saved."""
	spawn_rox(('-s', file))
