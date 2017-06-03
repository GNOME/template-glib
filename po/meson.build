i18n = import('i18n')

langs = [
  'cs',
  'de',
  'el',
  'es',
  'fur',
  'hu',
  'id',
  'pl',
  'pt',
  'pt_BR',
  'sl',
  'sr',
  'sv',
]

i18n.gettext('template-glib-1.0',
             languages: langs,
             args: [
               '--default-domain=' + meson.project_name(),
               '--from-code=UTF-8',
               '--add-comments',
               '--msgid-bugs-address=https://bugzilla.gnome.org/enter_bug.cgi?product=template-glib',

               '--keyword=_',
               '--keyword=N_',
               '--keyword=C_:1c,2',
               '--keyword=NC_:1c,2',
               '--keyword=g_dcgettext:2',
               '--keyword=g_dngettext:2,3',
               '--keyword=g_dpgettext2:2c,3',

               '--flag=N_:1:pass-c-format',
               '--flag=C_:2:pass-c-format',
               '--flag=NC_:2:pass-c-format',
               '--flag=g_dngettext:2:pass-c-format',
               '--flag=g_strdup_printf:1:c-format',
               '--flag=g_string_printf:2:c-format',
               '--flag=g_string_append_printf:2:c-format',
               '--flag=g_error_new:3:c-format',
               '--flag=g_set_error:4:c-format',
             ])
