"""If you want your program to be translated into multiple languages you need
to do the following:

- Pass all strings that should be translated through the '_' function, eg:
	print _('Hello World!')

- Create a Messages subdirectory in your application.

- Run 'pygettext *.py' to extract all the marked strings.

- Copy messages.pot as Messages/<lang>.po and edit (see ROX-Lib2's README).

- Use msgfmt to convert the .po files to .gmo files.

- In your application, use the rox.i18n.translation() function to set the _ function:
	__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))
  (for libraries, just do '_ ='; don't mess up the builtins)

Note that the marked strings must be fixed. If you're using formats, mark up the
format, eg:

	print _('You have %d lives remaining') % lives

You might like to look at the scripts in ROX-Lib2's Messages directory for
more help.
"""

import os

def _expand_lang(locale):
	from locale import normalize
	locale = normalize(locale)
	COMPONENT_CODESET   = 1 << 0
	COMPONENT_TERRITORY = 1 << 1
	COMPONENT_MODIFIER  = 1 << 2
	# split up the locale into its base components
	mask = 0
	pos = locale.find('@')
	if pos >= 0:
		modifier = locale[pos:]
		locale = locale[:pos]
		mask |= COMPONENT_MODIFIER
	else:
		modifier = ''
	pos = locale.find('.')
	if pos >= 0:
		codeset = locale[pos:]
		locale = locale[:pos]
		mask |= COMPONENT_CODESET
	else:
		codeset = ''
	pos = locale.find('_')
	if pos >= 0:
		territory = locale[pos:]
		locale = locale[:pos]
		mask |= COMPONENT_TERRITORY
	else:
		territory = ''
	language = locale
	ret = []
	for i in range(mask+1):
		if not (i & ~mask):  # if all components for this combo exist ...
			val = language
			if i & COMPONENT_TERRITORY: val += territory
			if i & COMPONENT_CODESET:   val += codeset
			if i & COMPONENT_MODIFIER:  val += modifier
			ret.append(val)
	ret.reverse()
	return ret

def expand_languages(languages = None):
	# Get some reasonable defaults for arguments that were not supplied
	if languages is None:
		languages = []
		for envar in ('LANGUAGE', 'LC_ALL', 'LC_MESSAGES', 'LANG'):
			val = os.environ.get(envar)
			if val:
				languages = val.split(':')
				break
        if 'C' not in languages:
		languages.append('C')

	# now normalize and expand the languages
	nelangs = []
	for lang in languages:
		for nelang in _expand_lang(lang):
			if nelang not in nelangs:
				nelangs.append(nelang)
	return nelangs

# Locate a .mo file using the ROX strategy
def find(messages_dir, languages = None):
	"""Look in messages_dir for a .gmo file for the user's preferred language
	(or override this with the 'languages' argument). Returns the filename, or
	None if there was no translation."""
	# select a language
	for lang in expand_languages(languages):
		if lang == 'C':
			break
		mofile = os.path.join(messages_dir, '%s.gmo' % lang)
		if os.path.exists(mofile):
			return mofile
	return None

def translation(messages_dir, languages = None):
	"""Load the translation for the user's language and return a function
	which translates a string into its unicode equivalent."""
	mofile = find(messages_dir, languages)
	if not mofile:
		return lambda x: x
	import gettext
	return gettext.GNUTranslations(file(mofile)).ugettext

langs = expand_languages()
