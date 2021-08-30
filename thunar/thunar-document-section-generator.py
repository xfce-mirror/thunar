import os

docs_dir = "../docs/reference/thunar/"

sections = "thunar-sections-skeleton.txt"
sections_skeleton =
"""
<INCLUDE>thunar/thunar-documented-headers.h</INCLUDE>

"""

docs_xml = "thunar-docs-skeleton.xml"
docs_xml_skeleton =
"""
"""

def skewer_to_camel(name):
    return ''.join(i.title() for i in name.split('-'))

for file_name in os.listdir() :
    if match := re.search("(.*)\.h", file_name) :
        header_name_skewer = match.group(1)
        header_name_camel  = skewer_to_camel(header_name_skewer)
        
    
    
