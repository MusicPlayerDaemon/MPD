if enable_database
  sqlite_dep = dependency('sqlite3', version: '>= 3.7.3',
                          fallback: ['sqlite3', 'sqlite3_dep'],
                          required: get_option('sqlite'))
else
  sqlite_dep = dependency('', required: false)
endif

conf.set('ENABLE_SQLITE', sqlite_dep.found())
if not sqlite_dep.found()
  subdir_done()
endif

sqlite = static_library(
  'sqlite',
  'Database.cxx',
  'Error.cxx',
  include_directories: inc,
  dependencies: [
    sqlite_dep,
  ],
)

sqlite_dep = declare_dependency(
  link_with: sqlite,
  dependencies: [
    sqlite_dep,
  ],
)
