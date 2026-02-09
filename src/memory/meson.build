memory = static_library(
  'memory',
  'HugeAllocator.cxx',
  'SparseBuffer.cxx',
  include_directories: inc,
  dependencies: [
    util_dep,
    system_dep,
  ],
)

memory_dep = declare_dependency(
  link_with: memory,
  dependencies: [
    util_dep,
  ],
)
