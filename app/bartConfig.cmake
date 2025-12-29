# ### Bielefeld AR Tracker ###
# ===============================
# 
# Sets required variables for finding bart library:
#
# BART_FOUND
# BART_INCLUDE_DIRS
# BART_LIBRARY_DIRS
# BART_LIBRARIES
# BART_VERSION
# 
# Example:
# find_package(bart 0.1)
#
# ###############################################################################

INCLUDE(FindPkgConfig)

GET_FILENAME_COMPONENT(CONFIG_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

SET(BART_FOUND TRUE)
SET(BART_INCLUDE_DIRS "${CONFIG_DIR}/../../include")
SET(BART_LIBRARY_DIRS "${CONFIG_DIR}/../../lib")
SET(BART_VERSION "0.1.0")
SET(BART_LIBRARIES "${CONFIG_DIR}/../../lib/bart")


# TODO: find dependencies here and append their include dirs to BART_INCLUDE_DIRS
