#!/usr/bin/env python3

import gi

gi.require_version('Template', '1.0')

from gi.repository import Gio
from gi.repository import Template

# Get our file to process
file = Gio.File.new_for_path('simple.tmpl')

# Create a new template and parse our input file
tmpl = Template.Template()
tmpl.parse_file(file, None)

# Create scope for expansion
scope = Template.Scope.new()

# Create and assign "title" variable in scope
title = scope.get('title')
title.assign_string('Example Title')

# Expand the template into stream
expanded = tmpl.expand_string(scope)
print(expanded)
