"""This module implements the Choices system for user preferences.
The environment variable CHOICESPATH gives a list of directories to search
for choices. Changed choices are saved back to the first directory in the
list.

The choices system is DEPRECATED. See choices.migrate().
"""

import os, sys
from os.path import exists

_migrated = {}

try:
	path = os.environ['CHOICESPATH']
	paths = path.split(':')
except KeyError:
	paths = [ os.environ['HOME'] + '/Choices',
		  '/usr/local/share/Choices',
		  '/usr/share/Choices' ]
	
def load(dir, leaf):
	"""When you want to load user choices, use this function. 'dir' is
	the subdirectory within Choices where the choices are saved (usually
	this will be the name of your program). 'leaf' is the file within it.
	If serveral files are present, the most important one is returned. If
	no files are there, returns None.
	Eg ('Edit', 'Options') - > '/usr/local/share/Choices/Edit/Options'"""

	assert dir not in _migrated

	for path in paths:
		if path:
			full = path + '/' + dir + '/' + leaf
			if exists(full):
				return full

	return None

def save(dir, leaf, create = 1):
	"""Returns a path to save to, or None if saving is disabled.
	If 'create' is FALSE then no directories are created. 'dir' and
	'leaf' are as for load()."""

	assert dir not in _migrated

	p = paths[0]
	if not p:
		return None

	if create and not os.path.exists(p):
		os.mkdir(p, 0x1ff)
	p = p + '/' + dir
	if create and not os.path.exists(p):
		os.mkdir(p, 0x1ff)
		
	return p + '/' + leaf

def migrate(dir, site):
	"""Move <Choices>/dir (if it exists) to $XDG_CONFIG_HOME/site/dir, and
	put a symlink in its place. The choices load and save functions cannot
	be used for 'dir' after this; use the basedir module instead. 'site'
	should be a domain name owned or managed by you. Eg:
	choices.migrate('Edit', 'rox.sourceforge.net')"""
	assert dir not in _migrated

	_migrated[dir] = True

	home_choices = paths[0]
	if not home_choices:
		return		# Saving disabled
	
	full = home_choices + '/' + dir
	if os.path.islink(full) or not os.path.exists(full):
		return

	import basedir
	dest = os.path.join(basedir.xdg_config_home, site, dir)
	if os.path.exists(dest):
		print >>sys.stderr, \
			"Old config directory '%s' and new config " \
			"directory '%s' both exist. Not migrating settings!" % \
			(full, dest)
		return
	
	site_dir = os.path.join(basedir.xdg_config_home, site)
	if not os.path.isdir(site_dir):
		os.makedirs(site_dir)
	os.rename(full, dest)
	os.symlink(dest, full)
