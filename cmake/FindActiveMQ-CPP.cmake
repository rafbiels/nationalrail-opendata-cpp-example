
find_path(ActiveMQ-CPP_INCLUDE_DIR
  NAMES activemq
  PATHS /usr/include /usr/local/include
  PATH_SUFFIXES activemq-cpp activemq-cpp-3.9.5
)

find_library(ActiveMQ-CPP_LIBRARY
  NAMES activemq-cpp
  PATHS /usr/lib /usr/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ActiveMQ-CPP
  FOUND_VAR ActiveMQ-CPP_FOUND
  REQUIRED_VARS
    ActiveMQ-CPP_LIBRARY
    ActiveMQ-CPP_INCLUDE_DIR
  VERSION_VAR ActiveMQ-CPP_VERSION
)

if(ActiveMQ-CPP_FOUND)
  set(ActiveMQ-CPP_LIBRARIES ${ActiveMQ-CPP_LIBRARY})
  set(ActiveMQ-CPP_INCLUDE_DIRS ${ActiveMQ-CPP_INCLUDE_DIR})
endif()

if(ActiveMQ-CPP_FOUND AND NOT TARGET ActiveMQ-CPP::activemq-cpp)
  add_library(ActiveMQ-CPP::activemq-cpp UNKNOWN IMPORTED)
  set_target_properties(ActiveMQ-CPP::activemq-cpp PROPERTIES
    IMPORTED_LOCATION "${ActiveMQ-CPP_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ActiveMQ-CPP_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  ActiveMQ-CPP_INCLUDE_DIR
  ActiveMQ-CPP_LIBRARY
)
