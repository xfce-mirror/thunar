"""This is an internal module. Do not use it. GTK 2.4 will contain functions
that replace those defined here."""

import os
import basedir
import rox

theme_dirs = [os.path.join(os.environ.get('HOME', '/'), '.icons')] + \
		list(basedir.load_data_paths('icons'))

class Index:
	"""A theme's index.theme file."""
	def __init__(self, dir):
		self.dir = dir
		sections = file(os.path.join(dir, "index.theme")).read().split('\n[')
		self.sections = {}
		for s in sections:
			lines = s.split('\n')
			sname = lines[0].strip()
			
			# Python < 2.2.2 doesn't support an argument to strip...
			assert sname[-1] == ']'
			if sname.startswith('['):
				sname = sname[1:-1]
			else:
				sname = sname[:-1]
			
			section = self.sections[sname] = {}
			for line in lines[1:]:
				if not line.strip(): continue
				if line.startswith('#'): continue
				key, value = map(str.strip, line.split('=', 1))
				section[key] = value

		subdirs = self.get('Icon Theme', 'Directories')
		
		subdirs = subdirs.replace(';', ',')	# Just in case...
		
		self.subdirs = [SubDir(self, d) for d in subdirs.split(',')]

	def get(self, section, key):
		"None if not found"
		return self.sections.get(section, {}).get(key, None)

class SubDir:
	"""A subdirectory within a theme."""
	def __init__(self, index, subdir):
		icontype = index.get(subdir, 'Type')
		self.name = subdir
		self.size = int(index.get(subdir, 'Size'))
		if icontype == "Fixed":
			self.min_size = self.max_size = self.size
		elif icontype == "Threshold":
			threshold = int(index.get(subdir, 'Threshold'))
			self.min_size = self.size - threshold
			self.max_size = self.size + threshold
		elif icontype == "Scaled":
			self.min_size = int(index.get(subdir, 'MinSize'))
			self.max_size = int(index.get(subdir, 'MaxSize'))
		else:
			self.min_size = self.max_size = 100000

class IconTheme:
	"""Icon themes are located by searching through various directories. You can use an IconTheme
	to convert an icon name into a suitable image."""
	
	def __init__(self, name):
		self.name = name

		self.indexes = []
		for leaf in theme_dirs:
			theme_dir = os.path.join(leaf, name)
			index_file = os.path.join(theme_dir, 'index.theme')
			if os.path.exists(os.path.join(index_file)):
				try:
					self.indexes.append(Index(theme_dir))
				except:
					rox.report_exception()
	
	def lookup_icon(self, iconname, size):
		icon = self._lookup_this_theme(iconname, size)
		if icon: return icon
		# XXX: inherits
	
	def _lookup_this_theme(self, iconname, size):
		dirs = []
		for i in self.indexes:
			for d in i.subdirs:
				if size < d.min_size:
					diff = d.min_size - size
				elif size > d.max_size:
					diff = size - d.max_size
				else:
					diff = 0
				dirs.append((diff, os.path.join(i.dir, d.name)))

		# Sort by closeness of size
		dirs.sort()

		for _, subdir in dirs:
			for extension in ("png", "svg"):
				filename = os.path.join(subdir,
					iconname + '.' + extension)
				if os.path.exists(filename):
					return filename
		return None

rox_theme = IconTheme('ROX')
