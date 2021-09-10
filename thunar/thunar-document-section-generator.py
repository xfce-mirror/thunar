import os
import re
import subprocess as shell

ignored_headers = ["thunar-private", "thunar-dbus-freedesktop-interfaces",  "thunar-dbus-service-infos", "thunar-marshal", "stamp-thunar-marshal"]

docs_dir = "../docs/reference/thunar/"

sections = "thunar-sections-skeleton.txt"
sections_skeleton = """
<INCLUDE>thunar/thunar-documented-headers.h</INCLUDE>

"""

docs_xml = "thunar-docs-skeleton.xml"
docs_xml_skeleton = """
"""

def skewer_to_camel(name):
    return ''.join(i.title() for i in name.split('-'))

for file_name in os.listdir() :
    if match := re.search("(.*)\.h", file_name) :
        header_name_skewer = match.group(1)
        if header_name_skewer in ignored_headers :
            continue
        header_name_camel  = skewer_to_camel(header_name_skewer)

        prototypes = shell.run(["ctags", "-x", "--c-kinds=fp", match.group(0)], text=True).stdout
        print (prototypes)
        
    
    
