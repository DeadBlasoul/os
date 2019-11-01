# Thread Building Blocks
set(tbb_root "${PROJECT_SOURCE_DIR}/third_party/tbb/")
include(${tbb_root}/cmake/TBBBuild.cmake)
tbb_build(TBB_ROOT ${tbb_root} CONFIG_DIR TBB_DIR)
